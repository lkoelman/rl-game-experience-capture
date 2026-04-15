#pragma once

#include <string>

#include "ActionMapping.hpp"

namespace trajectory::mapping {

// Loads and validates a game action catalog from `game-actions.yaml`.
GameDefinition LoadGameDefinition(const std::string& path);

// Loads a previously saved per-user mapping profile from YAML.
ActionMappingProfile LoadActionMappingProfile(const std::string& path);

// Writes a per-user mapping profile in the stable YAML shape expected by the tool.
void SaveActionMappingProfile(const ActionMappingProfile& profile, const std::string& path);

}  // namespace trajectory::mapping
