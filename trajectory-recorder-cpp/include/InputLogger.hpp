#pragma once

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdint>
#include <fstream>
#include <iosfwd>
#include <memory>
#include <string>
#include <unordered_set>

#include "gamepad.pb.h"

namespace trajectory {

std::string FormatVerboseState(const GamepadState& state);

// Captures SDL input state snapshots and appends them to actions.bin.
// The file stores full protobuf snapshots, not raw SDL events.
//
// The logger also owns the recorder-side virtual gamepad forwarding path used
// to work around the PRD's core problem: the target game must see a playable
// controller while the recorder simultaneously observes and saves the physical
// controller state. The intended runtime topology is:
// physical pad -> SDL in recorder -> ViGEm virtual Xbox pad -> game
//
// Physical-device hiding is still handled outside this class via HidHide or
// equivalent tooling; InputLogger only mirrors state into the virtual device.
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
    // Windows-only helper that owns one ViGEm client/target pair and mirrors
    // the tracked physical SDL gamepad state into a virtual Xbox controller.
    // Keeping it private prevents ViGEm details from leaking into the public
    // recorder interface.
    class VirtualGamepadForwarder;

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
    SDL_JoystickID gamepad_instance_id_{0};
    // Latest analog values by SDL_GamepadAxis index. Unmoved axes remain at 0.
    float axes_[SDL_GAMEPAD_AXIS_COUNT]{};
    // Pressed digital controls stored as SDL enum values for stable serialization.
    std::unordered_set<std::uint32_t> pressed_buttons_;
    std::unordered_set<std::uint32_t> pressed_keys_;
    std::unique_ptr<VirtualGamepadForwarder> virtual_gamepad_forwarder_;
};

}  // namespace trajectory
