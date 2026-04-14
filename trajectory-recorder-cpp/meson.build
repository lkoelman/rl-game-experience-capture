project(
  'trajectory_recorder',
  'cpp',
  version : '0.1.0',
  default_options : ['warning_level=3', 'cpp_std=c++20'],
)

cpp = meson.get_compiler('cpp')
protobuf_dep = dependency('protobuf')
sdl3_dep = dependency('sdl')

gst_root = meson.get_external_property('gstreamer_root', '')
if gst_root == ''
  error('Missing GStreamer root. Set it from the native file or build helper.')
endif

gst_include_dirs = include_directories(
  gst_root / 'include',
  gst_root / 'include' / 'gstreamer-1.0',
  gst_root / 'include' / 'glib-2.0',
  gst_root / 'lib' / 'glib-2.0' / 'include',
)
gst_lib_dir = gst_root / 'lib'
gst_dep = declare_dependency(
  include_directories : gst_include_dirs,
  dependencies : [
    cpp.find_library('gstreamer-1.0', dirs : gst_lib_dir),
    cpp.find_library('gobject-2.0', dirs : gst_lib_dir),
    cpp.find_library('glib-2.0', dirs : gst_lib_dir),
    cpp.find_library('intl', dirs : gst_lib_dir),
  ],
)
gst_app_dep = declare_dependency(
  dependencies : [
    cpp.find_library('gstapp-1.0', dirs : gst_lib_dir),
    cpp.find_library('gstbase-1.0', dirs : gst_lib_dir),
  ],
)

subdir('protos')

inc_dir = include_directories('include', '.', 'protos')

core_lib = static_library(
  'recorder_core',
  [
    'src/Session.cpp',
    'src/VideoRecorder.cpp',
    'src/InputLogger.cpp',
    'src/SyncLogger.cpp',
    # 'src/Replayer.cpp',  # required for convert_dataset, requires OpenCV
  ],
  generated_protos,
  include_directories : inc_dir,
  dependencies : [gst_dep, gst_app_dep, sdl3_dep, protobuf_dep],
)

executable(
  'record_session',
  'src/main_record.cpp',
  include_directories : inc_dir,
  link_with : core_lib,
  dependencies : [gst_dep, gst_app_dep, sdl3_dep, protobuf_dep],
)

# Requires OpenCV
# executable(
#   'convert_dataset',
#   'src/main_convert.cpp',
#   include_directories : inc_dir,
#   link_with : core_lib,
#   dependencies : [opencv_dep, protobuf_dep],
# )

test_inc_dir = include_directories('include')
test(
  'binary_io',
  executable(
    'binary_io_tests',
    'tests/BinaryIOTests.cpp',
    include_directories : test_inc_dir,
  ),
)

test(
  'record_cli',
  executable(
    'record_cli_tests',
    'tests/RecordCliTests.cpp',
    include_directories : test_inc_dir,
  ),
)

test(
  'video_recorder_path',
  executable(
    'video_recorder_path_tests',
    'tests/VideoRecorderPathTests.cpp',
    include_directories : test_inc_dir,
  ),
)
