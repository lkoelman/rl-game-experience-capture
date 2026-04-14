#pragma once

#include <sstream>
#include <string>

#include "CaptureTarget.hpp"

namespace trajectory {

inline std::string NormalizePathForGStreamer(std::string path) {
    for (char& ch : path) {
        if (ch == '\\') {
            ch = '/';
        }
    }
    return path;
}

inline std::string BuildPipelineDescriptionForTesting(const std::string& output_path, const CaptureTarget& capture_target) {
    std::ostringstream pipeline;
    pipeline << "d3d11screencapturesrc";
    if (capture_target.kind == CaptureTargetKind::monitor) {
        if (capture_target.monitor_handle != 0) {
            pipeline << " monitor-handle=" << capture_target.monitor_handle;
        } else {
            pipeline << " monitor-index=" << capture_target.monitor_index;
        }
    } else {
        pipeline << " window-handle=" << capture_target.window_handle;
    }
    pipeline << " ! videoconvert ! videoscale ! "
           "video/x-raw,framerate=30/1,width=1280,height=720 ! "
           "identity name=probe_point ! x264enc tune=zerolatency speed-preset=veryfast ! "
           "mp4mux ! filesink location=\"" << NormalizePathForGStreamer(output_path) << "\"";
    return pipeline.str();
}

}  // namespace trajectory
