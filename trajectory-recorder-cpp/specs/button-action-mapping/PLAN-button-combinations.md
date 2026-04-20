# Plan: Add Button Combination Support to `map_actions`

## Summary

Extend the action-mapping domain, YAML profile format, CLI, and FTXUI workflow so one saved action binding can represent a simultaneous combo instead of only a single control. Combinations will support up to a configurable maximum number of pressed controls, may include axis-as-button members, and will use top-level per-axis thresholds stored in the mapping profile. The mapper will also add a startup menu that lets the operator choose between normal action mapping and axis-threshold configuration.

## Key Changes

### Domain model and validation

- Keep `actions[].bindings` as “alternative bindings for one action”; add a new binding type for one simultaneous combo instead of reinterpreting the existing list.
- Extend `ActionBinding` so one binding can represent:
  - a single button
  - a single analog axis mapping
  - a single trigger mapping
  - one combo containing multiple pressed components
- Represent combo members as stable low-level controls:
  - button members use existing button names
  - trigger members use axis name plus the profile’s top-level threshold for that axis
  - non-trigger axis members require both axis name and direction (`positive` / `negative`)
- Add top-level profile storage for per-axis thresholds, recommended shape:
  - `axis_button_thresholds: { left_trigger: 0.5, right_trigger: 0.5, leftx: 0.5, ... }`
- Update validation so it checks:
  - combo size does not exceed the configured maximum
  - combo members are unique within one combo
  - eligible axis names and required directions are valid
  - threshold values are within `(0, 1]`
  - conflict detection canonicalizes combos order-independently, so the same combo in different member order is treated as the same binding
- Preserve existing required-action and unknown-action validation behavior.

### YAML IO and profile format

- Update `LoadActionMappingProfile()` / `SaveActionMappingProfile()` to support:
  - the new combo binding type
  - top-level `axis_button_thresholds`
- Keep the existing action-keyed YAML structure and add the minimum new shape needed for combos.
- Recommended persisted combo shape:
  - one binding entry with `type: combo`
  - `controls:` sequence of component entries
  - component entry shape depends on member type:
    - button member: `type: button`, `control: south`
    - trigger member: `type: axis_button`, `control: left_trigger`
    - directional stick member: `type: axis_button`, `control: leftx`, `direction: positive`
- When loading older profiles with no top-level thresholds, synthesize defaults at runtime instead of failing.
- Keep save output deterministic:
  - emit combo members in a stable canonical order
  - emit `axis_button_thresholds` in a stable axis-name order

### CLI and entry flow

- Extend `map_cli::Options` with a maximum combo size option, recommended flag:
  - `--max-combo-buttons <n>`
- Default the maximum simultaneous pressed controls to `2`.
- Validate the CLI value as a positive integer and reject invalid values early.
- Keep current positional arguments and `--resume-from` behavior unchanged.
- Change the first interactive screen in `RunMappingWorkflow()` from immediate class selection to a small menu with:
  - `Start action mapping`
  - `Configure axis thresholds`
  - `Cancel`
- If the operator chooses threshold configuration:
  - open a focused threshold editor first
  - return to the startup menu after save/exit so the user can then start mapping
- Resume behavior:
  - if `--resume-from` is provided, load both existing action bindings and existing top-level axis thresholds before the startup menu
  - preserve existing `created_at` when rewriting a resumed profile

### Capture and workflow behavior

- Extend `GamepadBindingCapture` to observe simultaneous pressed controls rather than only one current candidate.
- Combo capture behavior:
  - collect the currently active set of pressed button-like controls up to the configured max
  - when the active set changes, replace the remembered combo candidate with the new set
  - keep the current “remember last observed input until replaced or consumed” behavior
  - if the active set exceeds the max, surface a UI warning and do not accept the candidate until it is reduced
- Button-like controls for combos:
  - physical gamepad buttons
  - any axis crossing its configured threshold
  - non-trigger axes require sign-aware direction capture
- Mapping-screen behavior:
  - for digital actions, `Space` confirms the last remembered combo candidate, whether it is one button or multiple simultaneous controls
  - analog and trigger actions keep their current single-binding behavior unless implementation discovers a concrete reason to split their capture path more explicitly
  - the dialog should show the full remembered combo in human-readable form, not only one control
- Add a threshold-configuration UI:
  - use an FTXUI `Menu` listing all eligible axes
  - left/right or another explicit on-screen keybinding adjusts the selected axis threshold in small increments
  - show current value, default value, and save/cancel instructions
  - save writes thresholds back into the in-memory profile before the main review/save path

### Public interfaces and affected modules

- `include/ActionMapping.hpp`
  - add combo-capable binding representation
  - add profile-level axis-threshold storage
  - keep workflow-state APIs unless a small helper is needed for threshold editing
- `include/MapCli.hpp`
  - add parsed max-combo-size option and usage text
- `include/ActionMappingYaml.hpp` / `src/ActionMappingYaml.cpp`
  - no new top-level functions required; extend existing load/save semantics
- `include/GamepadBindingCapture.hpp`
  - extend observed binding/candidate representation so it can return a combo candidate and consume profile thresholds during polling
- `src/ActionMappingWorkflow.cpp`
  - add startup mode menu
  - add axis-threshold editor screen
  - update mapping dialog messaging and confirm logic for combos
- `src/main_map_actions.cpp`
  - pass max-combo-size and loaded threshold config into the workflow/capture path

## Test Plan

Follow the repo’s TDD preference during implementation, focusing on deterministic tests first.

Required test additions:

- `tests/ActionMappingTests.cpp`
  - validate conflict detection for the same combo across two actions
  - validate combo canonicalization ignores member order
  - validate invalid combo size and duplicate combo members fail
  - validate directional axis-button combo members are accepted/rejected correctly
- `tests/ActionMappingYamlTests.cpp`
  - round-trip a profile containing:
    - top-level `axis_button_thresholds`
    - a combo binding with button + trigger member
    - a combo binding with directional stick member
  - verify missing thresholds fall back to defaults on load
  - verify invalid threshold values fail clearly
- `tests/MapCliTests.cpp`
  - parse `--max-combo-buttons`
  - reject blank / non-numeric / zero / negative values
  - ensure coexistence with `--resume-from` and positional output path
- If capture normalization is factored into deterministic helpers, add focused tests there instead of trying to unit-test SDL event polling directly.

Acceptance scenarios to cover manually or in higher-level checks:

- map an action with `left_shoulder + south`
- map an action with `left_trigger + south`
- map an action with `leftx positive + south`
- configure thresholds first, then start mapping in the same run
- resume an existing profile and preserve both thresholds and prior action bindings

## Assumptions

- `actions[].bindings` remains a list of alternative bindings; one combo is represented by one new combo binding entry.
- Eligible axis-as-button combo members include any axis, not only triggers.
- Non-trigger axis combo members require explicit direction; triggers remain directionless.
- The recommended CLI flag is `--max-combo-buttons`.
- Default axis-button threshold is `0.5` for any eligible axis not explicitly configured in the profile.
- The threshold editor writes profile-level configuration only; it does not alter per-action mappings directly.
