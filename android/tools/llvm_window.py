"""Build-time ELF guest-pointer lowering; never runs a compiler on the phone."""
from pathlib import Path
import os
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'port/tools/macwin/macwin.cpp'
LLVM = Path(os.environ['BT3_LLVM'])
BITCAST = re.compile(r'^(\s*%[\w.$-]+ = )bitcast (ptr(?: addrspace\(27[12]\))? \S+) to (ptr(?: addrspace\(27[12]\))?)(?=,|$)', re.M)


def run(command):
    subprocess.run(list(map(str, command)), check=True)


def plugin(build):
    output = Path(build) / 'bt4_window.so'
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.stat().st_mtime < SOURCE.stat().st_mtime:
        flags = shlex.split(subprocess.check_output([
            LLVM / 'llvm-config', '--cxxflags', '--ldflags', '--libs', '--system-libs'], text=True))
        run([LLVM / 'clang++', '-shared', '-fPIC', '-O2', SOURCE, '-o', output, *flags])
    return output


def transform(source, output, build, *, signatures=None, port=False, native=False, definitions=None):
    source, output = Path(source), Path(output)
    fixed = source.with_suffix('.valid.ll')
    fixed.write_text(BITCAST.sub(r'\1addrspacecast \2 to \3', source.read_text()))
    args = [LLVM / 'opt', '-load-pass-plugin', plugin(build), '-bt3-elf',
            '-bt3-wrap=Progress_Main', '-bt3-abs=D_3BE71C=0x3be71c']
    if native: args += ['-bt3-native']
    if definitions:
        args += ['-passes=bt3-window,verify', f'-bt3-defs={definitions}', '-disable-output', fixed]
    else:
        # verify-each re-verifies the ENTIRE module after every function pass:
        # quadratic work on the C++ UI/JSON modules. Verify both boundaries.
        args += ['-passes=bt3-window,verify,default<O2>,verify']
        if signatures: args += [f'-bt3-sigs={signatures}']
        if port: args += ['-bt3-port']
        args += ['-S', fixed, '-o', output]
    run(args)
