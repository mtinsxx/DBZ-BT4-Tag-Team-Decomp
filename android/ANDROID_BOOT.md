# Explicit Android engine startup

`bt4_android_entry.c` exports the real `SDL_main`, called by upstream SDL3 JNI
from the wrapper's SDLActivity. `dlopen` initializes the guest address window,
ABI metadata and compact table relocations, but no longer reads `/proc/self/exe.dat`
or reserves game memory through the desktop `Port_MapMemory` constructor.
Desktop startup retains that constructor unchanged.

The Android entry receives the application's private files directory. It sets
the data/content roots, selects the strict existing Vulkan backend, identifies
the English/Spanish profile, and verifies all three base programs using the
existing size/CRC identity check: `SLUS_219.78`, `BIN/DBZP.BIN`, `BIN/DBZ4.BIN`.
Failures stop startup and produce `tenkaichi-native.log`; no synthetic frame is
presented. The main ELF supplied during reconstruction alone is insufficient.

`Port_AndroidMemoryPrepare` reserves heap, guest stack, compact allocator arena,
registers and scratchpad with checked `MAP_FIXED_NOREPLACE` mappings. A failed
reservation rolls back this attempt's earlier mappings, rounded to the host
page size. Existing mappings are never replaced. No game thread is started yet.

`elf_data.import_table` derives 2,388 import spans from the existing legal
`port/data/index.txt` and `.space` directives. Native destination labels follow
the PIE library's actual load bias; `.zero` and symbolic `.long` slots are not
imported. No disc is needed to compile this metadata and no game values are
included. The shared-library `.porthst` descriptors contain native addresses
and stay outside guest snapshots. Unsupported layout directives fail the build.

`Bt4Android_ImportPrograms` validates the main ELF32 little-endian MIPS section
table and file ranges, constructs the original main image, checks every import
range and computes the existing ordered FNV checksum before changing any
destination. A checksum difference is accepted only when the caller has already
verified the complete supported base-program profile, as in desktop startup.
Tests cover truncated sections, overflow, byte order, missing/invalid data,
checksum rejection without partial writes, and preservation of pointer slots.

Only after the import succeeds does `Port_AndroidRunGame` call the real
`Game_Main` on the reserved 16 MiB guest stack. The AAPCS64 trampoline preserves
callee-saved registers and the original SDL stack, leaves Android's reserved
x18 untouched, and restores the SDL stack if the game returns. SDLActivity
supplies Surface/audio/input/lifecycle JNI. The existing GS/VU1 renderer creates
the SDL GPU Vulkan device/window and presents the game's frames; no replacement
render loop or demo scene was added.

This path compiles and links but has **not executed on ARM64 hardware** here.
The owner must validate load bias/mappings, stack entry, asset import, startup,
Vulkan presentation and lifecycle on the Galaxy S24 FE. Android snapshot lists,
touchscreen UI and complete movie playback remain separate work. The six CRI
extension parsers preserve their original object contracts; the existing movie
path remains distinct and its software decoder is I-picture limited.
