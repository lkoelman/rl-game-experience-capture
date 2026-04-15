# Architecture

This document is the bootstrapping context for coding agents working on `trajectory-recorder-cpp`. It describes the code as it exists now, not only the original plan in `specs/recorder-initial/SPEC.md`.

## Purpose

The package records a gameplay session as three synchronized artifacts:

- `capture.mp4`: compressed screen recording written by GStreamer
- `sync.csv`: per-frame timestamp mapping written by `SyncLogger`
- `actions.bin`: length-prefixed protobuf snapshots of input state written by `InputLogger`

The intended downstream use is reinforcement-learning dataset construction from aligned `(frame, action)` pairs.

The project also includes an offline validation path for checking recorded session completeness and inspecting frame-to-input alignment.
It now also includes a gamepad action mapping path for defining how low-level controller inputs map to high-level in-game actions.

## Top-Level Structure

- `include/`: public headers for the core modules
- `src/`: implementation files and CLI entrypoints
- `protos/`: protobuf schema and Meson generation rules
- `tests/`: unit tests for deterministic helper code
- `specs/`: PRDs and implementation plans
- `scripts/build.ps1`: canonical Windows build entrypoint
- `docs/ARCHITECTURE.md`: this file

## Current Build Architecture

The project builds with Conan + Meson + Ninja on Windows.

Key points:

- Conan manages `protobuf`, `sdl`, `ftxui`, and `yaml-cpp`; OpenCV is currently commented out in `conanfile.txt`.
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
2. `trajectory::CaptureSelector`
3. `trajectory::CaptureSelection`
4. `trajectory::Session`
5. `trajectory::InputLogger`
6. `trajectory::VideoRecorder`
7. `trajectory::SyncLogger`

`main_record.cpp` is a thin CLI wrapper:

- parses capture-selection flags
- resolves a monitor/window target from CLI or the FTXUI selector
- initializes GStreamer with `gst_init`
- installs console shutdown handlers
- creates a `Session`
- pumps SDL input events on the main thread while waiting for shutdown
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
- `PumpEventsOnce()` forwards the main-thread SDL event pump to `InputLogger`
- `Stop()` stops `VideoRecorder` first, then `InputLogger`

Construction now also receives a resolved `CaptureTarget`, which it forwards to `VideoRecorder`.

The public header only forward-declares the owned classes to keep protobuf- and GStreamer-heavy dependencies out of the CLI compile boundary.

### `VideoRecorder`

Files:

- `include/VideoRecorder.hpp`
- `src/VideoRecorder.cpp`

`VideoRecorder` wraps the GStreamer pipeline and is responsible for screen capture and frame timestamp logging.

Current implementation details:

- pipeline is built from a string in `BuildPipelineDescription()`
- uses `d3d11screencapturesrc`
- accepts a resolved capture target and sets either `monitor-handle`/`monitor-index` or `window-handle`
- converts/scales to `1280x720` at `30 fps`
- inserts `identity name=probe_point` as frame synchronization hook (identifier)
- encodes with `x264enc`
- writes `mp4`

### `CaptureSelection`

Files:

- `include/CaptureSelection.hpp`
- `src/CaptureSelection.cpp`

This is the deterministic selection helper layer shared by the CLI and the interactive selector.

Responsibilities:

- represent enumerated monitor/window options
- resolve `--monitor <id>` to a capture target
- resolve `--window <title>` using case-insensitive substring matching
- format monitor/window labels for display
- build 1-9 shortcut pages for the TUI

### `CaptureSelector`

Files:

- `include/CaptureSelector.hpp`
- `src/CaptureSelector.cpp`

This is the Windows-only startup UI/integration layer.

Responsibilities:

- enumerate monitors via Win32 APIs, including display name, resolution, and position
- enumerate visible titled top-level windows and enrich them with process metadata
- run the FTXUI selector when the user did not provide `--monitor` or `--window`
- return a resolved `CaptureTarget` to `main_record.cpp`

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
- `Stop()` sends EOS, waits for EOS or ERROR on the bus so `mp4mux` can finalize the file, then sets the pipeline state to `NULL`, stops the main loop, joins the thread, and releases GStreamer resources

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

`InputLogger` runs SDL event polling on the main thread and writes input snapshots to `actions.bin`.

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
- SDL event pumping must stay on the main thread on Windows because SDL 3 asserts if `SDL_PollEvent()` runs off-thread

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

This remains one of the most isolated deterministic parts of the codebase.

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

## Validation Path

Files:

- `include/RecordingValidator.hpp`
- `src/RecordingValidator.cpp`
- `include/ValidateCli.hpp`
- `src/main_validate.cpp`

`validate_recording` is an offline consumer of recorder outputs.

Current responsibilities:

- discover session directories from a single session path or a parent output root
- validate that `capture.mp4`, `sync.csv`, and `actions.bin` exist and are non-empty
- parse `sync.csv` and `actions.bin`
- check monotonic timestamp ordering in both files
- compute summary metrics such as duration, dead periods, longest idle gap, distinct buttons/axes/keys, and average input rate
- emit `PASS` / `WARN` / `FAIL` verdicts
- render human-readable summaries or machine-readable JSON/CSV output
- provide a text-based step mode that walks aligned frames and shows the latest recorded input snapshot

Important current limitation:

- validator step mode is textual and alignment-based; it does not render decoded video frames

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

## Runtime Architecture: Action Mapping

The runtime path for action mapping is:

1. `src/main_map_actions.cpp`
2. `trajectory::map_cli`
3. `trajectory::mapping::LoadGameDefinition()`
4. `trajectory::mapping::GamepadBindingCapture`
5. `trajectory::mapping::RunMappingWorkflow()`
6. `trajectory::mapping::ValidateProfile()`
7. `trajectory::mapping::SaveActionMappingProfile()`

This path is intentionally separate from recording and validation. It produces configuration, not captured session artifacts.

### Mapping Domain Model

Files:

- `include/ActionMapping.hpp`
- `src/ActionMapping.cpp`

Responsibilities:

- represent game action definitions grouped by class
- represent per-user mapping profiles
- represent binding variants for buttons, analog axes, and thresholded triggers
- validate game definitions and mapping profiles
- provide a deterministic workflow state model for tests and the TUI flow

### YAML Mapping IO

Files:

- `include/ActionMappingYaml.hpp`
- `src/ActionMappingYaml.cpp`

Responsibilities:

- load `game-actions.yaml`
- load/write `action-mapping.yaml`
- enforce the expected YAML structure and binding field semantics

### Gamepad Binding Capture

Files:

- `include/GamepadBindingCapture.hpp`
- `src/GamepadBindingCapture.cpp`

Responsibilities:

- convert SDL gamepad events into stable binding names used in YAML profiles

Important current behavior:

- digital actions accept gamepad buttons
- analog actions accept joystick axes and currently store them as explicit per-axis bindings
- trigger actions store a threshold derived from the observed trigger press

### Mapping Workflow

Files:

- `include/ActionMappingWorkflow.hpp`
- `src/ActionMappingWorkflow.cpp`
- `include/MapCli.hpp`

Responsibilities:

- prompt for class selection
- guide the user through action-by-action mapping with FTXUI

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
- `tests/RecordCliTests.cpp`
- `tests/VideoRecorderPathTests.cpp`
- `tests/CaptureSelectionTests.cpp`
- `tests/ValidateCliTests.cpp`
- `tests/RecordingValidatorTests.cpp`

Covered behavior:

- little-endian `uint32_t` encoding
- length-prefixed payload round-trip
- short-read failure path
- CLI argument parsing for capture-selection flags
- pipeline-string generation for monitor/window targeting
- monitor/window selection resolution and TUI paging helpers
- validator CLI parsing
- session validation metrics and integrity failure cases

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
- `validate_recording`
- `map_actions`

`convert_dataset` is present in source but not currently enabled in the build.
