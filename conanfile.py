from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps

class WinInputRecorderConan(ConanFile):
    name = "win_input_recorder"
    version = "0.1"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("protobuf/3.21.12")
        self.requires("gtest/1.14.0")

    def build_requirements(self):
        self.tool_requires("cmake/3.27.1")

    def layout(self):
        self.folders.source = "."
        self.folders.build = "build"
        self.folders.generators = "build"
