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
// The file stores full protobuf snapshots, not raw SDL events.
class InputLogger {
public:
    explicit InputLogger(const std::string& output_path, bool verbose = false);
    ~InputLogger();

    // Opens the output file and initializes SDL.
    void Start();

    // Pumps pending SDL events on the calling thread.
    // Each input mutation updates the cached state and writes one full protobuf snapshot.
    // This is event-driven: idle periods produce no new records.
    void PumpEventsOnce();

    // Stops capture, closes the controller handle, and shuts SDL down.
    void Stop();

private:
    // Serializes one protobuf snapshot as a little-endian length-prefixed record.
    void WriteState(const GamepadState& state);

    // Converts cached SDL state into the on-disk protobuf schema.
    // `axes_` maps 1:1 to SDL_GamepadAxis slots. Button/key sets store currently pressed enum ids.
    GamepadState SnapshotState(std::uint64_t monotonic_ns) const;

    std::string output_path_;
    bool verbose_{false};
    std::ofstream out_bin_;
    std::atomic<bool> is_running_{false};
    mutable SDL_Mutex* state_mutex_{nullptr};
    SDL_Gamepad* gamepad_{nullptr};
    // Latest analog values by SDL_GamepadAxis index. Unmoved axes remain at 0.
    float axes_[SDL_GAMEPAD_AXIS_COUNT]{};
    // Pressed digital controls stored as SDL enum values for stable serialization.
    std::unordered_set<std::uint32_t> pressed_buttons_;
    std::unordered_set<std::uint32_t> pressed_keys_;
};

}  // namespace trajectory
