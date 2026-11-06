#!/usr/bin/env python3
"""Inventory SDK contracts without treating missing evidence as a C implementation."""
from pathlib import Path
import argparse
import collections
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BASELINE = ROOT / 'android/sdk-baseline.json'


def baseline():
    rows = json.loads(BASELINE.read_text())['unresolved']
    names = [r['symbol'] for r in rows]
    if len(names) != 102 or len(set(names)) != 102:
        raise ValueError('The original baseline must contain 102 distinct contracts')
    return rows


def compare(unresolved):
    original = {row['symbol'] for row in baseline()}
    current = set(unresolved)
    return dict(baseline_count=len(original), remaining=sorted(original & current),
                resolved=sorted(original - current), new=sorted(current - original))


def tables(data):
    """Read skeleton shape; offsets are table-relative, including padding/duplicate slots."""
    result = {}
    for source in sorted(Path(data).glob('*.s')):
        table, offset = None, 0
        for line_number, line in enumerate(source.read_text().splitlines(), 1):
            label = re.fullmatch(r'(\w+):', line.strip())
            if label:
                table, offset = label[1], 0
                if table in result:
                    raise ValueError(f'Duplicate table {table}')
                result[table] = dict(source=source.name, line=line_number, slots=[], words=[], size=0)
                continue
            if table is None or not line.strip() or line.startswith('#'):
                continue
            directive = re.fullmatch(r'\s*\.(space|zero|long)\s+(.+)', line)
            if directive:
                kind, value = directive.groups()
                if kind == 'long':
                    pointer = re.fullmatch(r'(\w+)(?:\s*([+-])\s*(0x[\da-fA-F]+|\d+))?', value)
                    if pointer and not pointer[1][0].isdigit():
                        delta = int(pointer[3], 0) * (-1 if pointer[2] == '-' else 1) if pointer[3] else 0
                        result[table]['slots'].append(dict(offset=offset, symbol=pointer[1], addend=delta, line=line_number))
                    else:
                        result[table]['words'].append(dict(offset=offset, value=int(value, 0), line=line_number))
                    offset += 4
                else:
                    offset += int(value, 0)
                result[table]['size'] = offset
            elif not re.match(r'\s*\.(globl|section|p2align)\b', line):
                raise ValueError(f'Unsupported skeleton directive: {source}:{line_number}: {line}')
            elif '.p2align' in line:
                raise ValueError(f'Alignment within a table needs explicit handling: {source}:{line_number}')
    return result


def object_references(builds, symbols, nm):
    refs = collections.defaultdict(list)
    count = 0
    for build in builds:
        # Generated fixups deliberately reference every table; they are not native consumers.
        objects = sorted(p for p in Path(build).glob('*.o') if not p.name.startswith('data_'))
        if not objects:
            raise ValueError(f'No native objects in {build}')
        for obj in objects:
            output = subprocess.check_output([nm, '--undefined-only', str(obj)], text=True)
            for line in output.splitlines():
                name = line.split()[-1] if line.split() else ''
                if name in symbols:
                    refs[name].append(obj.name)
        count += len(objects)
    return count, refs


def inventory(root=ROOT, builds=(), nm='llvm-nm'):
    rows = baseline()
    recovery_file = root / 'android/sdk-reconstruction.json'
    recovery = json.loads(recovery_file.read_text()) if recovery_file.exists() else dict(functions=[], data=[])
    recovered = {r['symbol']: r for r in recovery['functions']}
    if len(recovered) != len(recovery['functions']) or set(recovered) - {r['symbol'] for r in rows}:
        raise ValueError('Invalid reconstructed-function inventory')
    shape = tables(root / 'port/data')
    targets = {table for row in rows for table in row['tables']}
    count, refs = object_references(builds, targets | {r['symbol'] for r in rows}, nm) if builds else (0, {})
    incoming = collections.defaultdict(list)
    for table, entry in shape.items():
        for slot in entry['slots']:
            if slot['symbol'] in targets:
                incoming[slot['symbol']].append(dict(table=table, **slot))
    import sys
    sys.path.insert(0, str(root / 'port/tools'))
    import portsrc
    included = {str(p.relative_to(root)) for p in portsrc.sources()}
    candidates = collections.defaultdict(list)
    for path in sorted([*(root / 'src').rglob('*.c'), *(root / 'android/src').glob('*.c')]):
        text = path.read_text()
        for row in rows:
            name = row['symbol']
            for match in re.finditer(r'\b' + re.escape(name) + r'\s*\([^;{}]*\)\s*\{', text):
                candidates[name].append(dict(source=path.relative_to(root).as_posix(),
                    line=text.count('\n', 0, match.start()) + 1, included_in_game_build=str(path.relative_to(root)) in included,
                    included_in_runtime_build=path.parent == root / 'android/src'))
    contracts = []
    for row in rows:
        slots = []
        for table in row['tables']:
            entry = shape[table]
            for slot in entry['slots']:
                if slot['symbol'] == row['symbol']:
                    slots.append(dict(table=table, source='port/data/' + entry['source'], **slot))
        if not slots:
            # Verified TLB EntryHi scalar, not a native function relocation.
            if row['symbol'] == 'func_00100000':
                entry = shape['D_002EB190']
                word = next((w for w in entry['words'] if w['offset'] == 20 and w['value'] == 0x100000), None)
                if word:
                    slots.append(dict(table='D_002EB190', source='port/data/' + entry['source'],
                        symbol=row['symbol'], offset=20, addend=0, line=word['line'], classification='tlb_entry_hi_literal'))
            if not slots:
                raise ValueError(f'Baseline symbol disappeared without an audited disposition: {row["symbol"]}')
        reconstruction = recovered.get(row['symbol'])
        if reconstruction and not any(c['source'] == reconstruction['source'] for c in candidates[row['symbol']]):
            raise ValueError(f'Reconstructed function has no source definition: {row["symbol"]}')
        contracts.append(dict(**row, slots=slots, c_candidates=candidates[row['symbol']],
            native_consumers=refs.get(row['symbol'], []),
            signature=reconstruction['native_signature'] if reconstruction else None,
            signature_kind='native_adapter' if reconstruction else None,
            reconstruction=reconstruction, implementation_verified=bool(reconstruction)))
    return dict(schema=1, baseline_count=len(rows), slot_count=sum(len(c['slots']) for c in contracts),
        pointer_slot_count=sum(1 for c in contracts for s in c['slots'] if not s.get('classification')),
        native_objects_examined=count, asm_cod_present=(root / 'asm/cod').is_dir(),
        table_count=len(targets), contracts=contracts,
        tables={t: dict(**shape[t], native_consumers=refs.get(t, []), incoming=incoming[t]) for t in sorted(targets)},
        caveat='No direct object reference is not proof of unreachability: pointer arithmetic and imported disc values require separate evidence.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', action='append', type=Path, default=[])
    parser.add_argument('--nm', default='llvm-nm')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    report = inventory(builds=args.build, nm=args.nm)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + '\n')
    print(f'{report["baseline_count"]} contracts, {report["slot_count"]} slots, {report["table_count"]} tables; {report["native_objects_examined"]} objects examined')


if __name__ == '__main__':
    main()
