# trajectory-recorder-cpp

C++20 scaffold for recording synchronized gameplay trajectories as video frames plus input state streams for downstream reinforcement-learning dataset conversion.

## Software stack

- Conan for package management
- Meson and Ninja for builds
- GStreamer for capture
- SDL3 for input
- Protobuf for action serialization
- OpenCV for offline replay
- FTXUI for TUI
- yaml-cpp for mapping config IO

## Layout

- `include/` public headers
- `src/` implementation and CLI entrypoints
- `protos/` protobuf schema and Meson generation rules
- `tests/` no-dependency unit tests for binary framing helpers
- `specs/` product and implementation specs
- `scripts/build.ps1` helper to run Conan, Meson, and tests

## Dependency installation

### Windows

- Install Conan from https://conan.io/downloads
- Install Meson: https://mesonbuild.com/SimpleStart.html#windows1
- Install GStreamer 1.X runtime and development files from https://gstreamer.freedesktop.org/download/#windows

## Build

Prerequisites on Windows:

- Conan available on `PATH`
- Meson and Ninja available on `PATH`
- A working C++20 compiler toolchain
- GStreamer development/runtime packages installed
- ViGEmBus installed if you want to run the virtual gamepad bridge sample

The build helper links GStreamer directly from an existing Windows installation root instead of relying on pkg-config-derived linker flags for the GStreamer libraries.

If GStreamer is installed at `C:\Program Files\gstreamer\1.0\msvc_x86_64`, pass that root explicitly:

```powershell
./scripts/build.ps1 -GStreamerRoot 'C:\Program Files\gstreamer\1.0\msvc_x86_64'
```

The helper exports:

- `GSTREAMER_1_0_ROOT_X86_64`
- `PATH += <GStreamerRoot>\bin`
- `PKG_CONFIG_PATH += <GStreamerRoot>\lib\pkgconfig`

If GStreamer is installed somewhere else, pass the root explicitly:

```powershell
./scripts/build.ps1 -GStreamerRoot D:\path\to\gstreamer\1.0\msvc_x86_64
```

Open a Developer Powershell for VS 20XX (shell with compiler toolchain installed)

Then run:

```powershell
./scripts/build.ps1
```

The helper rebuilds Conan-managed dependencies to match the active MSVC runtime, build type, and C++20 setting when needed, and it uses the Conan-provided `protoc`.

## Fast Rebuild

If `builddir` already exists from a successful `./scripts/build.ps1` run, you can do an incremental rebuild without rerunning Conan or Meson reconfiguration.

Open a Developer PowerShell for VS and run:

```powershell
meson compile -C builddir
```

If you also want to rerun the test suite after the incremental rebuild:

```powershell
meson test -C builddir --print-errorlogs
```

Use `./scripts/build.ps1` again when dependencies, Conan settings, GStreamer location, or Meson configuration have changed.

## Clean Build Files

To remove the default build output directory created by the helper script:

```powershell
Remove-Item -Recurse -Force builddir
```

To remove Conan-generated temporary and build files created by `conan install`, delete the generated files from the build directory:

```powershell
Remove-Item -Recurse -Force builddir
```

If you want to clear Conan's local cache artifacts as well, use:

```powershell
conan remove "*" --confirm
```

This removes all cached Conan packages and metadata from your local Conan cache, so the next build will resolve and rebuild dependencies from scratch.

## Usage

After building, run the executables:

- `record_session [output_dir] [session_name]`
- `validate_recording <session_dir|sessions_root>`
- `map_actions <game-actions.yaml> [action-mapping.yaml] [--profile-name <name>] [--resume-from <existing.yaml>] [--max-combo-buttons <n>]`
- `virtual_gamepad_bridge`
- `convert_dataset <capture.mp4> <sync.csv> <actions.bin>`

### Record Game Sesion

`record_session` now supports pre-recording capture selection:

- `--monitor <id>` selects a monitor by the one-based ID shown in the selector
- `--window <title>` selects a titled window by case-insensitive substring match
- `--verbose` prints each recorded input snapshot to stdout as it is written
- if neither flag is provided, an FTXUI-based selector opens before recording starts

Examples:

```powershell
.\builddir\record_session.exe .\data test_session --monitor 1
.\builddir\record_session.exe .\data test_session --window "Notepad"
.\builddir\record_session.exe .\data test_session --monitor 1 --verbose
```

When running `record_session.exe` from PowerShell on Windows, use a Developer PowerShell and make sure the GStreamer runtime `bin` directory is on `PATH`. If Windows cannot load the GStreamer DLLs, the process can exit before `main()` starts and you will see no program output.
The recorder also pumps SDL input events on the main thread. If that polling is moved to a worker thread, SDL 3 can raise assertion popups on Windows.

Example:

```powershell
$env:GSTREAMER_1_0_ROOT_X86_64 = 'C:\Program Files\gstreamer\1.0\msvc_x86_64'
$env:PATH = "$env:GSTREAMER_1_0_ROOT_X86_64\bin;$env:PATH"

.\builddir\record_session.exe .\data test_session
```

If you want to rebuild and then run in the same shell:

```powershell
./scripts/build.ps1
$env:PATH = "C:\Program Files\gstreamer\1.0\msvc_x86_64\bin;$env:PATH"
.\builddir\record_session.exe .\data test_session
```

### Validate Recording

`validate_recording` validates one recorded session or scans a directory containing multiple session folders.

Examples:

```powershell
.\builddir\validate_recording.exe .\data\test_session
.\builddir\validate_recording.exe .\data --json
.\builddir\validate_recording.exe .\data --csv
.\builddir\validate_recording.exe .\data\test_session --mode step
```

The validator currently:

- checks for required files: `capture.mp4`, `sync.csv`, `actions.bin`
- parses `sync.csv` and `actions.bin`
- reports timing, dead-period, idle-gap, and input-frequency statistics
- supports summary, JSON, CSV, and text-based step-through modes

### Virtual Gamepad Bridge

`virtual_gamepad_bridge` is a Windows-only integration sample for ViGEmClient. Its goal is to test whether the gamepad forwarding used while recording a game session works correctly. It opens the first SDL gamepad it can see, creates a virtual Xbox 360 controller through ViGEmBus, and forwards live input changes into that virtual device.

Requirements:

- ViGEmBus must be installed before running the sample
- if you want a game to see only the virtual controller, hide the physical controller separately with [HidHide](https://github.com/nefarius/HidHide) or equivalent tooling

Run it from a Developer PowerShell after building:

```powershell
.\builddir\virtual_gamepad_bridge.exe
.\builddir\virtual_gamepad_bridge.exe --max-forward-log-lines-per-second 60
```

The sample prints basic lifecycle status plus rate-limited forwarded button logs in the form `<physical_gamepad_id>.<button> -> <virtual_gamepad_id>.<button>`. The default cap is 30 printed lines per second, and `--max-forward-log-lines-per-second <n>` lets you raise or lower that limit. It exits on `Ctrl+C`.

### Gamepad Mapping

`map_actions` builds a per-user mapping between raw gamepad inputs and in-game action IDs defined in a YAML game definition file.

Examples:

```powershell
.\builddir\map_actions.exe .\configs\game-actions.yaml
.\builddir\map_actions.exe .\configs\game-actions.yaml .\profiles\action-mapping.yaml --profile-name "steam-deck"
.\builddir\map_actions.exe .\configs\game-actions.yaml .\profiles\action-mapping.yaml --resume-from .\profiles\action-mapping.yaml
.\builddir\map_actions.exe .\configs\game-actions.yaml .\profiles\action-mapping.yaml --max-combo-buttons 3
```
