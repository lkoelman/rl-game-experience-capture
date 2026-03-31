param(
    [string]$BuildDir = "builddir",
    [string]$BuildType = "debug"
)

$ErrorActionPreference = "Stop"

conan install . --output-folder=$BuildDir --build=missing

$buildScript = "call ""$BuildDir\conanbuild.bat"" && " +
    "meson setup ""$BuildDir"" --buildtype $BuildType --native-file ""$BuildDir\conan_meson_native.ini"" --reconfigure && " +
    "meson compile -C ""$BuildDir"" && " +
    "meson test -C ""$BuildDir"" --print-errorlogs"

cmd /c $buildScript
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}
