#include <iostream>
#include <windows.h>
#include <conio.h>
#include <vector>
#include <string>
#include <algorithm>
#include "gamepad_recorder.h"
#include "window_recorder.h"
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

// Callback function for EnumWindows
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    std::vector<std::pair<HWND, std::wstring>>* windows = 
        reinterpret_cast<std::vector<std::pair<HWND, std::wstring>>*>(lParam);
    
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    WCHAR title[256];
    if (GetWindowTextW(hwnd, title, 256) == 0) {
        return TRUE;
    }

    std::wstring windowTitle(title);
    if (!windowTitle.empty()) {
        windows->push_back({hwnd, windowTitle});
    }

    return TRUE;
}

HWND SelectWindow(const std::string& partialName) {
    std::vector<std::pair<HWND, std::wstring>> matchingWindows;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&matchingWindows));

    // Convert partial name to wide string for case-insensitive comparison
    std::wstring searchTerm(partialName.begin(), partialName.end());
    std::transform(searchTerm.begin(), searchTerm.end(), searchTerm.begin(), ::towlower);

    std::vector<std::pair<HWND, std::wstring>> filteredWindows;
    for (const auto& window : matchingWindows) {
        std::wstring title = window.second;
        std::transform(title.begin(), title.end(), title.begin(), ::towlower);
        if (title.find(searchTerm) != std::wstring::npos) {
            filteredWindows.push_back(window);
        }
    }

    if (filteredWindows.empty()) {
        std::cout << "No windows found matching '" << partialName << "'\n";
        return nullptr;
    }

    std::cout << "\nMatching windows:\n";
    for (size_t i = 0; i < filteredWindows.size(); ++i) {
        std::wcout << i + 1 << ": " << filteredWindows[i].second << '\n';
    }

    size_t selection;
    while (true) {
        std::cout << "\nEnter window number (1-" << filteredWindows.size() 
                  << ") or 0 to cancel: ";
        std::cin >> selection;
        
        if (selection == 0) {
            return nullptr;
        }
        
        if (selection > 0 && selection <= filteredWindows.size()) {
            return filteredWindows[selection - 1].first;
        }
        
        std::cout << "Invalid selection. Please try again.\n";
    }
}

int main(int argc, char* argv[]) {
    std::cout << "Windows Input Recorder\n";
    
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <partial-window-name>\n";
        return 1;
    }

    HWND targetWindow = SelectWindow(argv[1]);
    if (!targetWindow) {
        std::cout << "No window selected. Exiting.\n";
        return 1;
    }

    // Create directory with timestamp
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    std::string timestamp = oss.str();
    std::string folder_name = timestamp + "_rl-exp";
    std::filesystem::create_directory(folder_name);

    std::cout << "\nPress 'Q' to quit, 'R' to start/stop recording\n";

    GamepadRecorder gamepad_recorder;
    WindowRecorder window_recorder;
    
    bool is_recording = false;

    while (true) {
        if (_kbhit()) {
            char ch = _getch();
            if (ch == 'q' || ch == 'Q') {
                std::cout << "Stopped recording.\n";
                break;
            }
            else if (ch == 'r' || ch == 'R') {
                is_recording = !is_recording;
                if (is_recording) {
                    gamepad_recorder.StartRecording(folder_name + "/gamepad_recording.csv");
                    window_recorder.StartRecording(targetWindow, folder_name + "/window_recording.csv");
                }
                else {
                    gamepad_recorder.StopRecording();
                    window_recorder.StopRecording();
                }
                std::cout << (is_recording ? "Recording started\n" : "Recording stopped\n");
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
