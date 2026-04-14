# Architecture

This document is the bootstrapping context for coding agents working on `trajectory-recorder-cpp`. It describes the code as it exists now, not only the original plan in `specs/recorder-initial/SPEC.md`.

## Purpose

The package records a gameplay session as three synchronized artifacts:

- `capture.mp4`: compressed screen recording written by GStreamer
- `sync.csv`: per-frame timestamp mapping written by `SyncLogger`
- `actions.bin`: length-prefixed protobuf snapshots of input state written by `InputLogger`

The intended downstream use is reinforcement-learning dataset construction from aligned `(frame, action)` pairs.

## Top-Level Structure

- `include/`: public headers for the core modules
- `src/`: implementation files and CLI entrypoints
- `protos/`: protobuf schema and Meson generation rules
- `tests/`: unit tests for deterministic helper code
- `scripts/build.ps1`: canonical Windows build entrypoint
- `docs/ARCHITECTURE.md`: this file

## Current Build Architecture

The project builds with Conan + Meson + Ninja on Windows.

Key points:

- Conan manages `protobuf` and `sdl`; OpenCV is currently commented out in `conanfile.txt`.
- GStreamer is not consumed through Conan. The build uses an existing Windows GStreamer installation root.
- `scripts/build.ps1` is the canonical build path. It:
  - validates the GStreamer root
  - injects GStreamer paths into the Conan/Meson environment
  - forces Conan dependency settings to match the active MSVC runtime, build type, and C++20 mode
  - forces a protobuf rebuild so the binary matches the active toolchain
  - discovers `protoc.exe` from the Conan protobuf package
  - patches `conan_meson_native.ini` so Meson sees the correct `pkg_config_path`, `cpp_std`, and `gstreamer_root`

Important implication for agents:

- If you change dependency discovery or the build graph, read `scripts/build.ps1` first. It contains non-trivial environment and native-file patching required for a successful Windows build.

## Runtime Architecture

The runtime path for recording is:

1. `src/main_record.cpp`
2. `trajectory::Session`
3. `trajectory::InputLogger`
4. `trajectory::VideoRecorder`
5. `trajectory::SyncLogger`

`main_record.cpp` is a thin CLI wrapper:

- initializes GStreamer with `gst_init`
- installs console shutdown handlers
- creates a `Session`
- blocks until Ctrl+C or another console shutdown event
- calls `Session::Stop()`

## Module Responsibilities

### `Session`

Files:

- `include/Session.hpp`
- `src/Session.cpp`

`Session` is the orchestrator. It owns:

- one `SyncLogger`
- one `VideoRecorder`
- one `InputLogger`

It also creates the per-session output directory:

- `<output_dir>/<session_name>/capture.mp4`
- `<output_dir>/<session_name>/sync.csv`
- `<output_dir>/<session_name>/actions.bin`

Current lifecycle:

- `Start()` starts `InputLogger` first, then `VideoRecorder`
- `Stop()` stops `VideoRecorder` first, then `InputLogger`

The public header only forward-declares the owned classes to keep protobuf- and GStreamer-heavy dependencies out of the CLI compile boundary.

### `VideoRecorder`

Files:

- `include/VideoRecorder.hpp`
- `src/VideoRecorder.cpp`

`VideoRecorder` wraps the GStreamer pipeline and is responsible for screen capture and frame timestamp logging.

Current implementation details:

- pipeline is built from a string in `BuildPipelineDescription()`
- uses `d3d11screencapturesrc`
- converts/scales to `1280x720` at `30 fps`
- inserts `identity name=probe_point`
- encodes with `x264enc`
- writes `mp4`

Synchronization behavior:

- a pad probe is attached to `probe_point`'s `src` pad
- on each `GstBuffer`, the probe captures a `steady_clock` nanosecond timestamp immediately
- the probe forwards `(monotonic_ns, GST_BUFFER_PTS(buffer))` to `SyncLogger`

Threading behavior:

- owns a GLib `GMainLoop`
- runs that loop on a dedicated `std::thread`
- installs a bus watch to terminate the loop on EOS or ERROR

Resource-management notes:

- GStreamer objects are managed manually with `gst_object_unref`
- `Stop()` sends EOS, sets pipeline state to `NULL`, stops the main loop, joins the thread, and releases GStreamer resources

### `SyncLogger`

Files:

- `include/SyncLogger.hpp`
- `src/SyncLogger.cpp`

`SyncLogger` is a minimal thread-safe CSV append-only logger.

Current file format:

- header row: `frame_index,monotonic_ns,pts`
- one row per observed video frame

Implementation details:

- protected by `std::mutex`
- frame numbering is local and monotonic via `frame_count_`

### `InputLogger`

Files:

- `include/InputLogger.hpp`
- `src/InputLogger.cpp`

`InputLogger` runs an SDL event loop on a worker thread and writes input snapshots to `actions.bin`.

Current input model:

- one optional `SDL_Gamepad*`
- per-axis floating-point state array
- pressed gamepad buttons as a set
- pressed keyboard scancodes as a set

Event handling behavior:

- initializes SDL with `SDL_INIT_EVENTS | SDL_INIT_GAMEPAD`
- opens a gamepad on `SDL_EVENT_GAMEPAD_ADDED`
- updates internal state on axis/button/key events
- serializes one full `GamepadState` snapshot after each processed event

Timestamp behavior:

- each snapshot gets `monotonic_ns` from `std::chrono::steady_clock`

Concurrency behavior:

- internal state is protected by an SDL mutex
- writing happens after a state snapshot is taken

Important current limitation:

- input logging is event-driven, not fixed-rate sampled
- if no input events occur, no new `GamepadState` records are written

### `BinaryIO`

File:

- `include/BinaryIO.hpp`

This is a small but important utility layer for binary framing.

Responsibilities:

- write/read `uint32_t` in little-endian order
- write/read length-prefixed payloads

It is used by:

- `InputLogger::WriteState()`
- `TrajectoryReplayer::LoadActions()`
- tests in `tests/BinaryIOTests.cpp`

This is currently the most isolated deterministic part of the codebase and the only part covered by automated tests.

### Protobuf Schema

File:

- `protos/gamepad.proto`

Current message:

- `trajectory.GamepadState`

Fields:

- `uint64 monotonic_ns`
- `repeated float axes`
- `repeated uint32 pressed_buttons`
- `repeated uint32 pressed_keys`

Generated files are produced by Meson/protoc and compiled into the core library.

## Replay / Conversion Path

Files:

- `include/Replayer.hpp`
- `src/Replayer.cpp`
- `src/main_convert.cpp`

`TrajectoryReplayer` exists in source form and implements:

- CSV loading from `sync.csv`
- binary action loading from `actions.bin`
- timestamp-to-action alignment via `std::upper_bound`
- frame iteration through OpenCV `VideoCapture`

Important current state:

- replay/conversion code still depends on OpenCV
- the OpenCV Conan dependency is currently commented out
- `src/Replayer.cpp` is not part of the current `core_lib`
- `convert_dataset` is commented out in `meson.build`

Implication for agents:

- replay code exists and is useful context, but it is not currently part of the supported build
- re-enabling replay requires restoring OpenCV dependency management and the Meson targets

## Data Formats

### `sync.csv`

Columns:

- `frame_index`
- `monotonic_ns`
- `pts`

Produced by:

- `SyncLogger`

Consumed by:

- `TrajectoryReplayer::LoadSyncData()`

### `actions.bin`

Format:

- repeated records
- each record begins with a 4-byte little-endian length prefix
- the prefix is followed by a serialized `trajectory::GamepadState` protobuf payload

Produced by:

- `InputLogger::WriteState()`

Consumed by:

- `TrajectoryReplayer::LoadActions()`

### `capture.mp4`

Produced by:

- `VideoRecorder`

Consumed by:

- `TrajectoryReplayer`

## Dependency Boundaries

Current external library usage is intentionally concentrated:

- GStreamer is isolated to `VideoRecorder` and `main_record.cpp`
- SDL is isolated to `InputLogger`
- Protobuf is isolated to the generated `gamepad.pb.*` files, `InputLogger`, and replay code
- OpenCV is isolated to replay/conversion code

This separation is useful when making changes:

- recording pipeline changes should usually stay in `VideoRecorder`
- input schema or capture changes usually affect `InputLogger` and `gamepad.proto`
- synchronization format changes affect both `SyncLogger` and `TrajectoryReplayer`

## Known Build/Architecture Constraints

These are important for agents because they explain several non-obvious build decisions.

### Windows-first build assumptions

The current implementation assumes:

- MSVC
- Windows console signal handling
- Windows GStreamer installation layout
- a GStreamer pipeline using `d3d11screencapturesrc`

### GStreamer discovery is customized

The build does not rely on Meson `dependency('gstreamer-1.0')` anymore.

Reason:

- pkg-config-derived linker flags from a Windows install under `Program Files` caused MSVC linker failures

Current solution:

- `meson.build` links GStreamer via explicit `find_library()` calls against a `gstreamer_root` passed through the native file

### Conan/Meson settings must stay aligned

The successful build depends on Conan dependencies being rebuilt with settings matching the actual Meson/MSVC build.

Reason:

- mismatched protobuf binaries previously caused linker errors

Current solution:

- `scripts/build.ps1` forces Conan install settings for `compiler.cppstd=20`, runtime, and build type
- it explicitly rebuilds protobuf from source when needed

### Generated protobuf headers are not treated as a public include surface

The protobuf-generated header is available for sources that need it, but the public `Session.hpp` avoids including `InputLogger.hpp`.

Reason:

- this keeps the generated-header dependency from leaking into `main_record.cpp`

## Testing Status

Current test coverage is minimal.

Enabled tests:

- `tests/BinaryIOTests.cpp`

Covered behavior:

- little-endian `uint32_t` encoding
- length-prefixed payload round-trip
- short-read failure path

Not currently covered by tests:

- `Session`
- `VideoRecorder`
- `InputLogger`
- replay path
- CLI behavior

Agents adding behavior should prefer adding tests around deterministic utilities first, and characterization tests for file formats when practical.

## Suggested Navigation Order For Agents

For most changes, read files in this order:

1. `README.md`
2. `scripts/build.ps1`
3. `meson.build`
4. the public header for the module you are changing
5. the corresponding `src/*.cpp`
6. `protos/gamepad.proto` if the change touches input serialization

Typical task entry points:

- recording lifecycle: `src/main_record.cpp`, `include/Session.hpp`, `src/Session.cpp`
- video pipeline: `include/VideoRecorder.hpp`, `src/VideoRecorder.cpp`
- input capture and binary framing: `include/InputLogger.hpp`, `src/InputLogger.cpp`, `include/BinaryIO.hpp`
- disabled replay path: `include/Replayer.hpp`, `src/Replayer.cpp`, `src/main_convert.cpp`

## Current Supported Output

At the moment, the actively supported built executable is:

- `record_session`

`convert_dataset` is present in source but not currently enabled in the build.
