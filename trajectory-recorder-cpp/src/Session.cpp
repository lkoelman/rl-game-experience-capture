#include "Session.hpp"

#include <filesystem>

namespace trajectory {

namespace {

std::filesystem::path EnsureSessionDirectory(const std::string& output_dir, const std::string& session_name) {
    auto session_dir = std::filesystem::path(output_dir) / session_name;
    std::filesystem::create_directories(session_dir);
    return session_dir;
}

}  // namespace

Session::Session(const std::string& output_dir, const std::string& session_name) {
    const auto session_dir = EnsureSessionDirectory(output_dir, session_name);
    sync_logger_ = std::make_shared<SyncLogger>((session_dir / "sync.csv").string());
    video_recorder_ = std::make_unique<VideoRecorder>((session_dir / "capture.mp4").string(), sync_logger_);
    input_logger_ = std::make_unique<InputLogger>((session_dir / "actions.bin").string());
}

void Session::Start() {
    input_logger_->Start();
    video_recorder_->Start();
}

void Session::Stop() {
    video_recorder_->Stop();
    input_logger_->Stop();
}

}  // namespace trajectory

