## ADDED Requirements

### Requirement: Batch session roots convert into one multi-episode dataset
The converter SHALL accept a batch input root containing multiple recorded session directories and SHALL write one LeRobotDataset whose retained sessions each become one episode.

#### Scenario: Convert multiple valid session directories
- **WHEN** the converter is run on a root directory containing multiple valid session folders and a shared action mapping profile
- **THEN** it creates one LeRobotDataset output
- **AND** each valid session folder becomes one episode in that dataset

#### Scenario: Preserve session boundaries
- **WHEN** two source session folders are both converted successfully
- **THEN** their frames SHALL NOT be merged into the same logical episode

### Requirement: Converter processes only session directories with required artifacts
The converter SHALL treat a session directory as valid input only when it contains the required recording artifacts needed for conversion.

#### Scenario: Required files are present
- **WHEN** a session directory contains `capture.mp4`, `sync.csv`, and `actions.bin`
- **THEN** the converter considers the directory eligible for validation and conversion

#### Scenario: Required files are missing
- **WHEN** a session directory is missing one or more required artifacts
- **THEN** the converter marks that session invalid

### Requirement: Default batch mode skips invalid sessions
The converter SHALL skip invalid session directories by default and continue converting remaining valid sessions.

#### Scenario: Invalid session in non-strict mode
- **WHEN** a batch contains both valid and invalid session directories and `--strict` is not specified
- **THEN** the converter skips the invalid sessions
- **AND** continues converting the valid sessions

#### Scenario: All sessions invalid in non-strict mode
- **WHEN** no session in the batch is valid and `--strict` is not specified
- **THEN** the converter fails the run without writing a usable dataset

### Requirement: Strict mode fails on invalid sessions
The converter SHALL support a strict mode that aborts conversion if any session directory is invalid.

#### Scenario: Invalid session in strict mode
- **WHEN** a batch contains at least one invalid session and `--strict` is specified
- **THEN** the converter fails the run
- **AND** SHALL NOT silently skip the invalid session

### Requirement: Frames align to the latest prior gamepad snapshot
The converter SHALL derive the action state for each retained video frame from the most recent gamepad snapshot whose timestamp is less than or equal to the frame timestamp.

#### Scenario: Snapshot exists before frame timestamp
- **WHEN** a video frame timestamp has one or more preceding gamepad snapshots
- **THEN** the converter uses the latest such snapshot to derive the frame action

#### Scenario: No newer event between adjacent frames
- **WHEN** two adjacent video frames occur without an intervening gamepad event
- **THEN** the derived action state for the later frame remains equal to the latest known snapshot

### Requirement: Leading pre-action video is bounded by configuration
The converter SHALL support a conversion argument that limits how much video before the first gamepad snapshot can be retained.

#### Scenario: Leading frame inside configured pre-action duration
- **WHEN** a video frame occurs before the first gamepad snapshot but within the configured maximum pre-action duration
- **THEN** the frame is retained
- **AND** its action is derived from the pre-first-action baseline state

#### Scenario: Leading frame exceeds configured pre-action duration
- **WHEN** a video frame occurs earlier than the configured maximum pre-action duration before the first gamepad snapshot
- **THEN** the frame is excluded from the converted episode

### Requirement: Dataset task text is provided as one constant conversion parameter
The converter SHALL accept one task string for the entire conversion and SHALL write that task for every converted frame.

#### Scenario: Constant task applied to all episodes
- **WHEN** the converter is run with a task string
- **THEN** every frame written to the dataset carries that same task value

### Requirement: Converted dataset preserves source frame cadence
The converter SHALL preserve the original recorded video cadence and SHALL NOT resample frames in v1.

#### Scenario: Source session converted without resampling
- **WHEN** a valid source session is converted
- **THEN** the output episode frame sequence reflects the retained source video frames in recorded order
- **AND** the converter does not synthesize intermediate frames
