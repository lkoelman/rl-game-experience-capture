#include "ActionMapping.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace trajectory::mapping {

namespace {

// Returns the button names that may appear in mapping profiles.
const std::unordered_set<std::string>& ValidButtons() {
    static const std::unordered_set<std::string> controls{
        "south",         "east",          "west",           "north",        "back",
        "guide",         "start",         "left_stick",     "right_stick",  "left_shoulder",
        "right_shoulder","dpad_up",       "dpad_down",      "dpad_left",    "dpad_right",
        "misc1",         "right_paddle1", "left_paddle1",   "right_paddle2","left_paddle2",
        "touchpad",
    };
    return controls;
}

// Returns the analog axis names accepted for joystick-backed mappings.
const std::unordered_set<std::string>& ValidAxes() {
    static const std::unordered_set<std::string> controls{"leftx", "lefty", "rightx", "righty"};
    return controls;
}

const std::unordered_set<std::string>& ValidSticks() {
    static const std::unordered_set<std::string> controls{"left_stick", "right_stick"};
    return controls;
}

// Returns the trigger axis names accepted for threshold-based bindings.
const std::unordered_set<std::string>& ValidTriggers() {
    static const std::unordered_set<std::string> controls{"left_trigger", "right_trigger"};
    return controls;
}

const std::vector<std::string>& EligibleAxisButtons() {
    static const std::vector<std::string> controls{
        "left_trigger", "leftx", "lefty", "right_trigger", "rightx", "righty",
    };
    return controls;
}

// Validates the serialized axis direction token used in YAML profiles.
bool IsValidDirection(const std::string& direction) {
    return direction == "negative" || direction == "positive" || direction == "any";
}

bool IsDirectionalAxisButton(const std::string& control) {
    return ValidAxes().contains(control);
}

bool IsTriggerAxisButton(const std::string& control) {
    return ValidTriggers().contains(control);
}

std::string ComboComponentConflictKey(const ComboComponent& component) {
    switch (component.type) {
    case ComboComponentType::button:
        return "button:" + component.control;
    case ComboComponentType::axis_button:
        return "axis_button:" + component.control + ":" + component.direction;
    }
    return {};
}

// Canonicalizes a binding into the key used for duplicate/conflict detection.
std::string BindingConflictKey(const ActionBinding& binding) {
    switch (binding.type) {
    case BindingType::button:
        return "button:" + binding.control;
    case BindingType::axis:
        return "axis:" + binding.control + ":" + binding.direction;
    case BindingType::stick:
        return "stick:" + binding.control;
    case BindingType::trigger:
        return "trigger:" + binding.control;
    case BindingType::combo: {
        std::vector<std::string> component_keys;
        component_keys.reserve(binding.combo_components.size());
        for (const auto& component : binding.combo_components) {
            component_keys.push_back(ComboComponentConflictKey(component));
        }
        std::sort(component_keys.begin(), component_keys.end());

        std::ostringstream key;
        key << "combo:";
        for (std::size_t index = 0; index < component_keys.size(); ++index) {
            if (index > 0) {
                key << "+";
            }
            key << component_keys[index];
        }
        return key.str();
    }
    }
    return {};
}

// Appends a validation issue and marks the aggregate result as not fully clean.
void AddIssue(ValidationResult& result, ValidationSeverity severity, std::string action_id, std::string message) {
    result.ok = false;
    result.issues.push_back(ValidationIssue{severity, std::move(action_id), std::move(message)});
}

}  // namespace

ComboComponent ComboComponent::Button(std::string control_name) {
    ComboComponent component;
    component.type = ComboComponentType::button;
    component.control = std::move(control_name);
    return component;
}

ComboComponent ComboComponent::AxisButton(std::string control_name, std::string direction_name) {
    ComboComponent component;
    component.type = ComboComponentType::axis_button;
    component.control = std::move(control_name);
    component.direction = std::move(direction_name);
    return component;
}

ActionBinding ActionBinding::Button(std::string control_name) {
    ActionBinding binding;
    binding.type = BindingType::button;
    binding.control = std::move(control_name);
    return binding;
}

ActionBinding ActionBinding::Axis(std::string control_name, std::string direction_name) {
    ActionBinding binding;
    binding.type = BindingType::axis;
    binding.control = std::move(control_name);
    binding.direction = std::move(direction_name);
    return binding;
}

ActionBinding ActionBinding::Stick(std::string control_name) {
    ActionBinding binding;
    binding.type = BindingType::stick;
    binding.control = std::move(control_name);
    return binding;
}

ActionBinding ActionBinding::Trigger(std::string control_name, float activation_threshold) {
    ActionBinding binding;
    binding.type = BindingType::trigger;
    binding.control = std::move(control_name);
    binding.threshold = activation_threshold;
    return binding;
}

ActionBinding ActionBinding::Combo(std::vector<ComboComponent> components) {
    ActionBinding binding;
    binding.type = BindingType::combo;
    binding.combo_components = std::move(components);
    return binding;
}

const ClassDefinition* FindClassDefinition(const GameDefinition& game, const std::string& class_id) {
    for (const auto& klass : game.classes) {
        if (klass.id == class_id) {
            return &klass;
        }
    }
    return nullptr;
}

std::vector<ActionDefinition> CollectActions(const GameDefinition& game, const std::string& class_id) {
    const ClassDefinition* klass = FindClassDefinition(game, class_id);
    if (klass == nullptr) {
        throw std::runtime_error("unknown class id: " + class_id);
    }

    return klass->actions;
}

std::vector<AxisButtonThreshold> BuildDefaultAxisButtonThresholds() {
    std::vector<AxisButtonThreshold> thresholds;
    thresholds.reserve(EligibleAxisButtons().size());
    for (const auto& control : EligibleAxisButtons()) {
        thresholds.push_back(AxisButtonThreshold{control, kDefaultAxisButtonThreshold});
    }
    return thresholds;
}

std::vector<AxisButtonThreshold> NormalizeAxisButtonThresholds(const std::vector<AxisButtonThreshold>& thresholds) {
    std::unordered_map<std::string, float> threshold_map;
    for (const auto& threshold : thresholds) {
        threshold_map[threshold.control] = threshold.threshold;
    }

    std::vector<AxisButtonThreshold> normalized;
    normalized.reserve(EligibleAxisButtons().size());
    for (const auto& control : EligibleAxisButtons()) {
        const auto it = threshold_map.find(control);
        normalized.push_back(AxisButtonThreshold{
            control,
            it == threshold_map.end() ? kDefaultAxisButtonThreshold : it->second,
        });
    }
    return normalized;
}

float ResolveAxisButtonThreshold(const ActionMappingProfile& profile, const std::string& control) {
    const auto normalized = NormalizeAxisButtonThresholds(profile.axis_button_thresholds);
    const auto it = std::find_if(normalized.begin(), normalized.end(), [&](const AxisButtonThreshold& threshold) {
        return threshold.control == control;
    });
    return it == normalized.end() ? kDefaultAxisButtonThreshold : it->threshold;
}

ValidationResult ValidateGameDefinition(const GameDefinition& game) {
    ValidationResult result;

    if (game.game_id.empty()) {
        AddIssue(result, ValidationSeverity::error, "", "game_id must not be empty");
    }

    std::unordered_set<std::string> class_ids;
    std::unordered_set<std::string> action_ids;
    for (const auto& klass : game.classes) {
        if (klass.id.empty()) {
            AddIssue(result, ValidationSeverity::error, "", "class id must not be empty");
        } else if (!class_ids.insert(klass.id).second) {
            AddIssue(result, ValidationSeverity::error, klass.id, "duplicate class id: " + klass.id);
        }

        for (const auto& action : klass.actions) {
            if (!action_ids.insert(action.id).second) {
                AddIssue(result, ValidationSeverity::error, action.id, "duplicate action id: " + action.id);
            }
        }

    }

    return result;
}

ValidationResult ValidateProfile(const GameDefinition& game, const ActionMappingProfile& profile, int max_combo_buttons) {
    ValidationResult result = ValidateGameDefinition(game);

    if (profile.game_id != game.game_id) {
        AddIssue(result, ValidationSeverity::error, "", "profile game_id does not match the loaded game definition");
        return result;
    }

    const ClassDefinition* klass = FindClassDefinition(game, profile.class_id);
    if (klass == nullptr) {
        AddIssue(result, ValidationSeverity::error, "", "unknown class id in profile: " + profile.class_id);
        return result;
    }

    const auto applicable_actions = CollectActions(game, profile.class_id);
    std::unordered_map<std::string, ActionDefinition> action_lookup;
    for (const auto& action : applicable_actions) {
        action_lookup.emplace(action.id, action);
    }

    std::unordered_map<std::string, std::string> used_bindings;
    std::unordered_set<std::string> seen_actions;
    std::unordered_set<std::string> seen_threshold_controls;
    for (const auto& threshold : profile.axis_button_thresholds) {
        if (!ValidAxes().contains(threshold.control) && !ValidTriggers().contains(threshold.control)) {
            AddIssue(result, ValidationSeverity::error, "", "unknown axis button threshold control: " + threshold.control);
        }
        if (!seen_threshold_controls.insert(threshold.control).second) {
            AddIssue(result, ValidationSeverity::error, "", "duplicate axis button threshold control: " + threshold.control);
        }
        if (threshold.threshold <= 0.0f || threshold.threshold > 1.0f) {
            AddIssue(result, ValidationSeverity::error, "", "axis button threshold must be within (0, 1] for control: " + threshold.control);
        }
    }

    for (const auto& action : profile.actions) {
        if (!seen_actions.insert(action.action_id).second) {
            AddIssue(result, ValidationSeverity::error, action.action_id, "duplicate action mapping entry: " + action.action_id);
            continue;
        }

        const auto it = action_lookup.find(action.action_id);
        if (it == action_lookup.end()) {
            AddIssue(result, ValidationSeverity::error, action.action_id, "profile references unknown action id: " + action.action_id);
            continue;
        }

        for (const auto& binding : action.bindings) {
            const ActionInputKind expected_kind = it->second.kind;
            switch (binding.type) {
            case BindingType::button:
                if (!ValidButtons().contains(binding.control)) {
                    AddIssue(result, ValidationSeverity::error, action.action_id, "unknown button control: " + binding.control);
                }
                if (expected_kind != ActionInputKind::digital) {
                    AddIssue(result, ValidationSeverity::error, action.action_id, "action kind " + DescribeBinding(binding) + " is not valid for non-digital action");
                }
                break;
            case BindingType::axis:
                if (!ValidAxes().contains(binding.control)) {
                    AddIssue(result, ValidationSeverity::error, action.action_id, "unknown analog axis control: " + binding.control);
                }
                if (!IsValidDirection(binding.direction)) {
                    AddIssue(result, ValidationSeverity::error, action.action_id, "invalid axis direction: " + binding.direction);
                }
                if (expected_kind != ActionInputKind::analog) {
                    AddIssue(result, ValidationSeverity::error, action.action_id, "vector2 actions require stick bindings, not scalar axes");
                }
                break;
            case BindingType::stick:
                if (!ValidSticks().contains(binding.control)) {
                    AddIssue(result, ValidationSeverity::error, action.action_id, "unknown stick control: " + binding.control);
                }
                if (expected_kind != ActionInputKind::vector2) {
                    AddIssue(result, ValidationSeverity::error, action.action_id, "analog actions cannot use stick bindings");
                }
                break;
            case BindingType::trigger:
                if (!ValidTriggers().contains(binding.control)) {
                    AddIssue(result, ValidationSeverity::error, action.action_id, "unknown trigger control: " + binding.control);
                }
                if (binding.threshold <= 0.0f || binding.threshold > 1.0f) {
                    AddIssue(result, ValidationSeverity::error, action.action_id, "trigger threshold must be within (0, 1]");
                }
                if (expected_kind != ActionInputKind::trigger) {
                    AddIssue(result, ValidationSeverity::error, action.action_id, "trigger bindings are only valid for trigger actions");
                }
                break;
            case BindingType::combo: {
                if (expected_kind != ActionInputKind::digital) {
                    AddIssue(result, ValidationSeverity::error, action.action_id, "combo bindings are only valid for digital actions");
                }
                if (binding.combo_components.empty()) {
                    AddIssue(result, ValidationSeverity::error, action.action_id, "combo binding must contain at least one component");
                    break;
                }
                if (static_cast<int>(binding.combo_components.size()) > max_combo_buttons) {
                    AddIssue(result,
                             ValidationSeverity::error,
                             action.action_id,
                             "combo exceeds maximum simultaneous controls (" + std::to_string(max_combo_buttons) + ")");
                }

                std::unordered_set<std::string> seen_components;
                for (const auto& component : binding.combo_components) {
                    const std::string component_key = ComboComponentConflictKey(component);
                    if (!seen_components.insert(component_key).second) {
                        AddIssue(result, ValidationSeverity::error, action.action_id, "duplicate combo member: " + component.control);
                    }

                    switch (component.type) {
                    case ComboComponentType::button:
                        if (!ValidButtons().contains(component.control)) {
                            AddIssue(result, ValidationSeverity::error, action.action_id, "unknown combo button control: " + component.control);
                        }
                        if (!component.direction.empty()) {
                            AddIssue(result, ValidationSeverity::error, action.action_id, "button combo member must not declare a direction");
                        }
                        break;
                    case ComboComponentType::axis_button:
                        if (!IsDirectionalAxisButton(component.control) && !IsTriggerAxisButton(component.control)) {
                            AddIssue(result, ValidationSeverity::error, action.action_id, "unknown axis button control: " + component.control);
                            break;
                        }
                        if (IsDirectionalAxisButton(component.control)) {
                            if (component.direction != "negative" && component.direction != "positive") {
                                AddIssue(result, ValidationSeverity::error, action.action_id, "axis button combo member requires direction: " + component.control);
                            }
                        } else if (!component.direction.empty()) {
                            AddIssue(result, ValidationSeverity::error, action.action_id, "trigger axis button combo member must not declare a direction");
                        }
                        break;
                    }
                }
                break;
            }
            }

            const std::string key = BindingConflictKey(binding);
            const auto used_it = used_bindings.find(key);
            if (used_it != used_bindings.end() && used_it->second != action.action_id) {
                AddIssue(result,
                         ValidationSeverity::error,
                         action.action_id,
                         "binding conflict for " + DescribeBinding(binding) + " between actions \"" + used_it->second + "\" and \"" + action.action_id + "\"");
            } else {
                used_bindings.emplace(key, action.action_id);
            }
        }
    }

    for (const auto& action : applicable_actions) {
        const auto it = std::find_if(profile.actions.begin(), profile.actions.end(), [&](const ProfileActionMapping& entry) {
            return entry.action_id == action.id;
        });
        if (it == profile.actions.end() || it->skipped || it->bindings.empty()) {
            if (action.required) {
                AddIssue(result, ValidationSeverity::warning, action.id, "required action is not mapped: " + action.id);
            }
        }
    }

    return result;
}

bool HasBlockingIssues(const ValidationResult& validation) {
    return std::any_of(validation.issues.begin(), validation.issues.end(), [](const ValidationIssue& issue) {
        return issue.severity == ValidationSeverity::error;
    });
}

std::string DescribeBinding(const ActionBinding& binding) {
    std::ostringstream description;
    switch (binding.type) {
    case BindingType::button:
        description << "button:" << binding.control;
        break;
    case BindingType::axis:
        description << "axis:" << binding.control << ":" << binding.direction;
        break;
    case BindingType::stick:
        description << "stick:" << binding.control;
        break;
    case BindingType::trigger:
        description << "trigger:" << binding.control << " threshold=" << binding.threshold;
        break;
    case BindingType::combo:
        description << "combo:";
        for (std::size_t index = 0; index < binding.combo_components.size(); ++index) {
            const auto& component = binding.combo_components[index];
            if (index > 0) {
                description << "+";
            }
            if (component.type == ComboComponentType::button) {
                description << "button:" << component.control;
            } else {
                description << "axis_button:" << component.control;
                if (!component.direction.empty()) {
                    description << ":" << component.direction;
                }
            }
        }
        break;
    }
    return description.str();
}

MappingWorkflowState::MappingWorkflowState(std::vector<ActionDefinition> actions,
                                           std::vector<ProfileActionMapping> existing_actions)
    : actions_(std::move(actions)) {
    profile_actions_.reserve(actions_.size());
    for (const auto& action : actions_) {
        ProfileActionMapping profile_action{action.id, false, {}};
        const auto existing_it = std::find_if(existing_actions.begin(), existing_actions.end(), [&](const ProfileActionMapping& existing) {
            return existing.action_id == action.id;
        });
        if (existing_it != existing_actions.end()) {
            profile_action = *existing_it;
        }
        profile_actions_.push_back(std::move(profile_action));
    }

    const auto unresolved_it = std::find_if(profile_actions_.begin(), profile_actions_.end(), [](const ProfileActionMapping& action) {
        return !action.skipped && action.bindings.empty();
    });
    current_index_ = unresolved_it == profile_actions_.end()
                         ? profile_actions_.size()
                         : static_cast<std::size_t>(std::distance(profile_actions_.begin(), unresolved_it));
}

bool MappingWorkflowState::IsFinished() const {
    return current_index_ >= actions_.size();
}

std::size_t MappingWorkflowState::CurrentIndex() const {
    return current_index_;
}

const ActionDefinition& MappingWorkflowState::CurrentAction() const {
    if (IsFinished()) {
        throw std::runtime_error("workflow has no current action");
    }
    return actions_[current_index_];
}

const std::vector<ActionDefinition>& MappingWorkflowState::Actions() const {
    return actions_;
}

std::size_t MappingWorkflowState::TotalActions() const {
    return actions_.size();
}

std::size_t MappingWorkflowState::MappedCount() const {
    return static_cast<std::size_t>(std::count_if(profile_actions_.begin(), profile_actions_.end(), [](const ProfileActionMapping& action) {
        return !action.bindings.empty();
    }));
}

std::size_t MappingWorkflowState::SkippedCount() const {
    return static_cast<std::size_t>(std::count_if(profile_actions_.begin(), profile_actions_.end(), [](const ProfileActionMapping& action) {
        return action.skipped;
    }));
}

std::size_t MappingWorkflowState::RemainingCount() const {
    return TotalActions() - MappedCount() - SkippedCount();
}

void MappingWorkflowState::AddBindingToCurrentAction(const ActionBinding& binding) {
    auto& action = profile_actions_.at(current_index_);
    action.skipped = false;
    action.bindings.push_back(binding);
}

void MappingWorkflowState::ReplaceCurrentActionBindings(std::vector<ActionBinding> bindings) {
    auto& action = profile_actions_.at(current_index_);
    action.skipped = false;
    action.bindings = std::move(bindings);
}

void MappingWorkflowState::ClearCurrentActionBindings() {
    auto& action = profile_actions_.at(current_index_);
    action.skipped = false;
    action.bindings.clear();
}

void MappingWorkflowState::SkipCurrentAction() {
    auto& action = profile_actions_.at(current_index_);
    action.skipped = true;
    action.bindings.clear();
}

void MappingWorkflowState::AdvanceAction() {
    if (!IsFinished()) {
        ++current_index_;
    }
}

void MappingWorkflowState::AdvanceOrSkipCurrentAction() {
    if (IsFinished()) {
        return;
    }

    auto& action = profile_actions_.at(current_index_);
    if (action.bindings.empty()) {
        action.skipped = true;
    }
    AdvanceAction();
}

void MappingWorkflowState::MoveToPreviousAction() {
    if (current_index_ > 0) {
        --current_index_;
    } else if (IsFinished() && !actions_.empty()) {
        current_index_ = actions_.size() - 1;
    }
}

bool MappingWorkflowState::SetCurrentActionById(const std::string& action_id) {
    for (std::size_t index = 0; index < actions_.size(); ++index) {
        if (actions_[index].id == action_id) {
            current_index_ = index;
            return true;
        }
    }
    return false;
}

MappingActionStatus MappingWorkflowState::StatusForAction(std::size_t index) const {
    const auto& action = profile_actions_.at(index);
    if (action.skipped) {
        return MappingActionStatus::skipped;
    }
    if (!action.bindings.empty()) {
        return MappingActionStatus::mapped;
    }
    return MappingActionStatus::unmapped;
}

const std::vector<ProfileActionMapping>& MappingWorkflowState::ActionStates() const {
    return profile_actions_;
}

std::vector<ProfileActionMapping> MappingWorkflowState::BuildProfileActions() const {
    return profile_actions_;
}

}  // namespace trajectory::mapping
