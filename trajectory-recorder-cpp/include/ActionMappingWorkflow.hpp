#pragma once

#include <optional>
#include <string>

#include "ActionMapping.hpp"

namespace trajectory::mapping {

class GamepadBindingCapture;

std::optional<ActionMappingProfile> RunMappingWorkflow(const GameDefinition& game,
                                                       GamepadBindingCapture& capture,
                                                       const std::string& profile_name);

}  // namespace trajectory::mapping
