#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#include "gamepad.pb.h"

namespace trajectory {

struct SyncRow {
    std::uint64_t frame_index;
    std::uint64_t monotonic_ns;
    std::uint64_t pts;
};

class TrajectoryReplayer {
public:
    TrajectoryReplayer(const std::string& video_path,
                       const std::string& sync_csv_path,
                       const std::string& actions_bin_path);

    bool GetNextStep(cv::Mat& out_frame, GamepadState& out_action);

private:
    void LoadSyncData(const std::string& path);
    void LoadActions(const std::string& path);
    GamepadState GetActionForTimestamp(std::uint64_t target_ns) const;

    cv::VideoCapture video_cap_;
    std::vector<SyncRow> sync_data_;
    std::vector<GamepadState> actions_;
    std::size_t current_frame_idx_{0};
};

}  // namespace trajectory

