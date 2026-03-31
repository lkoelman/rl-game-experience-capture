# trajectory-recorder-cpp

C++20 scaffold for recording synchronized gameplay trajectories as video frames plus input state streams for downstream reinforcement-learning dataset conversion.

## Planned stack

- Conan for package management
- Meson and Ninja for builds
- GStreamer for capture
- SDL3 for input
- Protobuf for action serialization
- OpenCV for offline replay

## Layout

- `include/` public headers
- `src/` implementation and CLI entrypoints
- `protos/` protobuf schema and Meson generation rules
- `tests/` no-dependency unit tests for binary framing helpers
- `scripts/build.ps1` helper to run Conan, Meson, and tests

## Dependency installation

### Windows

- Install Conan from https://conan.io/downloads
- Install Meson: https://mesonbuild.com/SimpleStart.html#windows1

## Build

Prerequisites on Windows:

- Conan available on `PATH`
- Meson and Ninja available on `PATH`
- A working C++20 compiler toolchain
- `protoc` available on `PATH`
- GStreamer development/runtime packages installed

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

## Executables

- `record_session [output_dir] [session_name]`
- `convert_dataset <capture.mp4> <sync.csv> <actions.bin>`
