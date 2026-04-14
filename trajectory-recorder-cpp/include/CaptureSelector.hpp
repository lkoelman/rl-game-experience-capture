#pragma once

#include <vector>

#include "CaptureSelection.hpp"

namespace trajectory {

std::vector<MonitorOption> EnumerateMonitors();
std::vector<WindowOption> EnumerateWindows();
SelectionResult PromptForCaptureTarget(const std::vector<MonitorOption>& monitors,
                                       const std::vector<WindowOption>& windows);

}  // namespace trajectory
