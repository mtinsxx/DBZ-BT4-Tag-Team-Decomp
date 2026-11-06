# Explicit Android engine startup — SAF revision

The first Galaxy test reached profile verification, then returned before
`Game_Main`: the desktop marker-based selector defaulted to Spanish without
installer metadata, rejecting the 2,115,696-byte English executable against the
2,115,688-byte Spanish expectation. The supplied private main was checked with
the actual new C verifier: size 2115696, CRC32 9d2224c1, English. Runtime profile
selection now uses `bt4_base_profile.c` and the existing size/CRC catalogue.
Checks of all three base programs remain mandatory. Unknown fingerprints stop
Android startup before the legacy desktop marker fallback.

`MainActivity.nativeSetIso` receives the selected document's fd, start offset
and declared length from `AssetFileDescriptor`. `Bt4Iso_MountFd` validates random
access and duplicates the descriptor; Java closes its borrowed fd afterward.
The app persists the SAF read grant and URI, never an assumed `_data` path.
Nonseekable providers fail explicitly; there is no whole-ISO copy fallback.
Setup can verify the source without entering gameplay, allowing cleanup of old
imports even when subsequent engine initialization fails. Cleanup revalidates
the source before removing only the old private import destinations.

The native source creates independent, bounded stdio views using `pread`:
Android funopen / host fopencookie. Raw ISO views serve ISO9660 and actual DVD
sector reads, preserving real LSNs/padding. `disc/...` resolves ISO files, and
`pzs4usN/NNNNN.bin` resolves a bounded member of `DATA/PZS4USN.AFS`. Empty members
are valid; corrupt offsets/counts or truncated backing data fail. Neither ISO
nor AFS content is extracted to disk. The existing file cache retains 32 open
entries in RAM. Descriptor/lock state is `.porthst`, not guest snapshot state.

`Port_FileOpenHost` preserves mods/aliases, then reads the selected ISO instead
of stale private base copies. Media and HUD portrait readers use `Port_ReadFile`
for internal `iso://` paths; normal desktop/mod paths retain standard fopen.
The JNI preflight checks the canonical three programs; the existing runtime
identity verifier also checks what its actual provider resolves, including any
base-program overrides, with observed/expected size and CRC in diagnostics.

`SDL_main` receives the private files directory, sets content roots and the
strict existing Vulkan backend, and logs the detected profile. Only supported
program data reaches initialization. `dlopen` configures the guest window, ABI
metadata and compact table relocations, and reserves the sparse guest ranges as
PROT_NONE before pointer constructors. Writable commit remains explicit. It does
not read the desktop `/proc/self/exe.dat`. Desktop keeps its existing constructor.

`Port_AndroidMemoryPrepare` commits the early reservations as read/write only
once every range is owned. No allocator pointer is exposed on partial failure.
See [GUEST_MEMORY.md](GUEST_MEMORY.md) for ownership, rollback and detailed mmap /
mprotect diagnostics; the compiler's fixed ELF/guest anchor is unchanged.
`elf_data.import_table` still derives 2,388 legal import spans from skeleton
metadata; pointer slots and zeros stay intact. The validated main/overlay are
read from the selected ISO into temporary RAM. `Bt4Android_ImportPrograms`
checks ELF32 MIPS ranges and all source spans before writing destinations. The
existing alternate-profile checksum rule remains gated on full base-program
verification; no integrity check is disabled.

After import, `Port_AndroidRunGame` calls real `Game_Main` on the 16 MiB guest
stack. The AAPCS64 trampoline preserves the SDL stack and reserved Android x18.
SDLActivity supplies JNI/Surface/audio/input/lifecycle; the existing GS/VU1 SDL
GPU Vulkan renderer supplies game frames. New diagnostics mark actual device
creation and window claiming; no substitute scene is rendered.

The Android process retains bionic/debuggerd's crash handler, rather than the
desktop handler which reset signals to SIG_DFL. The wrapper exports native/Java
logs, APK commits, device data and its own ApplicationExitInfo records/traces
when Android supplies them (native tombstones on supported system versions).
Missing traces are reported, not invented. The latest device log confirms ENG
validation and direct SAF reads, then reports guest memory mapping failure. It
contains no syscall errno or VMA snapshot. The revised reservation/commit path
and everything beyond it remain unverified on hardware.

Validation: 16 host CTest cases, 11 SDK/profile/ISO/memory sanitizer groups and 81
desktop tests pass. Synthetic tests cover exact ENG/ESP catalogue entries,
corruption, AFS boundaries/empty files, raw-source lifetime, nonzero offsets,
pipes, truncation, cache/media integration and sparse reads beyond 6 GiB.
The wrapper's Java test removes a 12 GiB sparse private import while preserving
external ISO/saves/mods/settings and rejecting symlink escape. ARM64 full links
have 274 game / 135 runtime units and zero SDK/ABS32 blockers. Only the owner
can establish actual Galaxy boot, Vulkan presentation and gameplay; complete
private game data/device access is unavailable here. Android state-list, modern
touch UI and complete MPEG movie decoding remain separate pending work.
