#include "Replayer.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "BinaryIO.hpp"

namespace trajectory {

TrajectoryReplayer::TrajectoryReplayer(const std::string& video_path,
                                       const std::string& sync_csv_path,
                                       const std::string& actions_bin_path)
    : video_cap_(video_path) {
    if (!video_cap_.isOpened()) {
        throw std::runtime_error("failed to open video file: " + video_path);
    }
    LoadSyncData(sync_csv_path);
    LoadActions(actions_bin_path);
}

bool TrajectoryReplayer::GetNextStep(cv::Mat& out_frame, GamepadState& out_action) {
    if (current_frame_idx_ >= sync_data_.size()) {
        return false;
    }
    if (!video_cap_.read(out_frame)) {
        return false;
    }

    // sync.csv is the authority for which monotonic timestamp belongs to the current decoded frame.
    const auto& row = sync_data_.at(current_frame_idx_++);
    out_action = GetActionForTimestamp(row.monotonic_ns);
    return true;
}

void TrajectoryReplayer::LoadSyncData(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open sync csv: " + path);
    }

    std::string line;
    std::getline(input, line);
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        std::stringstream stream(line);
        std::string cell;
        SyncRow row{};

        std::getline(stream, cell, ',');
        row.frame_index = std::stoull(cell);
        std::getline(stream, cell, ',');
        row.monotonic_ns = std::stoull(cell);
        std::getline(stream, cell, ',');
        row.pts = std::stoull(cell);
        sync_data_.push_back(row);
    }
}

void TrajectoryReplayer::LoadActions(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open actions binary: " + path);
    }

    while (input.peek() != std::char_traits<char>::eof()) {
        // The input stream format matches BinaryIO + protobuf serialization from InputLogger.
        const auto payload = ReadLengthPrefixedPayload(input);
        GamepadState state;
        if (!state.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
            throw std::runtime_error("failed to parse gamepad state protobuf");
        }
        actions_.push_back(std::move(state));
    }
}

GamepadState TrajectoryReplayer::GetActionForTimestamp(std::uint64_t target_ns) const {
    if (actions_.empty()) {
        throw std::runtime_error("no recorded actions available");
    }

    // Actions are append-only in time order, so upper_bound finds the latest snapshot at or before the frame.
    const auto upper = std::upper_bound(
        actions_.begin(),
        actions_.end(),
        target_ns,
        [](std::uint64_t ts, const GamepadState& state) { return ts < state.monotonic_ns(); });

    if (upper == actions_.begin()) {
        return actions_.front();
    }

    return *std::prev(upper);
}

}  // namespace trajectory
