# Guest memory ownership and Android boot diagnostics

## Evidence and limits

The Galaxy's new log proves that all three English base programs passed through
SAF, then `Port_AndroidMemoryPrepare` returned failure. The previous APK collapsed
page-size/window validation, six mmap operations and their errno values into
`Guest memory mapping failed`. That message cannot establish which operation
failed, its address, errno, or whether it was a collision. No old-process VMA
snapshot, mmap errno or tombstone was supplied. Do not describe a guessed errno
as an observed device result.

The code did have a concrete ownership defect: constructor 101 selected
`W = ELF load bias - 0x20000000`, but reserved nothing there. Metadata constructors,
Java/SDL and ISO verification could allocate into the future guest ranges before
the late six mmap calls. Computing pointer translations does not reserve virtual
memory. Host tests reproduce this late-collision case and verify preservation of
the conflicting data and rollback.

## Correction without moving pointers

The ELF image anchor remains exactly `W + 0x20000000`; the LLVM pass's constant
and dynamic pointer translations, 32-bit fields and startup fixups are unchanged.
Moving W after these constructors would invalidate pointers and is forbidden.
The native library and all dependent system libraries retain normal ASLR.

Constructor 101 now reserves only the six sparse guest intervals, **PROT_NONE**,
before compact-pointer constructors and subsequent verification allocations:

| Region | Guest address | Requested size |
| --- | --- | --- |
| Heap | 0x003be000 | 0x01c42000 |
| Stack | 0x02000000 | 0x01000000 |
| Compact allocator | 0x13000000 | 0x0c000000 |
| Hardware registers | 0x10000000 | 0x00010000 |
| GS registers | 0x12000000 | 0x00002000 |
| Scratchpad | 0x70000000 | 0x00004000 |

These are virtual address reservations, not data import or game startup; pages
are not touched. The ELF PT_LOAD/RELRO mappings already belong to the loader and
are never remapped. No complete 4 GiB reservation is attempted over the image.

`Bt4Addr_Plan` computes host addresses and outward page rounding with overflow
checks. On a 16 KiB host page, the heap starts at `W+0x003bc000` and has rounded
size `0x01c44000`, ending exactly at the stack start. GS's 8 KiB request reserves
16 KiB. 4, 16 and 64 KiB arithmetic is tested; real VM tests use the host's actual
page size, not a claim of Android 16 KiB execution.

Reservations use MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE. If the kernel
rejects that flag with EINVAL/EOPNOTSUPP, the only compatibility retry is a normal
**nonfixed hint**, followed by an exact-address check. A relocated mapping is
released and rejected. EEXIST, EPERM and other real failures are not bypassed.
An old kernel ignoring NOREPLACE is subject to the same check. MAP_FIXED is
never used and no mapping requests PROT_EXEC.

Explicit boot after the already validated ISO calls `mprotect(READ|WRITE)` only
on owned rounded reservations. All six must succeed before memory is ready or
allocator pointers become visible. Reservation/commit failures release only the
ranges owned by that transaction. Failed unmap retains ownership/records and
poisons the transaction rather than silently retrying. The compact allocator
uses the committed arena; it never attempts a duplicate mmap over its reservation.
Heap/stack/arena layout constants are compile-time checked against the existing
runtime. Desktop mapping behavior is unchanged.

## Diagnostics in the next device export

Early reports live in `.porthst` and survive until SDL_main configures stderr.
Before/after explicit commit the log prints:

- immutable W, ELF anchor and each PT_LOAD/RELRO address, size, alignment and R/W/X flags;
- named region, guest/requested host address and original size;
- host page size, aligned host start and rounded mapping length;
- mmap protection/flags, fd=-1, offset=0, returned address, result and errno;
- unsupported-flag errno/hint retry, and failure to release an unintended mapping;
- mprotect RW attempt/errno and rollback errno/ownership;
- first and latest failure plus a bounded `/proc/self/maps` overlap snapshot.

VMA snapshots are diagnostics only, never permission to overwrite memory. They
may reveal a collision present even before library constructors. A first early
failure is retained even if a later retry succeeds. Failure still blocks the
engine; a successful memory transaction automatically proceeds to real program
table import and `Game_Main` on its guest stack. Two additional boot messages
mark the start/end of that next data-initialization step. ISO import, profile
checks, persistent permissions, AFS and sector I/O are untouched.

## Verification and remaining device boundary

Tests cover real sparse reservations/RW access, pointer round trips, idempotent
commit, preservation of pre-existing mappings, late-collision rollback, injected
mprotect failure, unsupported/ignored NOREPLACE, permission denial without a
fallback, unmap errors, retained first-failure diagnostics, and page/overflow
math. Linker-wrapped syscalls are test fixtures only; production uses real OS
calls. Host/LLVM tests, ASan/UBSan, desktop checks and the complete strict ARM64
link remain required. LeakSanitizer is disabled by the managed executor's ptrace
restriction, as in the existing SDK suite.

The corrected APK has not executed on the Galaxy. If an interval was already
occupied before loading the engine, the fix deliberately reports/fails instead
of changing the fixed compiler anchor or clobbering another VMA. The new export
is then the real blocker for a justified next change. No successful Galaxy
memory initialization, program-table import, Vulkan frame or gameplay is claimed.
