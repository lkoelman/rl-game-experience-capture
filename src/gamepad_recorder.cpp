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
    recording_ = true;
    return true;
}

void GamepadRecorder::StopRecording() {
    if (!recording_) {
        return;
    }

    recording_ = false;

    // Save recording to file
    std::ofstream output(output_file_, std::ios::binary);
    if (!recording_data_.SerializeToOstream(&output)) {
        std::cerr << "Failed to write gamepad recording to file" << std::endl;
    }

    recording_data_.Clear();
}

void GamepadRecorder::Update() {
    if (!recording_) {
        return;
    }

    // Check all possible gamepad indices
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
        XINPUT_STATE state;
        if (XInputGetState(i, &state) == ERROR_SUCCESS) {
            auto* gamepad_state = recording_data_.add_states();
            
            // Set timestamp
            gamepad_state->set_timestamp(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()
            );

            // Set gamepad index
            gamepad_state->set_gamepad_index(i);

            // Set buttons bitmap
            gamepad_state->set_buttons(state.Gamepad.wButtons);

            // Set triggers
            gamepad_state->set_left_trigger(state.Gamepad.bLeftTrigger);
            gamepad_state->set_right_trigger(state.Gamepad.bRightTrigger);

            // Set thumbsticks
            auto* left_thumb = new recorder::GamepadState::ThumbStick();
            left_thumb->set_x(state.Gamepad.sThumbLX);
            left_thumb->set_y(state.Gamepad.sThumbLY);
            gamepad_state->set_allocated_left_thumb(left_thumb);

            auto* right_thumb = new recorder::GamepadState::ThumbStick();
            right_thumb->set_x(state.Gamepad.sThumbRX);
            right_thumb->set_y(state.Gamepad.sThumbRY);
            gamepad_state->set_allocated_right_thumb(right_thumb);
        }
    }
}
