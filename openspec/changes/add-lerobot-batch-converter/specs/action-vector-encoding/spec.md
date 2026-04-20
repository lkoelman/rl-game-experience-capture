## ADDED Requirements

### Requirement: Converter emits one stable dense action vector feature
The converter SHALL write one numeric `action` feature whose layout is deterministic for the selected game definition and mapping profile.

#### Scenario: Stable layout across sessions
- **WHEN** multiple sessions are converted using the same game definition and selected class list
- **THEN** every converted frame uses the same `action` vector length and slot ordering

### Requirement: Action layout follows ordered action definitions
The converter SHALL build the action vector from the ordered high-level action list produced by the selected `class_ids` in their declared order.

#### Scenario: Default actions precede class actions
- **WHEN** the selected class list contains `default` followed by a class-specific entry
- **THEN** the action vector places `default` action slots before the class-specific action slots

### Requirement: Action kinds map to fixed slot counts
The converter SHALL encode each action kind using a fixed number of float slots.

#### Scenario: Digital action uses one slot
- **WHEN** an action definition has kind `digital`
- **THEN** it contributes exactly one float slot to the `action` vector

#### Scenario: Analog or trigger action uses one slot
- **WHEN** an action definition has kind `analog` or `trigger`
- **THEN** it contributes exactly one float slot to the `action` vector

#### Scenario: Vector2 action uses two slots
- **WHEN** an action definition has kind `vector2`
- **THEN** it contributes exactly two float slots to the `action` vector

### Requirement: Digital action values are binary
The converter SHALL encode digital actions as `0.0` for inactive and `1.0` for active.

#### Scenario: Bound digital control is pressed
- **WHEN** the current aligned gamepad state activates a mapped digital action binding
- **THEN** the corresponding digital action slot is `1.0`

#### Scenario: Bound digital control is not pressed
- **WHEN** the current aligned gamepad state does not activate a mapped digital action binding
- **THEN** the corresponding digital action slot is `0.0`

### Requirement: Scalar action values preserve continuous control values
The converter SHALL encode `analog` and `trigger` actions as scalar float values derived from the aligned gamepad state and the mapped binding semantics.

#### Scenario: Trigger binding is active
- **WHEN** the aligned gamepad state exceeds the mapped trigger activation semantics
- **THEN** the corresponding scalar slot reflects the trigger-derived float value

#### Scenario: Axis binding is present
- **WHEN** an `analog` action is bound to an axis-based control
- **THEN** the corresponding scalar slot reflects the aligned axis-derived float value

### Requirement: Vector2 action values preserve two-axis control values
The converter SHALL encode each `vector2` action as two float slots representing the mapped 2D control state.

#### Scenario: Stick binding drives vector2 action
- **WHEN** a `vector2` action is bound to a stick control
- **THEN** the converter writes two float values representing the aligned control vector in the declared slot order

### Requirement: Unmapped actions remain in the layout and emit zeros
The converter SHALL keep unmapped actions in the action vector layout and SHALL emit zero values for their slots.

#### Scenario: Unmapped digital action
- **WHEN** an action definition has no usable binding in the mapping profile
- **THEN** the action remains present in the vector layout
- **AND** all slots for that action are `0.0`

#### Scenario: Incomplete profile still converts
- **WHEN** the mapping profile is incomplete but otherwise valid for conversion
- **THEN** the converter still writes the dataset
- **AND** unmapped action slots are zero-filled

### Requirement: Keyboard state is excluded from v1 action derivation
The converter SHALL ignore recorded keyboard state when deriving the v1 high-level action vector.

#### Scenario: Keyboard keys present in recorded snapshots
- **WHEN** recorded input snapshots include pressed keyboard keys
- **THEN** those keys do not affect the derived `action` vector in v1
