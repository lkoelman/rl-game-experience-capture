# Architecture

This document is the entry point for coding agents modifying `lerobot-converter`.
It describes the package as it exists now: what components exist, how data flows
through them, where the important interfaces are, and which constraints shape
safe changes.

## Purpose

`lerobot-converter` converts recorded gameplay sessions into a LeRobotDataset.

The source side is a batch of session directories produced by `trajectory-recorder-cpp`:

- `capture.mp4`
- `sync.csv`
- `actions.bin`

The converter also consumes:

- a game action catalog YAML
- an action mapping profile YAML

The output side is one LeRobotDataset with:

- one episode per retained session directory
- one video observation feature: `observation.images.main`
- one dense float action vector feature: `action`
- converter metadata stored in `meta/info.json` under `extensions.game_converter`

## Top-Level Structure

- `game2lerobot/`
  Runtime library code, including the CLI entrypoint in `cli.py`.
- `tests/`
  Pytest coverage for core conversion behavior and CLI argument wiring.
- `README.md`
  Basic operator usage example.
- `pyproject.toml`
  Package metadata and the `game2lerobot` console script.

## Runtime Flow

The conversion path is:

1. `game2lerobot.cli`
2. `game2lerobot.pipeline.convert_sessions()`
3. `game2lerobot.alignment`
4. `game2lerobot.parsing`
5. `game2lerobot.action_encoding`
6. `game2lerobot.metadata`
7. `lerobot.datasets.LeRobotDataset`

In practical terms:

1. The CLI reads:
   - `--session-root`
   - `--game-definition`
   - `--action-mapping`
   - `--output-root`
   - `--repo-id`
   - `--task`
   - `--max-pre-action-seconds`
   - `--strict`
2. YAML inputs are parsed into in-memory domain models.
3. Session directories are discovered under the batch root.
4. Each session is validated for required files.
5. For each valid session:
   - video frames are decoded from `capture.mp4`
   - frame timestamps are read from `sync.csv`
   - raw gamepad snapshots are read from `actions.bin`
   - the converter trims leading idle video according to `--max-pre-action-seconds`
   - each retained frame is aligned to the latest gamepad snapshot at or before that frame
   - the aligned snapshot is encoded into the dense `action` vector
   - frames are written into the current LeRobot episode
6. The episode is saved.
7. After all sessions:
   - the dataset is finalized
   - converter metadata is injected into `meta/info.json`

## Module Responsibilities

### `game2lerobot.constants`

Owns protocol-level constants shared across parsing and encoding:

- required session filenames
- stable button name to SDL button-id mapping
- stable axis names
- stick-to-axis expansion

Change this module when:

- recorder artifact names change
- control-name vocabulary changes
- you need to align with changes from the upstream mapper/recorder

Be careful:

- this is shared infrastructure; changing names here changes the interpretation of every mapping profile and raw snapshot

### `game2lerobot.models`

Owns the internal domain types that connect every stage:

- `ActionDefinition`
- `ActionBinding`
- `ActionLayoutEntry`
- `GamepadSnapshot`
- `SessionValidationResult`
- `GameClass`
- `GameDefinition`
- `ActionMappingProfile`
- `ConversionMetadata`
- `ConversionResult`

These are the handoff format between:

- YAML parsing
- binary parsing
- alignment
- action encoding
- dataset export

Change this module when:

- the internal contract between stages changes
- new metadata needs to be persisted
- action layout description needs more structure

Be careful:

- `ConversionResult.dataset` is intentionally excluded from equality checks so tests can compare results without comparing LeRobot internals

### `game2lerobot.alignment`

Owns session discovery and timeline trimming:

- `collect_session_dirs()`
- `validate_session_dir()`
- `trim_idle_frame_indices()`

Current behavior:

- session directories are immediate child directories of `session_root`
- a session is minimally valid if it contains `capture.mp4`, `sync.csv`, and `actions.bin`
- leading frames earlier than the configured pre-action window are dropped
- if there is no first action timestamp, all frames are retained

Change this module when:

- batch directory traversal rules change
- session validity rules become stricter
- pre-action trimming semantics change

### `game2lerobot.parsing`

Owns all source-file parsing:

- `read_sync_csv()`
- `read_actions_bin()`
- `load_game_definition()`
- `load_action_mapping_profile()`
- `read_video_frames()`

Important details:

- `sync.csv` is treated as the authoritative frame timestamp sequence
- `actions.bin` is read as a stream of little-endian length-prefixed protobuf payloads
- the protobuf schema is constructed dynamically in `_get_gamepad_state_message()`
- YAML parsing is intentionally permissive and currently extracts only the fields used by the v1 pipeline
- video frames are decoded eagerly into memory with PyAV

Change this module when:

- the recorder file formats change
- more YAML fields need to be preserved
- video decoding strategy changes

Be careful:

- `read_video_frames()` currently loads every frame into memory for the session; this is simple but not streaming-friendly
- the dynamic protobuf descriptor must stay consistent with `trajectory-recorder-cpp/protos/gamepad.proto`

### `game2lerobot.action_encoding`

Owns the v1 high-level action semantics:

- `collect_actions_by_class()`
- `build_action_layout()`
- `encode_action_vector()`

Current action encoding contract:

- action order is the flattened order of `class_ids`
- `vector2` contributes 2 slots
- `digital` contributes 1 slot
- `analog` contributes 1 slot
- `trigger` contributes 1 slot
- unmapped actions stay in the layout and emit zeros
- keyboard state is ignored

Binding semantics currently supported:

- `button`
- `axis`
- `stick`
- `trigger`
- `combo`

Important behavior:

- trigger values are thresholded
- analog axis values can be direction-filtered
- stick bindings expand to `(x, y)`
- combo bindings require all components to be active

Change this module when:

- the action vector contract changes
- a new binding type or action kind is added
- keyboard-derived actions are introduced

Be careful:

- this module defines the training-facing action representation
- changing slot order or per-kind semantics is a dataset contract change, not just a refactor

### `game2lerobot.metadata`

Owns dataset metadata and feature-schema helpers:

- `build_features()`
- `apply_converter_metadata()`

Current dataset schema:

- `observation.images.main`
  - dtype: `video`
- `action`
  - dtype: `float32`
  - shape: flat vector derived from action layout

Current metadata extension location:

- `meta/info.json`
- key: `extensions.game_converter`

Current metadata contents:

- `game_id`
- `class_ids`
- `profile_name`
- `settings`
  - `task`
  - `strict`
  - `max_pre_action_seconds`
- `action_layout`
- `converted_sessions`
- `skipped_sessions`

Change this module when:

- dataset feature names change
- the extension schema changes
- downstream consumers need richer metadata

Be careful:

- keep converter-owned metadata namespaced; do not overwrite stock LeRobot keys unless the upstream API requires it

### `game2lerobot.pipeline`

Owns orchestration of the whole batch conversion:

- `convert_sessions()`

This is the only module that touches both:

- source session artifacts
- LeRobot dataset writing

Current responsibilities:

- collect ordered action definitions from class ids
- iterate discovered sessions
- enforce strict vs best-effort behavior
- validate FPS consistency across the batch
- create the dataset lazily from the first valid session
- align frames to snapshots
- write episodes
- finalize the dataset
- patch `meta/info.json` with converter metadata

Internal helpers:

- `_resolve_expected_fps()`
- `_write_session_episode()`

Change this module when:

- batch semantics change
- dataset creation strategy changes
- episode writing needs different control flow

Be careful:

- this module is where failures become either skipped-session reasons or fatal errors
- changing exception handling here changes CLI behavior even if lower modules stay the same

### `game2lerobot.cli`

CLI boundary only.

Responsibilities:

- parse batch arguments with `argparse`
- load YAML inputs
- call `convert_sessions()`

This file should stay thin. Business logic belongs in the rest of `game2lerobot/`.

## External Interfaces

### CLI Interface

Current command:

```bash
uv run game2lerobot \
  --session-root <dir> \
  --game-definition <path> \
  --action-mapping <path> \
  --output-root <dir> \
  --repo-id <repo-id> \
  --task <text> \
  --max-pre-action-seconds <float> \
  [--strict]
```

Behavior:

- default mode skips invalid sessions and records the reason
- `--strict` fails on the first invalid session

### Python Library Interface

Primary callable:

- `game2lerobot.convert_sessions(...)`

Primary parser entrypoints:

- `load_game_definition(path)`
- `load_action_mapping_profile(path)`
- `read_sync_csv(path)`
- `read_actions_bin(path)`
- `read_video_frames(path)`

Primary encoding entrypoints:

- `collect_actions_by_class(...)`
- `build_action_layout(...)`
- `encode_action_vector(...)`

## Data Formats

### Session Root Layout

Expected batch input:

```text
<session-root>/
  <session-a>/
    capture.mp4
    sync.csv
    actions.bin
  <session-b>/
    capture.mp4
    sync.csv
    actions.bin
```

Session discovery is not recursive beyond immediate child directories.

### `sync.csv`

Expected columns:

- `frame_index`
- `monotonic_ns`
- `pts`

Current parser behavior:

- only `monotonic_ns` is used
- row order defines frame order

### `actions.bin`

Current binary framing:

- 4-byte little-endian payload length
- protobuf payload

Current protobuf message shape:

- `monotonic_ns: uint64`
- `axes: repeated float`
- `pressed_buttons: repeated uint32`
- `pressed_keys: repeated uint32`

### Game Definition YAML

Current required fields used by the converter:

- `game_id`
- `display_name` optional
- `classes[]`
  - `id`
  - `actions[]`
    - `id`
    - `kind`

Other YAML fields are currently ignored by runtime code.

### Action Mapping YAML

Current required fields used by the converter:

- `game_id`
- `class_ids`
- `profile_name`
- `complete`
- `actions`
  - per-action `bindings[]`
    - `type`
    - `control`
    - optional `direction`
    - optional `threshold`
    - optional `controls` for combos

### Output LeRobotDataset

Current dataset shape:

- one dataset per conversion run
- one episode per retained session
- one video observation feature
- one dense action vector feature

Metadata extension:

```text
meta/info.json
  extensions.game_converter
```

## Tests

Current tests:

- `tests/test_converter_core.py`
  - session discovery and validation
  - pre-action trimming
  - action layout and encoding
  - metadata extension writes
  - `sync.csv` and `actions.bin` parsing
  - batch conversion output
  - strict-mode failure
- `tests/test_cli.py`
  - CLI argument parsing and delegation

When changing behavior:

- prefer adding or adjusting tests in the narrowest module-relevant area first
- keep `test_converter_core.py` focused on converter semantics rather than CLI glue

## Known Constraints

### Codec Constraint

Dataset writing currently forces `vcodec="h264"` when creating the LeRobot dataset.

Reason:

- the default LeRobot encoder path crashed in this environment during integration testing

Implication:

- if you change video encoding, rerun full tests and verify the writer path in this environment

### FPS Constraint

The batch must have one effective FPS.

Current behavior:

- the first valid session establishes dataset FPS
- later sessions with a different decoded FPS fail that session
- in best-effort mode they are skipped
- in strict mode the run fails

### Memory Constraint

`read_video_frames()` decodes the full session into memory before writing.

Implication:

- this is simple and testable, but may become expensive for long sessions
- if you need streaming conversion, `parsing.py` and `pipeline.py` are the modules to change together

### YAML Parsing Constraint

The runtime parser is intentionally minimal.

Implication:

- it does not preserve every field from the source YAMLs
- if downstream metadata or validation needs more fields, extend the dataclasses and parsers together

### Keyboard Scope Constraint

Keyboard state exists in `actions.bin` but is ignored by the v1 action encoder.

Implication:

- adding keyboard-aware actions is not a parsing-only change
- it requires a contract change in `action_encoding.py`, tests, and likely the spec/task artifacts

### Package Surface Constraint

`game2lerobot.__init__` is the supported public import surface.

Implication:

- export public library entrypoints there
- keep CLI-specific wiring in `game2lerobot.cli`
