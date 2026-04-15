#include "ActionMapping.hpp"

#include <algorithm>
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

// Returns the trigger axis names accepted for threshold-based bindings.
const std::unordered_set<std::string>& ValidTriggers() {
    static const std::unordered_set<std::string> controls{"left_trigger", "right_trigger"};
    return controls;
}

// Validates the serialized axis direction token used in YAML profiles.
bool IsValidDirection(const std::string& direction) {
    return direction == "negative" || direction == "positive" || direction == "any";
}

// Canonicalizes a binding into the key used for duplicate/conflict detection.
std::string BindingConflictKey(const ActionBinding& binding) {
    switch (binding.type) {
    case BindingType::button:
        return "button:" + binding.control;
    case BindingType::axis:
        return "axis:" + binding.control + ":" + binding.direction;
    case BindingType::trigger:
        return "trigger:" + binding.control;
    }
    return {};
}

// Appends a validation issue and marks the aggregate result as not fully clean.
void AddIssue(ValidationResult& result, ValidationSeverity severity, std::string action_id, std::string message) {
    result.ok = false;
    result.issues.push_back(ValidationIssue{severity, std::move(action_id), std::move(message)});
}

}  // namespace

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

ActionBinding ActionBinding::Trigger(std::string control_name, float activation_threshold) {
    ActionBinding binding;
    binding.type = BindingType::trigger;
    binding.control = std::move(control_name);
    binding.threshold = activation_threshold;
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

ValidationResult ValidateProfile(const GameDefinition& game, const ActionMappingProfile& profile) {
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
            switch (binding.type) {
            case BindingType::button:
                if (!ValidButtons().contains(binding.control)) {
                    AddIssue(result, ValidationSeverity::error, action.action_id, "unknown button control: " + binding.control);
                }
                break;
            case BindingType::axis:
                if (!ValidAxes().contains(binding.control)) {
                    AddIssue(result, ValidationSeverity::error, action.action_id, "unknown analog axis control: " + binding.control);
                }
                if (!IsValidDirection(binding.direction)) {
                    AddIssue(result, ValidationSeverity::error, action.action_id, "invalid axis direction: " + binding.direction);
                }
                break;
            case BindingType::trigger:
                if (!ValidTriggers().contains(binding.control)) {
                    AddIssue(result, ValidationSeverity::error, action.action_id, "unknown trigger control: " + binding.control);
                }
                if (binding.threshold <= 0.0f || binding.threshold > 1.0f) {
                    AddIssue(result, ValidationSeverity::error, action.action_id, "trigger threshold must be within (0, 1]");
                }
                break;
            }

            const std::string key = BindingConflictKey(binding);
            const auto used_it = used_bindings.find(key);
            if (used_it != used_bindings.end() && used_it->second != action.action_id) {
                AddIssue(result,
                         ValidationSeverity::error,
                         action.action_id,
                         "binding conflict for " + binding.control + " between actions \"" + used_it->second + "\" and \"" + action.action_id + "\"");
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
    case BindingType::trigger:
        description << "trigger:" << binding.control << " threshold=" << binding.threshold;
        break;
    }
    return description.str();
}

MappingWorkflowState::MappingWorkflowState(std::vector<ActionDefinition> actions) : actions_(std::move(actions)) {
    profile_actions_.reserve(actions_.size());
    for (const auto& action : actions_) {
        profile_actions_.push_back(ProfileActionMapping{action.id, false, {}});
    }
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

bool MappingWorkflowState::SetCurrentActionById(const std::string& action_id) {
    for (std::size_t index = 0; index < actions_.size(); ++index) {
        if (actions_[index].id == action_id) {
            current_index_ = index;
            return true;
        }
    }
    return false;
}

const std::vector<ProfileActionMapping>& MappingWorkflowState::ActionStates() const {
    return profile_actions_;
}

std::vector<ProfileActionMapping> MappingWorkflowState::BuildProfileActions() const {
    return profile_actions_;
}

}  // namespace trajectory::mapping
