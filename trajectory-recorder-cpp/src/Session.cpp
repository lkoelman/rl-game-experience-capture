#include "Session.hpp"

#include <filesystem>

#include "InputLogger.hpp"
#include "SyncLogger.hpp"
#include "VideoRecorder.hpp"

namespace trajectory {

namespace {

// Materializes the per-session output folder before any recorder component starts writing.
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

Session::~Session() = default;

void Session::Start() {
    // Input starts first so the earliest controller/keyboard events are captured before video is live.
    input_logger_->Start();
    video_recorder_->Start();
}

void Session::PumpEventsOnce() {
    input_logger_->PumpEventsOnce();
}

void Session::Stop() {
    // Video stops first so the sync log is finalized before input capture exits.
    video_recorder_->Stop();
    input_logger_->Stop();
}

}  // namespace trajectory
