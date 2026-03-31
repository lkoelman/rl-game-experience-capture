#pragma once

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdint>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_set>

#include "gamepad.pb.h"

namespace trajectory {

class InputLogger {
public:
    explicit InputLogger(const std::string& output_path);
    ~InputLogger();

    void Start();
    void Stop();

private:
    void EventLoop();
    void WriteState(const GamepadState& state);
    GamepadState SnapshotState(std::uint64_t monotonic_ns) const;

    std::string output_path_;
    std::ofstream out_bin_;
    std::atomic<bool> is_running_{false};
    std::thread worker_thread_;
    mutable SDL_Mutex* state_mutex_{nullptr};
    SDL_Gamepad* gamepad_{nullptr};
    float axes_[SDL_GAMEPAD_AXIS_COUNT]{};
    std::unordered_set<std::uint32_t> pressed_buttons_;
    std::unordered_set<std::uint32_t> pressed_keys_;
};

}  // namespace trajectory
