# Android native port — engineering status

This directory is the beginning of a **real native ARM64 port**, not a
standalone Android game and not a simulated launcher.

## Completed in this stage
- Added a thread-safe, complete-snapshot touchscreen-to-libpad2 bridge.
- Preserved the original game pad format (active-high in the bridge,
  active-low when delivered to the game).
- Integrated touchscreen input with the existing game input pipeline on
  Android, alongside SDL keyboard/gamepad handling.
- Added host-side unit tests and a separate NDK ARM64 compilation check.

## Not completed / not claimed
- Android runtime and shared-library link of the game's original code.
- 32-bit PS2 pointer address translation on Android ARM64.
- Vulkan swapchain and Android SDL3 platform integration.
- Android Activity, touch event UI, JNI calls, ISO preparation.
- APK packaging, gameplay validation or performance measurements.

## Architecture constraint to resolve next

The game retains PS2-sized 32-bit pointers. Existing Linux/Windows builds
require address ranges below 4 GB; macOS ARM64 uses a custom relocated
window and LLVM address-transform pass. An Android shared library loaded
under PIE/ASLR **cannot assume** either desktop memory layout. Solve
this before claiming a native game build.

See `port/src/port_addr.h`, `port/src/plat_mem.c`,
`port/tools/ptr32.py` and `port/tools/macwin/`.

## Local host tests (no game data)

```sh
cmake -S android -B build/android-host
cmake --build build/android-host --parallel
ctest --test-dir build/android-host --output-on-failure
```

## Android arm64-v8a compilation check (no APK yet)

```sh
cmake -S android -B build/android-arm64 \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28
cmake --build build/android-arm64 --parallel
```

The workflow also runs these checks. Only the user can test actual
gameplay, visuals, controls and performance on their device. Static
checks and successful builds must never be presented as proof of gameplay.
