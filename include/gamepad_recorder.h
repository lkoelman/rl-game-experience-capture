#pragma once

#include <windows.h>
#include <xinput.h>
#include <string>
#include "gamepad_input.pb.h"

class GamepadRecorder {
public:
    GamepadRecorder();
    ~GamepadRecorder();

    bool StartRecording(const std::string& output_file);
    void StopRecording();
    void Update();

private:
    bool recording_;
    recorder::GamepadRecording recording_data_;
    std::string output_file_;
};
