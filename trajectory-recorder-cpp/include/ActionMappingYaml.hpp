#pragma once

#include <string>

#include "ActionMapping.hpp"

namespace trajectory::mapping {

GameDefinition LoadGameDefinition(const std::string& path);
ActionMappingProfile LoadActionMappingProfile(const std::string& path);
void SaveActionMappingProfile(const ActionMappingProfile& profile, const std::string& path);

}  // namespace trajectory::mapping
