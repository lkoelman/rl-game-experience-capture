# Plan: Implement `map_actions` Gamepad Action Mapping Tool

## Summary

Implement a new standalone executable, `map_actions`, that loads a YAML game action definition, guides the user through an FTXUI-based gamepad mapping workflow, validates the result, and writes a per-user `action-mapping.yaml` profile.

The implementation should stay aligned with the existing architecture:

- use SDL for live gamepad input capture
- use FTXUI for the terminal UI
- add `yaml-cpp` for YAML parsing and writing
- keep the mapping tool separate from the existing recorder and validator executables

## Key Changes

### Build and dependency changes

- Add `yaml-cpp` to `conanfile.txt`.
- Update `meson.build` to resolve and link `yaml-cpp`.
- Add a new mapping-focused library target for config parsing, mapping-domain logic, and TUI helpers.
- Add a new executable target: `map_actions`.

### New mapping domain model

- Add types for game action definitions:
  - game metadata
  - class/spec sections
  - action definitions with stable IDs, labels, and required/optional metadata
- Add types for mapping profiles:
  - profile metadata
  - per-action bindings
  - binding variants for button, trigger-threshold, and analog axis mappings
- Add validation result types for duplicate bindings, malformed configs, and incomplete required mappings.

### YAML IO

- Implement a parser for `game-actions.yaml`.
- Implement a writer for `action-mapping.yaml`.
- Enforce deterministic YAML structure and field naming so files remain human-editable and stable across runs.
- Reject malformed configs with clear file/field-specific errors.

### Live input capture

- Add a dedicated SDL-backed input listener for the mapping tool.
- Capture current gamepad button and axis activity without writing recorder outputs.
- Interpret trigger bindings as axis plus threshold.
- Represent joystick mappings as explicit axis/direction bindings.
- Expose a deterministic surface that the TUI can poll for the currently observed candidate input.

### TUI workflow

- Implement a class/spec selection screen backed by the parsed game definition.
- Implement an action-by-action mapping screen that shows:
  - current action label and optional description
  - currently detected raw input
  - mapped/skipped/remaining counts
  - key hints for confirm and skip
- Support:
  - `space` to confirm the current detected binding
  - `n` to skip/advance
  - review before save
  - revisit/edit of existing action mappings
- Add a final review screen that highlights:
  - required-but-unmapped actions
  - duplicate/conflicting bindings
  - complete vs incomplete profile status

### CLI entrypoint

- Add `src/main_map_actions.cpp`.
- Support an input path for `game-actions.yaml` and an optional output path for `action-mapping.yaml`.
- Initialize SDL, load config, run the TUI workflow, validate the result, and write the YAML output.
- Return non-zero exit codes for parse errors, SDL initialization failures, validation failures that block save, and write failures.

## Suggested Module Boundaries

- `include/` + `src/` mapping config model and validation helpers
- `include/` + `src/` YAML parser/writer layer
- `include/` + `src/` SDL mapping input listener
- `include/` + `src/` FTXUI mapping flow/state machine
- `src/main_map_actions.cpp` CLI entrypoint

Keep deterministic parsing, validation, and state-transition logic isolated from SDL and FTXUI where possible so those parts can be unit-tested directly.

## Test Plan

Follow the repo’s TDD preference:

1. Add failing tests for deterministic config parsing, validation, and workflow helpers first.
2. Implement the minimal code to pass.
3. Run the focused tests, then the full Meson test suite.

Required new tests:

- parse valid `game-actions.yaml`
- reject malformed YAML and missing required fields
- preserve stable action IDs and selected class/spec resolution
- write `action-mapping.yaml` with required metadata fields
- detect duplicate/conflicting bindings
- detect invalid trigger thresholds
- detect missing required actions
- verify progress/review state transitions for skip/confirm/edit flows
- verify analog axis and trigger interpretation helpers

Run verification with the existing Windows build path:

- `./scripts/build.ps1`

## Assumptions

- V1 is gamepad-only.
- `yaml-cpp` is an acceptable new dependency.
- `map_actions` is a new standalone executable.
- `action-mapping.yaml` is a per-user profile.
- Multi-binding per action is supported in v1.
- Joysticks are stored as explicit axis/direction bindings.
- Triggers are stored as thresholded axis activations.
- The tool defines how raw inputs should be interpreted as actions; it does not verify in-game execution.
