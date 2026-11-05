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
