#!/usr/bin/env python3
"""Compare actual old/new console code in RAM; not a device-memory benchmark."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', default='b79c559')
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--samples', type=int, default=3)
    args = parser.parse_args()
    if not 1 <= args.samples <= 100:
        parser.error('--samples must be between 1 and 100')
    revision = subprocess.check_output(['git', 'rev-parse', '--verify', args.baseline + '^{commit}'],
                                       cwd=ROOT, text=True).strip()
    output = {'baseline_commit': revision, 'environment': 'host RAM framebuffer; serial stubbed',
              'resolution': [1440, 900], 'lines': 1000, 'columns': 80, 'samples': {}}
    with tempfile.TemporaryDirectory(prefix='srvros-console-bench-') as directory:
        temporary = Path(directory)
        (temporary / 'baseline_console.c').write_bytes(subprocess.check_output(
            ['git', 'show', revision + ':kernel/src/console.c'], cwd=ROOT))
        for variant in ('baseline', 'current'):
            binary = temporary / variant
            command = [args.cc, '-std=c11', '-O2', '-ffunction-sections', '-fdata-sections',
                       '-Ishared/include', '-Ikernel/include', '-Ibuild/limine-protocol/include',
                       '-I' + directory, 'tools/benchmark_console.c', '-Wl,--gc-sections', '-o', str(binary)]
            if variant == 'baseline':
                command.append('-DSRVROS_BENCH_BASELINE')
            subprocess.run(command, cwd=ROOT, check=True)
            output['samples'][variant] = [subprocess.check_output([str(binary)], text=True).strip()
                                          for _ in range(args.samples)]
    destination = ROOT / 'build/review/console-host-benchmark.json'
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(output, indent=2) + '\n')
    print(destination.read_text(), end='')

if __name__ == '__main__':
    main()
