#pragma once

#include <memory>
#include <string>

#include "InputLogger.hpp"
#include "SyncLogger.hpp"
#include "VideoRecorder.hpp"

namespace trajectory {

class Session {
public:
    Session(const std::string& output_dir, const std::string& session_name);
    void Start();
    void Stop();

private:
    std::shared_ptr<SyncLogger> sync_logger_;
    std::unique_ptr<VideoRecorder> video_recorder_;
    std::unique_ptr<InputLogger> input_logger_;
};

}  // namespace trajectory

