#!/usr/bin/env python3
"""Test the real process futex code with deterministic scheduler/IRQ interleavings.

Extract verbatim declarations and functions; replace only privileged STI with a
modeled interrupt-enable boundary. No production function body is reimplemented.
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/review/futex-regression')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out / 'result.json').unlink(missing_ok=True)
    source = ROOT / 'kernel/src/process.c'
    text = source.read_text()
    declaration = text[text.index('struct process_futex {'):text.index('\nstruct process_exit_record {')]
    start = text.index('static struct process_futex *process_futex_find(')
    body = text[start:text.index('\nvoid process_mark_ready(', start)]
    instruction = '__asm__ volatile ("sti" : : : "memory");'
    assert body.count(instruction) == 1
    body = body.replace(instruction, 'host_enable_interrupts();')
    helpers = ''
    for begin, end in [('static void process_unmap_mapping_at(', 'static bool process_unmap_tracked_page('),
                       ('static bool mmap_apply_protection(', 'static int64_t process_mmap_impl(')]:
        helpers += text[text.index(begin):text.index(end)]
    (out / 'futex-under-test.h').write_text(declaration + '\nstatic struct process_futex futexes[PROCESS_MAX_FUTEXES];\n' + body + helpers)
    fixture = ROOT / 'tools/test_futex_contracts.c'
    executable = out / 'futex-contracts'
    command = shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c11', '-O2', '-g', '-Wall', '-Wextra', '-Werror',
        '-I', str(out), '-I', str(ROOT / 'shared/include'), str(fixture), '-o', str(executable)]
    subprocess.run(command, check=True)
    result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15)
    print(result.stdout, end=''); print(result.stderr, end='')
    record = {'source': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'platform': platform.platform(), 'compile_command': command, 'exit': result.returncode,
              'stdout': result.stdout, 'stderr': result.stderr,
              'sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in (source, fixture, Path(__file__))}}
    (out / 'result.json').write_text(json.dumps(record, indent=2) + '\n')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
