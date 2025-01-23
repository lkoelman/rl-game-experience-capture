#include <iostream>
#include <windows.h>
#include <conio.h>
#include "gamepad_recorder.h"
#include "window_recorder.h"

int main() {
    std::cout << "Windows Input Recorder\n";
    std::cout << "Press 'Q' to quit, 'R' to start/stop recording\n";

    GamepadRecorder gamepad_recorder;
    WindowRecorder window_recorder;
    
    bool is_recording = false;

    while (true) {
        if (_kbhit()) {
            char ch = _getch();
            if (ch == 'q' || ch == 'Q') {
                break;
            }
            else if (ch == 'r' || ch == 'R') {
                is_recording = !is_recording;
                if (is_recording) {
                    gamepad_recorder.StartRecording("gamepad_recording.pb");
                    // TODO: Implement window selection
                    // window_recorder.StartRecording(hwnd, "window_recording.pb");
                }
                else {
                    gamepad_recorder.StopRecording();
                    window_recorder.StopRecording();
                }
            }
        }

        if (is_recording) {
            gamepad_recorder.Update();
            window_recorder.Update();
        }

        Sleep(16); // ~60 fps
    }

    return 0;
}
