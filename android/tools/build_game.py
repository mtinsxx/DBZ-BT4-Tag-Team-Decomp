#!/usr/bin/env python3
"""Compile actual decompiled C to an Android ARM64 archive, then attempt a strict link.

No disc data, synthesized gameplay, missing-symbol stubs or on-device codegen.
Compilation diagnostics and the link outcome are retained in the build directory.
"""
from pathlib import Path
import argparse
import concurrent.futures
import json
import os
import subprocess
import sys
import traceback
from llvm_window import LLVM, ROOT, plugin, run, transform

sys.path.insert(0, str(ROOT / 'port/tools'))
import eeconst
import portsrc
import ptr32


def command(args, log, **kw):
    r = subprocess.run(list(map(str, args)), cwd=ROOT, capture_output=True, text=True, **kw)
    with log.open('a') as out:
        out.write('$ ' + ' '.join(map(str, args)) + '\n' + r.stdout + r.stderr)
    if r.returncode:
        raise RuntimeError(f'{args[0]} failed ({r.returncode}); see {log}')
    return r.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build/android-game')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--sources', nargs='+', help='explicit subset, relative to engine root')
    parser.add_argument('--compile-only', action='store_true', help='do not attempt the final strict link')
    parser.add_argument('--reuse-ir', action='store_true', help='reuse existing frontend IR (only when game sources, headers and frontend flags are unchanged)')
    parser.add_argument('--signatures-extra', type=Path, help='definitions from native runtime modules, collected before lowering')
    args = parser.parse_args()
    build = args.build.resolve()
    build.mkdir(parents=True, exist_ok=True)
    ndk = Path(os.environ['ANDROID_NDK_HOME'])
    ndk_bin = ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin'
    target = ['--target=aarch64-linux-android28', f'--sysroot={ndk_bin.parent}/sysroot']
    flags = [*target, '-std=gnu89', '-O2', '-fPIC', '-fcommon', '-fno-strict-aliasing',
             '-ffp-contract=off', '-fno-vectorize', '-fno-slp-vectorize', '-fno-stack-protector',
             '-fno-builtin', '-fms-extensions', '-w', '-Iinclude', '-Iport/include',
             '-include', 'port_libm.h', '-include', 'port_compat.h',
             '-Wno-error=return-mismatch', '-Wno-error=incompatible-function-pointer-types',
             '-Wno-error=int-conversion', '-Wno-error=incompatible-pointer-types',
             '-Wno-error=implicit-function-declaration', '-Wno-error=implicit-int']
    sources = [ROOT / s for s in args.sources] if args.sources else portsrc.sources()
    for source in sources:
        if not source.is_file() or not source.is_relative_to(ROOT / 'src'):
            parser.error(f'Not an engine C source: {source}')
    plugin(build)  # Build once before concurrent opt processes use it.

    def prepare(source):
        stem = source.relative_to(ROOT).as_posix().replace('/', '_')[:-2]
        log, pre, raw = (build / (stem + ext) for ext in ('.log', '.i', '.ll'))
        if (args.reuse_ir and raw.is_file() and raw.with_suffix('.defs').is_file()
                and raw.stat().st_mtime >= source.stat().st_mtime):
            return str(raw), None
        log.write_text('')
        try:
            prepared, _, missing = portsrc.prepare(source)
            if missing: raise RuntimeError('Missing decompilation C: ' + ', '.join(missing))
            text = command([LLVM / 'clang', *flags, '-I' + str(source.parent), '-E', prepared], log)
            pre.write_text(eeconst.transform(text))
            pre.write_text(ptr32.transform(str(pre), ['-x', 'cpp-output', *target, '-std=gnu89', '-fms-extensions', '-w']), encoding='latin-1')
            # Preprocessed input already contains the forced headers.
            clean, skip = [], False
            for flag in flags:
                if skip: skip = False
                elif flag == '-include': skip = True
                else: clean.append(flag)
            command([LLVM / 'clang', *clean, '-Xclang', '-disable-llvm-passes', '-S', '-emit-llvm',
                     '-x', 'cpp-output', pre, '-o', raw], log)
            transform(raw, raw, build, definitions=raw.with_suffix('.defs'))
            return str(raw), None
        except Exception:
            with log.open('a') as out: out.write(traceback.format_exc())
            return None, str(log)

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        prepared = list(pool.map(prepare, sources))
    failed = [error for _, error in prepared if error]
    raws = [Path(raw) for raw, _ in prepared if raw]
    sigs = build / 'signatures.txt'
    sigs.write_text(''.join(raw.with_suffix('.defs').read_text() for raw in raws))
    if args.signatures_extra:
        with sigs.open('a') as out: out.write(args.signatures_extra.read_text())

    def finish(raw):
        log, lowered, obj = raw.with_suffix('.log'), raw.with_suffix('.window.ll'), raw.with_suffix('.o')
        try:
            transform(raw, lowered, build, signatures=sigs)
            command([LLVM / 'llc', '-O2', '-filetype=obj', '-relocation-model=pic', lowered, '-o', obj], log)
            return str(obj), None
        except Exception:
            with log.open('a') as out: out.write(traceback.format_exc())
            return None, str(log)

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        compiled = list(pool.map(finish, raws))
    failed += [error for _, error in compiled if error]
    objects = [obj for obj, _ in compiled if obj]
    report = dict(target='arm64-v8a',api=28,requested=len(sources),compiled=len(objects),
                  failed=failed,subset=bool(args.sources),linked=False,gameplay_tested=False)
    archive = build / 'libbt4_game_code.a'
    archive.unlink(missing_ok=True)
    if objects: run([ndk_bin / 'llvm-ar', 'rcs', archive, *objects])
    (build / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    if failed:
        print(json.dumps(report, indent=2), flush=True)
        sys.exit('Game compilation is incomplete; no final library was linked.')
    if not args.compile_only:
        support = []
        for name in ('bt4_address_window', 'bt4_window_runtime', 'bt4_guest_abi'):
            obj = build / (name + '.o')
            command([ndk_bin / 'aarch64-linux-android28-clang', '-fPIC', '-O2',
                     '-Iandroid/include', '-Iport/src', '-c', ROOT / f'android/src/{name}.c', '-o', obj], build / 'link.log')
            support.append(str(obj))
        # A partial selection is explicitly labelled and never packaged as an APK.
        output = build / ('libbt4_game_subset.so' if args.sources else 'libtenkaichi_mobile.so')
        output.unlink(missing_ok=True)
        try:
            command([ndk_bin / 'aarch64-linux-android28-clang', '-shared', '-Wl,--no-undefined',
                     '-Wl,-Bsymbolic', '-Wl,-z,max-page-size=16384', '-Wl,--whole-archive', archive,
                     '-Wl,--no-whole-archive', *support, '-lm', '-ldl', '-llog', '-o', output], build / 'link.log')
            report['linked'] = True
        finally:
            (build / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
            print(json.dumps(report, indent=2), flush=True)
    print('Actual game source build completed. No APK or gameplay claim.', flush=True)


if __name__ == '__main__': main()
