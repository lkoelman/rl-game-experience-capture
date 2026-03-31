### Project Context

We are implementing a C++ package to record video game sessions as (state, action) trajectories so they can be used for reinforcement learning. I need a system that can be run on Windows computers with reasonable system performance, and records both video frames and game inputs with synchronized time stamps. The main issue is real time frame rate conversion (recording videos at a lower frame rate and resolution to achieve small file sizes) and timestamp synchronization between the video and gamepad or keyboard recordings

Here is an implementation plan for a C++20 game trajectory recorder, strictly utilizing Conan for package management, Meson (with Ninja) for the build system, GStreamer for video, and SDL3 for input. This blueprint is designed to be directly executed by a coding agent.

### 1. Project Layout

A standard, modern C++ project structure separating headers, sources, and build artifacts.

```text
trajectory_recorder_cpp/
├── conanfile.txt               # Conan dependencies
├── meson.build                 # Root Meson build definition
├── protos/
│   ├── gamepad.proto           # Protobuf schema
│   └── meson.build             # Meson rules to compile .proto to C++
├── include/
│   ├── Session.hpp             # Orchestrator
│   ├── VideoRecorder.hpp       # GStreamer pipeline wrapper
│   ├── InputLogger.hpp         # SDL3 event loop and serializer
│   ├── SyncLogger.hpp          # Fast CSV logger
│   └── Replayer.hpp            # Offline dataset converter
├── src/
│   ├── Session.cpp
│   ├── VideoRecorder.cpp
│   ├── InputLogger.cpp
│   ├── SyncLogger.cpp
│   ├── Replayer.cpp
│   ├── main_record.cpp         # CLI entrypoint for recording
│   └── main_convert.cpp        # CLI entrypoint for offline conversion
└── scripts/
    └── build.ps1               # Helper script to run conan install & meson setup
```

---

### 2. Dependency Management & Build System

#### A. Conan Dependencies (`conanfile.txt`)
Conan will manage Protobuf, OpenCV (for replay), and optionally SDL3.
*(Note: As SDL3 is highly bleeding-edge, if it is not yet in Conan Center, the agent should be instructed to build it from source or use a Conan wrapper recipe. GStreamer on Windows is typically installed via the official MSIs, which Meson will find via `pkg-config`.)*

```text
[requires]
protobuf/3.21.12
opencv/4.8.1
sdl/3.4.0 # Or appropriate version available in Conan

[generators]
PkgConfigDeps
MesonToolchain
```

#### B. Meson Build System (`meson.build`)
The root `meson.build` sets up the dependencies and links the targets.

```meson
project('trajectory_recorder', 'cpp',
  version : '0.1.0',
  default_options : ['warning_level=3', 'cpp_std=c++20'])

# Find Conan dependencies
protobuf_dep = dependency('protobuf')
sdl3_dep = dependency('sdl3')
opencv_dep = dependency('opencv4')

# Find system GStreamer (requires GStreamer MSIs and pkg-config installed)
gst_dep = dependency('gstreamer-1.0')
gst_app_dep = dependency('gstreamer-app-1.0')

# Protobuf Compilation (Requires protoc compiler)
protoc = find_program('protoc')
gen = generator(protoc, \
  output    : ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'],
  arguments : ['--proto_path=@CURRENT_SOURCE_DIR@/protos', '--cpp_out=@BUILD_DIR@', '@INPUT@'])

generated_protos = gen.process('protos/gamepad.proto')

inc_dir = include_directories('include', '.')

# Core Library
core_lib = static_library('recorder_core',
  'src/Session.cpp',
  'src/VideoRecorder.cpp',
  'src/InputLogger.cpp',
  'src/SyncLogger.cpp',
  generated_protos,
  include_directories : inc_dir,
  dependencies : [gst_dep, gst_app_dep, sdl3_dep, protobuf_dep])

# Executables
executable('record_session',
  'src/main_record.cpp',
  include_directories : inc_dir,
  link_with : core_lib,
  dependencies : [gst_dep, sdl3_dep, protobuf_dep])

executable('convert_dataset',
  'src/main_convert.cpp',
  'src/Replayer.cpp',
  include_directories : inc_dir,
  link_with : core_lib,
  dependencies : [opencv_dep, protobuf_dep])
```

---

### 3. Core Architecture & Class Stubs

In C++, `std::chrono::steady_clock` is guaranteed to be monotonic. On Windows, its underlying implementation maps perfectly to `QueryPerformanceCounter` (QPC), identical to Python's `time.perf_counter_ns()`.

#### A. Sync Logger (`include/SyncLogger.hpp`)
Needs to be ultra-fast and thread-safe.

```cpp
#pragma once
#include <fstream>
#include <mutex>
#include <string>
#include <cstdint>

class SyncLogger {
public:
    explicit SyncLogger(const std::string& filepath);
    ~SyncLogger();

    // Called by the GStreamer pad probe callback
    void LogFrame(uint64_t monotonic_ns, uint64_t pts);

private:
    std::ofstream out_csv_;
    std::mutex mtx_;
    uint64_t frame_count_{0};
};
```

#### B. Video Recorder (`include/VideoRecorder.hpp`)
Wraps the GStreamer C-API natively.

```cpp
#pragma once
#include <gst/gst.h>
#include <string>
#include <memory>
#include "SyncLogger.hpp"

class VideoRecorder {
public:
    VideoRecorder(const std::string& output_path, std::shared_ptr<SyncLogger> sync_logger);
    ~VideoRecorder();

    void Start();
    void Stop(); // Blocks until EOS is handled

private:
    static GstPadProbeReturn PadProbeCallback(GstPad* pad, GstPadProbeInfo* info, gpointer user_data);
    static gboolean BusCall(GstBus* bus, GstMessage* msg, gpointer data);

    std::string output_path_;
    std::shared_ptr<SyncLogger> sync_logger_;

    GstElement* pipeline_{nullptr};
    GMainLoop* loop_{nullptr};
    // Include thread handler for GLib MainLoop
};
```
*Implementation Note for Agent:* In `VideoRecorder.cpp`, the pipeline string will remain identical to the Python one (using `d3d11screencapturesrc` and `identity name=probe_point`). The `PadProbeCallback` must grab `std::chrono::steady_clock::now().time_since_epoch().count()` immediately upon execution.

#### C. Input Logger (`include/InputLogger.hpp`)
Runs the SDL3 event loop and serializes the generated C++ protobuf classes.

```cpp
#pragma once
#include <atomic>
#include <thread>
#include <fstream>
#include <string>
#include <SDL3/SDL.h>
#include "gamepad.pb.h" // Generated by Meson/protoc

class InputLogger {
public:
    explicit InputLogger(const std::string& output_path);
    ~InputLogger();

    void Start();
    void Stop();

private:
    void EventLoop();
    void WriteState(const trajectory::GamepadState& state);

    std::string output_path_;
    std::ofstream out_bin_;
    std::atomic<bool> is_running_{false};
    std::thread worker_thread_;
};
```
*Implementation Note for Agent:* In `WriteState`, serialize the protobuf to a `std::string` or byte array, compute its size, write the size as a `uint32_t` (little-endian) using `out_bin_.write()`, and then write the payload.

#### D. The Orchestrator (`include/Session.hpp`)
Handles graceful initialization and shutdown.

```cpp
#pragma once
#include <string>
#include <memory>
#include "VideoRecorder.hpp"
#include "InputLogger.hpp"
#include "SyncLogger.hpp"

class Session {
public:
    Session(const std::string& output_dir, const std::string& session_name);
    void Start();
    void Stop();

private:
    std::shared_ptr<SyncLogger> sync_logger_;
    std::unique_ptr<VideoRecorder> video_recorder_;
    std::unique_ptr<InputLogger> input_logger_;
};
```

---

### 4. Replay and Conversion (`include/Replayer.hpp`)

This module parses the outputs for reinforcement learning dataset construction.

```cpp
#pragma once
#include <string>
#include <vector>
#include <tuple>
#include <opencv2/opencv.hpp>
#include "gamepad.pb.h"

// Define a simple struct for the Sync CSV rows
struct SyncRow {
    uint64_t frame_index;
    uint64_t monotonic_ns;
    uint64_t pts;
};

class TrajectoryReplayer {
public:
    TrajectoryReplayer(const std::string& video_path,
                       const std::string& sync_csv_path,
                       const std::string& actions_bin_path);

    // Reads the next aligned (Image, Action) pair. Returns false if EOF.
    bool GetNextStep(cv::Mat& out_frame, trajectory::GamepadState& out_action);

private:
    void LoadSyncData(const std::string& path);
    void LoadActions(const std::string& path);
    trajectory::GamepadState GetActionForTimestamp(uint64_t target_ns);

    cv::VideoCapture video_cap_;
    std::vector<SyncRow> sync_data_;
    std::vector<trajectory::GamepadState> actions_;
    size_t current_frame_idx_{0};
};
```
*Implementation Note for Agent:* `LoadActions` must read the `uint32_t` length prefix, read that exact number of bytes into a buffer, and call `ParseFromArray` on the Protobuf object. `GetActionForTimestamp` should use `std::upper_bound` on the `actions_` vector to efficiently find the exact input state at the time of the frame.

---

### 5. CLI Entrypoint (`src/main_record.cpp`)

In C++, graceful shutdown requires handling OS-level signals. On Windows, `std::signal(SIGINT)` is insufficient for console applications; the agent must use `SetConsoleCtrlHandler` or cross-platform equivalent wrappers to trigger `Session::Stop()`.

```cpp
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include "Session.hpp"

std::atomic<bool> g_shutdown_requested{false};

void SignalHandler(int signum) {
    g_shutdown_requested = true;
}

int main(int argc, char* argv[]) {
    // Basic argument parsing (Agent can implement a robust parser like cxxopts if desired)
    std::string output_dir = "./data";
    std::string session_name = "cpp_session";

    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    try {
        // Must initialize GStreamer before use
        gst_init(&argc, &argv);

        Session session(output_dir, session_name);
        session.Start();

        std::cout << "Recording... Press Ctrl+C to stop." << std::endl;

        while (!g_shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "\nGraceful shutdown initiated..." << std::endl;
        session.Stop();
        std::cout << "Session saved successfully." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

### Guidance for the Coding Agent
When instructing your coding agent to implement this, emphasize these exact constraints:
1.  **Memory Management:** The C-API of GStreamer requires manual reference counting. Ensure the agent utilizes `gst_object_unref` or wraps GStreamer objects in smart pointers with custom deleters.
2.  **Multithreading:** The GLib main loop used by GStreamer to catch the EOS message must run in its own `std::thread` and must not block the main application thread or the SDL3 event loop.
3.  **Endianness:** When reading and writing the 4-byte length prefix for the Protobuf binary file, ensure the agent enforces Little-Endian byte order to maintain compatibility if read later by Python on a different architecture.