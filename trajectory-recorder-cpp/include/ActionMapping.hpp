#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace trajectory::mapping {

// Describes how a high-level action is expected to be bound in a user profile.
enum class ActionInputKind {
    digital,
    analog,
    trigger,
};

// Defines one canonical in-game action from the game action catalog.
// Fields:
// - `id`: stable machine identifier used in saved profiles and downstream pipelines.
// - `label`: user-facing name shown in the mapper UI.
// - `description`: optional operator guidance for selecting the right control.
// - `kind`: expected binding shape for this action.
// - `required`: whether an incomplete profile should warn if this action is unmapped.
struct ActionDefinition {
    std::string id;
    std::string label;
    std::string description;
    ActionInputKind kind{ActionInputKind::digital};
    bool required{false};
};

// Groups actions that only apply to a specific class specialization.
struct SpecializationDefinition {
    std::string id;
    std::string label;
    std::vector<ActionDefinition> actions;
};

// Groups base actions plus specialization-specific action sets for one class.
struct ClassDefinition {
    std::string id;
    std::string label;
    std::vector<ActionDefinition> actions;
    std::vector<SpecializationDefinition> specializations;
};

// Root catalog loaded from `game-actions.yaml`.
struct GameDefinition {
    std::string game_id;
    std::string display_name;
    std::vector<ClassDefinition> classes;
};

// Identifies the serialized binding representation used in a mapping profile.
enum class BindingType {
    button,
    axis,
    trigger,
};

// Stores one low-level controller binding as persisted in `action-mapping.yaml`.
// Fields:
// - `type`: binding representation to interpret.
// - `control`: stable button/axis/trigger name.
// - `direction`: axis direction qualifier for analog bindings.
// - `threshold`: activation threshold for trigger bindings.
struct ActionBinding {
    BindingType type{BindingType::button};
    std::string control;
    std::string direction;
    float threshold{0.5f};

    // Creates a button-backed binding using a stable gamepad button name.
    static ActionBinding Button(std::string control_name);

    // Creates an analog axis binding using a stable axis name and direction qualifier.
    static ActionBinding Axis(std::string control_name, std::string direction_name);

    // Creates a trigger binding using a stable trigger name and activation threshold.
    static ActionBinding Trigger(std::string control_name, float activation_threshold);
};

// Captures the mapping state for a single action inside a user profile.
struct ProfileActionMapping {
    std::string action_id;
    bool skipped{false};
    std::vector<ActionBinding> bindings;
};

// Persists one operator's action mapping profile for a selected class/spec.
// Fields:
// - metadata fields identify the game, class/spec, schema version, and profile label.
// - timestamps track profile creation/update time for offline tooling.
// - `complete` records whether required actions were fully mapped at save time.
// - `actions` stores the per-action low-level bindings.
struct ActionMappingProfile {
    int schema_version{1};
    std::string game_id;
    std::string class_id;
    std::string spec_id;
    std::string profile_name;
    std::string created_at;
    std::string updated_at;
    bool complete{false};
    std::vector<ProfileActionMapping> actions;
};

// Distinguishes warnings that still allow save from blocking validation errors.
enum class ValidationSeverity {
    warning,
    error,
};

// Describes one validation problem discovered while checking a catalog or profile.
struct ValidationIssue {
    ValidationSeverity severity{ValidationSeverity::error};
    std::string action_id;
    std::string message;
};

// Aggregates validation outcome and the issues that produced it.
struct ValidationResult {
    bool ok{true};
    std::vector<ValidationIssue> issues;
};

// Finds the selected class definition in the loaded game catalog.
const ClassDefinition* FindClassDefinition(const GameDefinition& game, const std::string& class_id);

// Finds the selected specialization definition inside an already-resolved class.
const SpecializationDefinition* FindSpecializationDefinition(const ClassDefinition& klass, const std::string& spec_id);

// Returns the ordered action list for a class plus its selected specialization.
std::vector<ActionDefinition> CollectActions(const GameDefinition& game, const std::string& class_id, const std::string& spec_id);

// Validates catalog structure such as duplicate ids and missing required metadata.
ValidationResult ValidateGameDefinition(const GameDefinition& game);

// Validates a user profile against the loaded game catalog and binding rules.
ValidationResult ValidateProfile(const GameDefinition& game, const ActionMappingProfile& profile);

// Reports whether the validation result contains save-blocking errors.
bool HasBlockingIssues(const ValidationResult& validation);

// Formats a binding into a stable, human-readable description for the mapper UI.
std::string DescribeBinding(const ActionBinding& binding);

// Tracks in-progress action mapping state across the interactive workflow.
// Fields:
// - `actions_`: ordered applicable action definitions for the selected class/spec.
// - `profile_actions_`: editable mapping state that will be written into the profile.
// - `current_index_`: current action position within the workflow.
class MappingWorkflowState {
public:
    // Seeds workflow state from the resolved action list for one class/spec selection.
    explicit MappingWorkflowState(std::vector<ActionDefinition> actions);

    // Returns true when the cursor has moved past the final action.
    bool IsFinished() const;

    // Returns the current action index within the workflow.
    std::size_t CurrentIndex() const;

    // Returns the action currently being mapped.
    const ActionDefinition& CurrentAction() const;

    // Returns the total number of actions in this workflow.
    std::size_t TotalActions() const;

    // Returns the number of actions that currently have one or more bindings.
    std::size_t MappedCount() const;

    // Returns the number of actions explicitly skipped by the operator.
    std::size_t SkippedCount() const;

    // Returns the number of actions that are neither mapped nor skipped.
    std::size_t RemainingCount() const;

    // Adds another binding to the current action and clears any skipped state.
    void AddBindingToCurrentAction(const ActionBinding& binding);

    // Clears the current action's bindings so the user can remap it from scratch.
    void ClearCurrentActionBindings();

    // Marks the current action as skipped and removes any captured bindings.
    void SkipCurrentAction();

    // Advances the workflow cursor to the next action if one exists.
    void AdvanceAction();

    // Jumps the workflow cursor to the named action, used by the review/edit step.
    bool SetCurrentActionById(const std::string& action_id);

    // Returns the editable per-action workflow state.
    const std::vector<ProfileActionMapping>& ActionStates() const;

    // Builds the profile action payload from the current workflow state.
    std::vector<ProfileActionMapping> BuildProfileActions() const;

private:
    std::vector<ActionDefinition> actions_;
    std::vector<ProfileActionMapping> profile_actions_;
    std::size_t current_index_{0};
};

}  // namespace trajectory::mapping
