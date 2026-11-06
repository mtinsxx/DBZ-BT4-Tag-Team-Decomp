#!/usr/bin/env python3
"""Build the original runtime/renderer and attempt the complete strict Android link."""
from pathlib import Path
import argparse
import concurrent.futures
import json
import os
import subprocess
import sys
import traceback
from llvm_window import ROOT, LLVM, plugin, run, transform
from build_game import command
sys.path.insert(0, str(ROOT / 'port/tools'))
import eeconst
import ptr32
import elf_data
from strict_link import link


def shaders(build):
    arrays = []
    sources = list(sorted((ROOT / 'port/src/gs/shaders').glob('*.*')))
    for name, define in [('gsnz', '#define NO_FRAG_DEPTH\n'), ('gswb', '#define NO_FRAG_DEPTH\n#define GS_BLOCK\n')]:
        path = build / f'{name}.frag'
        path.write_text((ROOT / 'port/src/gs/shaders/gs.frag').read_text().replace('\n', '\n' + define, 1))
        sources.append(path)
    for source in sources:
        spv = build / (source.name + '.spv')
        run(['glslangValidator', '-V', source, '-o', spv])
        run(['spirv-val', '--target-env', 'vulkan1.0', spv])
        name = 'k' + source.stem.capitalize() + source.suffix[1:].capitalize()
        for suffix, data in [('Spv', spv.read_bytes()), ('Glsl', source.read_bytes() + b'\0')]:
            arrays.append(f'static const unsigned char {name}{suffix}[] = {{' + ','.join(map(str, data)) + '};\n')
    (build / 'shaders.h').write_text(''.join(arrays))
    (build / 'bars_png.h').write_text(''.join(f'static const unsigned char kBars{name.capitalize()}Png[] = {{' +
        ','.join(map(str, (ROOT / f'port/assets/bars/{name}.png').read_bytes())) + '};\n' for name in ('trim', 'fill')))
    (build / 'port_version.h').write_text('#define PORT_VERSION "dev"\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-build', type=Path, required=True)
    parser.add_argument('--build', type=Path, default=ROOT / 'build/android-runtime')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--reuse-ir', action='store_true', help='only when source/header/frontend inputs have not changed')
    args = parser.parse_args()
    build, game = args.build.resolve(), args.game_build.resolve()
    build.mkdir(parents=True, exist_ok=True)
    # A failed rebuild must never leave a previous successful library/report.
    (build / 'libtenkaichi_mobile.so').unlink(missing_ok=True)
    (build / 'report.json').write_text(json.dumps(dict(stage='preparing', requested=0,
        compiled=0, failed=['Build preparation has not completed'], linked=False, gameplay_tested=False), indent=2) + '\n')
    game_report = json.loads((game / 'report.json').read_text())
    if game_report['subset'] or game_report['failed'] or game_report['compiled'] != game_report['requested']:
        sys.exit('A complete successfully compiled game archive is required, not a subset or partial archive.')
    ndk = Path(os.environ['ANDROID_NDK_HOME'])
    tools = ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin'
    prefix = Path(os.environ['BT4_ANDROID_PREFIX'])
    target = ['--target=aarch64-linux-android28', f'--sysroot={tools.parent}/sysroot']
    # The game's include/sys/file.h must not shadow Android <sys/file.h>.
    includes = ['-iquote' + str(ROOT / 'include')] + ['-I' + str(p) for p in (ROOT / 'port/include', ROOT / 'port/src',
        ROOT / 'port/src/gs', ROOT / 'port/third_party/imgui', ROOT / 'android/include', prefix / 'include', build)]
    common = [*target, '-O2', '-fPIC', '-fcommon', '-fno-strict-aliasing', '-ffp-contract=off',
              '-fno-vectorize', '-fno-slp-vectorize', '-fno-builtin', '-fms-extensions', '-w', *includes,
              '-Wno-error=return-mismatch', '-Wno-error=incompatible-function-pointer-types',
              '-Wno-error=int-conversion', '-Wno-error=incompatible-pointer-types',
              '-Wno-error=implicit-function-declaration', '-Wno-error=implicit-int']
    shaders(build)
    plugin(build)
    sources = []
    for source in sorted((ROOT / 'port/src').glob('*.c')):
        if source.name not in ('tag_team.c', 'tag_team_wire.c'):
            sources.append((source, source.name not in ('plat_libm.c', 'plat_fastvec.c')))
    sources += [(p, True) for p in (ROOT / 'src/port/vu0_a.c', ROOT / 'src/port/vu0_b.c')]
    sources += [(p, True) for p in sorted((ROOT / 'port/third_party/newlib_libm').glob('*.c'))]
    sources += [(p, False) for p in sorted((ROOT / 'port/src/gs').glob('*.c'))]
    sources += [(p, False) for p in sorted((ROOT / 'port/src/gs').glob('*.cpp'))]
    sources += [(p, False) for p in sorted((ROOT / 'port/third_party/imgui').glob('*.cpp')) if p.name != 'imgui_impl_dx9.cpp']
    sources += [(p, False) for p in sorted((ROOT / 'android/src').glob('*.c'))]

    def prepare(item):
        source, soft = item
        name = source.relative_to(ROOT).as_posix().replace('/', '_').rsplit('.', 1)[0]
        raw, pre, log = (build / (name + ext) for ext in ('.ll', '.i', '.log'))
        cpp = source.suffix == '.cpp'
        flags = [*common, '-std=c++17' if cpp else '-std=gnu99']
        if cpp:
            flags += ['-fno-exceptions', '-fno-rtti', '-DIMGUI_ENABLE_TEST_ENGINE',
                      '-isystem', str(tools.parent / 'sysroot/usr/include/c++/v1')]
        if soft:
            if source.parent.name == 'newlib_libm':
                flags += ['-I' + str(source.parent), '-include', 'newlib_shim.h']
            elif source.parent == ROOT / 'src/port':
                flags += ['-include', 'vu0_names.h', '-DREF_VU0_EXTERN_ARITH', '-include', 'port_libm.h']
            else:
                flags += ['-DSF_FAST_CALLS', '-include', 'math.h', '-include', 'stdlib.h', '-include', 'port_libm.h']
        if (args.reuse_ir and raw.exists() and raw.with_suffix('.defs').exists()
                and raw.stat().st_mtime >= source.stat().st_mtime):
            transform(raw, raw, build, native=not soft, definitions=raw.with_suffix('.defs'))
            return raw, soft, None
        log.write_text('')
        try:
            if soft:
                pre.write_text(eeconst.transform(command([LLVM / 'clang', *flags, '-E', source], log)))
                if source.parent.name != 'newlib_libm':
                    pre.write_text(ptr32.transform(str(pre), ['-x', 'cpp-output', *target, '-std=gnu99', '-fms-extensions', '-w']), encoding='latin-1')
                # Forced headers are already present in the preprocessed input.
                clean, skip = [], False
                for flag in flags:
                    if skip: skip = False
                    elif flag == '-include': skip = True
                    else: clean.append(flag)
                source, flags = pre, clean
            command([LLVM / ('clang++' if cpp else 'clang'), *flags, '-Xclang', '-disable-llvm-passes',
                     '-S', '-emit-llvm', source, '-o', raw], log)
            transform(raw, raw, build, native=not soft, definitions=raw.with_suffix('.defs'))
            return raw, soft, None
        except Exception:
            with log.open('a') as out: out.write(traceback.format_exc())
            return None, soft, str(log)

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        prepared = list(pool.map(prepare, sources))
    failed = [error for _, _, error in prepared if error]
    native_sigs = build / 'native-signatures.txt'
    native_sigs.write_text(''.join(raw.with_suffix('.defs').read_text() for raw, _, error in prepared if not error))
    sigs = build / 'signatures.txt'
    sigs.write_text(''.join(path.read_text() for path in sorted(game.glob('src_*.defs'))) + native_sigs.read_text())

    def finish(item):
        raw, soft, error = item
        if error: return None, None
        output, obj, log = raw.with_suffix('.window.ll'), raw.with_suffix('.o'), raw.with_suffix('.log')
        try:
            transform(raw, output, build, port=True, native=not soft, signatures=sigs)
            command([LLVM / 'llc', '-O2', '-filetype=obj', '-relocation-model=pic', output, '-o', obj], log)
            return obj, None
        except Exception:
            with log.open('a') as out: out.write(traceback.format_exc())
            return None, str(log)

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        compiled = list(pool.map(finish, prepared))
    failed += [error for _, error in compiled if error]
    objects = [obj for obj, _ in compiled if obj]
    # Legal skeleton data contains sizes/labels, not extracted commercial data.
    for source in sorted((ROOT / 'port/data').glob('*.s')):
        obj = build / ('data_' + source.stem + '.o')
        assembly, relocation, _ = elf_data.prepare(source, build)
        command([tools / 'aarch64-linux-android28-clang', '-c', assembly, '-o', obj], build / 'data.log')
        objects.append(obj)
        if relocation:
            reloc_obj = relocation.with_suffix('.o')
            command([tools / 'aarch64-linux-android28-clang', '-fPIC', '-O2',
                     '-I' + str(ROOT / 'android/include'), '-c', relocation, '-o', reloc_obj], build / 'data.log')
            objects.append(reloc_obj)
    imports = elf_data.import_table(ROOT, build)
    imports_obj = build / 'game_data_imports.o'
    command([tools / 'aarch64-linux-android28-clang', '-O2', '-fPIC',
             '-I' + str(ROOT / 'android/include'), '-c', imports, '-o', imports_obj], build / 'data.log')
    objects.append(imports_obj)
    report = dict(requested=len(sources), compiled=sum(bool(obj) for obj, _ in compiled),
                  failed=failed, linked=False, gameplay_tested=False)
    (build / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    if failed:
        print(json.dumps(report, indent=2), flush=True)
        sys.exit('Runtime compilation incomplete. No final shared library was linked.')
    # Calls in game code must use the runtime's actual signatures too.
    run([sys.executable, ROOT / 'android/tools/build_game.py', '--build', game, '--reuse-ir',
         '--compile-only', '--signatures-extra', native_sigs, '--jobs', args.jobs])
    response = build / 'link.rsp'
    output = build / 'libtenkaichi_mobile.so'
    link_args = ['-shared', '-Wl,--no-undefined', '-Wl,-Bsymbolic', '-Wl,-z,max-page-size=16384',
                 '-Wl,--error-limit=0', '-Wl,--whole-archive', str(game / 'libbt4_game_code.a'),
                 '-Wl,--no-whole-archive', *map(str, objects), '-L' + str(prefix / 'lib'),
                 '-lSDL3', '-lcurl', '-lssl', '-lcrypto', '-lz', '-ldl', '-llog', '-landroid', '-lm', '-lGLESv3',
                 '-static-libstdc++', '-o', str(output)]
    response.write_text('\n'.join('"' + a.replace('\\', '\\\\').replace('"', '\\"') + '"' for a in link_args))
    code = link(build, tools, report)
    print(json.dumps(report, indent=2), flush=True)
    if code:
        sys.exit(code)


if __name__ == '__main__': main()
