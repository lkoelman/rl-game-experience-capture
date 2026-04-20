## ADDED Requirements

### Requirement: Converted dataset stores converter metadata in info.json
The converter SHALL persist source and conversion metadata inside the generated LeRobotDataset `meta/info.json` rather than as dataset frame features.

#### Scenario: Metadata written into dataset info
- **WHEN** a dataset conversion completes successfully
- **THEN** the resulting `meta/info.json` contains a converter-defined metadata block

### Requirement: Converter metadata is namespaced
The converter SHALL store custom metadata in a dedicated namespaced object to avoid colliding with standard LeRobot metadata fields.

#### Scenario: Standard LeRobot fields remain intact
- **WHEN** custom converter metadata is written
- **THEN** standard LeRobot fields such as `fps`, `features`, and file path templates remain unchanged in purpose
- **AND** custom fields are stored under a dedicated extension namespace

### Requirement: Metadata includes source game and mapping identity
The converter metadata SHALL record the source game and mapping information used to derive the dataset.

#### Scenario: Source game and profile are recorded
- **WHEN** a dataset is converted from a game definition and action mapping profile
- **THEN** the metadata includes the source `game_id`
- **AND** the selected `class_ids`
- **AND** the mapping profile identity fields available from the profile

### Requirement: Metadata includes conversion settings
The converter metadata SHALL record the conversion settings that materially affect dataset semantics.

#### Scenario: Conversion arguments are recorded
- **WHEN** the converter is run with task text, strictness, and maximum pre-action duration settings
- **THEN** those settings are stored in the dataset metadata

### Requirement: Metadata includes action layout description
The converter metadata SHALL describe the emitted action vector layout so downstream consumers can interpret vector slots without re-deriving them from source files.

#### Scenario: Action slot mapping recorded
- **WHEN** the converter emits a dense `action` vector
- **THEN** the dataset metadata includes a description of the slot ordering and the source action definition for each segment

### Requirement: Metadata includes session-level conversion outcomes
The converter metadata SHALL include a record of which session directories were converted and which were skipped.

#### Scenario: Mixed valid and invalid sessions in non-strict mode
- **WHEN** some sessions are converted and others are skipped
- **THEN** the metadata records the retained session identifiers
- **AND** records the skipped session identifiers with their failure reasons

### Requirement: Observation-only v1 schema is reflected in dataset metadata
The output dataset metadata SHALL define a feature schema containing the video observation feature and the dense action feature, and SHALL NOT include raw keyboard or raw controller-state observation features in v1.

#### Scenario: V1 feature schema written
- **WHEN** a v1 dataset is created by the converter
- **THEN** `meta/info.json` declares the observation video feature and the dense `action` feature
- **AND** does not declare raw keyboard-state or raw gamepad-state features
