#include "RecordingSession.hpp"

#include <filesystem>

#include "GamepadLogger.hpp"
#include "FrameTimestampLogger.hpp"
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

RecordingSession::RecordingSession(const std::string& output_dir, const std::string& session_name, CaptureTarget capture_target, bool verbose) {
    const auto session_dir = EnsureSessionDirectory(output_dir, session_name);
    frame_timestamp_logger_ = std::make_shared<FrameTimestampLogger>((session_dir / "sync.csv").string());
    video_recorder_ =
        std::make_unique<VideoRecorder>((session_dir / "capture.mp4").string(), std::move(capture_target), frame_timestamp_logger_);
    gamepad_logger_ = std::make_unique<GamepadLogger>((session_dir / "actions.bin").string(), verbose);
}

RecordingSession::~RecordingSession() = default;

void RecordingSession::Start() {
    StartInputPreview();
    StartRecording();
}

void RecordingSession::StartInputPreview() {
    gamepad_logger_->StartForwarding();
}

GamepadPumpResult RecordingSession::PumpInputPreviewOnce() {
    return gamepad_logger_->PumpEventsOnce(GamepadPumpMode::preview);
}

void RecordingSession::StartRecording() {
    // Input recording starts first so the initial controller state is captured before video is live.
    gamepad_logger_->BeginRecording();
    video_recorder_->Start();
}

void RecordingSession::PumpEventsOnce() {
    gamepad_logger_->PumpEventsOnce();
}

void RecordingSession::Stop() {
    // Video stops first so the sync log is finalized before input capture exits.
    video_recorder_->Stop();
    gamepad_logger_->Stop();
}

}  // namespace trajectory
