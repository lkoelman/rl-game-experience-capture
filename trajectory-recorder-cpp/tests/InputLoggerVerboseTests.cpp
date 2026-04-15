#include <stdexcept>
#include <string>

#include "InputLogger.hpp"
#include "TestWindowsSetup.hpp"

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestFormatVerboseStateIncludesAllFields() {
    trajectory::GamepadState state;
    state.set_monotonic_ns(123456789ULL);
    state.add_axes(-1.0f);
    state.add_axes(0.5f);
    state.add_pressed_buttons(1);
    state.add_pressed_buttons(7);
    state.add_pressed_keys(4);
    state.add_pressed_keys(42);

    const std::string formatted = trajectory::FormatVerboseState(state);

    Expect(formatted == "input monotonic_ns=123456789 axes=[-1,0.5] buttons=[1,7] keys=[4,42]",
           "verbose formatter should emit a stable single-line snapshot");
}

void TestFormatVerboseStateHandlesEmptyCollections() {
    trajectory::GamepadState state;
    state.set_monotonic_ns(77ULL);

    const std::string formatted = trajectory::FormatVerboseState(state);

    Expect(formatted.find("monotonic_ns=77") != std::string::npos, "timestamp should be included");
    Expect(formatted.find("axes=[]") != std::string::npos, "empty axes should be represented explicitly");
    Expect(formatted.find("buttons=[]") != std::string::npos, "empty buttons should be represented explicitly");
    Expect(formatted.find("keys=[]") != std::string::npos, "empty keys should be represented explicitly");
}

}  // namespace

int main() {
    trajectory::test_support::DisableWindowsErrorDialogs();
    TestFormatVerboseStateIncludesAllFields();
    TestFormatVerboseStateHandlesEmptyCollections();
    return 0;
}
