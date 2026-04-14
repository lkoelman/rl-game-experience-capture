#include <stdexcept>
#include <string>

#include "CaptureTarget.hpp"
#include "VideoRecorderPipeline.hpp"

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestWindowsPathUsesForwardSlashes() {
    const std::string normalized = trajectory::NormalizePathForGStreamer(
        "C:\\Users\\lucas\\workspace\\trajectory-recorder-cpp\\builddir\\hello-out\\hello-session3\\capture.mp4");

    Expect(
        normalized == "C:/Users/lucas/workspace/trajectory-recorder-cpp/builddir/hello-out/hello-session3/capture.mp4",
        "windows path should be normalized to forward slashes for GStreamer");
}

void TestPipelineUsesNormalizedFilesinkPath() {
    const std::string pipeline = trajectory::BuildPipelineDescriptionForTesting(
        "C:\\tmp\\hello-out\\hello-session3\\capture.mp4", trajectory::CaptureTarget::PrimaryMonitor());

    Expect(
        pipeline.find("filesink location=\"C:/tmp/hello-out/hello-session3/capture.mp4\"") != std::string::npos,
        "pipeline should embed the normalized filesink path");
}

void TestMonitorTargetUsesMonitorIndexProperty() {
    const std::string pipeline = trajectory::BuildPipelineDescriptionForTesting(
        "C:\\tmp\\capture.mp4", trajectory::CaptureTarget::Monitor(1, 0));

    Expect(
        pipeline.find("d3d11screencapturesrc monitor-index=0") != std::string::npos,
        "monitor capture should target the resolved zero-based monitor index");
}

void TestWindowTargetUsesWindowHandleProperty() {
    const std::string pipeline = trajectory::BuildPipelineDescriptionForTesting(
        "C:\\tmp\\capture.mp4", trajectory::CaptureTarget::Window(0x1234ULL, "Notepad"));

    Expect(
        pipeline.find("d3d11screencapturesrc window-handle=4660") != std::string::npos,
        "window capture should target the resolved window handle");
}

}  // namespace

int main() {
    TestWindowsPathUsesForwardSlashes();
    TestPipelineUsesNormalizedFilesinkPath();
    TestMonitorTargetUsesMonitorIndexProperty();
    TestWindowTargetUsesWindowHandleProperty();
    return 0;
}
