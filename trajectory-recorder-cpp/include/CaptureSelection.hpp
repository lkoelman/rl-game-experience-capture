#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "CaptureTarget.hpp"

namespace trajectory {

struct MonitorOption {
    int id{0};
    int monitor_index{0};
    std::string display_name;
    int width{0};
    int height{0};
    int origin_x{0};
    int origin_y{0};
    std::uint64_t monitor_handle{0};
};

struct WindowOption {
    std::uint64_t window_handle{0};
    std::string title;
    std::string process_name;
    std::uint32_t process_id{0};
};

struct SelectionResult {
    bool ok{false};
    CaptureTarget target = CaptureTarget::PrimaryMonitor();
    std::string error;
};

struct SelectionPageEntry {
    int shortcut_digit{0};
    std::size_t item_index{0};
    std::string label;
};

struct SelectionPage {
    std::vector<SelectionPageEntry> entries;
    bool has_more{false};
};

SelectionResult ResolveMonitorTarget(const std::vector<MonitorOption>& monitors, int monitor_id);
SelectionResult ResolveWindowTarget(const std::vector<WindowOption>& windows, const std::string& query);
std::string BuildMonitorLabel(const MonitorOption& monitor);
std::string BuildWindowLabel(const WindowOption& window);
SelectionPage BuildSelectionPage(const std::vector<std::string>& labels, std::size_t page_index, std::size_t page_size);

}  // namespace trajectory
