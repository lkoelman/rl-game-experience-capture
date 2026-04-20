#include "ActionMappingYaml.hpp"

#include <fstream>
#include <stdexcept>

#include <yaml-cpp/yaml.h>

namespace trajectory::mapping {

namespace {

// Parses the YAML action kind token into the mapper's internal enum.
ActionInputKind ParseActionInputKind(const YAML::Node& node, const std::string& path) {
    const std::string value = node.as<std::string>();
    if (value == "digital") {
        return ActionInputKind::digital;
    }
    if (value == "analog") {
        return ActionInputKind::analog;
    }
    if (value == "vector2") {
        return ActionInputKind::vector2;
    }
    if (value == "trigger") {
        return ActionInputKind::trigger;
    }
    throw std::runtime_error(path + " must be one of: digital, analog, vector2, trigger");
}

// Parses one action definition node from the game catalog.
ActionDefinition ParseActionDefinition(const YAML::Node& node, const std::string& path) {
    if (!node["id"] || !node["label"] || !node["kind"]) {
        throw std::runtime_error(path + " must contain id, label, and kind");
    }

    ActionDefinition action;
    action.id = node["id"].as<std::string>();
    action.label = node["label"].as<std::string>();
    action.description = node["description"] ? node["description"].as<std::string>() : "";
    action.kind = ParseActionInputKind(node["kind"], path + ".kind");
    action.required = node["required"] ? node["required"].as<bool>() : false;
    return action;
}

// Parses an action list while preserving catalog order for the mapper workflow.
std::vector<ActionDefinition> ParseActionList(const YAML::Node& node, const std::string& path) {
    std::vector<ActionDefinition> actions;
    if (!node) {
        return actions;
    }
    if (!node.IsSequence()) {
        throw std::runtime_error(path + " must be a sequence");
    }
    for (std::size_t index = 0; index < node.size(); ++index) {
        actions.push_back(ParseActionDefinition(node[index], path + "[" + std::to_string(index) + "]"));
    }
    return actions;
}

ComboComponent ParseComboComponent(const YAML::Node& node, const std::string& path) {
    if (!node["type"] || !node["control"]) {
        throw std::runtime_error(path + " must contain type and control");
    }

    const std::string type = node["type"].as<std::string>();
    const std::string control = node["control"].as<std::string>();
    if (type == "button") {
        return ComboComponent::Button(control);
    }
    if (type == "axis_button") {
        return ComboComponent::AxisButton(control, node["direction"] ? node["direction"].as<std::string>() : "");
    }
    throw std::runtime_error(path + " has unknown combo component type: " + type);
}

// Parses one saved low-level binding entry from an action mapping profile.
ActionBinding ParseBinding(const YAML::Node& node, const std::string& path) {
    if (!node["type"] || !node["control"]) {
        if (!(node["type"] && node["type"].as<std::string>() == "combo")) {
            throw std::runtime_error(path + " must contain type and control");
        }
    }

    const std::string type = node["type"].as<std::string>();
    if (type == "button") {
        const std::string control = node["control"].as<std::string>();
        return ActionBinding::Button(control);
    }
    if (type == "axis") {
        const std::string control = node["control"].as<std::string>();
        return ActionBinding::Axis(control, node["direction"] ? node["direction"].as<std::string>() : "any");
    }
    if (type == "stick") {
        const std::string control = node["control"].as<std::string>();
        return ActionBinding::Stick(control);
    }
    if (type == "trigger") {
        const std::string control = node["control"].as<std::string>();
        if (!node["threshold"]) {
            throw std::runtime_error(path + " threshold is required for trigger bindings");
        }
        const float threshold = node["threshold"].as<float>();
        if (threshold <= 0.0f || threshold > 1.0f) {
            throw std::runtime_error(path + " threshold must be within (0, 1]");
        }
        return ActionBinding::Trigger(control, threshold);
    }
    if (type == "combo") {
        const YAML::Node controls = node["controls"];
        if (!controls || !controls.IsSequence()) {
            throw std::runtime_error(path + " controls must be a sequence");
        }
        std::vector<ComboComponent> components;
        components.reserve(controls.size());
        for (std::size_t index = 0; index < controls.size(); ++index) {
            components.push_back(ParseComboComponent(controls[index], path + ".controls[" + std::to_string(index) + "]"));
        }
        return ActionBinding::Combo(std::move(components));
    }

    throw std::runtime_error(path + " has unknown binding type: " + type);
}

// Returns the stable YAML token for a serialized binding type.
std::string BindingTypeName(BindingType type) {
    switch (type) {
    case BindingType::button:
        return "button";
    case BindingType::axis:
        return "axis";
    case BindingType::stick:
        return "stick";
    case BindingType::trigger:
        return "trigger";
    case BindingType::combo:
        return "combo";
    }
    return "button";
}

std::string ComboComponentTypeName(ComboComponentType type) {
    switch (type) {
    case ComboComponentType::button:
        return "button";
    case ComboComponentType::axis_button:
        return "axis_button";
    }
    return "button";
}

}  // namespace

GameDefinition LoadGameDefinition(const std::string& path) {
    const YAML::Node root = YAML::LoadFile(path);
    if (!root["game_id"]) {
        throw std::runtime_error("game definition is missing game_id");
    }

    GameDefinition game;
    game.game_id = root["game_id"].as<std::string>();
    game.display_name = root["display_name"] ? root["display_name"].as<std::string>() : "";

    const YAML::Node classes_node = root["classes"];
    if (!classes_node || !classes_node.IsSequence()) {
        throw std::runtime_error("game definition must contain a classes sequence");
    }

    for (std::size_t class_index = 0; class_index < classes_node.size(); ++class_index) {
        const YAML::Node class_node = classes_node[class_index];
        if (!class_node["id"] || !class_node["label"]) {
            throw std::runtime_error("classes[" + std::to_string(class_index) + "] must contain id and label");
        }

        ClassDefinition klass;
        klass.id = class_node["id"].as<std::string>();
        klass.label = class_node["label"].as<std::string>();
        klass.actions = ParseActionList(class_node["actions"], "classes[" + std::to_string(class_index) + "].actions");

        game.classes.push_back(std::move(klass));
    }

    const auto validation = ValidateGameDefinition(game);
    if (HasBlockingIssues(validation)) {
        throw std::runtime_error(validation.issues.front().message);
    }
    return game;
}

ActionMappingProfile LoadActionMappingProfile(const std::string& path) {
    const YAML::Node root = YAML::LoadFile(path);

    ActionMappingProfile profile;
    profile.schema_version = root["schema_version"] ? root["schema_version"].as<int>() : 1;
    profile.game_id = root["game_id"] ? root["game_id"].as<std::string>() : "";
    const YAML::Node class_ids_node = root["class_ids"];
    if (class_ids_node) {
        if (!class_ids_node.IsSequence()) {
            throw std::runtime_error("class_ids must be a sequence");
        }
        for (std::size_t index = 0; index < class_ids_node.size(); ++index) {
            profile.class_ids.push_back(class_ids_node[index].as<std::string>());
        }
    }
    if (!class_ids_node) {
        throw std::runtime_error("action mapping profile is missing class_ids");
    }
    profile.profile_name = root["profile_name"] ? root["profile_name"].as<std::string>() : "";
    profile.created_at = root["created_at"] ? root["created_at"].as<std::string>() : "";
    profile.updated_at = root["updated_at"] ? root["updated_at"].as<std::string>() : "";
    profile.complete = root["complete"] ? root["complete"].as<bool>() : false;
    const YAML::Node thresholds_node = root["axis_button_thresholds"];
    if (thresholds_node) {
        if (!thresholds_node.IsMap()) {
            throw std::runtime_error("axis_button_thresholds must be a map keyed by axis control");
        }
        for (const auto& item : thresholds_node) {
            profile.axis_button_thresholds.push_back(AxisButtonThreshold{
                item.first.as<std::string>(),
                item.second.as<float>(),
            });
        }
    }
    profile.axis_button_thresholds = NormalizeAxisButtonThresholds(profile.axis_button_thresholds);

    const YAML::Node actions_node = root["actions"];
    if (actions_node) {
        if (!actions_node.IsMap()) {
            throw std::runtime_error("actions must be a map keyed by action id");
        }
        for (const auto& item : actions_node) {
            ProfileActionMapping action;
            action.action_id = item.first.as<std::string>();
            action.skipped = item.second["skipped"] ? item.second["skipped"].as<bool>() : false;
            const YAML::Node bindings = item.second["bindings"];
            if (bindings) {
                if (!bindings.IsSequence()) {
                    throw std::runtime_error("bindings for action " + action.action_id + " must be a sequence");
                }
                for (std::size_t index = 0; index < bindings.size(); ++index) {
                    action.bindings.push_back(ParseBinding(bindings[index], "actions." + action.action_id + ".bindings[" + std::to_string(index) + "]"));
                }
            }
            profile.actions.push_back(std::move(action));
        }
    }

    return profile;
}

void SaveActionMappingProfile(const ActionMappingProfile& profile, const std::string& path) {
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "schema_version" << YAML::Value << profile.schema_version;
    out << YAML::Key << "game_id" << YAML::Value << profile.game_id;
    out << YAML::Key << "class_ids" << YAML::Value << YAML::BeginSeq;
    for (const auto& class_id : profile.class_ids) {
        out << class_id;
    }
    out << YAML::EndSeq;
    out << YAML::Key << "profile_name" << YAML::Value << profile.profile_name;
    if (!profile.created_at.empty()) {
        out << YAML::Key << "created_at" << YAML::Value << profile.created_at;
    }
    if (!profile.updated_at.empty()) {
        out << YAML::Key << "updated_at" << YAML::Value << profile.updated_at;
    }
    out << YAML::Key << "complete" << YAML::Value << profile.complete;
    out << YAML::Key << "axis_button_thresholds" << YAML::Value << YAML::BeginMap;
    for (const auto& threshold : NormalizeAxisButtonThresholds(profile.axis_button_thresholds)) {
        out << YAML::Key << threshold.control << YAML::Value << threshold.threshold;
    }
    out << YAML::EndMap;
    out << YAML::Key << "actions" << YAML::Value << YAML::BeginMap;
    for (const auto& action : profile.actions) {
        out << YAML::Key << action.action_id << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "skipped" << YAML::Value << action.skipped;
        out << YAML::Key << "bindings" << YAML::Value << YAML::BeginSeq;
        for (const auto& binding : action.bindings) {
            out << YAML::BeginMap;
            out << YAML::Key << "type" << YAML::Value << BindingTypeName(binding.type);
            if (binding.type == BindingType::button || binding.type == BindingType::axis ||
                binding.type == BindingType::stick || binding.type == BindingType::trigger) {
                out << YAML::Key << "control" << YAML::Value << binding.control;
            }
            if (binding.type == BindingType::axis) {
                out << YAML::Key << "direction" << YAML::Value << binding.direction;
            }
            if (binding.type == BindingType::trigger) {
                out << YAML::Key << "threshold" << YAML::Value << binding.threshold;
            }
            if (binding.type == BindingType::combo) {
                out << YAML::Key << "controls" << YAML::Value << YAML::BeginSeq;
                for (const auto& component : binding.combo_components) {
                    out << YAML::BeginMap;
                    out << YAML::Key << "type" << YAML::Value << ComboComponentTypeName(component.type);
                    out << YAML::Key << "control" << YAML::Value << component.control;
                    if (component.type == ComboComponentType::axis_button && !component.direction.empty()) {
                        out << YAML::Key << "direction" << YAML::Value << component.direction;
                    }
                    out << YAML::EndMap;
                }
                out << YAML::EndSeq;
            }
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;
        out << YAML::EndMap;
    }
    out << YAML::EndMap;
    out << YAML::EndMap;

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open mapping profile for write: " + path);
    }
    file << out.c_str();
}

}  // namespace trajectory::mapping
