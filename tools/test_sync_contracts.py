#!/usr/bin/env python3
"""Run focused synchronization regressions against real libc sources and modeled OS services.

Most OS services are modeled. One native host worker forces child completion
before the modeled create returns. Guest threadstress supplies kernel tests.
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
    parser.add_argument('--output', type=Path, default=ROOT / 'build/review/sync-regression')
    parser.add_argument('--audit-thread-names', action='store_true',
                        help='run the separate known-failing thread-name round-trip audit')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out / 'result.json').unlink(missing_ok=True)
    if not platform.system().startswith(('Darwin', 'Linux', 'Windows', 'MSYS', 'MINGW')):
        parser.error('this host probe needs a GCC/Clang-style C toolchain')

    headers = [ROOT / 'userspace/lib/include' / name for name in ('pthread.h', 'semaphore.h', 'sched.h', 'limits.h')]
    names = set()
    for header in headers:
        names.update(re.findall(r'\b((?:pthread|sem|sched)_[a-zA-Z0-9_]+)\s*\(', header.read_text()))
    names.update(('__errno_location', 'clock_gettime', 'sched_yield', 'mmap', 'munmap'))
    renamed = out / 'rename.h'
    renamed.write_text(''.join(f'#define {name} audit_{name}\n' for name in sorted(names)))
    sources = [ROOT / path for path in (
        'tools/test_sync_contracts.c',
        'userspace/lib/src/posix_pthread.c',
        'userspace/lib/src/posix_semaphore.c')]
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    shim = ROOT / 'tools/test_sync_host_threads.c'
    shim_object = out / 'host-threads.o'
    shim_command = compiler + ['-std=c11', '-O0', '-g', '-Wall', '-Wextra', '-Werror',
                               '-pthread', '-c', str(shim), '-o', str(shim_object)]
    subprocess.run(shim_command, check=True, cwd=ROOT)
    executable = out / 'sync-contracts'
    command = compiler + ['-std=c11', '-O0', '-g', '-Wall', '-Wextra', '-Werror',
                          '-I', str(ROOT / 'userspace/lib/include'),
                          '-I', str(ROOT / 'shared/include'), '-include', str(renamed)]
    command += [str(path) for path in sources] + [str(shim_object), '-pthread', '-o', str(executable)]
    subprocess.run(command, check=True, cwd=ROOT)
    results = []
    modes = ('thread-names',) if args.audit_thread_names else (None, 'supplied', 'owned', 'detached', 'lifecycle')
    for mode in modes:
        result = subprocess.run([str(executable)] + ([mode] if mode else []),
                                capture_output=True, text=True, timeout=15)
        print(result.stdout, end='')
        if result.stderr:
            print(result.stderr, end='')
        results.append({'mode': mode or 'contracts', 'exit': result.returncode,
                        'stdout': result.stdout, 'stderr': result.stderr})
    exit_code = next((r['exit'] for r in results if r['exit']), 0)
    record = {'source': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT)),
              'platform': platform.platform(), 'compile_command': command, 'shim_compile_command': shim_command,
              'exit': exit_code, 'cases': results,
              'sha256': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                         for path in sources + headers + [shim]}}
    (out / 'result.json').write_text(json.dumps(record, indent=2) + '\n')
    return exit_code


if __name__ == '__main__':
    raise SystemExit(main())
