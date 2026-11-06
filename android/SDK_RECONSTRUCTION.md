# Verified SDK reconstruction — owner executable supplied 2026-11-06

## Input and scope

The owner's main executable was privately identified as the supported B14 Rev2
English **main-program profile**. Its flat-image SHA-1 is
`a4776fbadd2d266f42cf559a3530c2e6454d01a3`, already pinned in `bt4_disc.py`.
All **103 baseline table words** matched their original addresses. This verifies
the main program/address map, not the whole ISO or its overlays/assets.

The main ELF, flat image and R5900 assembly remain under ignored local paths.
No original machine code, resource strings, GUID values or commercial data are
embedded in the new sources/tests or uploaded to GitHub Actions. Production
methods reference the original skeleton resource symbols; tests supply their
own synthetic resources.

The original **102-symbol baseline is immutable**. The first reconstructed
sets resolve **39 baseline entries: 38 actual functions and one wrongly
classified scalar**, leaving **63 undefined symbols**. No table was removed,
no weak/stub symbol was introduced and no callback reference was nulled to
make the linker pass. The complete library still fails `--no-undefined`.

[sdk-reconstruction.json](sdk-reconstruction.json) lists every reconstructed
function, its native signature and source file. These are native ABI adapters
for recovered EE register/memory contracts, not claims of original C prototype
spelling or a byte-matching EE rebuild. No gameplay/device execution occurred.

## Memory helpers: six real implementations

| Original entry | Recovered register contract / behavior |
| --- | --- |
| `0x2BA2D0`, `0x2BA518`, `0x2BA938`, `0x2BC748` | `$a0=destination`, `$a1=source`, unsigned `$a2=byte count`; copy `count >> 2` aligned 32-bit words forward, discard the 0–3 byte tail, return zero |
| `0x2BA688` | Same arguments; copy every byte forward, return zero |
| `0x2BA308` | `$a0=start`, `$a1=end`, `$a2=word`; read successive 32-bit words, return a match strictly below `end`, else null. The original reads the word at `end` before rejecting it; even an empty/reversed range performs its first read |

`bt4_sdk_memory.c` preserves zero-count behavior, high-bit words, forward
overlap propagation and access order using volatile loads/stores. It does not
substitute memcpy/memmove or a generic search with different bounds. The search
requires readable aligned memory at its final probe, as the original does.

The surrounding records are EE kernel patch/syscall-installation metadata,
not arbitrary gameplay callbacks. Their method bodies, rather than address
proximity, establish the contracts. The native helper implementations perform
real memory operations; they do not try to install PS2 kernel patches on Android.

## One scalar was incorrectly treated as executable code

`D_002EB190+0x14` contains **EntryHi `0x00100000` in the second 16-byte TLB
record**, not a function pointer. Evidence: `0x2BC968` reads 16-byte records
from `D_002EB330+0x14`, loads all four words as scalar arguments and passes them
to the kernel TLB-entry syscall at `0x2BC888` (syscall number `0x56`). The same
routine programs CP0 Wired and iterates the related mapping arrays.

The skeleton now keeps `.long 0x00100000` in exactly the same four bytes.
`data_literals.py` scopes the correction to this table/offset. Both real-data
conversion and skeleton regeneration preserve it; regeneration checks the
original word and retains the existing checksum partition (the four bytes
remain excluded from imported value runs). A real code pointer named
`func_00100000` elsewhere remains symbolic. This is a data-classification fix,
not a dummy implementation of the program entry point.

## Leaf driver fields and SJRBF accessors: seven implementations

| Entry | Recovered effect |
| --- | --- |
| `0x27BD48` | Store error callback/context guest words at `0x2D3FE4` / `0x2D3FE8`, in that order |
| `0x27F7F8` | Store callback/context at `0x2D97F4` / `0x2D97F8` |
| `0x28B838` | Store callback/context at `0x2E6F5C` / `0x2E6F60` |
| `0x27C4E0`, `0x28C158` | Return the sector size `0x800`; these constants are the complete observed functions |
| `0x283330` | Load the raw 32-bit UUID address at object `+8`, without a validity check or lock |
| `0x283338` | Store callback context at object `+0x3C`, then callback at `+0x38` |

Stored addresses are serialized guest words, not truncated host pointers.
The LLVM signature adapter encodes pointer arguments/results when a game caller
uses pointer types across these explicitly integer-word native interfaces.

## Three CRI stream interfaces: all 27 table methods

The original error-resource references identify the classes: **SJMEM** at
`D_002DE590`, **SJRBF** at `D_002DEA78`, and **SJUNI** at `D_002E2AB0`.
The nine methods of each table have been reconstructed. SJRBF's two leaf
accessors are counted in the preceding group, leaving seven additional methods.

Their objects and chunk descriptors keep explicit 32-bit fields and checked
compile-time sizes: SJMEM `0x24`, SJRBF `0x40`, SJUNI `0x30`, queue nodes
`0x10`, chunk `{address, signed size}` eight bytes. Native pointers occur only
at calls; saved addresses, links, callbacks and contexts stay compact.

### SJMEM

Destroy, UUID lookup, error registration, reset, total-size query, GetChunk,
PutChunk, UngetChunk and availability query follow the original modes and
validation paths. Mode 1 consumes from a memory stream. Mode 0 produces empty
chunks; invalid modes report `-3` where observed. PutChunk in modes 0/1
deliberately leaves its descriptor unchanged. Unget updates/clamps position
and availability before checking the returned address. Signed negative
requests and 32-bit wrap arithmetic are preserved rather than silently clamped.

### SJRBF

Separate read/write positions, available counts and four transfer counters
are preserved. Chunk length is the signed minimum of requested bytes,
available bytes and contiguous ring-plus-mirror space. GetChunk advances modulo
capacity. PutChunk copies the prefix into the suffix mirror and copies overflow
back into the ring head, then commits counts. Unget verifies the modulo address
and, as in the original, decrements the reservation counter even on mismatch.
Zero-capacity/error paths leave outputs unchanged where the original does.

The internal copy at `0x2A9A1C` was inspected too: the native equivalent keeps
its forward 16-byte / 8-byte / byte access granularity, including overlap,
instead of assuming that every incoming range is memcpy-compatible.

### SJUNI

Four queues share the original finite free-node pool; the adapter does not
invent host allocations when the pool is exhausted. Reset links pool nodes
without clearing their reserved words. GetChunk pops a complete head or,
only when the split byte equals one, splits a partial prefix. Put/Unget merge
contiguous tails/heads in split mode, otherwise allocate and link a pool node.
Availability returns the **whole first descriptor's size**, with different
exact-size versus splittable acceptance rules. Empty queues and invalid handles
retain their distinct output/error behavior. All links use the configured
guest address window.

### Critical sections, callbacks and error resources

All three original interfaces enter the same CRI critical-section hooks.
The Android adapter uses a recursive pthread mutex, retains optional hook
callbacks and the original nesting/token fields, and allows callbacks to
re-enter methods. Native mutex/once state resides in `.porthst`, outside
deterministic game snapshots.

Error dispatch retains the object's `-3` callbacks, the shared imported error
buffer and the optional global logger. Callback argument adaptation consults
the actual registered integer/pointer ABI; compact contexts are not passed as
untranslated host addresses. Original error IDs/descriptions are referenced
through imported symbols, without shipping their values.

## Actual intermediate strict-link results

| Set | Runtime units compiled | Remaining | Resolved from original 102 | New undefined |
| --- | --- | --- | --- | --- |
| Memory helpers | 117/117 | 96 | 6 | 0 |
| TLB scalar correction (data relink) | 117/117 | 95 | 7 | 0 |
| Leaf fields/accessors | 118/118 | 88 | 14 | 0 |
| SJMEM and shared native CRI support | 120/120 | 79 | 23 | 0 |
| SJRBF | 121/121 | 72 | 30 | 0 |
| SJUNI | 122/122 | 63 | 39 | 0 |

Each attempt uses the whole 274-unit game archive and `--no-undefined`.
Logs/reports stay in local build output and are reproduced by the full Actions
workflow; the original commercial input is not required to build these sources.

Host contracts cover word-copy tails/overlap, search bounds, compact high-bit
fields, callback registration, error propagation, reentrancy, two concurrent
consumers, ring wrap/mirroring and accounting, four queue modes, split/merge,
pool exhaustion, negative requests and destruction. Synthetic generator tests
also verify the TLB scalar's scope and checksum partition. SDK contract code is
compiled with warnings as errors. ASan/UBSan runs use GCC's installed runtime;
LLVM's optional sanitizer runtime is absent in this workspace. Leak scanning
is disabled because the managed executor cannot run its ptrace scan; these
tests do not establish leak freedom or device gameplay.

## Remaining 63 symbols and next work

- **37 file-driver entries**: DVD (`dvCi`), memory-file (`mfCi`) and host-file
  (`htCi`). Registration and sector-size leaves are implemented; open/read,
  seek/status, completion and directory/provider dependencies still need full
  reconstruction/adaptation. They are not blindly aliased to ADXF functions.
- **16 R5900 MMI pixel/interpolation kernels** in `D_002E7788` / `D_002E77A8`:
  packed loads, alignment shifts, byte expansion and interpolation need portable
  scalar/NEON equivalents and pixel-exact tests.
- **10 IPU/MPEG dispatch targets** in `D_002FAA08`: original methods access
  IPU control/command registers (`0x10002010` / `0x10002000`) and bitstream
  helpers. They need a verified native decoder boundary, not mapped PS2
  hardware registers, invented callbacks or empty methods.

All remaining signatures/transitive dependencies must be inspected before
implementation. The main ELF now supplies the evidence; no additional file is
required to continue this SDK analysis. Later full data/overlay integration
still needs the owner's compatible resources. Full link, initialization,
JNI/lifecycle, Vulkan presentation and an APK remain subsequent work.
