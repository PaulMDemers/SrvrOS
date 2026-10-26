#!/usr/bin/env python3
"""Audit real eventfd/epoll adapters; --audit runs explicitly failing contract probes.

The adapter prefix and close/dup/fcntl functions are copied verbatim, with #line records.
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
LIFETIME = ('close-capacity', 'instance-reuse', 'watched-reuse', 'aliases', 'replacement', 'wait-retirement', 'failure-retirement')
WAITS = ('zero-timeout', 'ready-wait', 'eventfd-wakeup', 'wait-deadlines',
         'wait-infinite', 'wait-control', 'wait-interruption', 'wait-drain')
COUNTERS = ('counter-overflow', 'reserved-value', 'counter-writable',
            'counter-boundaries', 'counter-ready-after-drain')
MODES = ('semaphore-read', 'invalid-flags', 'eventfd-flags', 'semaphore-readiness')
ROTATION = ('epoll-fairness', 'epoll-hup-only', 'epoll-rotation', 'epoll-rotation-control')
ONESHOT = ('epoll-oneshot', 'epoll-priority', 'epoll-rearm', 'epoll-oneshot-races')
AUDITS = ('epoll-edge',)


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
    def section(begin, end):
        start = text.index(begin)
        finish = text.index(end, start)
        line = text[:start].count('\n') + 1
        return f'#line {line} "{file_source}"\n' + text[start:finish]
    declarations = ('int __posix_socket_is_pseudo(int);\nint __posix_socket_close(int);\n'
                    'void __posix_socket_note_close(int);\nvoid __posix_socket_note_dup(int,int);\n'
                    'int __posix_socket_dup(int);\nint __posix_socket_fcntl(int,int,uint64_t);\n'
                    'int __posix_signal_dispatch_pending(void);\n'
                    'int __posix_signal_dispatch_pending_restartable(void);\n')
    extracted = out / 'adapter.c'
    extracted.write_text(f'#line 1 "{adapter}"\n' + prefix + '\n' + declarations +
                         section('int close(int fd) {', '\nint fsync(') +
                         section('static int dup_locked(', '\nint pipe2(') +
                         section('static int fcntl_dupfd_locked(', '\nint fcntl64('))
    names = ('__errno_location eventfd epoll_create1 epoll_ctl epoll_wait epoll_pwait '
             'close dup dup2 dup3 pipe poll fcntl usleep sysconf getenv fprintf stderr '
             'pthread_mutex_lock pthread_mutex_unlock clock_gettime').split()
    renamed = out / 'rename.h'
    renamed.write_text(''.join(f'#define {name} audit_{name}\n' for name in names))
    executable = out / 'event-adapters'
    command = shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c11', '-O0', '-g', '-Wall', '-Wextra', '-Werror',
        '-I', str(ROOT / 'userspace/lib/include'), '-I', str(ROOT / 'shared/include'),
        '-include', str(renamed), str(extracted), str(fixture), '-o', str(executable)]
    subprocess.run(command, check=True, cwd=ROOT)
    modes = ('controls',) + LIFETIME + WAITS + COUNTERS + MODES + ROTATION + ONESHOT + (AUDITS if args.audit == 'all' else ((args.audit,) if args.audit else ()))
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
