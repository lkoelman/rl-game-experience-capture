#include "gamepad_recorder.h"
#include <fstream>
#include <chrono>
#include <iostream>

GamepadRecorder::GamepadRecorder()
    : recording_(false)
{
}

GamepadRecorder::~GamepadRecorder() {
    StopRecording();
}

bool GamepadRecorder::StartRecording(const std::string& output_file) {
    if (recording_) {
        return false;
    }

    output_file_ = output_file;
    csv_file_.open(output_file_, std::ios::out);
    if (!csv_file_.is_open()) {
        return false;
    }

    // Write CSV header
    csv_file_ << "timestamp,gamepad_index,buttons,left_trigger,right_trigger,left_thumb_x,left_thumb_y,right_thumb_x,right_thumb_y\n";
    
    recording_ = true;
    return true;
}

void GamepadRecorder::StopRecording() {
    if (!recording_) {
        return;
    }

    recording_ = false;
    csv_file_.close();
}

void GamepadRecorder::Update() {
    if (!recording_) {
        return;
    }

    // Check all possible gamepad indices
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
        XINPUT_STATE state;
        if (XInputGetState(i, &state) == ERROR_SUCCESS) {
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            csv_file_ << timestamp << ","
                     << i << ","
                     << state.Gamepad.wButtons << ","
                     << static_cast<int>(state.Gamepad.bLeftTrigger) << ","
                     << static_cast<int>(state.Gamepad.bRightTrigger) << ","
                     << state.Gamepad.sThumbLX << ","
                     << state.Gamepad.sThumbLY << ","
                     << state.Gamepad.sThumbRX << ","
                     << state.Gamepad.sThumbRY << "\n";
        }
    }
}
