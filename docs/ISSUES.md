
# Enhancements

## Write gamepad state at fixes rate

The current event-driven architecture of `InputLogger` writes the full gamepad state whenever a gamepad event occurs, i.e. when a button is pressed or released. This may make the logged gamepad states sparse, making it more difficult to chop up recorded episodes. In addition to the event-driven serialization, we may want to write the full gamepad state at a fixed rate (e.g. at video FPS).

Optional: we could also look at triggering a gamepad state write when `VideoRecorder::PadProbeCallback` is called. This makes the architecture more complex (and classes more coupled?) but at least we know for sure we have a full gamepad state for each frame.

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