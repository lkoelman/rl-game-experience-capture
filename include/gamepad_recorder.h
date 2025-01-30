#pragma once

#include <windows.h>
#include <xinput.h>
#include <string>
#include <fstream>

class GamepadRecorder {
public:
    GamepadRecorder();
    ~GamepadRecorder();

    bool StartRecording(const std::string& output_file);
    void StopRecording();
    void Update();

private:
    bool recording_;
    std::ofstream csv_file_;
    std::string output_file_;
};
