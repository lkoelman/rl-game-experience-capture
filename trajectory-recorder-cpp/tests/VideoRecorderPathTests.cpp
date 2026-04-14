#include <stdexcept>
#include <string>

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
        "C:\\tmp\\hello-out\\hello-session3\\capture.mp4");

    Expect(
        pipeline.find("filesink location=\"C:/tmp/hello-out/hello-session3/capture.mp4\"") != std::string::npos,
        "pipeline should embed the normalized filesink path");
}

}  // namespace

int main() {
    TestWindowsPathUsesForwardSlashes();
    TestPipelineUsesNormalizedFilesinkPath();
    return 0;
}
