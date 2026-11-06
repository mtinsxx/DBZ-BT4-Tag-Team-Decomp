#!/usr/bin/env python3
"""Privately disassemble the EE SDK range from the owner's verified executable.

This does not rename a BT4 executable to BT3, alter splat's original checksums,
claim exact function boundaries, or add proprietary bytes to source control.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys

from sdk_audit import ROOT, inventory
sys.path.insert(0, str(ROOT / 'port/tools'))
import bt4_disc

START, END, BASE = 0x273CF0, 0x2BF6B0, 0x100000


def identify_elf(elf):
    flat = bt4_disc.flat_image(elf)
    digest = hashlib.sha1(flat).hexdigest()
    known = {values[0]: 'bt4-b14-rev2-' + language for language, values in bt4_disc.PROFILES.items()}
    yaml = (ROOT / 'config/SLUS_216.78.yaml').read_text()
    known[re.search(r'^sha1: (\w+)', yaml, re.M)[1]] = 'bt3-usa-original'
    if digest not in known:
        raise ValueError('Unknown executable checksum. Keep the original file; do not rename or patch it to bypass this check.')
    return flat, known[digest], digest


def verify_slots(flat, audit):
    """Refuse address maps from incompatible versions, even after profile recognition."""
    checked = []
    for contract in audit['contracts']:
        for slot in contract['slots']:
            address = int(slot['table'][2:], 16) + slot['offset']
            offset = address - BASE
            if offset < 0 or offset + 4 > len(flat):
                raise ValueError(f'Table slot outside image: {address:#x}')
            actual = struct.unpack_from('<I', flat, offset)[0]
            expected = int(contract['ps2_address'], 16) + slot['addend']
            if actual != expected:
                raise ValueError(f'Address map mismatch: {slot["table"]}+{slot["offset"]:#x}: {actual:#x}, expected {expected:#x}')
            checked.append(dict(symbol=contract['symbol'], slot=f'0x{address:08X}', target=f'0x{actual:08X}'))
    return checked


def disassemble(flat_path, output, symbols):
    command = [sys.executable, '-m', 'spimdisasm', 'singleFileDisasm', str(flat_path), str(output),
        '--start', hex(START - BASE), '--end', hex(END - BASE), '--vram', hex(START),
        '--instr-category', 'r5900', '--endian', 'little', '--abi', 'EABI64', '--compiler', 'EEGCC',
        '--gp', '0x304270', '--symbol-addrs', str(symbols), '--no-libultra-syms',
        '--no-hardware-regs', '--no-default-banned', '--no-write-binary', '--quiet']
    subprocess.run(command, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument('--elf', type=Path, help='SLUS_219.78 (BT4) or SLUS_216.78 (original BT3 USA)')
    inputs.add_argument('--iso', type=Path, help='supported BT4 B14 Rev2 ISO; only the three base programs are read')
    parser.add_argument('--out', type=Path, default=ROOT / 'build/sdk-evidence')
    args = parser.parse_args()
    out = args.out.resolve()
    if out.is_relative_to(ROOT) and not out.is_relative_to(ROOT / 'build'):
        parser.error('Evidence inside the repository must stay under ignored build/.')
    if out.exists():
        parser.error('Output already exists; choose a new private output directory.')
    try:
        if args.iso:
            programs = bt4_disc.read_programs(args.iso)
            hashes = (hashlib.sha1(bt4_disc.flat_image(programs[bt4_disc.PROGRAMS[0]])).hexdigest(),
                      *(hashlib.sha1(programs[name]).hexdigest() for name in bt4_disc.PROGRAMS[1:]))
            if hashes not in bt4_disc.PROFILES.values():
                raise ValueError('Unsupported BT4 disc profile (base programs do not match).')
            elf = programs[bt4_disc.PROGRAMS[0]]
        else:
            if args.elf.stat().st_size > 16 * 1024 * 1024:
                raise ValueError('Unexpected executable size (maximum 16 MiB).')
            elf = args.elf.read_bytes()
        flat, profile, digest = identify_elf(elf)
        audit = inventory()
        checked = verify_slots(flat, audit)
        # No extracted output is written before identity AND all table positions pass.
        out.mkdir(parents=True)
        image = out / 'main.flat.bin'
        image.write_bytes(flat)
        symbols = out / 'sdk-symbols.txt'
        # 0x100000 lies outside this SDK slice and is not presumed to be callable.
        symbols.write_text(''.join(f'{r["symbol"]} = {r["ps2_address"]}; // type:func\n'
            for r in audit['contracts'] if START <= int(r['ps2_address'], 16) < END))
        disassemble(image, out / 'sdk', symbols)
        (out / 'identity.json').write_text(json.dumps(dict(profile=profile, main_flat_sha1=digest,
            slots=checked, bounds=[hex(START), hex(END)],
            warning='Disassembler boundaries/types are hypotheses. Inspect callers and delay slots before writing C.'), indent=2) + '\n')
        print(f'Private evidence: {out}; {profile}; {len(checked)} slots verified. Do not commit or publish its contents.')
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
