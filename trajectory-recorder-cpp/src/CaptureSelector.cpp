#include "CaptureSelector.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

namespace trajectory {

namespace {

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return "";
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return "";
    }

    std::string result(static_cast<std::size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr) <= 0) {
        return "";
    }
    return result;
}

std::string Trim(const std::string& value) {
    const auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, (end - start) + 1);
}

std::string ExtractProcessName(DWORD process_id) {
    if (process_id == 0) {
        return "";
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr) {
        return "";
    }

    std::wstring buffer(MAX_PATH, L'\0');
    DWORD size = static_cast<DWORD>(buffer.size());
    std::string process_name;
    if (QueryFullProcessImageNameW(process, 0, buffer.data(), &size) != 0) {
        buffer.resize(size);
        process_name = std::filesystem::path(buffer).filename().string();
    }

    CloseHandle(process);
    return process_name;
}

BOOL CALLBACK MonitorEnumProc(HMONITOR monitor_handle, HDC, LPRECT, LPARAM user_data) {
    auto* monitors = reinterpret_cast<std::vector<MonitorOption>*>(user_data);

    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor_handle, &info)) {
        return TRUE;
    }

    DISPLAY_DEVICEW display_device{};
    display_device.cb = sizeof(display_device);
    std::string display_name = WideToUtf8(info.szDevice);
    if (EnumDisplayDevicesW(info.szDevice, 0, &display_device, 0) != 0) {
        const std::string model = Trim(WideToUtf8(display_device.DeviceString));
        if (!model.empty()) {
            display_name = model;
        }
    }

    monitors->push_back(MonitorOption{
        0,
        0,
        display_name,
        info.rcMonitor.right - info.rcMonitor.left,
        info.rcMonitor.bottom - info.rcMonitor.top,
        info.rcMonitor.left,
        info.rcMonitor.top,
        static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(monitor_handle)),
    });
    return TRUE;
}

BOOL CALLBACK WindowEnumProc(HWND window_handle, LPARAM user_data) {
    auto* windows = reinterpret_cast<std::vector<WindowOption>*>(user_data);
    if (!IsWindowVisible(window_handle) || window_handle == GetConsoleWindow()) {
        return TRUE;
    }

    if (GetWindow(window_handle, GW_OWNER) != nullptr) {
        return TRUE;
    }

    const LONG_PTR ex_style = GetWindowLongPtrW(window_handle, GWL_EXSTYLE);
    if ((ex_style & WS_EX_TOOLWINDOW) != 0) {
        return TRUE;
    }

    const int title_length = GetWindowTextLengthW(window_handle);
    if (title_length <= 0) {
        return TRUE;
    }

    std::wstring title(static_cast<std::size_t>(title_length) + 1, L'\0');
    const int copied = GetWindowTextW(window_handle, title.data(), static_cast<int>(title.size()));
    if (copied <= 0) {
        return TRUE;
    }
    title.resize(static_cast<std::size_t>(copied));

    DWORD process_id = 0;
    GetWindowThreadProcessId(window_handle, &process_id);

    windows->push_back(WindowOption{
        static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(window_handle)),
        Trim(WideToUtf8(title)),
        ExtractProcessName(process_id),
        process_id,
    });
    return TRUE;
}

std::string SelectionModeName(bool select_windows) {
    return select_windows ? "Window" : "Fullscreen / monitor";
}

}  // namespace

std::vector<MonitorOption> EnumerateMonitors() {
    std::vector<MonitorOption> monitors;
    EnumDisplayMonitors(nullptr, nullptr, &MonitorEnumProc, reinterpret_cast<LPARAM>(&monitors));

    std::sort(monitors.begin(), monitors.end(), [](const MonitorOption& left, const MonitorOption& right) {
        if (left.origin_y != right.origin_y) {
            return left.origin_y < right.origin_y;
        }
        return left.origin_x < right.origin_x;
    });

    for (std::size_t index = 0; index < monitors.size(); ++index) {
        monitors[index].id = static_cast<int>(index + 1);
        monitors[index].monitor_index = static_cast<int>(index);
    }

    return monitors;
}

std::vector<WindowOption> EnumerateWindows() {
    std::vector<WindowOption> windows;
    EnumWindows(&WindowEnumProc, reinterpret_cast<LPARAM>(&windows));

    windows.erase(std::remove_if(windows.begin(), windows.end(), [](const WindowOption& window) {
                      return window.title.empty();
                  }),
                  windows.end());

    std::sort(windows.begin(), windows.end(), [](const WindowOption& left, const WindowOption& right) {
        if (left.title != right.title) {
            return left.title < right.title;
        }
        if (left.process_name != right.process_name) {
            return left.process_name < right.process_name;
        }
        return left.process_id < right.process_id;
    });
    return windows;
}

SelectionResult PromptForCaptureTarget(const std::vector<MonitorOption>& monitors,
                                       const std::vector<WindowOption>& windows) {
    if (monitors.empty() && windows.empty()) {
        return SelectionResult{false, CaptureTarget::PrimaryMonitor(), "No monitors or titled windows are available for capture."};
    }

    const std::vector<std::string> monitor_labels = [&] {
        std::vector<std::string> labels;
        labels.reserve(monitors.size());
        for (const auto& monitor : monitors) {
            labels.push_back(BuildMonitorLabel(monitor));
        }
        return labels;
    }();

    const std::vector<std::string> window_labels = [&] {
        std::vector<std::string> labels;
        labels.reserve(windows.size());
        for (const auto& window : windows) {
            labels.push_back(BuildWindowLabel(window));
        }
        return labels;
    }();

    bool select_windows = windows.empty() ? false : monitors.empty();
    bool choosing_mode = !monitors.empty() && !windows.empty();
    std::size_t page_index = 0;
    SelectionResult result{false, CaptureTarget::PrimaryMonitor(), "Selection cancelled."};

    auto screen = ftxui::ScreenInteractive::TerminalOutput();

    auto component = ftxui::Renderer([&] {
        using namespace ftxui;

        Elements body;
        body.push_back(text("Select the capture target") | bold);
        body.push_back(separator());

        if (choosing_mode) {
            body.push_back(text("Press 0 for fullscreen / monitor capture."));
            body.push_back(text("Press 1 for window capture."));
            body.push_back(text("Esc or q cancels."));
            body.push_back(separator());
            body.push_back(text("[0] Fullscreen / monitor"));
            body.push_back(text("[1] Window"));
        } else {
            const auto& labels = select_windows ? window_labels : monitor_labels;
            const SelectionPage page = BuildSelectionPage(labels, page_index, 9);

            body.push_back(text("Mode: " + SelectionModeName(select_windows)) | bold);
            body.push_back(text("Press 1-9 to choose an entry. Press Space for more entries."));
            body.push_back(text("Backspace returns to the mode picker. Esc or q cancels."));
            body.push_back(separator());

            if (page.entries.empty()) {
                body.push_back(text("No targets available in this mode."));
            } else {
                for (const auto& entry : page.entries) {
                    body.push_back(text("[" + std::to_string(entry.shortcut_digit) + "] " + entry.label));
                }
            }

            if (page.has_more) {
                body.push_back(separator());
                body.push_back(text("Space: show more"));
            }
        }

        return vbox(std::move(body)) | border;
    });

    component = ftxui::CatchEvent(component, [&](ftxui::Event event) {
        if (event == ftxui::Event::Escape || (event.is_character() && event.character() == "q")) {
            result = SelectionResult{false, CaptureTarget::PrimaryMonitor(), "Selection cancelled."};
            screen.ExitLoopClosure()();
            return true;
        }

        if (choosing_mode) {
            if (event.is_character() && event.character() == "0" && !monitors.empty()) {
                select_windows = false;
                choosing_mode = false;
                page_index = 0;
                return true;
            }
            if (event.is_character() && event.character() == "1" && !windows.empty()) {
                select_windows = true;
                choosing_mode = false;
                page_index = 0;
                return true;
            }
            return false;
        }

        const auto& labels = select_windows ? window_labels : monitor_labels;
        const SelectionPage page = BuildSelectionPage(labels, page_index, 9);

        if (event == ftxui::Event::Backspace) {
            if (!monitors.empty() && !windows.empty()) {
                choosing_mode = true;
                page_index = 0;
                return true;
            }
            return false;
        }

        if (event.is_character() && event.character() == " ") {
            if (page.has_more) {
                ++page_index;
                return true;
            }
            page_index = 0;
            return true;
        }

        if (!event.is_character()) {
            return false;
        }

        const std::string character = event.character();
        if (character.size() != 1 || character[0] < '1' || character[0] > '9') {
            return false;
        }

        const std::size_t page_offset = static_cast<std::size_t>(character[0] - '1');
        if (page_offset >= page.entries.size()) {
            return true;
        }

        const std::size_t item_index = page.entries[page_offset].item_index;
        if (select_windows) {
            result = SelectionResult{true, CaptureTarget::Window(windows[item_index].window_handle, windows[item_index].title), ""};
        } else {
            result = SelectionResult{
                true,
                CaptureTarget::Monitor(monitors[item_index].id, monitors[item_index].monitor_index, monitors[item_index].monitor_handle),
                "",
            };
        }

        screen.ExitLoopClosure()();
        return true;
    });

    screen.Loop(component);
    return result;
}

}  // namespace trajectory
