# Original 102 SDK link contracts (comparison baseline)

Current result after private analysis of the owner's verified main executable:
**16 undefined symbols remain**. See [SDK_RECONSTRUCTION.md](SDK_RECONSTRUCTION.md)
for the 85 actual function implementations, TLB scalar correction, contracts,
tests and intermediate strict-link results. The original inventory below stays
unchanged for name-by-name comparisons; it is not the current unresolved count.

Follow-up: [SDK investigation and private evidence generation](SDK_INVESTIGATION.md)
records the 2026-11-06 audit, all 15 table classifications, 103 pointer slots,
source-selection findings and the Android-only extraction workflow. The
baseline below is immutable; current dispositions are recorded separately.

Observed on 2026-11-05 with the Android LLVM bring-up pipeline.

All 274 original game translation units and all 116 runtime/vendor/Android
translation units compile for ARM64. The strict final link fails on the 102
symbols below; every remaining reference originates in a legacy SDK/CRI data
table. No ABS32 relocation or other linker error remains in that attempt.

These are not replaced by generated stubs, weak definitions, null entries or
guessed return values. The source contains table addresses but no verified
C implementations/signatures for these entries. A correct next step must
identify each table contract and connect a genuine native implementation,
or prove an obsolete table can be removed without changing game behavior.
Unidentified entries require additional static evidence from legally supplied
original program data or matching SDK documentation; addresses alone do not
establish semantics.

`sceCdSearchFile` was the remaining code-level call and now uses the real ISO9660
metadata reader. The other unresolved entries were deliberately retained.

Reproduce from the engine checkout:

```bash
python3 android/tools/build_game.py --build build/android-game --compile-only
python3 android/tools/build_dependencies.py --build build/android-dependencies
python3 android/tools/build_runtime.py --game-build build/android-game --build build/android-runtime
python3 android/tools/analyze_link.py build/android-runtime/link.log
```

The runtime command returns nonzero and records the full inventory in
`build/android-runtime/report.json`. This documented failure is not a passing
game build. APK/JNI/lifecycle and device testing remain subsequent work.

| Missing symbol | Original table labels |
| --- | --- |
| `dvCiGetFileSize` | `D_002D3FF8` |
| `dvCiOpen` | `D_002D3FF8` |
| `dvCiReqRd` | `D_002D3FF8` |
| `func_00100000` | `D_002EB190` |
| `func_0027BCD0` | `D_002D3FF8` |
| `func_0027BD48` | `D_002D3FF8` |
| `func_0027BDD8` | `D_002D3FF8` |
| `func_0027C0F0` | `D_002D3FF8` |
| `func_0027C148` | `D_002D3FF8` |
| `func_0027C1E0` | `D_002D3FF8` |
| `func_0027C3C0` | `D_002D3FF8` |
| `func_0027C4A8` | `D_002D3FF8` |
| `func_0027C4E0` | `D_002D3FF8` |
| `func_0027C4E8` | `D_002D3FF8` |
| `func_0027C680` | `D_002D3FF8` |
| `func_0027F7A0` | `D_002D9800` |
| `func_0027F7F8` | `D_002D9800` |
| `func_0027F810` | `D_002D9800` |
| `func_0027F9B8` | `D_002D9800` |
| `func_0027FA00` | `D_002D9800` |
| `func_0027FAC0` | `D_002D9800` |
| `func_0027FC80` | `D_002D9800` |
| `func_0027FCD0` | `D_002D9800` |
| `func_0027FD08` | `D_002D9800` |
| `func_0027FD40` | `D_002D9800` |
| `func_0027FDC0` | `D_002D9800` |
| `func_0027FE80` | `D_002D9800` |
| `func_002826A0` | `D_002DE590` |
| `func_00282758` | `D_002DE590` |
| `func_002827E8` | `D_002DE590` |
| `func_00282898` | `D_002DE590` |
| `func_00282930` | `D_002DE590` |
| `func_00282978` | `D_002DE590` |
| `func_00282AB8` | `D_002DE590` |
| `func_00282BA8` | `D_002DE590` |
| `func_00282CF8` | `D_002DE590` |
| `func_00283278` | `D_002DEA78` |
| `func_00283330` | `D_002DEA78` |
| `func_00283338` | `D_002DEA78` |
| `func_00283348` | `D_002DEA78` |
| `func_002833F8` | `D_002DEA78` |
| `func_00283448` | `D_002DEA78` |
| `func_00283648` | `D_002DEA78` |
| `func_00283818` | `D_002DEA78` |
| `func_00283A48` | `D_002DEA78` |
| `func_00283E98` | `D_002E2AB0` |
| `func_00283F50` | `D_002E2AB0` |
| `func_00283FE0` | `D_002E2AB0` |
| `func_00284090` | `D_002E2AB0` |
| `func_00284198` | `D_002E2AB0` |
| `func_00284298` | `D_002E2AB0` |
| `func_00284448` | `D_002E2AB0` |
| `func_002845C0` | `D_002E2AB0` |
| `func_00284730` | `D_002E2AB0` |
| `func_0028B7C0` | `D_002E6F68` |
| `func_0028B838` | `D_002E6F68` |
| `func_0028B850` | `D_002E6F68` |
| `func_0028B8F0` | `D_002E6F68` |
| `func_0028BD28` | `D_002E6F68` |
| `func_0028BDC8` | `D_002E6F68` |
| `func_0028BE60` | `D_002E6F68` |
| `func_0028C028` | `D_002E6F68` |
| `func_0028C120` | `D_002E6F68` |
| `func_0028C158` | `D_002E6F68` |
| `func_0028C160` | `D_002E6F68` |
| `func_0028C288` | `D_002E6F68` |
| `func_0029F2F8` | `D_002E7788` |
| `func_0029F370` | `D_002E77A8` |
| `func_0029F408` | `D_002E7788` |
| `func_0029F4C0` | `D_002E77A8` |
| `func_0029F590` | `D_002E7788` |
| `func_0029F640` | `D_002E77A8` |
| `func_0029F6F8` | `D_002E7788` |
| `func_0029F7F0` | `D_002E77A8` |
| `func_0029F8F0` | `D_002E7788` |
| `func_0029F990` | `D_002E77A8` |
| `func_0029FA40` | `D_002E7788` |
| `func_0029FB20` | `D_002E77A8` |
| `func_0029FC08` | `D_002E7788` |
| `func_0029FCE0` | `D_002E77A8` |
| `func_0029FDB0` | `D_002E7788` |
| `func_0029FED0` | `D_002E77A8` |
| `func_002A09D8` | `D_002FAA08` |
| `func_002A0B18` | `D_002FAA08` |
| `func_002A0C00` | `D_002FAA08` |
| `func_002A0C10` | `D_002FAA08` |
| `func_002A0C20` | `D_002FAA08` |
| `func_002A0E08` | `D_002FAA08` |
| `func_002A0E18` | `D_002FAA08` |
| `func_002A11E8` | `D_002FAA08` |
| `func_002A1278` | `D_002FAA08` |
| `func_002A1310` | `D_002FAA08` |
| `func_002BA2D0` | `D_002E9D50` |
| `func_002BA308` | `D_002E9D50` |
| `func_002BA518` | `D_002EA510` |
| `func_002BA688` | `D_002EA528` |
| `func_002BA938` | `D_002EAC98` |
| `func_002BC748` | `D_002EB080` |
| `htCiOpen` | `D_002E6F68` |
| `htCiReqRd` | `D_002E6F68` |
| `mfCiOpen` | `D_002D9800` |
| `mfCiReqRd` | `D_002D9800` |
