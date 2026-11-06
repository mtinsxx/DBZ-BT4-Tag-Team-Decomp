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
- Added opt-in ELF lowering to the existing LLVM window pass, with runtime
  initializers for compact pointers and checked host-to-guest conversions.
- Added callback signature registration/adaptation without a 4 GiB alignment
  assumption, plus direct cross-translation-unit signature reconciliation.
- Compiled all 274 original game C translation units to ARM64 objects.
- Strictly linked an explicitly labelled subset containing the original
  alignment and intrusive-list implementations (not the complete game).
- Compiled all 122 runtime/vendor/Android translation units, including the
  existing GS/VU1, SDL GPU/Vulkan, audio, menu and network sources.
- Removed assembly-table ABS32 relocations with checked startup fixups;
  direct native/software-float boundaries have integer-bit adapters.
- Implemented real ISO9660 metadata lookup for `sceCdSearchFile`, verified
  with synthetic data. No commercial ISO or assets are included.

## Not completed / not claimed
- Android runtime and shared-library link of the game's original code.
- Complete runtime ABI/data/stack integration and execution on Android ARM64.
- Vulkan swapchain and Android SDL3 platform integration.
- Android Activity, touch event UI, JNI calls, ISO preparation.
- APK packaging, gameplay validation or performance measurements.

## Address-window module

`android/include/bt4_address_window.h` + `android/src/bt4_address_window.c`
implement conversion and safe memory-mapping primitives. The separate
`build_game.py` pipeline now applies LLVM guest-pointer lowering to the
original game code. The foundation CMake libraries alone do not run that
pipeline. Cross compilation does not imply execution on ARM64: the loaded
image, data fixups, ABI and guest stack must agree on one window. Never fall
back to `MAP_FIXED`, low-address assumptions or disabling ASLR.

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

## Compiler and runtime work

The ELF mode (`-bt3-elf`) of the shared LLVM pass now lowers guest memory
operations and compact pointer initializers without absolute 32-bit ELF
relocations. Checked conversions use the ASLR-selected `Port_Window`;
indirect-call adaptation uses registered callee signatures, rather than the
low bits of a host pointer. Tests execute transformed code and the original
list implementation on the host at multiple ASLR bases and emit ARM64
objects. This is still not proof that the complete runtime ABI, floating-point
boundaries, guest stack or data loading are correct on Android.

See [build pipeline and remaining integration](BUILD.md) for reproducible
commands. No compiler is embedded in an Android application.

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

## SDK investigation

The full ARM64 link has advanced from 102 to **63** SDK/CRI targets. See
[SDK_RECONSTRUCTION.md](SDK_RECONSTRUCTION.md) for the verified native methods,
compact stream layouts, tests and scalar correction. See
[SDK_INVESTIGATION.md](SDK_INVESTIGATION.md) for the 15-table audit, strict-link
comparison tool and private R5900 evidence generation from the owner's ELF/ISO.
This is separate from the passing foundation/module builds above.
