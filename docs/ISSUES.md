
# Enhancements

## Reuse gamepad.proto in lerobot-converter

It looks like the GamepadState message format is defined again inside the python code. We want to reuse the (compiled) protobuf message.

## Build Binaries in CI

- can we build on Windows runner with MSVC 2022 compiler toolchain?
- integrate with GitHub releases

## Recording UI for Streamers

- low friction recording UI for gamers and streamers
- features
    - manage recordings
    - one-click upload to HF or custom platform