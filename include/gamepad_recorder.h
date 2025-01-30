#pragma once

#include <windows.h>
#include <xinput.h>
#include <string>
#include <fstream>

class XinputGamepadRecorder {
public:
    XinputGamepadRecorder();
    ~XinputGamepadRecorder();

    bool StartRecording(const std::string& output_file);
    void StopRecording();
    void Update();

private:
    bool recording_;
    std::ofstream csv_file_;
    std::string output_file_;
};
