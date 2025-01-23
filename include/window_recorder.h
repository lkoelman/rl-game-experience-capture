#pragma once

#include <windows.h>
#include <string>
#include "window_capture.pb.h"

class WindowRecorder {
public:
    WindowRecorder();
    ~WindowRecorder();

    bool StartRecording(HWND window_handle, const std::string& output_file);
    void StopRecording();
    void Update();

private:
    bool recording_;
    HWND target_window_;
    recorder::WindowRecording recording_data_;
    std::string output_file_;
};
