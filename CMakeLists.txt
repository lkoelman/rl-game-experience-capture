cmake_minimum_required(VERSION 3.15)
project(win_input_recorder VERSION 0.1)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(protobuf CONFIG REQUIRED VERSION 3.21.12)

# TODO: first try the setup from this conan example: https://docs.conan.io/2/examples/graph/tool_requires/using_protobuf.html and https://github.com/conan-io/examples2/blob/main/examples/graph/tool_requires/using_protobuf/myaddresser/CMakeLists.txt
# TODO: alternatively, try to isolate the proto files like in https://stackoverflow.com/questions/77957876/how-to-properly-install-and-link-protobuf-with-conan2-cmake-when-system-protoc
# and create separate CMakeLists.txt file: include it with add_subdirectory(protos)

# Protobuf files
set(PROTO_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/gamepad_input.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/window_capture.proto"
)

# Generate protobuf files
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Main executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/gamepad_recorder.cpp
    src/window_recorder.cpp
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${protobuf_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}/proto
)

# FIXME: should we use ${protobuf_LIBRARIES} instead of protobuf::protobuf or protobuf::libprotobuf
target_link_libraries(${PROJECT_NAME} PRIVATE
    protobuf::libprotobuf
    xinput
    d3d11
    dxgi
)

# Tests
# enable_testing()
# add_subdirectory(tests)
