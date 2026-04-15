#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace trajectory::mapping {

enum class ActionInputKind {
    digital,
    analog,
    trigger,
};

struct ActionDefinition {
    std::string id;
    std::string label;
    std::string description;
    ActionInputKind kind{ActionInputKind::digital};
    bool required{false};
};

struct SpecializationDefinition {
    std::string id;
    std::string label;
    std::vector<ActionDefinition> actions;
};

struct ClassDefinition {
    std::string id;
    std::string label;
    std::vector<ActionDefinition> actions;
    std::vector<SpecializationDefinition> specializations;
};

struct GameDefinition {
    std::string game_id;
    std::string display_name;
    std::vector<ClassDefinition> classes;
};

enum class BindingType {
    button,
    axis,
    trigger,
};

struct ActionBinding {
    BindingType type{BindingType::button};
    std::string control;
    std::string direction;
    float threshold{0.5f};

    static ActionBinding Button(std::string control_name);
    static ActionBinding Axis(std::string control_name, std::string direction_name);
    static ActionBinding Trigger(std::string control_name, float activation_threshold);
};

struct ProfileActionMapping {
    std::string action_id;
    bool skipped{false};
    std::vector<ActionBinding> bindings;
};

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

enum class ValidationSeverity {
    warning,
    error,
};

struct ValidationIssue {
    ValidationSeverity severity{ValidationSeverity::error};
    std::string action_id;
    std::string message;
};

struct ValidationResult {
    bool ok{true};
    std::vector<ValidationIssue> issues;
};

const ClassDefinition* FindClassDefinition(const GameDefinition& game, const std::string& class_id);
const SpecializationDefinition* FindSpecializationDefinition(const ClassDefinition& klass, const std::string& spec_id);
std::vector<ActionDefinition> CollectActions(const GameDefinition& game, const std::string& class_id, const std::string& spec_id);
ValidationResult ValidateGameDefinition(const GameDefinition& game);
ValidationResult ValidateProfile(const GameDefinition& game, const ActionMappingProfile& profile);
bool HasBlockingIssues(const ValidationResult& validation);
std::string DescribeBinding(const ActionBinding& binding);

class MappingWorkflowState {
public:
    explicit MappingWorkflowState(std::vector<ActionDefinition> actions);

    bool IsFinished() const;
    std::size_t CurrentIndex() const;
    const ActionDefinition& CurrentAction() const;
    std::size_t TotalActions() const;
    std::size_t MappedCount() const;
    std::size_t SkippedCount() const;
    std::size_t RemainingCount() const;

    void AddBindingToCurrentAction(const ActionBinding& binding);
    void ClearCurrentActionBindings();
    void SkipCurrentAction();
    void AdvanceAction();
    bool SetCurrentActionById(const std::string& action_id);

    const std::vector<ProfileActionMapping>& ActionStates() const;
    std::vector<ProfileActionMapping> BuildProfileActions() const;

private:
    std::vector<ActionDefinition> actions_;
    std::vector<ProfileActionMapping> profile_actions_;
    std::size_t current_index_{0};
};

}  // namespace trajectory::mapping
