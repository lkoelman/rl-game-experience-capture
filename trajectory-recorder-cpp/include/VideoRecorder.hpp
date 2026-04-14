#pragma once

#include <gst/gst.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "SyncLogger.hpp"

namespace trajectory {

// Owns the GStreamer pipeline that captures video and emits sync timestamps.
class VideoRecorder {
public:
    VideoRecorder(const std::string& output_path, std::shared_ptr<SyncLogger> sync_logger);
    ~VideoRecorder();

    // Builds the pipeline, attaches the probe, and starts the GLib main loop thread.
    void Start();

    // Sends EOS and blocks until the GLib loop and pipeline are torn down.
    void Stop();

private:
    // Runs on the GStreamer streaming thread to pair frame PTS with a monotonic clock timestamp.
    static GstPadProbeReturn PadProbeCallback(GstPad* pad, GstPadProbeInfo* info, gpointer user_data);

    // Runs on the GLib main loop thread to terminate on EOS or pipeline error.
    static gboolean BusCall(GstBus* bus, GstMessage* msg, gpointer data);

    std::string output_path_;
    std::shared_ptr<SyncLogger> sync_logger_;
    GstElement* pipeline_{nullptr};
    GMainLoop* loop_{nullptr};
    std::thread loop_thread_;
    std::mutex stop_mutex_;
    std::condition_variable stop_cv_;
    bool eos_or_error_received_{false};
    std::string stop_error_message_;
    bool started_{false};
};

}  // namespace trajectory
