cmake_minimum_required(VERSION 3.22)
project(bt4_android_foundation LANGUAGES C)

# First deliverable: compile the genuine libpad2 touchscreen bridge for
# Android arm64-v8a. This is NOT a game runtime or an APK yet.
add_library(bt4_android_touch STATIC ../port/src/gs/android_touch.c)
target_include_directories(bt4_android_touch PUBLIC ../port/src/gs)
target_compile_features(bt4_android_touch PRIVATE c_std_99)
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(bt4_android_touch PRIVATE -Wall -Wextra -Werror)
endif()
find_package(Threads REQUIRED)
target_link_libraries(bt4_android_touch PUBLIC Threads::Threads)

if(NOT ANDROID)
    enable_testing()
    add_executable(bt4_android_touch_tests tests/touch_test.c)
    target_link_libraries(bt4_android_touch_tests PRIVATE bt4_android_touch)
    add_test(NAME touchscreen_bridge COMMAND bt4_android_touch_tests)
endif()
