#pragma once

#include <string>

namespace trajectory {

inline std::string NormalizePathForGStreamer(std::string path) {
    for (char& ch : path) {
        if (ch == '\\') {
            ch = '/';
        }
    }
    return path;
}

inline std::string BuildPipelineDescriptionForTesting(const std::string& output_path) {
    return "d3d11screencapturesrc ! videoconvert ! videoscale ! "
           "video/x-raw,framerate=30/1,width=1280,height=720 ! "
           "identity name=probe_point ! x264enc tune=zerolatency speed-preset=veryfast ! "
           "mp4mux ! filesink location=\"" +
           NormalizePathForGStreamer(output_path) + "\"";
}

}  // namespace trajectory
