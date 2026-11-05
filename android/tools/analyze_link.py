#!/usr/bin/env python3
"""Classify strict-link failures and trace SDK references to original tables."""
from pathlib import Path
import argparse
import json
import re

ROOT = Path(__file__).resolve().parents[2]


def analyze(log):
    text = Path(log).read_text()
    text = text[text.rfind('$ '):]  # Also accepts logs retained from older runs.
    names = sorted(set(re.findall(r'undefined symbol: (\S+)', text)))
    references = {name: [] for name in names}
    for source in sorted((ROOT / 'port/data').glob('*.s')):
        label = None
        for line in source.read_text().splitlines():
            match = re.match(r'^([A-Za-z_]\w*):', line)
            if match: label = match.group(1)
            match = re.match(r'\s*\.long\s+([A-Za-z_]\w*)', line)
            if match and match.group(1) in references:
                ref = dict(source=source.relative_to(ROOT).as_posix(), table=label)
                if ref not in references[match.group(1)]: references[match.group(1)].append(ref)
    return dict(abs32_errors=text.count('R_AARCH64_ABS32'),
                unresolved_count=len(names),
                unresolved=[dict(symbol=name, original_tables=references[name]) for name in names],
                other_errors=[line for line in text.splitlines() if line.startswith('ld.lld: error:') and 'undefined symbol:' not in line])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('log', type=Path)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    report = analyze(args.log)
    text = json.dumps(report, indent=2) + '\n'
    if args.out: args.out.write_text(text)
    else: print(text, end='')


if __name__ == '__main__': main()
