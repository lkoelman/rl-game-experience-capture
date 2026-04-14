# Implement Pre-Recording Monitor/Window Selection

## Summary

Add a Windows-only target-selection step before `Session` starts so `record_session` can capture either a chosen monitor or a chosen window instead of always using the default `d3d11screencapturesrc` target.

The flow should be:

- Parse CLI args first.
- If `--monitor <id>` or `--window <title>` is provided, resolve it and skip the TUI.
- Otherwise launch an FTXUI selector before GStreamer/session startup.
- Pass the resolved capture target into `Session` and then into `VideoRecorder`, which will set `d3d11screencapturesrc monitor-index` or `window-handle` in the pipeline string.

Chosen defaults from the planning pass:

- Invalid or ambiguous `--monitor` / `--window` should fail fast with a clear error and non-zero exit.
- `--window <title>` should use case-insensitive substring matching.
- Monitor IDs shown in the TUI and accepted by `--monitor` should be one-based (`1, 2, 3...`), then mapped internally to GStreamer’s zero-based monitor index.

## Key Changes

### CLI and startup flow

- Extend `record_cli::Options` to include an optional capture target:
  - monitor by one-based user-facing ID
  - window by title query
- Update `TryParseArguments()` to support named flags while preserving existing positional `output_dir` and `session_name`.
- Update usage/help text in `include/RecordCli.hpp` and README to document:
  - `record_session [output_dir] [session_name] [--monitor <id> | --window <title>]`
  - that the TUI appears when neither selector flag is provided
- In `src/main_record.cpp`, resolve the capture target before GStreamer init and before constructing `Session`.

### Capture target model and recorder integration

- Introduce a small public capture-target type used by CLI, `Session`, and `VideoRecorder`, for example:
  - `CaptureMode::monitor`
  - `CaptureMode::window`
  - monitor target with user-facing ID + resolved zero-based monitor index
  - window target with resolved `HWND`
- Change `Session` construction to accept the resolved capture target and forward it to `VideoRecorder`.
- Replace the current single-argument pipeline builder in `include/VideoRecorderPipeline.hpp` with a variant that injects the correct `d3d11screencapturesrc` properties:
  - monitor capture: `monitor-index=<resolved_zero_based_index>`
  - window capture: `window-handle=<HWND_as_uint64>`
- Keep `VideoRecorder` responsible only for pipeline creation and recording; keep Win32 enumeration/TUI logic outside it.

### Windows enumeration and FTXUI selector

- Add a Windows-only selector module that:
  - enumerates monitors via Win32 APIs and gathers identifying text such as display name/model when available, plus resolution and position as fallback
  - enumerates top-level visible windows with non-empty titles and captures `HWND`, title, and ideally process name/PID for disambiguation
  - resolves CLI selectors:
    - `--monitor <id>` by one-based displayed monitor ID
    - `--window <title>` by case-insensitive substring match, erroring on zero or multiple matches
- Add an FTXUI TUI module that:
  - starts with a mode choice: monitor/fullscreen vs window
  - shows the first page of 1-9 targets with direct numeric shortcuts
  - supports `<space>` to page to more results
  - clearly shows enough metadata to distinguish similar windows/monitors
  - returns a fully resolved capture target or cancel/error
- Keep the TUI isolated from recorder internals so future non-TUI selection methods can reuse the same enumeration/resolution layer.

### Build and dependency changes

- Add FTXUI as a package dependency in Conan and wire it into Meson.
- Keep the new selector/TUI code Windows-only; avoid introducing FTXUI into the non-interactive recorder path beyond startup selection.
- Update docs if needed:
  - `README.md`
  - `docs/ARCHITECTURE.md`

## Public Interfaces / Types

- `record_cli::Options` gains optional capture-selection fields.
- `TryParseArguments()` behavior expands to named options plus existing positional args.
- `Session` constructor gains a capture-target parameter.
- `VideoRecorder` constructor and pipeline builder gain a capture-target parameter.
- New selector-facing types should stay small and explicit, e.g. `CaptureTarget`, `MonitorInfo`, `WindowInfo`, `SelectionResult`.

## Test Plan

Follow TDD for each behavior slice.

Add or extend tests for:

- CLI parsing:
  - default args still work
  - `--monitor 1` parses correctly
  - `--window "Notepad"` parses correctly
  - conflicting `--monitor` + `--window` fails clearly
  - invalid numeric monitor IDs fail clearly
  - usage text reflects new syntax
- Pipeline generation:
  - monitor target injects `monitor-index=<zero_based>`
  - window target injects `window-handle=<uint64>`
  - output path normalization still works
- Resolution logic:
  - case-insensitive substring matching for windows
  - zero matches returns a clear error
  - multiple matches returns a clear ambiguity error
  - one-based monitor IDs map correctly to zero-based internal indices
- Selection presentation logic:
  - formatting of monitor/window labels is deterministic
  - page slicing / numeric shortcut mapping behaves correctly for >9 entries

Use pure unit tests for parsing, target-resolution, label formatting, and pipeline-string generation. Avoid relying on live desktop enumeration in automated tests; keep Win32 enumeration behind seams that can be fed fake monitor/window lists.

## Assumptions

- Only visible top-level windows with non-empty titles are selectable.
- If richer monitor model information is unavailable, the TUI should fall back to display name, resolution, and virtual-screen position.
- Ambiguous window-title matches are errors rather than implicit first-match selection.
- The selector runs before recording starts; there is no mid-session retargeting.
- This is a Windows-first feature and does not need a cross-platform abstraction beyond keeping the non-Windows build surface clean.
