#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#include "gamepad.pb.h"

namespace trajectory {

// One parsed row from sync.csv.
struct SyncRow {
    std::uint64_t frame_index;
    std::uint64_t monotonic_ns;
    std::uint64_t pts;
};

// Offline helper that reconstructs aligned (frame, action) pairs from recorder output files.
class TrajectoryReplayer {
public:
    TrajectoryReplayer(const std::string& video_path,
                       const std::string& sync_csv_path,
                       const std::string& actions_bin_path);

    // Advances the replay by one video frame and returns the latest action at that frame timestamp.
    bool GetNextStep(cv::Mat& out_frame, GamepadState& out_action);

private:
    // Loads frame timestamps recorded by SyncLogger.
    void LoadSyncData(const std::string& path);

    // Loads length-prefixed GamepadState records produced by InputLogger.
    void LoadActions(const std::string& path);

    // Returns the last action whose monotonic timestamp is <= the frame timestamp.
    GamepadState GetActionForTimestamp(std::uint64_t target_ns) const;

    cv::VideoCapture video_cap_;
    std::vector<SyncRow> sync_data_;
    std::vector<GamepadState> actions_;
    std::size_t current_frame_idx_{0};
};

}  // namespace trajectory
