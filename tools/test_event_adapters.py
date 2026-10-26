#!/usr/bin/env python3
"""Audit real eventfd/epoll adapters; --audit runs explicitly failing contract probes.

The adapter prefix and public close() are copied verbatim, with #line records.
Native descriptors/poll/sleep are modeled; this does not test kernel scheduling.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
AUDITS = ('close-capacity', 'instance-reuse', 'watched-reuse', 'zero-timeout',
          'ready-wait', 'counter-overflow', 'reserved-value')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--audit', choices=('all',) + AUDITS)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/review/event-adapters')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out / 'result.json').unlink(missing_ok=True)
    adapter = ROOT / 'userspace/lib/src/posix_node_compat.c'
    file_source = ROOT / 'userspace/lib/src/posix_file.c'
    fixture = ROOT / 'tools/test_event_adapters.c'
    text = adapter.read_text()
    assert text.count('\nint inotify_init1(') == 1
    prefix = text[:text.index('\nint inotify_init1(')]
    text = file_source.read_text()
    start = text.index('int close(int fd) {')
    end = text.index('\nint fsync(', start)
    declarations = ('int __posix_socket_is_pseudo(int);\nint __posix_socket_close(int);\n'
                    'void __posix_socket_note_close(int);\n')
    extracted = out / 'adapter.c'
    close_line = text[:start].count('\n') + 1
    extracted.write_text(f'#line 1 "{adapter}"\n' + prefix + '\n' + declarations +
                         f'#line {close_line} "{file_source}"\n' + text[start:end])
    names = ('__errno_location eventfd epoll_create1 epoll_ctl epoll_wait epoll_pwait '
             'close pipe poll fcntl usleep getenv fprintf stderr').split()
    renamed = out / 'rename.h'
    renamed.write_text(''.join(f'#define {name} audit_{name}\n' for name in names))
    executable = out / 'event-adapters'
    command = shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c11', '-O0', '-g', '-Wall', '-Wextra', '-Werror',
        '-I', str(ROOT / 'userspace/lib/include'), '-I', str(ROOT / 'shared/include'),
        '-include', str(renamed), str(extracted), str(fixture), '-o', str(executable)]
    subprocess.run(command, check=True, cwd=ROOT)
    modes = ('controls',) + (AUDITS if args.audit == 'all' else ((args.audit,) if args.audit else ()))
    results = []
    for mode in modes:
        result = subprocess.run([str(executable), mode], capture_output=True, text=True, timeout=10)
        print(result.stdout, end='')
        print(result.stderr, end='')
        results.append(dict(mode=mode, exit=result.returncode, stdout=result.stdout, stderr=result.stderr))
    code = int(any(r['exit'] != 0 for r in results))
    record = dict(source=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  platform=platform.platform(), command=command, cases=results, exit=code,
                  sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in (adapter, file_source, fixture, Path(__file__).resolve(),
                                    ROOT / "userspace/lib/include/sys/eventfd.h",
                                    ROOT / "userspace/lib/include/sys/epoll.h")})
    (out / 'result.json').write_text(json.dumps(record, indent=2) + '\n')
    return code


if __name__ == '__main__':
    raise SystemExit(main())
