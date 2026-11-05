#!/usr/bin/env python3
"""Execute transformed code on Linux; separately emit an Android ARM64 object."""
from pathlib import Path
import argparse
import os
import subprocess
import sys
from llvm_window import LLVM, ROOT, run, transform, plugin
sys.path.insert(0, str(ROOT / 'port/tools'))
import ptr32
import elf_data


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, default=ROOT / 'build/android-llvm-tests')
    args = parser.parse_args()
    build = args.build.resolve()
    build.mkdir(parents=True, exist_ok=True)
    assembly, relocations, count = elf_data.prepare(ROOT / 'android/tests/llvm_data.s', build)
    assert count == 2
    # Exercise the original list implementation, not a replacement implementation.
    list_pre = build / 'list.i'
    list_pre.write_text(subprocess.check_output([LLVM / 'clang', '-E', '-I' + str(ROOT / 'include'),
        '-include', ROOT / 'port/include/port_compat.h', ROOT / 'src/sys/list.c'], text=True))
    list_pre.write_text(ptr32.transform(str(list_pre), ['-x', 'cpp-output', '-std=gnu89', '-fms-extensions', '-w']), encoding='latin-1')
    run([LLVM / 'clang', '-O2', '-fPIC', '-fno-strict-aliasing', '-fms-extensions',
         '-Xclang', '-disable-llvm-passes', '-S', '-emit-llvm', list_pre, '-o', build / 'list.ll'])
    transform(build / 'list.ll', build / 'list.window.ll', build)
    run([LLVM / 'llc', '-O2', '-filetype=obj', '-relocation-model=pic', build / 'list.window.ll', '-o', build / 'list.o'])
    for target in ('host', 'android'):
        flags = []
        if target == 'android':
            ndk = Path(os.environ['ANDROID_NDK_HOME'])
            flags = ['--target=aarch64-linux-android28', f'--sysroot={ndk}/toolchains/llvm/prebuilt/linux-x86_64/sysroot']
        source = build / f'{target}.ll'
        output = build / f'{target}.window.ll'
        obj = build / f'{target}.o'
        data_obj, fixup_obj = build / f'{target}.data.o', build / f'{target}.fixup.o'
        run([LLVM / 'clang', *flags, '-c', assembly, '-o', data_obj])
        run([LLVM / 'clang', *flags, '-fPIC', '-I' + str(ROOT / 'android/include'), '-c', relocations, '-o', fixup_obj])
        reloc_text = subprocess.check_output([LLVM / 'llvm-readelf', '-r', data_obj, fixup_obj], text=True)
        assert 'R_AARCH64_ABS32' not in reloc_text and 'R_X86_64_32 ' not in reloc_text, reloc_text
        assert '.porthst' in subprocess.check_output([LLVM / 'llvm-readelf', '-S', fixup_obj], text=True)
        run([LLVM / 'clang', *flags, '-O2', '-fPIC', '-fms-extensions', '-fno-strict-aliasing',
             '-Xclang', '-disable-llvm-passes', '-S', '-emit-llvm', ROOT / 'android/tests/llvm_guest.c', '-o', source])
        other = build / f'{target}.other.ll'
        run([LLVM / 'clang', *flags, '-O2', '-fPIC', '-Xclang', '-disable-llvm-passes',
             '-S', '-emit-llvm', ROOT / 'android/tests/llvm_other.c', '-o', other])
        transform(source, output, build, definitions=source.with_suffix('.defs'))
        transform(other, other, build, definitions=other.with_suffix('.defs'))
        native = build / f'{target}.native.ll'
        run([LLVM / 'clang', *flags, '-O2', '-fPIC', '-Xclang', '-disable-llvm-passes',
             '-S', '-emit-llvm', ROOT / 'android/tests/llvm_native.c', '-o', native])
        transform(native, native, build, native=True, definitions=native.with_suffix('.defs'))
        sigs = build / f'{target}.signatures'
        sigs.write_text(source.with_suffix('.defs').read_text() + other.with_suffix('.defs').read_text() + native.with_suffix('.defs').read_text())
        transform(source, output, build, signatures=sigs)
        other_output = build / f'{target}.other.window.ll'
        transform(other, other_output, build, signatures=sigs)
        other_obj = build / f'{target}.other.o'
        run([LLVM / 'llc', '-O2', '-filetype=obj', '-relocation-model=pic', other_output, '-o', other_obj])
        native_output, native_obj = build / f'{target}.native.window.ll', build / f'{target}.native.o'
        transform(native, native_output, build, port=True, native=True, signatures=sigs)
        run([LLVM / 'llc', '-O2', '-filetype=obj', '-relocation-model=pic', native_output, '-o', native_obj])
        assert '@bt4.native.native_scale' in output.read_text()
        assert '@bt4.native.native_variadic' in output.read_text()
        run([LLVM / 'llc', '-O2', '-filetype=obj', '-relocation-model=pic', output, '-o', obj])
        if target == 'host':
            exe = build / 'guest-abi-test'
            run([LLVM / 'clang', '-g', '-fPIE', '-pie', ROOT / 'android/tests/llvm_host.c',
                 ROOT / 'android/src/bt4_guest_abi.c', '-I' + str(ROOT / 'android/include'),
                 '-I' + str(ROOT / 'port/src'), obj, other_obj, native_obj, build / 'list.o', data_obj, fixup_obj, '-ldl', '-o', exe])
            # Fresh processes exercise distinct loader-selected bases.
            for _ in range(3): run([exe])
    run([LLVM / 'llvm-readelf', '-h', build / 'android.o'])
    # The new ELF switch must leave the original Mach-O lowering available.
    mac = build / 'macos.ll'
    run([LLVM / 'clang', '--target=arm64-apple-macos11', '-O2', '-fPIC', '-fms-extensions',
         '-Xclang', '-disable-llvm-passes', '-S', '-emit-llvm', ROOT / 'android/tests/llvm_macos.c', '-o', mac])
    run([LLVM / 'opt', '-load-pass-plugin', plugin(build), '-passes=bt3-window,default<O2>',
         '-verify-each', '-S', mac, '-o', build / 'macos.window.ll'])
    run([LLVM / 'llc', '-O2', '-filetype=obj', build / 'macos.window.ll', '-o', build / 'macos.o'])
    run([LLVM / 'llvm-readobj', '--file-headers', build / 'macos.o'])


if __name__ == '__main__': main()
