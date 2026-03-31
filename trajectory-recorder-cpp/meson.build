project(
  'trajectory_recorder',
  'cpp',
  version : '0.1.0',
  default_options : ['warning_level=3', 'cpp_std=c++20'],
)

protobuf_dep = dependency('protobuf')
sdl3_dep = dependency('sdl3')
opencv_dep = dependency('opencv4')
gst_dep = dependency('gstreamer-1.0')
gst_app_dep = dependency('gstreamer-app-1.0')

subdir('protos')

inc_dir = include_directories('include', meson.current_build_dir())

core_lib = static_library(
  'recorder_core',
  [
    'src/Session.cpp',
    'src/VideoRecorder.cpp',
    'src/InputLogger.cpp',
    'src/SyncLogger.cpp',
    'src/Replayer.cpp',
  ],
  generated_protos,
  include_directories : inc_dir,
  dependencies : [gst_dep, gst_app_dep, sdl3_dep, protobuf_dep, opencv_dep],
)

executable(
  'record_session',
  'src/main_record.cpp',
  include_directories : inc_dir,
  link_with : core_lib,
  dependencies : [gst_dep, gst_app_dep, sdl3_dep, protobuf_dep],
)

executable(
  'convert_dataset',
  'src/main_convert.cpp',
  include_directories : inc_dir,
  link_with : core_lib,
  dependencies : [opencv_dep, protobuf_dep],
)

test_inc_dir = include_directories('include')
test(
  'binary_io',
  executable(
    'binary_io_tests',
    'tests/BinaryIOTests.cpp',
    include_directories : test_inc_dir,
  ),
)

