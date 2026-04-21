#include <stdexcept>
#include <string>

#include "TestWindowsSetup.hpp"
#include "VirtualGamepadBridge.hpp"

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestNeutralStateBuildsEmptyReport() {
    trajectory::virtual_gamepad::PhysicalGamepadState state;

    const XUSB_REPORT report = trajectory::virtual_gamepad::BuildXusbReport(state);

    Expect(report.wButtons == 0, "neutral state should not press any buttons");
    Expect(report.bLeftTrigger == 0, "neutral state should not press left trigger");
    Expect(report.bRightTrigger == 0, "neutral state should not press right trigger");
    Expect(report.sThumbLX == 0, "neutral state should center left X");
    Expect(report.sThumbLY == 0, "neutral state should center left Y");
    Expect(report.sThumbRX == 0, "neutral state should center right X");
    Expect(report.sThumbRY == 0, "neutral state should center right Y");
}

void TestFaceButtonsMapToXusbButtons() {
    trajectory::virtual_gamepad::PhysicalGamepadState state;
    trajectory::virtual_gamepad::ApplyButtonChange(state, SDL_GAMEPAD_BUTTON_SOUTH, true);
    trajectory::virtual_gamepad::ApplyButtonChange(state, SDL_GAMEPAD_BUTTON_EAST, true);
    trajectory::virtual_gamepad::ApplyButtonChange(state, SDL_GAMEPAD_BUTTON_WEST, true);
    trajectory::virtual_gamepad::ApplyButtonChange(state, SDL_GAMEPAD_BUTTON_NORTH, true);

    const XUSB_REPORT report = trajectory::virtual_gamepad::BuildXusbReport(state);

    Expect((report.wButtons & XUSB_GAMEPAD_A) != 0, "south button should map to A");
    Expect((report.wButtons & XUSB_GAMEPAD_B) != 0, "east button should map to B");
    Expect((report.wButtons & XUSB_GAMEPAD_X) != 0, "west button should map to X");
    Expect((report.wButtons & XUSB_GAMEPAD_Y) != 0, "north button should map to Y");
}

void TestAxisValuesMapToThumbsticksAndTriggers() {
    trajectory::virtual_gamepad::PhysicalGamepadState state;
    trajectory::virtual_gamepad::ApplyAxisMotion(state, SDL_GAMEPAD_AXIS_LEFTX, 1234);
    trajectory::virtual_gamepad::ApplyAxisMotion(state, SDL_GAMEPAD_AXIS_LEFTY, -2345);
    trajectory::virtual_gamepad::ApplyAxisMotion(state, SDL_GAMEPAD_AXIS_RIGHTX, 3456);
    trajectory::virtual_gamepad::ApplyAxisMotion(state, SDL_GAMEPAD_AXIS_RIGHTY, -4567);
    trajectory::virtual_gamepad::ApplyAxisMotion(state, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767);
    trajectory::virtual_gamepad::ApplyAxisMotion(state, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 16384);

    const XUSB_REPORT report = trajectory::virtual_gamepad::BuildXusbReport(state);

    Expect(report.sThumbLX == 1234, "left stick X should preserve SDL value");
    Expect(report.sThumbLY == -2345, "left stick Y should preserve SDL value");
    Expect(report.sThumbRX == 3456, "right stick X should preserve SDL value");
    Expect(report.sThumbRY == -4567, "right stick Y should preserve SDL value");
    Expect(report.bLeftTrigger == 255, "fully pressed left trigger should saturate to 255");
    Expect(report.bRightTrigger == 128, "half-pressed right trigger should scale to 128");
}

void TestDpadAndMetaButtonsMapToExpectedFlags() {
    trajectory::virtual_gamepad::PhysicalGamepadState state;
    trajectory::virtual_gamepad::ApplyButtonChange(state, SDL_GAMEPAD_BUTTON_DPAD_UP, true);
    trajectory::virtual_gamepad::ApplyButtonChange(state, SDL_GAMEPAD_BUTTON_DPAD_LEFT, true);
    trajectory::virtual_gamepad::ApplyButtonChange(state, SDL_GAMEPAD_BUTTON_START, true);
    trajectory::virtual_gamepad::ApplyButtonChange(state, SDL_GAMEPAD_BUTTON_BACK, true);
    trajectory::virtual_gamepad::ApplyButtonChange(state, SDL_GAMEPAD_BUTTON_GUIDE, true);

    const XUSB_REPORT report = trajectory::virtual_gamepad::BuildXusbReport(state);

    Expect((report.wButtons & XUSB_GAMEPAD_DPAD_UP) != 0, "dpad up should map to XUSB dpad up");
    Expect((report.wButtons & XUSB_GAMEPAD_DPAD_LEFT) != 0, "dpad left should map to XUSB dpad left");
    Expect((report.wButtons & XUSB_GAMEPAD_START) != 0, "start should map to start");
    Expect((report.wButtons & XUSB_GAMEPAD_BACK) != 0, "back should map to back");
    Expect((report.wButtons & XUSB_GAMEPAD_GUIDE) != 0, "guide should map to guide");
}

void TestFormatForwardedButtonLogLineUsesPhysicalAndVirtualNames() {
    const std::string line = trajectory::virtual_gamepad::FormatForwardedButtonLogLine(17, SDL_GAMEPAD_BUTTON_SOUTH, "2");

    Expect(line == "17.south -> 2.A", "forwarded button log should show physical and virtual control names");
}

void TestRateLimiterEmitsImmediatelyAndThenWaitsForInterval() {
    trajectory::virtual_gamepad::ForwardingLogRateLimiter limiter(30);

    Expect(limiter.ShouldEmit(0), "first log line should be emitted immediately");
    Expect(!limiter.ShouldEmit(33'000'000ULL), "rate limiter should suppress lines before the interval elapses");
    Expect(limiter.ShouldEmit(34'000'000ULL), "rate limiter should emit after the interval elapses");
}

void TestRateLimiterUsesConfiguredRate() {
    trajectory::virtual_gamepad::ForwardingLogRateLimiter limiter(5);

    Expect(limiter.ShouldEmit(10), "custom limiter should still emit immediately");
    Expect(!limiter.ShouldEmit(100'000'000ULL), "custom limiter should enforce its own interval");
    Expect(limiter.ShouldEmit(210'000'000ULL), "custom limiter should allow slower configured rates");
}

}  // namespace

int main() {
    trajectory::test_support::DisableWindowsErrorDialogs();
    TestNeutralStateBuildsEmptyReport();
    TestFaceButtonsMapToXusbButtons();
    TestAxisValuesMapToThumbsticksAndTriggers();
    TestDpadAndMetaButtonsMapToExpectedFlags();
    TestFormatForwardedButtonLogLineUsesPhysicalAndVirtualNames();
    TestRateLimiterEmitsImmediatelyAndThenWaitsForInterval();
    TestRateLimiterUsesConfiguredRate();
    return 0;
}
