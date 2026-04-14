# PRD: Enhanced Recording Validation Tool

## Summary

Add a new executable that validates sessions recorded by `record_session` and helps a developer quickly answer three questions:

1. Is this session structurally valid and complete?
2. Does the session contain meaningful input activity worth keeping?
3. Can I inspect the frame-by-frame alignment between video frames and recorded input snapshots?

This tool extends the existing proposal with integrity checks, richer summary statistics, warning/failure classification, multi-session reporting, and a more capable interactive step mode.

The validator operates on a single session directory or on a parent output directory containing multiple session directories.

A session directory is expected to contain:

- `capture.mp4`
- `sync.csv`
- `actions.bin`

## Goals

- Provide a fast offline check that a recorded session is usable for downstream replay or dataset construction.
- Surface recording defects clearly, not just raw statistics.
- Make it easy to identify dead recordings, malformed data, long idle periods, and timing anomalies.
- Support both human-readable summaries and machine-readable output for automation.
- Provide an interactive mode for inspecting frame-to-input alignment.

## Non-Goals

- The validator does not modify or repair recordings.
- The validator does not re-encode video.
- The validator does not infer unrecorded inputs between snapshots.
- The validator does not attempt to determine gameplay quality or semantic task success.

## Important Data Model Constraint

`actions.bin` is event-driven, not fixed-rate sampled.

This means:

- A new `GamepadState` is written only when an input event is processed.
- In step mode, the displayed input for a frame is the latest recorded state whose timestamp is less than or equal to that frame timestamp.
- If many frames occur with no new input event, the aligned state may remain unchanged for a long span.
- Statistics and warnings must be phrased in terms of recorded snapshots and effective aligned state, not continuous ground truth.

## User Personas

- Developer manually checking whether a recording session looks correct.
- Dataset curator reviewing many sessions and discarding bad ones.
- CI or scripting user who wants a machine-readable validation result.

## Executable

Add a new executable named `validate_recording`.

## Invocation

### Single session

```powershell
validate_recording <session_dir>
```

### Multiple sessions

```powershell
validate_recording <sessions_root>
```

When a root directory is provided, the tool should discover immediate child directories that look like session folders and validate each one consecutively.

## Operating Modes

### 1. Full Summary Mode

Default mode.

Walk the recording and print a structured summary for each session.

#### Required baseline statistics

- total frame count
- total action snapshot count
- recording duration
- average gamepad input events per second
- dead period at the start: duration from first frame to first recorded input
- dead period at the end: duration from last recorded input to last frame
- list of all buttons pressed and their frequency
- list of all axes moved and their frequency

#### Additional required statistics

##### Session duration and timing

- first frame timestamp
- last frame timestamp
- total wall-clock duration derived from `sync.csv`
- average frame interval
- minimum frame interval
- maximum frame interval
- estimated effective FPS from `sync.csv`

##### Input activity

- count of distinct buttons ever pressed
- count of distinct axes ever moved beyond a configurable activity threshold
- count of distinct keyboard keys ever pressed
- total number of button press observations
- total number of axis movement observations
- total number of keyboard activity observations
- longest idle span with no new input snapshots anywhere in the recording
- percentage of frames that reuse a stale previously-recorded input snapshot
- timestamp of first input event
- timestamp of last input event

##### Alignment and coverage

- whether every decoded frame has a corresponding `sync.csv` row
- whether every frame aligns to at least one effective input state
- number of frames that occur before the first input snapshot
- number of frames that occur after the last input snapshot
- whether the session contains zero input snapshots

##### Integrity checks

- required files present or missing
- `capture.mp4` can be opened
- `sync.csv` can be parsed
- `actions.bin` can be parsed
- `sync.csv` rows are monotonic by `frame_index`
- `sync.csv` timestamps are monotonic by `monotonic_ns`
- action timestamps are monotonic by `monotonic_ns`
- `sync.csv` row count matches decoded video frame count
- malformed or truncated binary payloads are detected and reported
- empty files are detected and reported

#### Derived warnings and failures

Each session must end with a verdict:

- `PASS`
- `WARN`
- `FAIL`

Examples of conditions:

##### Fail conditions

- missing required file
- video cannot be opened
- malformed `sync.csv`
- malformed or truncated `actions.bin`
- non-monotonic timestamps
- decoded frame count and `sync.csv` row count differ significantly
- zero frames in session

##### Warn conditions

- zero input snapshots
- long dead period at start
- long dead period at end
- unusually long idle span in middle of session
- very low input event rate
- highly irregular frame intervals
- large percentage of frames aligned to stale unchanged input

Thresholds should be configurable by CLI flags, with sensible defaults.

#### Multi-session output

When validating a root containing multiple sessions:

- print a one-line result for each session first
- then print the full detailed summary for each session
- print an aggregate summary at the end

Aggregate summary must include:

- number of sessions scanned
- number of passes, warnings, and failures
- number of sessions with no input
- average session duration
- average input events per second across sessions
- most common warning/failure categories

### 2. Step Mode

Interactive inspection mode.

The user can step through the recording frame by frame using arrow keys.

For each frame, display:

- frame index
- elapsed time since first frame
- frame timestamp
- delta from previous frame
- latest aligned input timestamp
- current effective input state
- whether the displayed input is newly updated on this frame or carried forward from an earlier snapshot

#### Required displayed input details

- pressed buttons
- moved axes and values
- pressed keyboard keys

#### Required change view

For each frame, also show changes relative to the previous displayed frame:

- button pressed
- button released
- axis changed beyond threshold
- key pressed
- key released

#### Navigation controls

- right arrow: next frame
- left arrow: previous frame
- up arrow: jump forward by a larger step
- down arrow: jump backward by a larger step
- dedicated commands for:
  - next input change
  - previous input change
  - jump to first input
  - jump to last input
  - jump to longest idle gap
  - quit

The UI may remain terminal-based and text-first. It does not need to render the actual video frame if that adds substantial complexity. v1 may validate and inspect alignment based on timestamps and decoded frame progression, with a textual per-frame view.

## CLI Options

The tool should support:

- `--mode summary|step`
- `--json`
- `--csv`
- `--warnings-only`
- `--session <name>` when a root directory is provided
- `--axis-threshold <float>` for considering an axis “moved”
- `--warn-start-dead-ms <n>`
- `--warn-end-dead-ms <n>`
- `--warn-idle-gap-ms <n>`
- `--warn-min-input-rate <float>`
- `--strict` to promote warnings to failures for automation

If no mode is specified, default to `summary`.

If `--json` or `--csv` is specified, summary mode remains the source of exported data. Step mode should remain human-interactive only.

## Output Requirements

### Human-readable summary

The human-readable output should be easy to scan and include:

- session header
- verdict
- integrity results
- timing statistics
- input activity statistics
- warning/failure list

### JSON output

JSON output should include:

- session path
- verdict
- list of warnings
- list of failures
- all computed metrics as named fields
- aggregate summary object when validating multiple sessions

### CSV output

CSV output should emit one row per session with key summary fields, including:

- session name
- verdict
- duration
- frame count
- action snapshot count
- average input rate
- start dead period
- end dead period
- longest idle span
- warning count
- failure count

## Public Interfaces / Data Structures

The implementation should introduce an internal validation model that separates parsing/alignment from presentation.

Expected internal concepts:

- parsed session artifacts
- session validation metrics
- session issues with severity
- session verdict
- batch aggregate summary

No changes are required to the recording file formats for v1.

## Acceptance Criteria

### Summary mode

- A valid session directory produces a readable summary without errors.
- A root directory containing multiple sessions validates each session and prints aggregate results.
- Missing or malformed session artifacts produce explicit failures rather than crashes.
- The tool reports both required baseline statistics and the enhanced integrity/timing metrics.
- Verdicts are consistent with detected issues.
- `--json` emits machine-readable results for all sessions processed.
- `--csv` emits one summary row per session.

### Step mode

- The user can move forward and backward through aligned frames.
- The tool shows elapsed time since first frame.
- The tool shows the effective aligned input state for the current frame.
- The tool highlights changes since the previous displayed frame.
- The tool can jump to the next and previous input-change frame.
- The tool exits cleanly on quit.

## Edge Cases

The validator must handle:

- session with no input events
- session with one frame only
- session with one input snapshot only
- session where first input occurs after many frames
- session where last input occurs long before final frame
- empty `actions.bin`
- empty `sync.csv`
- extra files in session directory
- multiple sessions where some pass and some fail
- frame count mismatch between decoded video and `sync.csv`

## Suggested Implementation Notes

- Reuse the existing replay/parsing concepts where possible instead of duplicating binary and CSV parsing logic.
- Keep validation logic independent from terminal rendering so summary mode and step mode share the same parsed/aligned session model.
- Prefer deterministic tests around parsing, timestamp alignment, warning generation, and verdict classification.
- If video rendering in step mode is deferred, document that the mode is frame-index-based textual inspection rather than visual playback.

## Test Plan

Add tests for:

- single valid session summary
- root directory with multiple sessions
- missing file failure cases
- malformed `sync.csv`
- truncated `actions.bin`
- non-monotonic timestamp detection
- zero-input-session warning
- dead-period calculations
- longest-idle-gap calculation
- stale-input-frame percentage calculation
- JSON output shape
- CSV row generation
- step-mode navigation over aligned frame data
- step-mode change detection between displayed frames

## Open Defaults Chosen For v1

Unless changed later, use these defaults:

- summary mode is the default
- axis activity threshold: small non-zero threshold to avoid noise
- batch scan only immediate child directories of the provided root
- verdict is `FAIL` if structural parsing/integrity fails
- verdict is `WARN` for suspicious but parseable sessions
- step mode is text-based, not video-rendered
