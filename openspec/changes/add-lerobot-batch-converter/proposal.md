## Why

Recorded gameplay sessions can already be captured as synchronized video and raw gamepad input, and action mappings can already translate low-level controller state into a game-specific action vocabulary. What is missing is a defined conversion path from those session artifacts into a LeRobotDataset that can be used directly by downstream training and tooling.

This change is needed now because the recorder and mapper pipelines are already in place, example game definition and mapping files exist, and the next step in the roadmap depends on turning captured sessions into a reusable training dataset format.

## What Changes

- Add a batch-oriented conversion tool that ingests a directory of recorded session folders and produces one LeRobotDataset containing one episode per valid session.
- Define v1 conversion behavior for aligning video frames to event-driven gamepad snapshots using the last known gamepad state at or before each frame.
- Define a simple v1 high-level action encoding that builds one dense float action vector from the ordered mapped action list, with digital actions encoded as `0/1`, scalar analog or trigger actions encoded as one float, and `vector2` actions encoded as two floats.
- Restrict v1 observations to the recorded video stream only. Raw controller state and keyboard state are not exported as dataset features.
- Add conversion parameters for dataset task text, maximum pre-action video duration, and strict versus best-effort batch validation behavior.
- Define how source recording and mapping metadata is preserved in the generated LeRobotDataset metadata stored in `meta/info.json`.

## Capabilities

### New Capabilities
- `batch-session-conversion`: Convert a directory of recorded gameplay sessions plus a shared action mapping profile into a single multi-episode LeRobotDataset.
- `action-vector-encoding`: Derive a stable dense action vector from mapped high-level game actions for each converted frame.
- `conversion-metadata-persistence`: Persist source game, mapping, and conversion metadata into LeRobotDataset metadata for downstream inspection.

### Modified Capabilities

## Impact

- Adds a new converter CLI in `lerobot-converter`.
- Reads artifacts produced by `trajectory-recorder-cpp` and consumes action mapping YAML produced by the mapping workflow.
- Depends on the local `lerobot` dataset creation APIs and the LeRobot v3 metadata model.
- Establishes the v1 dataset contract that later encoding and metadata enhancements will extend.
