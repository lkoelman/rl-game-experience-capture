# Plan: Enhance `map_actions` Workflow UI and Resume Support

## Summary

Update `map_actions` to use a two-column, real-time mapping workflow instead of the current blocking capture plus separate review flow. The mapper will preload an optional existing profile, start on the first unmapped action, show live gamepad input, use keyboard-driven navigation (`Space`, `Left`, `Right`, `Enter`), then transition to a review/save screen before writing the profile.

## Key Changes

### CLI and persisted profile behavior

- Extend `trajectory::map_cli::Options` and `TryParseArguments()` to support `--resume-from <existing-mapping.yaml>` while keeping the current positional output path unchanged.
- Keep `map_actions <game-actions.yaml> [action-mapping.yaml] [--profile-name <name>]` valid; new usage becomes `map_actions <game-actions.yaml> [action-mapping.yaml] [--profile-name <name>] [--resume-from <existing.yaml>]`.
- In `main_map_actions.cpp`, if `--resume-from` is provided:
  - load the existing profile with `LoadActionMappingProfile()`
  - validate that `game_id` matches the loaded game definition
  - after class selection, preload only entries for the selected `class_id`
  - preserve existing bindings/skipped state for actions in that class
  - ignore actions from other classes when constructing the in-memory workflow state
- Save behavior:
  - `Enter` exits the capture workflow into review/save mode, not directly to disk
  - review shows warnings/conflicts/unmapped required actions
  - save still writes if there are only warnings
  - blocking validation errors still prevent write

### Workflow/domain interfaces

- Expand `MappingWorkflowState` so it can be seeded from an existing profile state rather than always starting empty.
- Add state operations needed by the new UI:
  - jump by index, not only by action id
  - move to previous/next action without forcing `finished`
  - find first unmapped/unskipped action
  - replace current bindings from the currently observed candidate
  - expose per-action status for menu rendering: `mapped`, `skipped`, `unmapped`, `current`
- Keep the saved schema unchanged unless implementation discovery forces a compatibility fix.
- Keep `ActionMappingProfile.actions` fully materialized on save for the selected class, including already mapped entries loaded from `--resume-from`.

### Live binding capture

- Refactor `GamepadBindingCapture` away from the current blocking `WaitForBinding(...)` model.
- New capture surface should support:
  - non-blocking polling during the FTXUI event loop
  - current observed candidate binding for the selected action kind
  - stable user-facing label for the currently pressed control
  - reset/clear of transient observation after a confirm or action change
- Detection rules:
  - digital actions observe the currently pressed gamepad button in real time
  - analog actions observe active stick axes and store explicit axis bindings
  - trigger actions observe trigger motion and keep threshold-based bindings
- Preserve existing binding naming conventions (`south`, `leftx`, `right_trigger`, etc.) so YAML compatibility remains stable.

### FTXUI layout and interaction

- Replace the current single-pane capture screen in `src/ActionMappingWorkflow.cpp` with a two-column component layout.
- Left column:
  - current action details
  - currently observed live binding, updating while input changes
  - current saved bindings for that action
  - keyboard hints
  - mapped/skipped/remaining counts
- Right column:
  - FTXUI `Menu` showing all actions for the selected class
  - current action highlighted in the list
  - each row annotated with status (`mapped`, `skipped`, `unmapped`)
- Keyboard behavior in mapping mode:
  - `Space`: confirm the currently observed binding for the current action
  - `Right`: advance to the next action; if no binding exists, mark current action skipped and advance
  - `Left`: move to the previous action without mutating other actions
  - `Enter`: stop mapping and open review/save
  - `Esc` or `q`: cancel without saving
- Initial cursor position when resuming: first unmapped and unskipped action; if all actions are already resolved, open directly into review/save.
- Keep class selection as a separate initial screen.

### Review/save flow and validation

- Keep a distinct review screen after mapping stops.
- Review must list all actions and current status, highlight required-but-unmapped actions, and surface duplicate/conflicting bindings before save.
- Review should allow selecting an action to return to mapping/edit mode, then return to review again.
- On final save:
  - update `updated_at`
  - preserve `created_at` from resumed profiles when present; otherwise set both timestamps to “now”
  - compute `complete` from unresolved required-action warnings
- Keep current validation semantics unless implementation reveals the need to adjust a specific issue class.

### Tests and docs

- Follow TDD for each behavior slice:
  1. add or adjust a failing test
  2. run the focused Meson test
  3. implement the minimal change
  4. rerun focused tests, then the broader suite
- Add or extend tests for:
  - `MapCliTests.cpp`: `--resume-from` parsing, missing value failure, coexistence with positional output path
  - `ActionMappingTests.cpp`: workflow preload from existing mappings, first-unmapped cursor selection, left/right navigation semantics, right-arrow skip behavior, review/edit round-trip semantics
  - deterministic capture helper coverage if capture normalization/extraction is split from SDL polling
  - `ActionMappingYamlTests.cpp`: resumed profile timestamp preservation and selected-class preload assumptions if those rules live in YAML/domain helpers
- Update `README.md` usage/examples for `--resume-from` and the revised key bindings.
- Update `docs/ARCHITECTURE.md` to reflect the non-blocking capture model and the new review-driven stop flow.

## Acceptance Test Scenarios

- Fresh mapping session:
  - user selects a class
  - sees live pressed button text update
  - presses `Space` to confirm
  - uses `Right` to advance and `Left` to go back
  - presses `Enter`, reviews, saves successfully
- Skip path:
  - action has no observed binding
  - pressing `Right` marks it skipped and advances
- Resume path:
  - existing profile is loaded with `--resume-from`
  - mapped/skipped actions appear in the menu immediately
  - cursor starts on the first unresolved action
  - saving preserves prior mappings and writes newly added ones
- Conflict path:
  - conflicting bindings are shown in review
  - save is blocked only for validation errors, not warnings
- Cancellation path:
  - `Esc` or `q` exits without writing output

## Assumptions

- `--resume-from` is the chosen CLI shape.
- `Enter` means “stop mapping and go to review/save”, not immediate write.
- Resume behavior is class-scoped: preload only mappings for the selected class and start at the first unresolved action.
- Output YAML schema stays backward-compatible; this change is UX and workflow focused, not a file-format redesign.
