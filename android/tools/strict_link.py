#!/usr/bin/env python3
"""Repeat an existing complete link and compare its diagnostics to the 102-symbol baseline.

This only relinks already compiled objects. Rebuild changed sources with build_runtime.py.
An unresolved symbol remains a failed build, even if the baseline is unchanged.
"""
from pathlib import Path
import argparse
import json
import os
import shlex
import subprocess

from analyze_link import analyze
from sdk_audit import compare


def link(build, tools, report):
    build = Path(build).resolve()
    output = build / 'libtenkaichi_mobile.so'
    # Clear any successful output before validation, including missing-input failures.
    output.unlink(missing_ok=True)
    report.update(linked=False, gameplay_tested=False, stage='linking')
    report.pop('link_errors', None)
    report.pop('sdk_comparison', None)
    report.pop('link_driver_error', None)
    log = build / 'link.log'
    log.write_text('')
    code = 1
    try:
        if report['failed'] or report['compiled'] != report['requested']:
            raise ValueError('Refusing to link incomplete runtime objects.')
        response = build / 'link.rsp'
        args = shlex.split(response.read_text())
        if '-Wl,--no-undefined' not in args:
            raise ValueError('Strict link response is missing --no-undefined.')
        if '-o' not in args or Path(args[args.index('-o') + 1]).resolve() != output:
            raise ValueError('Link output must be this build directory/libtenkaichi_mobile.so.')
        for arg in args:
            if arg.endswith(('.o', '.a')) and not Path(arg).is_file():
                raise ValueError('Missing compiled input: ' + arg)
        process = subprocess.run([str(Path(tools) / 'aarch64-linux-android28-clang++'), '@' + str(response)],
                                 capture_output=True, text=True)
        log.write_text(process.stdout + process.stderr)
        code = process.returncode
        report['linked'] = code == 0 and output.is_file()
        if not report['linked'] and code == 0:
            raise ValueError('Linker returned success without producing the library.')
    except (OSError, ValueError) as error:
        report['link_driver_error'] = str(error)
        log.write_text(log.read_text() + f'link driver error: {error}\n')
    finally:
        errors = analyze(log)
        report['link_errors'] = errors
        available = 'link_driver_error' not in report and not errors['other_errors'] and (
            report['linked'] or errors['unresolved_count'] > 0)
        report['sdk_comparison'] = (dict(available=True, **compare(r['symbol'] for r in errors['unresolved']))
            if available else dict(available=False, baseline_count=102, remaining=[], resolved=[], new=[]))
        report.update(stage='linked' if report['linked'] else 'link_failed', link_exit_code=code)
        if not report['linked']:
            output.unlink(missing_ok=True)
        (build / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    return 0 if report['linked'] else code or 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    report = json.loads((args.build / 'report.json').read_text())
    tools = Path(os.environ['ANDROID_NDK_HOME']) / 'toolchains/llvm/prebuilt/linux-x86_64/bin'
    code = link(args.build, tools, report)
    delta = report['sdk_comparison']
    print(f'linked={report["linked"]}; comparison_available={delta["available"]}; remaining={len(delta["remaining"])}; resolved={len(delta["resolved"])}; new={len(delta["new"])}; exit={code}')
    raise SystemExit(code)


if __name__ == '__main__':
    main()
