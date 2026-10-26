#!/usr/bin/env python3
"""Compile real libc sources with modeled OS services; report known contract defects.

Default exit 1 means defects remain; --expect-known-defects checks the audit
receipt (exit 0 only for all four observations), not successful conformance.
No guest or host thread is created. No runtime source is patched.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expect-known-defects', action='store_true')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/review/sync-audit')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out / 'result.json').unlink(missing_ok=True)
    if platform.system() not in ('Darwin', 'Linux'):
        parser.error('this host probe supports Darwin/Linux toolchains')

    headers = [ROOT / 'userspace/lib/include' / name for name in ('pthread.h', 'semaphore.h', 'sched.h')]
    names = set()
    for header in headers:
        names.update(re.findall(r'\b((?:pthread|sem|sched)_[a-zA-Z0-9_]+)\s*\(', header.read_text()))
    names.update(('__errno_location', 'clock_gettime', 'sched_yield', 'mmap', 'munmap'))
    renamed = out / 'rename.h'
    renamed.write_text(''.join(f'#define {name} audit_{name}\n' for name in sorted(names)))
    sources = [ROOT / path for path in (
        'tools/audit_sync_contracts.c',
        'userspace/lib/src/posix_pthread.c',
        'userspace/lib/src/posix_semaphore.c')]
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    executable = out / 'sync-contracts'
    command = compiler + ['-std=c11', '-O0', '-g', '-Wall', '-Wextra', '-Werror',
                          '-I', str(ROOT / 'userspace/lib/include'),
                          '-I', str(ROOT / 'shared/include'), '-include', str(renamed)]
    command += [str(path) for path in sources] + ['-o', str(executable)]
    subprocess.run(command, check=True, cwd=ROOT)
    result = subprocess.run([str(executable)] + (['--expect-known-defects'] if args.expect_known_defects else []),
                            capture_output=True, text=True, timeout=15)
    print(result.stdout, end='')
    if result.stderr:
        print(result.stderr, end='')
    record = {'source': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT)),
              'platform': platform.platform(), 'compile_command': command,
              'expect_known_defects': args.expect_known_defects, 'exit': result.returncode,
              'stdout': result.stdout, 'stderr': result.stderr,
              'sha256': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                         for path in sources + headers}}
    (out / 'result.json').write_text(json.dumps(record, indent=2) + '\n')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
