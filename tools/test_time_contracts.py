#!/usr/bin/env python3
"""Compile real libc timing functions with prefixed symbols and modeled OS services."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/review/time-regression')
    parser.add_argument('--audit-times-fallback', action='store_true',
                        help='Run the isolated known-failure elapsed-time fallback audit')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out / 'result.json').unlink(missing_ok=True)
    names = ('__errno_location time clock times utime clock_gettime clock_getres '
             'nanosleep clock_nanosleep gettimeofday utimes sleep usleep utimensat').split()
    renamed = out / 'rename.h'
    renamed.write_text(''.join(f'#define {name} audit_{name}\n' for name in names))
    sources = [ROOT / path for path in ('tools/test_time_contracts.c', 'userspace/lib/src/posix_time.c')]
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    executable = out / 'time-contracts'
    command = compiler + ['-std=c11', '-O0', '-g', '-Wall', '-Wextra', '-Werror',
                          '-I', str(ROOT / 'userspace/lib/include'),
                          '-I', str(ROOT / 'shared/include'), '-include', str(renamed)]
    command += [str(path) for path in sources] + ['-o', str(executable)]
    subprocess.run(command, check=True, cwd=ROOT)
    mode = ['times-fallback-audit'] if args.audit_times_fallback else []
    result = subprocess.run([str(executable)] + mode, capture_output=True, text=True, timeout=15)
    print(result.stdout, end='')
    print(result.stderr, end='')
    record = {'source': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT)),
              'platform': platform.platform(), 'compile_command': command,
              'mode': mode[0] if mode else 'contracts', 'exit': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr,
              'sha256': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                         for path in sources}}
    (out / 'result.json').write_text(json.dumps(record, indent=2) + '\n')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
