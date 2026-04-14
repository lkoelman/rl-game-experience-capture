#pragma once

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdint>
#include <fstream>
#include <iosfwd>
#include <string>
#include <unordered_set>

#include "gamepad.pb.h"

namespace trajectory {

std::string FormatVerboseState(const GamepadState& state);

// Captures SDL input state snapshots and appends them to actions.bin.
class InputLogger {
public:
    explicit InputLogger(const std::string& output_path, bool verbose = false);
    ~InputLogger();

    // Opens the output file and initializes SDL.
    void Start();

    // Pumps pending SDL events on the calling thread and persists snapshots for input changes.
    void PumpEventsOnce();

    // Stops capture, closes the controller handle, and shuts SDL down.
    void Stop();

private:
    // Serializes one protobuf snapshot as a little-endian length-prefixed record.
    void WriteState(const GamepadState& state);

    // Converts the current in-memory input state into the on-disk protobuf schema.
    GamepadState SnapshotState(std::uint64_t monotonic_ns) const;

    std::string output_path_;
    bool verbose_{false};
    std::ofstream out_bin_;
    std::atomic<bool> is_running_{false};
    mutable SDL_Mutex* state_mutex_{nullptr};
    SDL_Gamepad* gamepad_{nullptr};
    float axes_[SDL_GAMEPAD_AXIS_COUNT]{};
    std::unordered_set<std::uint32_t> pressed_buttons_;
    std::unordered_set<std::uint32_t> pressed_keys_;
};

}  // namespace trajectory
