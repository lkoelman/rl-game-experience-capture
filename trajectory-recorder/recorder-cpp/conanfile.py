from conan import ConanFile
from conan.tools.cmake import cmake_layout

class WinInputRecorderConan(ConanFile):
    name = "win_input_recorder"
    version = "0.1"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("sdl/2.30.9")  # SDL2 for gamepad support

    def build_requirements(self):
        self.tool_requires("cmake/3.27.1")

    def layout(self):
        cmake_layout(self)
