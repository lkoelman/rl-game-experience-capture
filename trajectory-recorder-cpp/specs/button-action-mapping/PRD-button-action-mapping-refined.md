# PRD: Gamepad Action Mapping Tool

## Summary

Add a gamepad action mapping tool that converts low-level recorded controller inputs into a user-defined mapping of high-level in-game actions. The output of this tool will support downstream dataset conversion for supervised learning and reinforcement learning by expressing actions in a game-specific vocabulary rather than raw button and axis values.

The tool is gamepad-first in v1. It will use a YAML game definition file plus an interactive terminal UI to produce a per-user `action-mapping.yaml` profile.

## Problem

The current recorder captures low-level input state such as gamepad buttons, gamepad axes, and keyboard scancodes. That data is useful for raw replay and alignment, but it is not the right abstraction for training pipelines when:

- multiple controller layouts may represent the same in-game action
- different character classes or specializations expose different abilities
- downstream models should learn game actions like `jump`, `interact`, or `cast_fireball`, not raw device events

We need a tool that lets a user define how their actual controller setup maps to the in-game action space used by a specific game/class/spec profile.

## Goals

- Define a stable, human-editable game action vocabulary in YAML.
- Guide a user through mapping gamepad inputs to those actions in an interactive TUI.
- Support multiple bindings per action.
- Support joystick mappings as analog inputs.
- Support trigger mappings with configurable activation thresholds.
- Save a per-user action mapping profile in YAML for later dataset conversion.
- Make incomplete or conflicting mappings visible before the profile is saved.

## Non-Goals

- Mouse mapping in v1.
- Keyboard mapping in v1.
- Verifying that the game itself executed the intended action.
- Automatic inference of bindings from recorded sessions.
- Replacing the raw `actions.bin` recorder format.

## Users

- Developers building trajectory datasets from recorded gameplay.
- Players or operators who know the current in-game class/spec and controller layout.

## User Experience

The tool should be exposed as a standalone command-line program with an interactive terminal UI.

High-level flow:

1. Load a YAML game definition file.
2. Prompt the user to select the relevant class and/or specialization.
3. Walk through each applicable in-game action.
4. For each action, listen for gamepad input and let the user confirm or skip.
5. Present a review screen with mapped, skipped, and unresolved actions.
6. Save a per-user `action-mapping.yaml` profile.

Required interaction behavior:

- `space` confirms the currently detected binding for the current action
- `n` skips the current action and advances
- the UI shows progress: mapped / skipped / remaining
- the UI shows the raw input currently being detected
- the UI allows the user to review and edit before saving
- the UI warns when required actions remain unmapped

## Functional Requirements

### 1. Game Definition File

The tool must consume a YAML file that defines the game action vocabulary.

The file must support:

- a stable game identifier
- action sections grouped by class, specialization, or equivalent gameplay grouping
- stable action IDs
- user-facing labels for each action
- optional descriptive text or prompts
- whether an action is required or optional
- optional action metadata such as press/hold/toggle style

The action ID, not the display label, is the canonical identifier used by downstream tooling.

### 2. Mapping Profile Output

The tool must write a per-user `action-mapping.yaml` file.

The mapping profile must include:

- schema/tool version
- game identifier
- selected class/spec
- profile name or identifier
- creation/update timestamp
- per-action binding entries keyed by stable action ID

### 3. Binding Support

V1 must support:

- gamepad button bindings
- trigger bindings with configurable threshold
- joystick axis bindings using explicit axis/direction representation
- multiple bindings per action

V1 must not require:

- mouse bindings
- keyboard bindings
- combo/chord/sequence bindings

### 4. Validation

The tool must validate:

- unknown action IDs or section references
- malformed config structure
- invalid trigger thresholds
- duplicate or conflicting bindings
- missing required actions at save time

The tool should allow saving an incomplete mapping profile, but it must clearly mark the profile as incomplete and warn the user before save.

### 5. Review and Edit

Before writing the output file, the tool must provide a review step that:

- lists all actions and their current mapping status
- allows revisiting specific actions
- highlights required-but-unmapped actions
- highlights duplicate or conflicting bindings

## Data and Semantics Constraints

- V1 is gamepad-only because the current recorder already captures gamepad state and the product should stay aligned with that supported input path.
- Joysticks are analog and must not be reduced to button-like directions in the base schema.
- Triggers are axis inputs, but in the mapping profile they must support a threshold that distinguishes active from inactive.
- The current recorder is event-driven and does not emit explicit high-level action events. This tool defines an interpretation layer; it does not prove that an in-game action fired.
- Action IDs must remain stable across config revisions so historical datasets remain interpretable.
- The mapping output represents one user’s actual control profile, not a global default for all users.

## Open Issues Deferred From V1

These are intentionally out of scope for the first version but should remain visible in the PRD:

- keyboard and mouse support
- action combos or modifier chords
- richer analog semantics such as named vectors like `move_vector` or `aim_vector`
- inheritance or override layers between global defaults and per-user profiles
- automatic verification against game telemetry or on-screen state

## Success Criteria

- A user can load a game definition file, choose a class/spec, map their controller inputs, and save `action-mapping.yaml` without manually editing YAML.
- The saved mapping file is stable, human-readable, and keyed by canonical action IDs.
- The tool clearly surfaces skipped, incomplete, and conflicting mappings before save.
- The resulting profile contains enough structured information for downstream code to translate raw recorded inputs into the game’s action vocabulary.
