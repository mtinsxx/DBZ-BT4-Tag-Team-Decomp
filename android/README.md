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
- Added guarded 32-bit guest <-> 64-bit host address conversion with range checks.
- Added a non-destructive mmap helper: MAP_FIXED_NOREPLACE, verifying exact placement and handling host page-size alignment.
- Added unit tests for conversion, unmapped-range rejection, and safe collision failure.

## Not completed / not claimed
- Android runtime and shared-library link of the game's original code.
- 32-bit PS2 pointer address translation on Android ARM64.
- Vulkan swapchain and Android SDL3 platform integration.
- Android Activity, touch event UI, JNI calls, ISO preparation.
- APK packaging, gameplay validation or performance measurements.

## Address-window module

The new `android/include/bt4_address_window.h` + `android/src/bt4_address_window.c` implement independently verifiable conversion and memory-mapping primitives. They are wired into port-level address conversions and guest region mappings, but **not yet into all game-code pointer operations**. Cross compiling these functions does not imply the game executes on ARM64. On Android, the LLVM pass and the ASLR-safe library placement must agree on the same runtime guest base. Never fall back to `MAP_FIXED`, low-address assumptions, or disabling ASLR.

## Engine memory integration (Android only)

- `port/src/port_addr.h` now converts port-level addresses using the
  dynamically configured `Port_Window` base on Android, leaving desktop
  behavior unchanged.
- `android/src/bt4_window_runtime.c` derives `Port_Window` from the
  ASLR-loaded ELF image using `dladdr`, without disabling ASLR.
- `port/src/plat_mem.c` uses non-destructive mappings for the guest stack,
  heap, register blocks, scratchpad and allocator arena on Android.
- The existing engine memory source is separately **compiled** for Android
  ARM64 in CI. This proves syntax/toolchain compatibility of that source,
  **not** correct runtime placement or execution.
- Mappings may fail due to conflicts with the Android loader's current
  address layout; this is a runtime constraint and must be diagnosed using
  real device logs, not bypassed with `MAP_FIXED`.

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
