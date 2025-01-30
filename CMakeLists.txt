cmake_minimum_required(VERSION 3.15)
project(win_input_recorder VERSION 0.1)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

# Main executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/gamepad_recorder.cpp
    src/sdl_input_recorder.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    xinput
    SDL2::SDL2main
    SDL2::SDL2-static
)

# Tests
# enable_testing()
# add_subdirectory(tests)
