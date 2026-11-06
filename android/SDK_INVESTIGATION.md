# SDK investigation — 2026-11-06

## Result and evidence limits

The strict ARM64 link still has **102 distinct undefined symbols**. There are
**103 pointer slots in 15 tables**, because `func_002A0C10` occurs twice in
`D_002FAA08`, at offsets `0x00` and `0x18`. No implementation of any of these
102 symbols was found in `src/`, including disabled decompilation attempts.
No symbol has been aliased, stubbed, nulled or declared resolved by this work.

The complete baseline (names, original PS2 addresses and owning tables) is
[sdk-baseline.json](sdk-baseline.json). The initial diagnostics are recorded
in [SDK_BLOCKERS.md](SDK_BLOCKERS.md). Addresses of named entries come from
`config/symbol_addrs.txt`; `func_XXXXXXXX` encodes an address, not a signature.
`docs/decomp_guide.md` requires inspecting instructions and original call targets
before naming/reconstructing functions. That evidence is currently absent:
there is no `asm/cod/`, `disc/` executable or ISO in this checkout. The owner
confirmed that no generated assembly checkout is available.

## Classification of all 15 tables

"Candidate" below is a hypothesis, not a recovered ABI or implementation.
Offsets/counts are directly observable in the committed skeletons.

| Table | Distinct missing targets / slots | Classification and evidence | Native disposition |
| --- | --- | --- | --- |
| `D_002D3FF8` | 14 / 14 | DVD/file-driver dispatch; includes `dvCiGetFileSize`, `dvCiOpen`, `dvCiReqRd`; same slot pattern as host driver | High-level file I/O already supplied by `plat_file.c`; original driver ABI unverified |
| `D_002D9800` | 14 / 14 | `mfCi` file-driver dispatch (`mfCiOpen`, `mfCiReqRd`); memory-file backend is a candidate | No verified native caller; do not equate these methods to ADXF functions |
| `D_002DE590` | 9 / 9 | CRI-range object interface, 12-byte prefix then 9 callbacks, `0x2826A0..0x282CF8` | Exact class, signatures and purpose unknown |
| `D_002DEA78` | 9 / 9 | Same interface shape, `0x283278..0x283A48` | Exact class, signatures and purpose unknown |
| `D_002E2AB0` | 9 / 9 | Same interface shape, `0x283E98..0x284730` | Exact class, signatures and purpose unknown |
| `D_002E6F68` | 14 / 14 | Host/file-driver dispatch (`htCiOpen`, `htCiReqRd`); analogous to DVD driver | Native file I/O already exists; original driver ABI unverified |
| `D_002E7788` | 8 / 8 | Packed 8-method dispatch, alternating code ranges with following table | Codec/format specialization is a candidate; exact purpose unknown |
| `D_002E77A8` | 8 / 8 | Paired 8-method dispatch adjacent to preceding table | Exact purpose/signatures unknown |
| `D_002FAA08` | 10 / 11 | SDK-range dispatch with repeated method `0x2A0C10` | Exact interface unknown; repetition must be preserved |
| `D_002E9D50` | 2 / 2 | Callbacks at `+4`, `+12`, code `0x2BA308`, `0x2BA2D0`; near `_InitSys` range | Kernel/startup association is a candidate, not established |
| `D_002EA510` | 1 / 1 | Callback record, `+4 → 0x2BA518` | Exact registration/consumer unknown |
| `D_002EA528` | 1 / 1 | Callback record, `+4 → 0x2BA688` | Exact registration/consumer unknown |
| `D_002EAC98` | 1 / 1 | Callback record, `+4 → 0x2BA938` | Exact registration/consumer unknown |
| `D_002EB080` | 1 / 1 | Callback record, `+4 → 0x2BC748` | Exact registration/consumer unknown |
| `D_002EB190` | 1 / 1 | `+0x14 → 0x100000`; referenced by `D_002EB330+0x14` | Could be an original absolute value misidentified as a function; no evidence to reclassify it yet |

All except `D_002FAA08` are in `port/data/cod_1C3400.data.s`;
that table is in `port/data/cod_1F44C0.rodata.s`.

## Consumers, source selection and native backends

`sdk_audit.py` examined the **274 game and 116 runtime object files** with
LLVM nm, excluding generated table/fixup objects which deliberately reference
all skeleton data. It found **zero direct native references** to any of the 15
tables or the 102 missing targets. Across skeleton data, the only incoming
symbolic edge to these tables is `D_002EB330+0x14 → D_002EB190`.

This is useful evidence, **not proof of dead data**. `Port_LoadGameData` in
`port/src/plat_mem.c` imports `.space` values from the owner's executable.
Pointer arithmetic/interior accesses and imported data need separate analysis
before removing original tables. Generated relocation constructors currently
keep their 103 targets live; removing those references merely to get a green
link would conceal the unresolved contracts.

The actual source selection is `port/tools/portsrc.py` plus
`android/tools/build_runtime.py`. `src/cri/` contains only `adxt.c`:
`adxt_GetSfreq(ADXT*)` has a real C implementation at original `0x273CA0`, but
is outside the original game source selection. It is **not one of the 102**.
Adding it would introduce `ADXERR_CallErrFunc1` and `ADXSJD_GetSfreq`
dependencies, and its PS2 `ADXT` layout differs from the native audio player's
layout. It cannot be added or substituted blindly.

Existing native behavior is reached through `ADXF_*` in `plat_file.c`
(partition/archive IDs, sectors, host cache and status), `ADXT_*` in
`port/src/gs/snd_adx.c` (real decoder/SDL stream), native GS/VU render paths,
and bionic/SDL instead of the EE kernel and IOP. `sceCdSearchFile` already uses
the ISO9660 metadata reader. Named `*CiOpen`/`*CiReqRd` methods are lower-level
driver contracts; their names do not establish argument order, units, handles,
completion/error semantics or locking. No verified implementation was found
omitted from the Android build for any baseline symbol.

For every unresolved method, reconstruction still requires its instructions,
callers/indirect call sites, register/stack argument use, return paths, object
field offsets and global/string references. The available source does not
establish the 102 complete signatures. The audit records them as `null` rather
than generating C declarations from guesses.

## Private evidence generation from a legitimate copy

The original `configure.py` can extract and split **BT3 USA** with splat.
Its inputs are `disc/SLUS_216.78`, a generated flat ROM, and `disc/BIN/DBZP.BIN`.
It requires MIPS binutils under `tools/binutils/`, splat and the project's
extensions. A matching rebuild also requires EE GCC 2.96. Those PS2 tools and
commercial inputs are not installed by the Android cloud bootstrap.
Its YAML SHA-1 checks target original BT3. A BT4 `SLUS_219.78` must not be
renamed to bypass that identity check.

For the **102 SDK symbols alone**, the complete main ELF is sufficient:
`SLUS_219.78` from the owner's supported BT4 B14 Rev2 ISO, or original
`SLUS_216.78` from BT3 USA. The new private evidence path needs neither
EE GCC nor PS2 binutils. It uses the same R5900-capable disassembly stack
as splat (spimdisasm/Rabbitizer), with pinned versions:

```bash
cd /workspace/BT4-Native-Android
source tools/cloud/env.sh
python3 -m pip install -r engine/android/requirements-evidence.txt
python3 engine/android/tools/sdk_evidence.py --elf /path/to/SLUS_219.78
# Alternatively, read only the three base programs from an available BT4 ISO:
python3 engine/android/tools/sdk_evidence.py --iso /path/to/BT4.iso \
  --out /workspace/BT4-Native-Android/engine/build/sdk-evidence-iso
```

It verifies the main flat-image SHA-1 against the existing BT4 profiles or
original BT3 YAML, then checks **every one of the 103 original table words**
against the baseline address map. A differing version/map is rejected before
writing extracted output. ISO mode additionally verifies all three BT4 base
program hashes. ELF-only mode verifies the main executable, not all game data.

Output stays under ignored `engine/build/sdk-evidence/`: a flat image, a
symbol map, SDK-range assembly, and a private identity/slot report. This is a
targeted investigation slice `0x273CF0..0x2BF6B0`, not a matching splat build
or an assertion of exact function boundaries/signatures. `func_00100000`
lies outside the SDK slice and is deliberately not declared callable there.
Inspect its table and ELF/startup metadata separately after inputs arrive.
Disassembler-generated boundaries must be checked against callers, branches
and delay slots before reconstruction. **Do not commit, attach to public PRs,
or publish any generated image or assembly.** No Actions upload includes these.

### Extracting on Android

1. Open **your existing ISO** in an archive app capable of reading ISO9660
   (for example ZArchiver). Extract the root file `SLUS_219.78` unchanged to
   a private folder in Downloads. For original BT3 USA, it is `SLUS_216.78`.
   This avoids uploading the multi-gigabyte ISO.
2. For validating the complete BT4 program profile as well, also extract
   `BIN/DBZP.BIN` and `BIN/DBZ4.BIN`, keeping their names. They are not needed
   to inspect these main-executable SDK functions, but are needed for later
   overlay/profile work. Assets, AFS archives, movies and music are unnecessary
   for this investigation.
3. Supply the extracted executable through a private file attachment or private
   workspace transfer available in your client. Do not add it to the public
   GitHub repository, a PR attachment, a release, or a public Actions artifact.
   No file transfer or actual-disc analysis has occurred in this session.

If the archive app cannot read the ISO, extraction can also happen in the cloud
once the ISO is privately available, using `sdk_evidence.py --iso`; no desktop
computer or Android emulator is required. If the executable is not one of the
recognized profiles, retain its original name and hash and investigate that
version before applying this map.

## Reproduce the audit and strict link

From the main checkout after sourcing `tools/cloud/env.sh`:

```bash
python3 engine/android/tools/sdk_audit.py \
  --build build/game-arm64 --build build/runtime-arm64 \
  --nm "$BT3_LLVM/llvm-nm" --out build/sdk-audit.json
python3 engine/android/tools/strict_link.py --build build/runtime-arm64
```

`strict_link.py` only relinks existing objects; it does not rebuild changed
source. Its command returns nonzero for missing contracts and records names
resolved/remaining/new relative to the immutable baseline, plus other errors.
It validates strict flags/inputs, removes stale libraries on failure, and does
not claim any symbols resolved when the link driver cannot run. The full
builder now shares this path and invalidates stale output before preparation.

The main repository's **Complete ARM64 engine (strict link)** Actions workflow
bootstraps the pinned Debian 13 environment, tests LLVM/Android/desktop,
compiles the real engine/runtime and retains diagnostics only. **An unchanged
102-symbol link failure is a failed workflow**, not a passing game build.
The seven new host regressions cover slot positions/duplication, address-map
rejection, unchanged-count symbol replacement, a synthetic R5900 load/return
with delay slot, unknown program rejection and failed-link artifact cleanup.
Actual game disassembly remains untested until the owner's executable arrives.

Next prerequisite: privately provide the main executable. Then inspect actual
SDK instructions and consumers, reconstruct verified contracts or prove
specific obsolete records removable, rebuild affected ARM64 units and repeat
the strict link. JNI/Activity/Vulkan presentation and an experimental APK
remain blocked on the complete runtime and subsequent initialization/data work.
