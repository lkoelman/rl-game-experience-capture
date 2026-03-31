#pragma once

#include <gst/gst.h>

#include <memory>
#include <string>
#include <thread>

#include "SyncLogger.hpp"

namespace trajectory {

class VideoRecorder {
public:
    VideoRecorder(const std::string& output_path, std::shared_ptr<SyncLogger> sync_logger);
    ~VideoRecorder();

    void Start();
    void Stop();

private:
    static GstPadProbeReturn PadProbeCallback(GstPad* pad, GstPadProbeInfo* info, gpointer user_data);
    static gboolean BusCall(GstBus* bus, GstMessage* msg, gpointer data);

    std::string output_path_;
    std::shared_ptr<SyncLogger> sync_logger_;
    GstElement* pipeline_{nullptr};
    GMainLoop* loop_{nullptr};
    std::thread loop_thread_;
    bool started_{false};
};

}  // namespace trajectory

