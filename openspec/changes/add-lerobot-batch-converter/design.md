## Context

The existing pipeline already produces the source artifacts needed for offline dataset generation:

- `trajectory-recorder-cpp` records per-session `capture.mp4`, `sync.csv`, and `actions.bin`.
- `map_actions` produces `action-mapping.yaml`, which translates raw controller controls into named high-level game actions defined in a game definition YAML.
- `lerobot` provides the LeRobotDataset v3 write path through `LeRobotDataset.create()`, `add_frame()`, `save_episode()`, and `finalize()`.

The converter is a cross-cutting change because it has to bridge three representations:

1. Frame-timestamped recorded gameplay sessions
2. Event-driven raw gamepad state snapshots
3. LeRobotDataset’s frame-oriented episode model with explicit feature schema and metadata

The main constraints are:

- v1 is batch-oriented and converts a root of session folders into one dataset.
- v1 supports gamepad input only and ignores keyboard state for action derivation.
- v1 preserves the original recorded video frame cadence and does not resample.
- v1 observations are video-only.
- v1 stores source and conversion metadata in `meta/info.json`, which in practice is a JSON dictionary written by `lerobot` and can carry explicitly defined extension fields.

## Goals / Non-Goals

**Goals:**

- Produce one LeRobotDataset with one episode per valid session directory.
- Define deterministic frame-to-action alignment based on the most recent gamepad snapshot at or before each video frame.
- Define a stable dense action vector layout from the ordered high-level action list.
- Support best-effort batch conversion by default and strict failure behavior with a flag.
- Persist useful source, mapping, and conversion metadata in dataset metadata for downstream inspection.

**Non-Goals:**

- Support keyboard-derived actions.
- Export raw controller state as dataset features.
- Support alternate action encodings such as tokens, bins, or learned embeddings.
- Resample video or action streams to a target FPS different from the recorded source.
- Introduce per-frame or per-session task manifests in v1 beyond one constant task string for the dataset.

## Decisions

### Decision: Convert one session folder into one LeRobot episode

The converter will treat each valid session directory under the input root as one episode in the output dataset.

Rationale:

- It matches the current recording model, which already produces session-bounded artifacts.
- It keeps error handling simple in batch mode: invalid sessions can be skipped without corrupting valid ones.
- It maps naturally onto LeRobot’s episode abstraction.

Alternative considered:

- Aggregating multiple sessions into one episode was rejected because it would destroy session boundaries and complicate provenance.

### Decision: Use video frames as the authoritative sampling timeline

The converter will iterate video frames in recorded order, use `sync.csv` as the frame timestamp source, and derive one output frame per retained video frame.

Rationale:

- LeRobotDataset is frame-oriented at write time.
- The recording pipeline already treats the video stream as the visual ground truth and stores the alignment data in `sync.csv`.
- This avoids inventing synthetic frame times from sparse input events.

Alternative considered:

- Sampling from input events and seeking nearest video frames was rejected because it would undersample observations and produce variable visual cadence.

### Decision: Align with last-known gamepad state and bound the pre-action region

For each video frame, the converter will use the most recent gamepad snapshot whose `monotonic_ns` is less than or equal to the frame timestamp. Frames before the first snapshot are retained only while they stay within the configured maximum pre-action duration; older leading frames are dropped.

Rationale:

- Input logging is event-driven, so holding the latest state is the only stable interpretation of “button still held” and unchanged sticks or triggers.
- Making the pre-action window configurable preserves useful context while preventing arbitrarily long inactive video prefixes from dominating the dataset.

Alternatives considered:

- Dropping all frames before the first action was rejected because it removes useful approach context.
- Zero-filling the entire leading region without a bound was rejected because it can preserve irrelevant idle footage.

### Decision: Build one dense float action vector from ordered mapped actions

The converter will collect actions in the exact ordered list returned by the selected `class_ids`, then append slots by action kind:

- `digital` → 1 float slot with values `0.0` or `1.0`
- `analog` → 1 float slot
- `trigger` → 1 float slot
- `vector2` → 2 float slots

Unmapped actions remain in the layout and emit zeros. The resulting dataset feature is a single `action` float vector.

Rationale:

- It is the simplest useful v1 contract and is directly compatible with LeRobot’s low-dimensional feature model.
- Keeping unmapped actions in the vector preserves shape stability across runs and across partially complete profiles.

Alternatives considered:

- Excluding unmapped actions was rejected because it would make the feature shape depend on profile completeness.
- Multiple per-action features were rejected because a single vector is easier to consume in training code and easier to version in later specs.

### Decision: Restrict observations to one visual feature in v1

The output dataset will contain one video-backed observation feature representing the recorded gameplay frames and no raw controller observation features.

Rationale:

- This matches the current product decision for v1.
- It keeps the initial schema small while still preserving the essential observation stream.

Alternative considered:

- Adding raw axes and button features was rejected because the user explicitly scoped v1 to image/video observations only.

### Decision: Extend `meta/info.json` with a namespaced converter metadata block

The converter will store source and conversion metadata in `meta/info.json` under an explicit extension key rather than as dataset frame features. The design will use a dedicated namespaced object such as `extensions.game_converter` to avoid colliding with stock LeRobot fields.

Expected contents include:

- source game and display metadata
- selected `class_ids`
- mapping profile metadata
- converter settings such as task text, strictness, and pre-action duration
- source session inventory and skipped-session summaries
- action layout description for the emitted dense vector

Rationale:

- The Hugging Face LeRobot v3 format description identifies `info.json` as the central schema and dataset metadata file.
- The local implementation reads and writes `info.json` as a plain dictionary and does not enforce a closed schema for unknown top-level keys.
- Using a namespaced extension block keeps the custom metadata explicit and reduces the risk of future collisions with upstream fields.

Alternatives considered:

- Storing metadata as frame features was rejected because the user explicitly excluded that.
- Writing only an external manifest was rejected because the user wants the metadata inside LeRobot metadata.

### Decision: Preserve recorded FPS and validate consistency per session

The dataset FPS will be derived from the recorded source and must match the session video cadence used during conversion. v1 will not resample.

Rationale:

- The user explicitly ruled out resampling.
- Resampling would require defining interpolation behavior for both visual and action streams, which is outside v1.

Alternative considered:

- Accepting a target FPS argument was rejected for v1 because it creates a second alignment problem the current design does not need.

### Decision: Best-effort batch conversion by default, strict mode as opt-in

The converter will skip invalid sessions by default, report them in conversion output and stored metadata, and continue converting valid sessions. With `--strict`, any invalid session will fail the run.

Rationale:

- Batch conversion should be resilient to imperfect capture directories.
- Strict mode still supports validation-driven workflows and CI.

Alternative considered:

- Always failing on first invalid session was rejected because it blocks useful batch progress.

## Risks / Trade-offs

- [LeRobot metadata extension fields may not be recognized by upstream tooling] → Use a namespaced block and keep stock fields unchanged so generic readers still function.
- [Recorded sessions may have inconsistent or malformed timing artifacts] → Validate required files and timing assumptions per session, skip by default, and surface structured failure reasons.
- [Action profile completeness may vary across games or sessions] → Keep the vector layout stable and emit zeros for unmapped actions while recording completeness and unmapped action IDs in metadata.
- [Example mappings may contain semantically incomplete bindings despite `complete: true`] → Treat profile completeness as advisory and derive action values from actual bindings present in the profile.
- [Preserving original FPS may make mixed-FPS batches ambiguous] → Require a consistent dataset FPS strategy and fail or skip sessions whose cadence cannot be reconciled with the dataset-level FPS.

## Migration Plan

- No runtime migration is required because this introduces a new converter path rather than changing an existing one.
- Existing recorded session directories remain the source of truth and are read-only inputs.
- If the metadata extension shape changes later, future converter versions can write a newer extension schema version inside the namespaced metadata block.

## Open Questions

- Whether mixed-FPS session roots should be rejected entirely or allowed only when all retained sessions share the same effective FPS.
- Whether the CLI should require the game definition path explicitly even though the mapping profile already contains `game_id`.
