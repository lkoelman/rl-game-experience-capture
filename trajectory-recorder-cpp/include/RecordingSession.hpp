#pragma once

#include <memory>
#include <string>

#include "CaptureTarget.hpp"
#include "GamepadPump.hpp"

namespace trajectory {

class GamepadLogger;
class FrameTimestampLogger;
class VideoRecorder;

// Coordinates the recorder subcomponents for a single output session.
class RecordingSession {
public:
    RecordingSession(const std::string& output_dir, const std::string& session_name, CaptureTarget capture_target, bool verbose);
    ~RecordingSession();

    // Starts virtual gamepad forwarding without writing session artifacts.
    void StartInputPreview();

    // Pumps input forwarding before recording begins.
    GamepadPumpResult PumpInputPreviewOnce();

    // Starts action logging and video capture for the session directory.
    void StartRecording();

    // Starts input capture and video capture for the session directory.
    void Start();

    // Pumps per-frame recorder work that must stay on the main thread.
    void PumpEventsOnce();

    // Stops video first so frame logging closes before input capture shuts down.
    void Stop();

private:
    std::shared_ptr<FrameTimestampLogger> frame_timestamp_logger_;
    std::unique_ptr<VideoRecorder> video_recorder_;
    std::unique_ptr<GamepadLogger> gamepad_logger_;
};

}  // namespace trajectory
