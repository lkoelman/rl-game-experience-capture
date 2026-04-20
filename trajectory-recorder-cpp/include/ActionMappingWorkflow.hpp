#pragma once

#include <optional>
#include <string>

#include "ActionMapping.hpp"

namespace trajectory::mapping {

class GamepadBindingCapture;

// Runs the interactive class selection and per-action mapping flow.
// Returns `std::nullopt` when the operator cancels before saving.
std::optional<ActionMappingProfile> RunMappingWorkflow(const GameDefinition& game,
                                                       GamepadBindingCapture& capture,
                                                       const std::string& profile_name,
                                                       const ActionMappingProfile* existing_profile = nullptr);

}  // namespace trajectory::mapping
