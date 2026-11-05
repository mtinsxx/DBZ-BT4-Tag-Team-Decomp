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

# Groundwork for position-independent PS2 guest addresses on Android.
# A successful build does NOT yet prove full game address translation.
add_library(bt4_android_address STATIC src/bt4_address_window.c)
target_include_directories(bt4_android_address PUBLIC include)
target_compile_features(bt4_android_address PRIVATE c_std_99)
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(bt4_android_address PRIVATE -Wall -Wextra -Werror)
endif()

# The ARM64 runtime adapter resolves Port_Window from the actual loaded
# ELF image. No low-address mapping assumptions or ASLR disablement.
if(ANDROID)
    add_library(bt4_android_window STATIC src/bt4_window_runtime.c)
    target_include_directories(bt4_android_window PRIVATE ../port/src)
    target_link_libraries(bt4_android_window PUBLIC bt4_android_address ${CMAKE_DL_LIBS})
    target_compile_features(bt4_android_window PRIVATE c_std_99)

    # Compile the ACTUAL engine memory source for Android. Linking it to
    # the complete game still depends on generated game objects and the
    # address-space 271 LLVM transformation.
    add_library(bt4_android_memory OBJECT ../port/src/plat_mem.c)
    target_include_directories(bt4_android_memory PRIVATE ../port/src include)
    target_compile_features(bt4_android_memory PRIVATE c_std_99)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(bt4_android_window PRIVATE -Wall -Wextra -Werror)
    endif()
endif()

if(NOT ANDROID)
    enable_testing()
    add_executable(bt4_android_touch_tests tests/touch_test.c)
    target_link_libraries(bt4_android_touch_tests PRIVATE bt4_android_touch)
    add_test(NAME touchscreen_bridge COMMAND bt4_android_touch_tests)
    add_executable(bt4_android_address_tests tests/address_window_test.c)
    target_link_libraries(bt4_android_address_tests PRIVATE bt4_android_address)
    add_test(NAME guest_address_window COMMAND bt4_android_address_tests)
endif()
