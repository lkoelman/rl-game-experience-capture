cmake_minimum_required(VERSION 3.15)
project(win_input_recorder VERSION 0.1)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(protobuf CONFIG REQUIRED)

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
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    protobuf::libprotobuf
    xinput
    d3d11
    dxgi
)

# Tests
enable_testing()
add_subdirectory(tests)
