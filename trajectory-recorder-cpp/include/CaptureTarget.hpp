#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace trajectory {

enum class CaptureTargetKind {
    monitor,
    window,
};

struct CaptureTarget {
    CaptureTargetKind kind{CaptureTargetKind::monitor};
    int monitor_id{1};
    int monitor_index{-1};
    std::uint64_t monitor_handle{0};
    std::uint64_t window_handle{0};
    std::string window_title;

    static CaptureTarget PrimaryMonitor() {
        return Monitor(1, -1);
    }

    static CaptureTarget Monitor(int monitor_id, int monitor_index, std::uint64_t monitor_handle = 0) {
        CaptureTarget target;
        target.kind = CaptureTargetKind::monitor;
        target.monitor_id = monitor_id;
        target.monitor_index = monitor_index;
        target.monitor_handle = monitor_handle;
        return target;
    }

    static CaptureTarget Window(std::uint64_t window_handle, std::string window_title) {
        CaptureTarget target;
        target.kind = CaptureTargetKind::window;
        target.window_handle = window_handle;
        target.window_title = std::move(window_title);
        return target;
    }
};

}  // namespace trajectory
