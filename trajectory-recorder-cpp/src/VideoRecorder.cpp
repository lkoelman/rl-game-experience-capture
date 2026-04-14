#include "VideoRecorder.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

namespace trajectory {

namespace {

std::uint64_t NowMonotonicNs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

std::string BuildPipelineDescription(const std::string& output_path) {
    return "d3d11screencapturesrc ! videoconvert ! videoscale ! "
           "video/x-raw,framerate=30/1,width=1280,height=720 ! "
           "identity name=probe_point ! x264enc tune=zerolatency speed-preset=veryfast ! "
           "mp4mux ! filesink location=\"" +
           output_path + "\"";
}

}  // namespace

VideoRecorder::VideoRecorder(const std::string& output_path, std::shared_ptr<SyncLogger> sync_logger)
    : output_path_(output_path), sync_logger_(std::move(sync_logger)) {}

VideoRecorder::~VideoRecorder() {
    Stop();
}

void VideoRecorder::Start() {
    if (started_) {
        return;
    }

    GError* error = nullptr;
    pipeline_ = gst_parse_launch(BuildPipelineDescription(output_path_).c_str(), &error);
    if (pipeline_ == nullptr) {
        const std::string message = error != nullptr ? error->message : "unknown GStreamer pipeline error";
        if (error != nullptr) {
            g_error_free(error);
        }
        throw std::runtime_error(message);
    }

    auto* probe = gst_bin_get_by_name(GST_BIN(pipeline_), "probe_point");
    if (probe == nullptr) {
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        throw std::runtime_error("failed to find probe_point element in pipeline");
    }

    auto* src_pad = gst_element_get_static_pad(probe, "src");
    if (src_pad == nullptr) {
        gst_object_unref(probe);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        throw std::runtime_error("failed to get probe_point src pad");
    }

    // Add probe callback to log frame timestamps
    gst_pad_add_probe(src_pad, GST_PAD_PROBE_TYPE_BUFFER, &VideoRecorder::PadProbeCallback, this, nullptr);
    gst_object_unref(src_pad);
    gst_object_unref(probe);

    auto* bus = gst_element_get_bus(pipeline_);
    loop_ = g_main_loop_new(nullptr, FALSE);
    gst_bus_add_watch(bus, &VideoRecorder::BusCall, this);
    gst_object_unref(bus);

    loop_thread_ = std::thread([this]() { g_main_loop_run(loop_); });

    if (gst_element_set_state(pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        Stop();
        throw std::runtime_error("failed to start GStreamer pipeline");
    }

    started_ = true;
}

void VideoRecorder::Stop() {
    if (!started_ && pipeline_ == nullptr) {
        return;
    }

    if (pipeline_ != nullptr) {
        gst_element_send_event(pipeline_, gst_event_new_eos());
        gst_element_set_state(pipeline_, GST_STATE_NULL);
    }

    if (loop_ != nullptr) {
        g_main_loop_quit(loop_);
    }
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
    if (loop_ != nullptr) {
        g_main_loop_unref(loop_);
        loop_ = nullptr;
    }
    if (pipeline_ != nullptr) {
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }

    started_ = false;
}

GstPadProbeReturn VideoRecorder::PadProbeCallback(GstPad*, GstPadProbeInfo* info, gpointer user_data) {
    if ((info->type & GST_PAD_PROBE_TYPE_BUFFER) == 0) {
        return GST_PAD_PROBE_OK;
    }

    auto* self = static_cast<VideoRecorder*>(user_data);
    auto* buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (buffer != nullptr && self->sync_logger_ != nullptr) {
        self->sync_logger_->LogFrame(NowMonotonicNs(), GST_BUFFER_PTS(buffer));
    }
    return GST_PAD_PROBE_OK;
}

gboolean VideoRecorder::BusCall(GstBus*, GstMessage* msg, gpointer data) {
    auto* self = static_cast<VideoRecorder*>(data);
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
        if (self->loop_ != nullptr) {
            g_main_loop_quit(self->loop_);
        }
        return FALSE;
    case GST_MESSAGE_ERROR:
        if (self->loop_ != nullptr) {
            g_main_loop_quit(self->loop_);
        }
        return FALSE;
    default:
        return TRUE;
    }
}

}  // namespace trajectory
