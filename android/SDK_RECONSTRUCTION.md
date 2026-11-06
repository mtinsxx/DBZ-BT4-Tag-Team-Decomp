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
sets resolve **86 baseline entries: 85 actual functions and one wrongly
classified scalar**, leaving **16 undefined symbols**. No table was removed,
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

## Memory-file driver: 13 additional entries

`D_002D9800` dispatches a memory-backed Ci driver, not a host pathname API.
The recovered handle is 0x38 bytes with a 20-byte serialized name, signed-byte
status, sector geometry, position, read count and current request fields.
The original pool at `D_002D9868` has 40 handles. `mfCiOpen`/`mfCiReqRd` and
all remaining table methods are implemented in `bt4_sdk_mf.c`.

Names encode a compact guest hexadecimal address, any one non-hex delimiter,
and a hexadecimal byte length. The parser's second conversion follows the
original 64-bit strtoul result before truncation to 32 bits. Seek origins,
clamping, synchronous completion, signed status, busy/zero-count paths, sector
resizing and commands 201/203/205/401 retain their observed contracts. Commands
203/205 take a filename directly, unlike command 201's handle argument.
The server method `0x27F7A0` scans active entries; its original per-entry helper
at `0x27F798` is genuinely just return/nop. Reads already finish synchronously;
no fabricated asynchronous completion was added.

Two native safety changes are explicit: Open rejects names that would overflow
the 20-byte field, and Read copies only valid backing bytes before zero-filling
the rounded sector tail. The original copied the full rounded span first,
which could overread an unpadded allocation. Final bytes match for valid,
non-overlapping source/destination ranges. Synthetic tests read a 53-byte
allocation into a 2048-byte sector under ASan, check guard bytes, pool exhaustion,
seek/status/commands and all eight integer/pointer combinations of the original
three-argument driver error callback. Original error resource values remain
external; no commercial strings are included.

## Sixteen MMI pixel kernels: native scalar reconstruction

`bt4_sdk_pixels.c` implements all 16 entries in `D_002E7788`/`D_002E77A8`.
The 0x1c-byte argument record carries compact destination/source addresses,
shift, two heights and stride. These are ahead-of-time C algorithms, without
PS2 instruction interpretation or dynamic code translation.

| Modes | Luma entry | Chroma entry |
| --- | --- | --- |
| Copy | 0x29F2F8 | 0x29F370 |
| Vertical | 0x29F408 | 0x29F4C0 |
| Horizontal | 0x29F590 | 0x29F640 |
| Bilinear | 0x29F6F8 | 0x29F7F0 |
| Copy + destination blend | 0x29F8F0 | 0x29F990 |
| Vertical + blend | 0x29FA40 | 0x29FB20 |
| Horizontal + blend | 0x29FC08 | 0x29FCE0 |
| Bilinear + blend | 0x29FDB0 | 0x29FED0 |

Recovered semantics include QFSRV shifts/rotation, separate chroma planes,
16-bit outputs, source/destination strides and the second-height source jump.
Vertical filters retain the previous sample across that jump. Integer rounding
follows the actual instructions: PADDH wrap, logical PSRLH, signed-positive
conditional bias in chroma and the upper luma vertical/horizontal lanes.
The blended luma bilinear routine changes its interpolation bias from two to
one after the first output. Chroma vertical second-phase gating uses the low
16 height bits. These observed differences are preserved explicitly.

Tests compare all 16 kernels against an independent pixel-coordinate oracle:
1,536 randomized mode/shift/geometry cases plus explicit golden rounding,
wrap, zero-height and phase-mask cases, checking whole destination buffers.
This proves the documented scalar contracts against the test oracle, not
execution equivalence measured on a console or a gameplay result. SIMD/NEON
optimization remains future work after the scalar baseline.

## Eight DVD/host field methods

`bt4_sdk_file_fields.c` recovers seek, tell, signed-byte status and completed-byte
count for the DVD and host work records (0x48 and 0x30 bytes). DVD positions,
sector count and completion count are at +0x0c/+0x08/+0x14; host equivalents
are +0x1c/+0x10/+0x28. Both status bytes are at +2. Seek clears completed
sectors, clamps the updated position and preserves the 32-bit wrap of ADDU.
The read-byte method returns `completed_sectors << 11` only at status one.
Unknown fields stay opaque. These methods do not synthesize provider progress.

Tests cover all 256 status-byte values, overflow/clamping, all seek origins,
unknown origins, error callbacks and neighboring-byte preservation. They run
with the memory-file contract fixture under ASan/UBSan.

## Four IPU-table entries are diagnostics, not decoder operations

`0x2A0C00`, `0x2A0C10`, `0x2A0E08`, `0x2A0E18` tail-call `0x29C5A0` with
four distinct imported messages. Its helper `0x297E50` dispatches event type
zero: decoder+0x868 is a compact outer handle, decoder+0x0c enables dispatch,
outer+0x40 points to callback records, with callback/context/EE-gp at
record+0x0c/+0x10/+0x14. The event descriptor is two 32-bit words `{0, message}`.
Absent table/handler returns zero; otherwise the handler's return word is
propagated. With no outer handle or dispatch disabled, `0x29C428` prints through
the imported one-string format. The format's content is not embedded here.

The native implementation uses real `Port_LowAlloc`/`Port_LowFree` for the
transient descriptor. An Android stack pointer may be outside the compact
window; truncating a local C array address would break a guest callback.
Pointer/integer callback parameters use matching C call types and metadata;
integer returns retain their bits and pointer returns are encoded back into
a compact word. Unsupported return/argument ABIs abort explicitly. The saved
EE gp has no native PIC register effect. No IPU register is touched by these
four genuine diagnostic implementations.

The separate host contract uses its own strings and isolated guest-heap fixture
and checks all eight pointer/integer argument combinations, high-bit contexts
and returns, pointer returns, nested callback re-entry and cleanup, missing
handlers and the actual stdout route. This does not validate video decoding.

## Actual intermediate strict-link results

| Set | Runtime units compiled | Remaining | Resolved from original 102 | New undefined |
| --- | --- | --- | --- | --- |
| Memory helpers | 117/117 | 96 | 6 | 0 |
| TLB scalar correction (data relink) | 117/117 | 95 | 7 | 0 |
| Leaf fields/accessors | 118/118 | 88 | 14 | 0 |
| SJMEM and shared native CRI support | 120/120 | 79 | 23 | 0 |
| SJRBF | 121/121 | 72 | 30 | 0 |
| SJUNI | 122/122 | 63 | 39 | 0 |
| Memory-file driver | 123/123 | 50 | 52 | 0 |
| MMI pixel kernels | 124/124 | 34 | 68 | 0 |
| DVD/host field methods | 125/125 | 26 | 76 | 0 |
| IPU diagnostic leaves | 126/126 | 22 | 80 | 0 |
| DVD/host metadata and commands | 127/127 | 16 | 86 | 0 |

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

## Remaining 16 symbols and next work

- **Ten I/O methods**: five DVD (`dvCi`) and five host-file (`htCi`). Each
  group still needs server/update, open/read and close/stop. Recovered query
  methods below provide metadata, not request submission or completion. The
  DVD request allocator/read/abort/completion helpers are at `0x279D98`,
  `0x279EC0`, `0x27A368`, `0x27A238`, `0x279E20`; host descriptors and
  override providers still require a verified native handle translation.
  A directory override's descriptor is not evidence that it can be passed
  directly to POSIX read/close. Do not fabricate progress or silently ignore
  override handles.
- **Six IPU/MPEG parser/decoder entries**: `0x2A09D8`, `0x2A0B18`,
  `0x2A0C20`, `0x2A11E8`, `0x2A1278`, `0x2A1310`. They transitively
  consume bits through `0x29ECB8`/`0x29EEC8`, including IPU command/control
  (`0x10002000`/`0x10002010`), DMA/refill and decoder fields. For example,
  `0x2A09D8` reads 28/16-bit extension fields and updates +0x134/+0x138,
  +0x144/+0x148/+0x14c/+0x150; `0x2A1278` consumes a fixed sequence of
  bit widths; `0x2A1310` fills 16-bit fields at +0x19c/+0x1a8. Their native
  bitstream/refill contract is not implemented. Do not simply add these
  bodies with an undefined `0x29ECB8` and claim the old blockers resolved.

The existing `plat_movie.c`/`plat_mpeg2.c` boundary decodes from a host movie
path through sceMpeg* entry points. Its handle/layout and picture pipeline are
not the original CRI object/bit reader; blindly redirecting the six callbacks
there would lose their state semantics. A verified integration is still needed.

All remaining signatures/transitive dependencies must be inspected before
implementation. The main ELF now supplies the evidence; no additional file is
required to continue this SDK analysis. Later full data/overlay integration
still needs the owner's compatible resources. Full link, initialization,
JNI/lifecycle, Vulkan presentation and an APK remain subsequent work.

The earlier published checkpoint (engine `693381a`, main `ecb25d9`) was rebuilt
from a clean Debian 13 environment by Actions run
[42158816651](https://github.com/mtinsxx/BT4-Native-Android/actions/runs/42158816651):
9/9 host tests, 3/3 sanitizer contract groups and desktop regressions passed;
122/122 runtime units compiled and the strict link failed with exactly the
expected 63 symbols, zero ABS32 and no other linker errors. The newer 86-entry
checkpoint has separate local logs and must be checked in its own fresh CI run.

Local final host checks: 12/12 CTest with LLVM enabled; 10/10 SDK Python tool
tests; 6/6 SDK contract groups under ASan/UBSan; 81/81 desktop regressions.
The original Mach-O lowering test still emits its ARM64 object. The pixel
oracle and synthetic callbacks are unit evidence, not commercial-data execution
or an on-device test. The native Android runtime has 127 source units now.

## LLVM callback regression discovered after the 80-entry checkpoint

The actual IPU diagnostic source is now included in the LLVM execution test,
not only compiled as an ARM64 runtime object. A transformed guest callback
reads its compact two-word event descriptor and exercises pointer/integer
arguments through all four diagnostic entries at three loader-selected bases.
Both host execution and Android ARM64 object emission are checked.

The new guest test exposed invalid IR when conditional pointer-to-integer
constant expressions were PHI inputs: the ELF pass inserted checked conversion
instructions immediately before the PHI in its merge block. LLVM rejected
that ordering. The correction materializes each incoming value before the
predecessor terminator and splits a shared incoming edge first. Checks must
execute only on the selected edge. Incoming indices are re-read after splitting,
including duplicate switch edges. The original Mach-O constant path is unchanged.

`llvm_phi.ll` covers a critical branch edge, duplicate switch predecessors and
a checked foreign libc pointer: skipping its edge returns normally, selecting
it triggers the existing range guard. The end-to-end callback case and the
explicit IR regressions pass; the Mach-O ARM64 object is still emitted. This
fix does not resolve more SDK symbols or establish decoder/gameplay behavior.

## Six DVD/host query and command methods

`bt4_sdk_ci_query.c` implements `dvCiGetFileSize`, `0x27BDD8`, `0x27C680`,
`0x28B850`, `0x28B8F0` and `0x28C288`. The shared override lookup is recovered
from `0x27CE20`/`0x27DDC0`: compact table base, entry count, names offset in
8-byte units, stride-minus-one, two 32-bit words per entry. DVD matches ASCII
case-insensitively with slash normalization; host matches case-sensitively
with slash normalization. Zero-size entries fall back to the real provider.
DVD also preserves the imported whole-disc sentinel and the 17-character
hex-sector/hex-count filename form. All path prefixes/suffixes and error text
remain references to imported resources, not embedded game bytes.

Native DVD metadata reuses `Bt4Iso_Find` on `BT4_ISO`. Host metadata maps the
recovered host-device/base path into `Port_FileSize`, retaining the existing
extracted-data cache and its mod/alias precedence. This deliberately follows
the native port's immutable-content cache semantics; it does not promise fresh
filesystem stats after an external edit to an already cached file. Querying
real files does not supply the missing asynchronous request/handle provider.
Oversized constructed paths fail through the appropriate error path instead
of overflowing the original fixed buffers.

Extended commands retain their different meanings: DVD 202/204/301 are genuine
constant-zero leaves, 203/205 query a filename, 302 reads work+4. Host splits
64-bit file sizes and caps completed bytes by the file size using unsigned
comparison, including the original sign extension of override size words.
Both use command 299 for a non-null capability query, unlike memory-file's 401.
DVD command lookup failure does not invoke the named size getter's callback.

Targeted tests run the actual ISO reader and `plat_fcache.c` against synthetic
ISO/files, a sparse file of 0x180000035 bytes, native mod/alias selection,
multiple override records, zero-size fallback, negative/overflow byte counts,
missing files, callback distinctions, capability commands and overlong paths.
Only unrelated stage/song/state services are isolated test fixtures; production
links their existing implementations. The query group passes warnings-as-errors
and ASan/UBSan. After the group, the one final complete ARM64 build compiled
274/274 game and 127/127 runtime units; strict link failed with exactly 16
original symbols, no new undefined symbols, zero ABS32 and no other errors.
