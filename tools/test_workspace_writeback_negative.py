#!/usr/bin/env python3
"""Verify the actual-source matrix rejects swallowed close errors/lost failure latch."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    record = json.loads(args.record.read_text())
    if record['exit'] or not any(c['mode'] == 'workspace-barrier' for c in record['cases']):
        raise RuntimeError('Require a passing workspace writeback matrix')
    for name, digest in record['sha256'].items():
        if hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != digest:
            raise RuntimeError(f'Source changed after positive run: {name}')
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    variants = (
        ('latch', 'workspace.c', 'if (!active || failed) return false;',
         'if (!active) return false;', 'workspace-barrier', 'workspace_sync()'),
        ('close', 'process.c', 'int64_t result=content_release(file->content);',
         'content_release(file->content);int64_t result=0;',
         'workspace-write-1', 'read_file_close(1)==SRV_ERR_IO'),
    )
    results = []
    for name, filename, old, new, mode, assertion in variants:
        source = (ROOT / 'kernel/src' / filename).read_text()
        if source.count(old) != 1:
            raise RuntimeError(f'Negative control anchor changed: {name}')
        variant = out / f'{name}-{filename}'
        variant.write_text(source.replace(old, new))
        bridge = (ROOT / 'tools/test_process_writeback.c').read_text()
        for original in ('process.c', 'workspace.c'):
            target = variant if original == filename else ROOT / 'kernel/src' / original
            bridge = bridge.replace(f'"../kernel/src/{original}"', f'"{target}"')
        adapter = out / f'{name}-bridge.c'; adapter.write_text(bridge)
        command = list(record['command'])
        command[command.index(str(ROOT / 'tools/test_process_writeback.c'))] = str(adapter)
        command[command.index('-o') + 1] = str(out / name)
        command += ['-I', str(ROOT / 'kernel/src')]
        subprocess.run(command, cwd=ROOT, check=True)
        run = subprocess.run([str(out / name), mode], capture_output=True, text=True)
        if run.returncode == 0 or assertion not in run.stderr:
            raise RuntimeError(f'Negative control did not hit expected assertion: {name}: {run.stderr}')
        results.append(dict(name=name, mode=mode, exit=run.returncode,
                            stderr=run.stderr, command=command))
        print(f'{name}: correctly rejected by {assertion}', flush=True)
    (out / 'result.json').write_text(json.dumps(dict(source=record['source'], cases=results), indent=2) + '\n')


if __name__ == '__main__':
    main()
