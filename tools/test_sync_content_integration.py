#!/usr/bin/env python3
"""Real process/exFAT sync of a growing screenshot, with opt-in I/O equivalence.

No controller or scheduler timing is emulated as native acceptance. The host
clock has deterministic callback costs solely to exercise joined observations.
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
from test_storage_leases import empty_image
from exfat_test_image import ExfatFixture, check_clean_image

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'build/review/sync-content-integration')
    parser.add_argument('--limine-include', type=Path, default=ROOT/'build/limine-protocol/include')
    parser.add_argument('--compile-only', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    sources = ['tools/exfat_host_driver.c', 'tools/test_process_writeback.c',
               'kernel/src/fs/exfat.c', 'kernel/src/vfs.c', 'kernel/src/vfs_namespace.c',
               'kernel/src/autotest_storage.c']
    dependencies = sources + ['tools/test_sync_content_bridge.inc', 'kernel/src/process.c',
                              'kernel/src/sync_timing.c', 'kernel/include/srvros/sync_timing.h',
                              'tools/test_sync_content_integration.py']
    executable = out/'sync-content'
    command = shlex.split(os.environ.get('CC', 'cc'))
    if platform.system() == 'Darwin':
        command += ['-arch', 'x86_64']
    command += ['-DSRVROS_EXFAT_HOST_TEST', '-DSRVROS_EXFAT_COST_TEST',
                '-DSRVROS_SYNC_CONTENT_HOST_TEST', '-std=c11', '-O1', '-g',
                '-Wall', '-Wextra', '-Werror', '-ffunction-sections', '-fdata-sections',
                '-idirafter', str(ROOT/'userspace/lib/include'),
                '-I', str(ROOT/'shared/include'), '-I', str(ROOT/'kernel/include'),
                '-I', str(args.limine_include.resolve())]
    command += [str(ROOT/p) for p in sources]
    command += ['-Wl,-dead_strip' if platform.system() == 'Darwin' else '-Wl,--gc-sections',
                '-o', str(executable)]
    subprocess.run(command, check=True, cwd=ROOT)
    if args.compile_only:
        print('sync-content integration compiled:', executable)
        return 0
    fixture = empty_image(32*1024*1024, cluster_shift=8)
    fixture.add_directory_files('work', {'capture.tmp': b'', 'other.txt': b'', 'anchor.txt': b'unchanged'})
    seed = bytes(fixture.data)
    image = out/'work.exfat'
    results = []
    for fault in (False, True):
        twins = []
        for enabled in (False, True):
            mode = 'sync-content-' + ('fail-' if fault else '') + ('on' if enabled else 'off')
            image.write_bytes(seed)
            process = subprocess.run([str(executable), str(image), mode], capture_output=True,
                                     text=True, timeout=120, cwd=ROOT)
            (out/(mode+'.log')).write_text(process.stdout+process.stderr)
            assert process.returncode == 0, process.stdout+process.stderr
            trace = re.search(r'sync-content-trace: callbacks=(\d+) hash=([0-9a-f]{16}) writes=(\d+) flushes=(\d+) write_bytes=(\d+)', process.stdout)
            assert trace and int(trace[1]) > 0 and int(trace[3]) > 0 and int(trace[4]) > 0
            media = image.read_bytes()
            state = ExfatFixture(media)
            expected = bytes((i*37+11)&255 for i in range(1366*768*4+54))
            if not fault:
                # ExfatFixture's public reader accepts nested path components.
                assert state.read('work/capture.tmp') == expected
                assert state.read('work/other.txt') == bytes([65])*17
                check_clean_image(image)
            else:
                # The first failing callback is the next mutation's dirty-flag
                # write: previous durable bytes must remain exactly observable.
                assert state.read('work/capture.tmp') == expected[:32*65536]
                assert state.read('work/other.txt') == bytes([32])*17
            assert state.read('work/anchor.txt') == b'unchanged'
            row = dict(mode=mode, trace=list(trace.groups()), image_sha256=hashlib.sha256(media).hexdigest(),
                       stdout=process.stdout)
            twins.append(row)
        assert twins[0]['trace'] == twins[1]['trace'], 'Diagnostics changed ordered callback/data fingerprint'
        assert twins[0]['image_sha256'] == twins[1]['image_sha256'], 'Diagnostics changed final media bytes'
        results.extend(twins)
    record = dict(status='PASS', scope='Host real process/exFAT/VFS integration; not native scheduling/controller acceptance',
                  command=command, cases=results,
                  sha256={p: hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in dependencies})
    (out/'result.json').write_text(json.dumps(record, indent=2)+'\n')
    print('sync-content integration: PASS full-size interleaving, shared content, identity reuse, failure retention and identical observer on/off I/O/media')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
