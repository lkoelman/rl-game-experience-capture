#include "VideoRecorder.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <utility>

#include "VideoRecorderPipeline.hpp"

namespace trajectory {

namespace {

constexpr auto kStopTimeout = std::chrono::seconds(5);

// Uses steady_clock so video/input alignment stays in the same monotonic time domain.
std::uint64_t NowMonotonicNs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

}  // namespace

VideoRecorder::VideoRecorder(const std::string& output_path, CaptureTarget capture_target, std::shared_ptr<SyncLogger> sync_logger)
    : output_path_(output_path), sync_logger_(std::move(sync_logger)), capture_target_(std::move(capture_target)) {}

VideoRecorder::~VideoRecorder() {
    Stop();
}

void VideoRecorder::Start() {
    if (started_) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(stop_mutex_);
        eos_or_error_received_ = false;
        stop_error_message_.clear();
    }

    GError* error = nullptr;
    pipeline_ = gst_parse_launch(BuildPipelineDescriptionForTesting(output_path_, capture_target_).c_str(), &error);
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

    // The probe is the only place where video PTS and the local monotonic clock are observed together.
    gst_pad_add_probe(src_pad, GST_PAD_PROBE_TYPE_BUFFER, &VideoRecorder::PadProbeCallback, this, nullptr);
    gst_object_unref(src_pad);
    gst_object_unref(probe);

    auto* bus = gst_element_get_bus(pipeline_);
    loop_ = g_main_loop_new(nullptr, FALSE);
    gst_bus_add_watch(bus, &VideoRecorder::BusCall, this);
    gst_object_unref(bus);

    // GStreamer bus messages are handled on a dedicated GLib loop instead of the caller thread.
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

    if (pipeline_ != nullptr && started_) {
        bool wait_for_finalize = false;
        {
            std::lock_guard<std::mutex> lock(stop_mutex_);
            eos_or_error_received_ = false;
            stop_error_message_.clear();
        }

        // EOS lets muxers flush their trailers before the pipeline is forced to NULL.
        if (!gst_element_send_event(pipeline_, gst_event_new_eos())) {
            std::cerr << "Warning: failed to send EOS to the GStreamer pipeline during shutdown." << std::endl;
        } else {
            wait_for_finalize = true;
        }

        if (wait_for_finalize) {
            std::unique_lock<std::mutex> lock(stop_mutex_);
            if (!stop_cv_.wait_for(lock, kStopTimeout, [this]() { return eos_or_error_received_; })) {
                std::cerr << "Warning: timed out waiting for the video pipeline to finalize after EOS." << std::endl;
            } else if (!stop_error_message_.empty()) {
                std::cerr << "Warning: video pipeline reported an error while finalizing output: " << stop_error_message_ << std::endl;
            }
        }
    }

    if (pipeline_ != nullptr) {
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
        {
            std::lock_guard<std::mutex> lock(self->stop_mutex_);
            self->eos_or_error_received_ = true;
            self->stop_error_message_.clear();
        }
        self->stop_cv_.notify_all();
        if (self->loop_ != nullptr) {
            g_main_loop_quit(self->loop_);
        }
        return FALSE;
    case GST_MESSAGE_ERROR:
        {
            GError* error = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_error(msg, &error, &debug);

            std::lock_guard<std::mutex> lock(self->stop_mutex_);
            self->eos_or_error_received_ = true;
            self->stop_error_message_ = error != nullptr ? error->message : "unknown GStreamer error";

            if (debug != nullptr) {
                self->stop_error_message_ += " (";
                self->stop_error_message_ += debug;
                self->stop_error_message_ += ")";
            }

            if (error != nullptr) {
                g_error_free(error);
            }
            if (debug != nullptr) {
                g_free(debug);
            }
        }
        self->stop_cv_.notify_all();
        if (self->loop_ != nullptr) {
            g_main_loop_quit(self->loop_);
        }
        return FALSE;
    default:
        return TRUE;
    }
}

}  // namespace trajectory
