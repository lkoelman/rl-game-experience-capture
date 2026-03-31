#pragma once

#include <memory>
#include <string>

namespace trajectory {

class InputLogger;
class SyncLogger;
class VideoRecorder;

class Session {
public:
    Session(const std::string& output_dir, const std::string& session_name);
    ~Session();
    void Start();
    void Stop();

private:
    std::shared_ptr<SyncLogger> sync_logger_;
    std::unique_ptr<VideoRecorder> video_recorder_;
    std::unique_ptr<InputLogger> input_logger_;
};

}  // namespace trajectory
