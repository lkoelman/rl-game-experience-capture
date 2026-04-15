#include <stdexcept>
#include <string>
#include <vector>

#include "CaptureSelection.hpp"
#include "TestWindowsSetup.hpp"

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestMonitorIdResolvesToZeroBasedIndex() {
    const std::vector<trajectory::MonitorOption> monitors{
        {1, 0, "Primary", 1920, 1080, 0, 0},
        {2, 1, "Secondary", 2560, 1440, 1920, 0},
    };

    const trajectory::SelectionResult result = trajectory::ResolveMonitorTarget(monitors, 2);

    Expect(result.ok, "known monitor id should resolve");
    Expect(result.target.kind == trajectory::CaptureTargetKind::monitor, "monitor resolution should produce a monitor target");
    Expect(result.target.monitor_id == 2, "monitor resolution should preserve the one-based id");
    Expect(result.target.monitor_index == 1, "monitor resolution should expose the zero-based monitor index");
}

void TestUnknownMonitorIdFailsClearly() {
    const std::vector<trajectory::MonitorOption> monitors{
        {1, 0, "Primary", 1920, 1080, 0, 0},
    };

    const trajectory::SelectionResult result = trajectory::ResolveMonitorTarget(monitors, 3);

    Expect(!result.ok, "unknown monitor id should fail");
    Expect(result.error.find("No monitor matches id 3") != std::string::npos,
           "missing monitor error should explain the unresolved id");
}

void TestWindowQueryMatchesCaseInsensitiveSubstring() {
    const std::vector<trajectory::WindowOption> windows{
        {0x1001ULL, "Visual Studio Code", "Code.exe", 1234},
        {0x1002ULL, "Notepad", "notepad.exe", 5678},
    };

    const trajectory::SelectionResult result = trajectory::ResolveWindowTarget(windows, "studio");

    Expect(result.ok, "case-insensitive substring should resolve a unique window");
    Expect(result.target.kind == trajectory::CaptureTargetKind::window, "window resolution should produce a window target");
    Expect(result.target.window_handle == 0x1001ULL, "window resolution should preserve the hwnd");
    Expect(result.target.window_title == "Visual Studio Code", "window resolution should preserve the title");
}

void TestMissingWindowQueryFailsClearly() {
    const std::vector<trajectory::WindowOption> windows{
        {0x1001ULL, "Visual Studio Code", "Code.exe", 1234},
    };

    const trajectory::SelectionResult result = trajectory::ResolveWindowTarget(windows, "notepad");

    Expect(!result.ok, "missing window should fail");
    Expect(result.error.find("No window title contains \"notepad\"") != std::string::npos,
           "missing window error should explain the unmatched query");
}

void TestAmbiguousWindowQueryFailsClearly() {
    const std::vector<trajectory::WindowOption> windows{
        {0x1001ULL, "Project - Notepad", "notepad.exe", 1234},
        {0x1002ULL, "Notes - Notepad", "notepad.exe", 5678},
    };

    const trajectory::SelectionResult result = trajectory::ResolveWindowTarget(windows, "notepad");

    Expect(!result.ok, "ambiguous window query should fail");
    Expect(result.error.find("Multiple windows match") != std::string::npos,
           "ambiguous window error should explain the conflict");
}

void TestMonitorLabelIncludesDisplayDetails() {
    const trajectory::MonitorOption monitor{2, 1, "DELL U2720Q", 2560, 1440, 1920, 0};

    const std::string label = trajectory::BuildMonitorLabel(monitor);

    Expect(label.find("[2]") != std::string::npos, "monitor label should include the one-based id");
    Expect(label.find("DELL U2720Q") != std::string::npos, "monitor label should include the display name");
    Expect(label.find("2560x1440") != std::string::npos, "monitor label should include the resolution");
}

void TestWindowLabelIncludesTitleAndProcess() {
    const trajectory::WindowOption window{0x1001ULL, "Visual Studio Code", "Code.exe", 1234};

    const std::string label = trajectory::BuildWindowLabel(window);

    Expect(label.find("Visual Studio Code") != std::string::npos, "window label should include the title");
    Expect(label.find("Code.exe") != std::string::npos, "window label should include the process name");
    Expect(label.find("1234") != std::string::npos, "window label should include the process id");
}

void TestPageBuildsNumericShortcutSlice() {
    std::vector<std::string> labels;
    for (int index = 1; index <= 12; ++index) {
        labels.push_back("Window " + std::to_string(index));
    }

    const trajectory::SelectionPage first_page = trajectory::BuildSelectionPage(labels, 0, 9);
    const trajectory::SelectionPage second_page = trajectory::BuildSelectionPage(labels, 1, 9);

    Expect(first_page.entries.size() == 9, "first page should expose up to nine numeric shortcuts");
    Expect(first_page.entries.front().shortcut_digit == 1, "shortcut digits should start at 1");
    Expect(first_page.entries.back().shortcut_digit == 9, "shortcut digits should end at 9");
    Expect(first_page.has_more, "first page should report additional entries");
    Expect(second_page.entries.size() == 3, "second page should expose remaining entries");
    Expect(!second_page.has_more, "last page should not report additional entries");
}

}  // namespace

int main() {
    trajectory::test_support::DisableWindowsErrorDialogs();
    TestMonitorIdResolvesToZeroBasedIndex();
    TestUnknownMonitorIdFailsClearly();
    TestWindowQueryMatchesCaseInsensitiveSubstring();
    TestMissingWindowQueryFailsClearly();
    TestAmbiguousWindowQueryFailsClearly();
    TestMonitorLabelIncludesDisplayDetails();
    TestWindowLabelIncludesTitleAndProcess();
    TestPageBuildsNumericShortcutSlice();
    return 0;
}
