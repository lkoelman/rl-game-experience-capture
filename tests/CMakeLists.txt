find_package(GTest CONFIG REQUIRED)

add_executable(recorder_tests
    gamepad_recorder_test.cpp
    window_recorder_test.cpp
)

target_link_libraries(recorder_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME recorder_tests COMMAND recorder_tests)
