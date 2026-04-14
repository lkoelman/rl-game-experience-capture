#include "CaptureSelection.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace trajectory {

namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

}  // namespace

SelectionResult ResolveMonitorTarget(const std::vector<MonitorOption>& monitors, int monitor_id) {
    for (const auto& monitor : monitors) {
        if (monitor.id == monitor_id) {
            return SelectionResult{
                true,
                CaptureTarget::Monitor(monitor.id, monitor.monitor_index),
                "",
            };
        }
    }

    return SelectionResult{
        false,
        CaptureTarget::PrimaryMonitor(),
        "No monitor matches id " + std::to_string(monitor_id) + ".",
    };
}

SelectionResult ResolveWindowTarget(const std::vector<WindowOption>& windows, const std::string& query) {
    const std::string lowered_query = ToLower(query);
    const WindowOption* resolved_window = nullptr;

    for (const auto& window : windows) {
        if (ToLower(window.title).find(lowered_query) == std::string::npos) {
            continue;
        }

        if (resolved_window != nullptr) {
            return SelectionResult{
                false,
                CaptureTarget::PrimaryMonitor(),
                "Multiple windows match \"" + query + "\". Refine the title or use the selector.",
            };
        }

        resolved_window = &window;
    }

    if (resolved_window == nullptr) {
        return SelectionResult{
            false,
            CaptureTarget::PrimaryMonitor(),
            "No window title contains \"" + query + "\".",
        };
    }

    return SelectionResult{
        true,
        CaptureTarget::Window(resolved_window->window_handle, resolved_window->title),
        "",
    };
}

std::string BuildMonitorLabel(const MonitorOption& monitor) {
    std::ostringstream label;
    label << '[' << monitor.id << "] " << monitor.display_name << " - "
          << monitor.width << 'x' << monitor.height
          << " @ (" << monitor.origin_x << ", " << monitor.origin_y << ')';
    return label.str();
}

std::string BuildWindowLabel(const WindowOption& window) {
    std::ostringstream label;
    label << window.title;
    if (!window.process_name.empty()) {
        label << " [" << window.process_name;
        if (window.process_id != 0) {
            label << " pid=" << window.process_id;
        }
        label << ']';
    } else if (window.process_id != 0) {
        label << " [pid=" << window.process_id << ']';
    }
    return label.str();
}

SelectionPage BuildSelectionPage(const std::vector<std::string>& labels, std::size_t page_index, std::size_t page_size) {
    SelectionPage page;
    if (page_size == 0) {
        return page;
    }

    const std::size_t start_index = page_index * page_size;
    const std::size_t end_index = std::min(labels.size(), start_index + page_size);

    for (std::size_t index = start_index; index < end_index; ++index) {
        page.entries.push_back(SelectionPageEntry{
            static_cast<int>((index - start_index) + 1),
            index,
            labels[index],
        });
    }

    page.has_more = end_index < labels.size();
    return page;
}

}  // namespace trajectory
