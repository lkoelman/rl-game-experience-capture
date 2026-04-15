#include "ActionMappingYaml.hpp"

#include <fstream>
#include <stdexcept>

#include <yaml-cpp/yaml.h>

namespace trajectory::mapping {

namespace {

ActionInputKind ParseActionInputKind(const YAML::Node& node, const std::string& path) {
    const std::string value = node.as<std::string>();
    if (value == "digital") {
        return ActionInputKind::digital;
    }
    if (value == "analog") {
        return ActionInputKind::analog;
    }
    if (value == "trigger") {
        return ActionInputKind::trigger;
    }
    throw std::runtime_error(path + " must be one of: digital, analog, trigger");
}

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

ActionBinding ParseBinding(const YAML::Node& node, const std::string& path) {
    if (!node["type"] || !node["control"]) {
        throw std::runtime_error(path + " must contain type and control");
    }

    const std::string type = node["type"].as<std::string>();
    const std::string control = node["control"].as<std::string>();
    if (type == "button") {
        return ActionBinding::Button(control);
    }
    if (type == "axis") {
        return ActionBinding::Axis(control, node["direction"] ? node["direction"].as<std::string>() : "any");
    }
    if (type == "trigger") {
        if (!node["threshold"]) {
            throw std::runtime_error(path + " threshold is required for trigger bindings");
        }
        const float threshold = node["threshold"].as<float>();
        if (threshold <= 0.0f || threshold > 1.0f) {
            throw std::runtime_error(path + " threshold must be within (0, 1]");
        }
        return ActionBinding::Trigger(control, threshold);
    }

    throw std::runtime_error(path + " has unknown binding type: " + type);
}

std::string BindingTypeName(BindingType type) {
    switch (type) {
    case BindingType::button:
        return "button";
    case BindingType::axis:
        return "axis";
    case BindingType::trigger:
        return "trigger";
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

        const YAML::Node specs_node = class_node["specializations"];
        if (specs_node) {
            if (!specs_node.IsSequence()) {
                throw std::runtime_error("classes[" + std::to_string(class_index) + "].specializations must be a sequence");
            }
            for (std::size_t spec_index = 0; spec_index < specs_node.size(); ++spec_index) {
                const YAML::Node spec_node = specs_node[spec_index];
                if (!spec_node["id"] || !spec_node["label"]) {
                    throw std::runtime_error("specializations entries must contain id and label");
                }

                SpecializationDefinition spec;
                spec.id = spec_node["id"].as<std::string>();
                spec.label = spec_node["label"].as<std::string>();
                spec.actions = ParseActionList(spec_node["actions"], "specializations[" + std::to_string(spec_index) + "].actions");
                klass.specializations.push_back(std::move(spec));
            }
        }

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
    profile.class_id = root["class_id"] ? root["class_id"].as<std::string>() : "";
    profile.spec_id = root["spec_id"] ? root["spec_id"].as<std::string>() : "";
    profile.profile_name = root["profile_name"] ? root["profile_name"].as<std::string>() : "";
    profile.created_at = root["created_at"] ? root["created_at"].as<std::string>() : "";
    profile.updated_at = root["updated_at"] ? root["updated_at"].as<std::string>() : "";
    profile.complete = root["complete"] ? root["complete"].as<bool>() : false;

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
    out << YAML::Key << "class_id" << YAML::Value << profile.class_id;
    if (!profile.spec_id.empty()) {
        out << YAML::Key << "spec_id" << YAML::Value << profile.spec_id;
    }
    out << YAML::Key << "profile_name" << YAML::Value << profile.profile_name;
    if (!profile.created_at.empty()) {
        out << YAML::Key << "created_at" << YAML::Value << profile.created_at;
    }
    if (!profile.updated_at.empty()) {
        out << YAML::Key << "updated_at" << YAML::Value << profile.updated_at;
    }
    out << YAML::Key << "complete" << YAML::Value << profile.complete;
    out << YAML::Key << "actions" << YAML::Value << YAML::BeginMap;
    for (const auto& action : profile.actions) {
        out << YAML::Key << action.action_id << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "skipped" << YAML::Value << action.skipped;
        out << YAML::Key << "bindings" << YAML::Value << YAML::BeginSeq;
        for (const auto& binding : action.bindings) {
            out << YAML::BeginMap;
            out << YAML::Key << "type" << YAML::Value << BindingTypeName(binding.type);
            out << YAML::Key << "control" << YAML::Value << binding.control;
            if (binding.type == BindingType::axis) {
                out << YAML::Key << "direction" << YAML::Value << binding.direction;
            }
            if (binding.type == BindingType::trigger) {
                out << YAML::Key << "threshold" << YAML::Value << binding.threshold;
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
