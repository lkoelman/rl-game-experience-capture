#include <iostream>
#include <windows.h>
#include <conio.h>
#include <vector>
#include <string>
#include <algorithm>
// #include "gamepad_recorder.h"
#include "sdl_input_recorder.h"
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>


int main(int argc, char* argv[]) {
    std::cout << "Gamepad State Recorder\n";
    
    if (argc != 1) {
        std::cout << "Usage: " << argv[0] << "\n";
        return 1;
    }

    const float GAMEPAD_SAVE_FREQUENCY = 60.0;
    const float gamepad_save_interval_ms = int(1.0 / GAMEPAD_SAVE_FREQUENCY * 1000.0);

    // Create directory with timestamp
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    std::string timestamp = oss.str();
    std::string folder_name = timestamp + "_rl-exp";
    std::filesystem::create_directory(folder_name);

    std::cout << "\nPress 'Q' to top recording and quit\n";

    // For now, do window recording using external screen capture software (windows game bar, ffmpeg, gstreamer)
    SdlInputRecorder gamepad_recorder;
    // XinputGamepadRecorder gamepad_recorder;
    // WindowRecorder window_recorder;
    
    gamepad_recorder.StartRecording(folder_name + "/gamepad_state.csv");
    // window_recorder.StartRecording(targetWindow, folder_name + "/window_recording.csv");

    while (true) {
        
        // Listen for keypress
        if (_kbhit()) {
            char ch = _getch();
            if (ch == 'q' || ch == 'Q') {
                std::cout << "Stopped recording.\n";
                gamepad_recorder.StopRecording();
                // window_recorder.StopRecording();
                break;
            }
        }

        // Save gamepad state at recording frequency
        gamepad_recorder.Update();
        // window_recorder.Update();

        Sleep(gamepad_save_interval_ms);
    }

    return 0;
}
