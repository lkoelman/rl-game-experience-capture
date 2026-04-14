param(
    [string]$BuildDir = "builddir",
    [string]$BuildType = "debug",
    [string]$GStreamerRoot = "C:\Program Files\gstreamer\1.0\msvc_x86_64"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $GStreamerRoot)) {
    throw "GStreamer root not found: $GStreamerRoot"
}

$gstreamerRootShort = (cmd /c "for %I in (""$GStreamerRoot"") do @echo %~sI").Trim()
if ([string]::IsNullOrWhiteSpace($gstreamerRootShort)) {
    $gstreamerRootShort = $GStreamerRoot
}

$gstreamerBin = Join-Path $gstreamerRootShort "bin"
$gstreamerLibPkgConfig = Join-Path $gstreamerRootShort "lib\pkgconfig"
$gstreamerPluginPkgConfig = Join-Path $gstreamerRootShort "lib\gstreamer-1.0\pkgconfig"
$gstreamerSharePkgConfig = Join-Path $gstreamerRootShort "share\pkgconfig"

$pkgConfigPaths = @($gstreamerLibPkgConfig)
if (Test-Path $gstreamerPluginPkgConfig) {
    $pkgConfigPaths += $gstreamerPluginPkgConfig
}
if (Test-Path $gstreamerSharePkgConfig) {
    $pkgConfigPaths += $gstreamerSharePkgConfig
}
if ($env:PKG_CONFIG_PATH) {
    $pkgConfigPaths += $env:PKG_CONFIG_PATH
}
$pkgConfigPath = $pkgConfigPaths -join ';'

$conanBuildType = switch ($BuildType.ToLowerInvariant()) {
    "debug" { "Debug" }
    "release" { "Release" }
    default { throw "Unsupported build type for Conan settings: $BuildType" }
}

conan install . `
    --output-folder=$BuildDir `
    --build=missing `
    --build=protobuf/* `
    -s compiler.cppstd=20 `
    -s build_type=$conanBuildType `
    -s compiler.runtime=dynamic `
    -s compiler.runtime_type=$conanBuildType

$protobufPc = Join-Path $BuildDir "protobuf.pc"
$protobufPrefixLine = Select-String -Path $protobufPc -Pattern "^prefix="
if ($null -eq $protobufPrefixLine) {
    throw "Could not determine Conan protobuf package prefix from $protobufPc"
}
$protobufPrefix = ($protobufPrefixLine.Line -replace "^prefix=", "").Trim()
$protocPath = Join-Path $protobufPrefix "bin\protoc.exe"
if (-not (Test-Path $protocPath)) {
    throw "Conan protobuf package does not contain protoc.exe at $protocPath"
}
$protocBin = Split-Path $protocPath -Parent

$mesonNativeFile = Join-Path $BuildDir "conan_meson_native.ini"
$mesonPkgConfigPaths = @((Resolve-Path $BuildDir).Path) + $pkgConfigPaths
$mesonPkgConfigValue = "pkg_config_path = [" + (($mesonPkgConfigPaths | ForEach-Object {
    "'" + (($_ -replace '\\', '/')) + "'"
}) -join ", ") + "]"
$mesonNative = Get-Content $mesonNativeFile
$mesonNative = $mesonNative -replace "^pkg_config_path = .*$", $mesonPkgConfigValue
$mesonNative = $mesonNative -replace "^cpp_std = .*$", "cpp_std = 'c++20'"
$gstreamerRootValue = "gstreamer_root = '" + ($gstreamerRootShort -replace '\\', '/') + "'"
$propertiesIndex = [Array]::IndexOf($mesonNative, '[properties]')
if ($propertiesIndex -ge 0) {
    $insertIndex = $propertiesIndex + 1
    $mesonNative = @($mesonNative[0..$propertiesIndex] + $gstreamerRootValue + $mesonNative[$insertIndex..($mesonNative.Length - 1)])
}
$mesonNative | Set-Content $mesonNativeFile

$buildScript = "set ""GSTREAMER_1_0_ROOT_X86_64=$gstreamerRootShort"" && " +
    "set ""PATH=$protocBin;$gstreamerBin;%PATH%"" && " +
    "set ""PKG_CONFIG_PATH=$pkgConfigPath"" && " +
    "set ""PROTOC=$protocPath"" && " +
    "call ""$BuildDir\conanbuild.bat"" && " +
    "meson setup ""$BuildDir"" --buildtype $BuildType --native-file ""$BuildDir\conan_meson_native.ini"" --reconfigure && " +
    "meson compile -C ""$BuildDir"" && " +
    "meson test -C ""$BuildDir"" --print-errorlogs"

cmd /c $buildScript
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}
