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
sets now resolve **102 baseline entries: 101 actual functions and one wrongly
classified scalar**, leaving **0 undefined symbols**. No table was removed,
no weak/stub symbol was introduced and no callback reference was nulled to
make the linker pass. The complete library passes `--no-undefined` (274 game units, 132 runtime units).

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
| DVD/host native I/O | 128/128 | 6 | 96 | 0 |
| IPU payload parsers + native input | 130/130 | 0 | 102 | 0 |
| Explicit Android bootstrap + data import | 132/132 | 0 | 102 | 0 |

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

## Previous checkpoint: six pending symbols (superseded below)

DVD/host dispatch targets are now implemented. The remaining six symbols
belong to the MPEG-2 extension dispatch table, described below.

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
expected 63 symbols, zero ABS32 and no other linker errors. The newer 96-entry
checkpoint has separate local logs and must be checked in its own fresh CI run.

Local final host checks: 12/12 CTest with LLVM enabled; 10/10 SDK Python tool
tests; 6/6 SDK contract groups under ASan/UBSan; 81/81 desktop regressions.
The original Mach-O lowering test still emits its ARM64 object. The pixel
oracle and synthetic callbacks are unit evidence, not commercial-data execution
or an on-device test. The native Android runtime has 128 source units now.

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

## Ten DVD/host I/O operations

`bt4_sdk_ci_io.c` implements both servers, Open/Read/Stop/Close for the DVD
and host drivers. It reuses the previously verified metadata/path helpers,
`Bt4Iso_Find`, `Port_FileSize` and `Port_FileReadAt`. Original pool allocation
selects the last free slot: 40 DVD records of 0x48 bytes, 13 host records of
0x30 bytes. These are distinct from the 40 first-free memory-file records.

The original request allocator at `0x279D98` has **one shared 0x38-byte
request**, `D_002D3320`, used by both drivers. Allocation/release update
`D_002D3380`/`D_002D3384`. Recovered request fields retain their offsets:
used/type/state/reserved byte at 0..3 (initial reserved byte 255), abort-request
word +4, DVD sector/count/destination/mode at +8/+0xc/+0x10/+0x14, host
destination +0x18, signed 64-bit byte count/sector position +0x20/+0x28,
descriptor +0x30. In particular, +4 is an abort flag, not an invented errno slot.
Jump tables at `0x2F5E80` and `0x2F8400` map request state 0 to stopped, 3 to
successful completion and 9 to work status 3/error; other states stay pending.
The optional service-guard callback and per-driver recursion guards are retained.

The native backend schedules real reads cooperatively: an asynchronous ReqRd
reserves the one slot, the appropriate Server performs the actual read and
only then commits completion/counters; Stop can cancel an unexecuted request.
Synchronous mode performs that read before returning. It uses a recursive native
critical section, so Stop waits if a Server is currently performing I/O. There
is no background IOP thread, fabricated descriptor or success without bytes.
DVD reads preserve actual ISO sector padding. Host reads retain a zero-filled
last sector and use the real remaining extent, fixing the original count that
could exceed the file/destination span. Request sizes are checked against the
compact destination range. Native read failure produces status 3 and a zero
synchronous result, rather than reproducing the EE synchronous path's ignored
error result. `Bt4Ci_LastIoError()` exposes zero/negative errno separately in
`.porthst`; it does not repurpose original game fields. ISO modes other than
2048-byte sectors fail with ENOTSUP rather than inventing raw-CD bytes.

DVD synchronous completion preserves its observed requested-count/clamped-count
difference. Seek/Stop/status and command behavior remain consistent with the
previously implemented field methods. Empty host files retain the original
Open rejection even though existence/size queries can succeed.

### Host descriptors, restore and content ownership

Normal host opens obtain a stable positive logical id for an immutable relative
path in the existing native file cache. The mapping and actual OS resources
live outside saved game memory. A hash collision or conflicting mapping fails
Open, never aliases another file. Repeated opens and restored work records
continue to address the same real path; the cache owns FILE lifetimes/eviction.
This follows its existing immutable-content/per-content-mount assumptions.

An original override id cannot safely be treated as a Linux fd. Native setup
can associate it with a real provider path through
`Bt4Ci_BindHostDescriptor(id, relative_path)`. That call verifies backing data
and refuses to change an existing id's path. Unbound override ids cause an
explicit Open error. A future initializer producing override tables must make
these associations; there is no automatic translation of arbitrary saved PS2
OS descriptors. Close does not invalidate borrowed mappings or immutable path
ids, which would break restore/replay. This is an explicit native provider
boundary, not a claim that the Android content initializer already exists.

### Verification

The targeted I/O suite runs actual ISO and cached-file reads, not mocked bytes.
It verifies different DVD/host padding, deferred and synchronous completion,
cross-driver one-slot exclusion, service guards, cancellation/Close, pool
exhaustion, override binding/restore/conflicts, EOF and real read errors. A
sparse file is read past 6 GB to validate sector-to-byte conversion and native
64-bit offsets. A declared extent larger than its real backing produces EIO;
a missing ISO produces ENOENT and unsupported sector format produces ENOTSUP.
Guard bytes remain intact. Warnings-as-errors and ASan/UBSan pass. The metadata
inventory regression also passes with 95 recovered functions.

The final local build compiles **274/274 game and 128/128 runtime units**;
`--no-undefined` fails with exactly the six IPU extension targets, zero new
undefined symbols, zero ABS32 and no other linker errors. Full library,
initialization/JNI/Activity/Vulkan presentation and an installable APK are
therefore still not produced. Desktop sources are unchanged; earlier desktop
and LLVM regression results remain separate evidence, not newly rerun tests.

## Refined IPU input/refill evidence

`D_002FAA08` is indexed by the MPEG-2 extension id, not an arbitrary method
number. Its remaining entries correspond to the standard extension payloads:

| Extension id | Original target | Payload |
| --- | --- | --- |
| 1 | 0x2A09D8 | sequence extension |
| 2 | 0x2A11E8 | sequence display extension |
| 3 | 0x2A0B18 | quantization-matrix extension |
| 4 | 0x2A1278 | copyright extension |
| 7 | 0x2A1310 | picture display extension |
| 8 | 0x2A0C20 | picture coding extension |

The event-one callback path is `0x29EB38`/`0x29E9D0` -> `0x297EC8`.
The outer handle is decoder+0x868; outer+0x40 points to decoder/callback records.
Event one uses callback/context/gp at +0x18/+0x1c/+0x20 and a descriptor whose
first word is one. It asks for data when fewer than 32 bits are available and
input DMA QWC is zero. The native bit reader must actually advance/refill and
observe decoder+0x878, not return a stale +0x848 cache word.

`0x297C18(outer, data, length)` builds input DMA tags through `0x2971F0` at
an aligned area derived from decoder+0x118f, then starts the input channel.
The original rounds the transfer span and masks addresses to the PS2 physical
range. `0x299558` reloads large transfers from the DMA interrupt. Separately,
`0x297CE8(outer)` sets decoder+0x878. Output setup at `0x297C58` updates
+0xc4/+0xec/+0xf0/+0xf4/+0xf8; it must not be mistaken for the input feed.

The quantization extension issues IPU commands 0x50000000/0x58000000; waiting
for them also consumes matrix payload. Skipping the commands without consuming
those bytes corrupts the next header position. A native implementation still
needs a verified input queue, byte/bit cursor, refill/EOF/callback bridge,
matrix loading and control-state mapping. The existing `plat_mpeg2.c` reader
and I-frame decoder can supply reusable logic, but its object and callback
protocol differ, and `plat_movie.c` does not construct the original CRI callback
records. No guessed bridge or zero-returning bit reader has been added.
The supplied main ELF contains the instructions needed to continue. Remaining
work is native implementation and validation of this bridge, not another file
upload. No video/gameplay execution has been validated.

## Current milestone: all six IPU extension parsers implemented

The previous six-symbol checkpoint above is superseded. `bt4_sdk_ipu.c` now
implements all six targets, with native `void(void *decoder)` contracts. The
original dispatcher at 0x2A0E28 calls the table target and immediately aligns
the bitstream; its return value is unused. These are payload handlers, not
whole-frame decoders. The earlier IPU error leaves remain unchanged in meaning.

| Target | Reconstructed behavior |
| --- | --- |
| 0x2A09D8 | Sequence fields, profile/chroma diagnostics, progressive state, dimension and bitrate extensions; original unsigned wrap arithmetic retained |
| 0x2A11E8 | Conditional color-description consumption, matrix coefficients and 14-bit display dimensions |
| 0x2A0B18 | Luma matrix flags and two 64-byte matrix loads at the current bit position; reports the original unsupported chroma-matrix flags without guessing extra payload consumption |
| 0x2A1278 | Copyright payload: widths 1/8/1/7/1/20/1/22/1/22, including the original discarded/marker fields |
| 0x2A0C20 | Four f_code values, DC precision, structure, prediction/scan flags, preserved first-structure cache and optional composite-display payload |
| 0x2A1310 | One to three unsigned horizontal/vertical display offsets according to progressive, structure, repeat and top-field-first state |

The native `bt4_ipu_input.c` backend supplies the missing transitive behavior,
not an undefined Sony bit-reader call. It copies exactly the declared input
length into compact-heap chunks, reads MSB-first across fragmented input,
maintains actual consumed/available counts and updates original cache words.
Event-one refill dispatch follows outer+0x40 and callback/context at +0x18/+0x1c,
using a real compact 8-byte descriptor and checked guest/native callback kinds.
The handler may synchronously feed data or arrange another thread to do so.
The recovered 0x297C18 feed and 0x297CE8 end interfaces are native adapters;
no PS2 DMA address mask is applied to Android pointers. A condition variable
coordinates producers without holding the queue mutex across user callbacks.
Unbound input, EOF before a complete field and allocation/range failures are
explicit errors; no missing bits are fabricated as zero.

Input state and 32-bit chunk links occupy the original private aligned DMA
workspace (decoder+0x118f aligned down, within the constructor's 0x19a0 bytes)
and `Port_LowAlloc` memory. Only OS locks/condition variables live in `.porthst`.
Quiescent state/heap restoration therefore restores the exact cursor and queue,
not an external sidecar cursor. The native initializer supports quiescent reset
without overwriting the public decoder fields or callback records.

SETIQ-equivalent matrix loads consume exactly 512 bits without byte alignment.
Default intra matrices and wire-to-raster mapping reuse the existing
`plat_mpeg2.c` tables through `port_mpeg_quant.h`; no duplicate movie decoder or
emulator is added. MPEG control state preserves unrelated bits and normalizes
reserved DC precision 3 to the hardware's readback value 1. Optional/composite
fields, structure-cache preservation and display-offset branch counts follow
the recovered instructions. The original CRI object is deliberately not cast
to the incompatible `plat_movie.c` host decoder handle.

### Executed validation

- Six handlers, fragmented refill, pointer/integer/void callbacks, asynchronous
  producer, truncation/EOF errors, all display-count branches and control bits.
- Matrix combinations at a non-byte-aligned cursor, independent raster oracle,
  preserved next-field guard, default matrices, both chroma diagnostics.
- Quiescent snapshot/replay including the chunk allocator, plus allocation
  canaries and cleanup. Tests contain only synthetic bitstreams/resources.
- **14/14 host CTest cases**, including the actual LLVM guest/native diagnostic
  bridge and preserved Mach-O ARM64 lowering; **8/8 SDK groups under ASan/UBSan**
  (LeakSanitizer remains disabled by executor restriction); **81/81 desktop tests**.
- Strict links: **130/130** runtime units after the six IPU functions; then
  **132/132** after explicit Android bootstrap/import. Both use the whole
  **274-unit** game archive and `--no-undefined`: **0 unresolved, 0 new SDK
  symbols, 0 ABS32, 0 other LLD errors**. `libtenkaichi_mobile.so` was generated.
- Experimental APK assembled using SDLActivity JNI and the real game/Vulkan
  entry, signed and ZIP-aligned for 16 KiB pages. See [ANDROID_BOOT.md](ANDROID_BOOT.md).

The native library/APK has not run on the Galaxy. The complete compatible data
set, including DBZP.BIN and DBZ4.BIN, is absent from this workspace; the supplied
main ELF alone cannot establish boot. No gameplay, full movie decoding,
performance or touchscreen UI completion is claimed. User-data import and
hardware diagnostics are the next validation boundary, not another unresolved
SDK symbol. Commercial inputs/disassembly stay private and untracked.
