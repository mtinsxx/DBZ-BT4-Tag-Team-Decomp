"""Keep 4-byte game tables, replacing ELF ABS32 pointers with startup fixups."""
from pathlib import Path
import hashlib
import re

POINTER = re.compile(r'^(\s*)\.long\s+([A-Za-z_]\w*)\s*(?:([+-])\s*(0x[\da-fA-F]+|\d+))?\s*$', re.M)
ABSOLUTE = {'D_3BE71C': 0x3be71c}


def prepare(source, build):
    source, build = Path(source), Path(build)
    key = hashlib.sha256(source.name.encode()).hexdigest()[:16]
    entries, declarations = [], set()

    def replace(match):
        indent, symbol, sign, number = match.groups()
        delta = (int(number, 0) * (-1 if sign == '-' else 1)) if number else 0
        if symbol in ABSOLUTE:
            return f'{indent}.long {ABSOLUTE[symbol] + delta:#x}'
        slot = f'__bt4_slot_{key}_{len(entries)}'
        declarations.add(symbol)
        entries.append((slot, symbol, delta))
        # Labels add no bytes and preserve all original structure offsets.
        return f'{indent}.globl {slot}\n{indent}.hidden {slot}\n{slot}:\n{indent}.long 0'

    assembly = POINTER.sub(replace, source.read_text())
    # Native startup imports only .space runs; symbolic pointer slots retain
    # their relocated values. This label contains no original game bytes.
    start = '__bt4_data_' + key
    assembly = assembly.replace('.p2align 4',
        f'.p2align 4\n.globl {start}\n.hidden {start}\n{start}:', 1)
    if re.search(r'^\s*\.long\s+[A-Za-z_]', assembly, re.M):
        raise ValueError(f'Unsupported symbolic .long expression in {source}')
    asm = build / ('data_' + source.name)
    asm.write_text(assembly + '\n.section .note.GNU-stack,"",%progbits\n')
    cfile = None
    if entries:
        cfile = build / ('data_' + source.stem + '.reloc.c')
        lines = ['#include "bt4_guest_abi.h"']
        lines += [f'extern const unsigned char {s}[];' for s in sorted(declarations)]
        lines += [f'extern uint32_t {slot};' for slot, _, _ in entries]
        lines += ['static const Bt4DataRelocation table[] __attribute__((section(".porthst"))) = {']
        lines += [f'{{ &{slot}, {symbol} + ({delta}) }},' for slot, symbol, delta in entries]
        lines += ['};', '__attribute__((constructor(104))) static void relocate(void) {',
                  'Bt4Android_ApplyRelocations(table, sizeof(table)/sizeof(table[0]));', '}']
        cfile.write_text('\n'.join(lines) + '\n')
    return asm, cfile, len(entries)


def import_table(root, build):
    """Native addresses and disc offsets, using the existing legal skeleton index."""
    where, checksum = {}, None
    for line in (root / 'port/data/index.txt').read_text().splitlines():
        p = line.split()
        if not p or p[0].startswith('#'):
            continue
        if p[0] == 'checksum':
            checksum = int(p[1], 16)
        else:
            where[p[0]] = (int(p[1]), int(p[2], 16))
    if checksum is None:
        raise ValueError('Missing skeleton checksum')
    declarations, records = [], []
    for name, (which, origin) in sorted(where.items()):
        source = root / 'port/data' / (name + '.s')
        start = '__bt4_data_' + hashlib.sha256(source.name.encode()).hexdigest()[:16]
        declarations.append(f'extern unsigned char {start}[];')
        offset = 0
        for line in source.read_text().splitlines():
            p = line.split()
            if not p:
                continue
            if p[0] in ('.space', '.zero'):
                size = int(p[1], 0)
                if p[0] == '.space':
                    records.append(f'{{ {start} + {offset}, {which}, {origin + offset}, {size} }},')
                offset += size
            elif p[0] == '.long':
                offset += 4
            elif p[0].startswith('.') and p[0] not in ('.section', '.p2align', '.globl'):
                raise ValueError(f'Unsupported import layout directive in {source}: {line}')
    output = build / 'game_data_imports.c'
    output.write_text('\n'.join(['#include "bt4_android_boot.h"', *declarations,
        'const Bt4GameDataRun bt4_game_data_runs[] __attribute__((section(".porthst"))) = {',
        *records, '};', f'const size_t bt4_game_data_count = {len(records)};',
        f'const uint64_t bt4_game_data_checksum = UINT64_C(0x{checksum:016x});']) + '\n')
    return output
