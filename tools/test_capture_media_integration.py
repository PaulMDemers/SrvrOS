#!/usr/bin/env python3
"""Complete screenshot saves with retained readers and real media corruption.

Uses real process/exFAT/VFS and the production save wrapper on a disposable image.
This is not native syscall-admission, scheduling or controller acceptance.
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
CASES = ('success', 'success-retained', 'temp-corrupt', 'temp-corrupt-retained',
         'final-corrupt', 'final-corrupt-retained', 'temp-reader-busy')


def exists(state, path):
    try:
        state.read(path)
    except StopIteration:
        return False
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'build/review/capture-media-integration')
    parser.add_argument('--limine-include', type=Path, default=ROOT/'build/limine-protocol/include')
    parser.add_argument('--compile-only', action='store_true')
    parser.add_argument('--baseline-save', type=Path,
                        help='Optional historical save.c: reproduce old full-wrapper final-proof false success')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    sources = ['tools/test_capture_media_driver.c', 'tools/test_capture_media_bridge.c',
               'kernel/src/fs/exfat.c', 'kernel/src/vfs.c', 'kernel/src/vfs_namespace.c',
               'kernel/src/autotest_storage.c', 'kernel/src/file_verify.c']
    dependencies = sources + ['tools/exfat_host_driver.c', 'tools/test_process_writeback.c',
                             'tools/test_sync_content_bridge.inc', 'tools/test_process_salvage.inc',
                             'kernel/src/process.c', 'kernel/src/sync_timing.c',
                             'shared/include/srvros/file_verify.h', 'kernel/include/srvros/file_verify_kernel.h',
                             'userspace/shotworker/save.c', 'tools/test_capture_media_integration.py']
    fixture = empty_image(32*1024*1024, cluster_shift=8)
    fixture.add_directory_files('work', {'anchor.txt': b'unchanged'})
    seed = bytes(fixture.data)
    expected = bytes((i*37+11)&255 for i in range(200000))
    corrupted = bytes([expected[0] ^ 0x80]) + expected[1:]
    image = out/'disposable.exfat'
    results = []
    builds = [('current', None)]
    if args.baseline_save:
        baseline = args.baseline_save.resolve()
        assert baseline.is_file()
        builds.append(('baseline', baseline))
    try:
        for kind, save_source in builds:
            executable = out/('capture-media-'+kind)
            command = shlex.split(os.environ.get('CC', 'cc'))
            if platform.system() == 'Darwin':
                command += ['-arch', 'x86_64']
            command += ['-DSRVROS_EXFAT_HOST_TEST', '-DSRVROS_EXFAT_COST_TEST',
                        '-DSRVROS_SYNC_CONTENT_HOST_TEST', '-std=c11', '-O1', '-g',
                        '-Wall', '-Wextra', '-Werror', '-ffunction-sections', '-fdata-sections',
                        '-idirafter', str(ROOT/'userspace/lib/include'),
                        '-I', str(ROOT/'shared/include'), '-I', str(ROOT/'kernel/include'),
                        '-I', str(ROOT/'userspace/shotworker'), '-I', str(args.limine_include.resolve())]
            if save_source:
                command += ['-DCAPTURE_MEDIA_EXPECT_LEGACY',
                            '-DCAPTURE_MEDIA_SAVE_SOURCE='+json.dumps(str(save_source))]
            command += [str(ROOT/p) for p in sources]
            command += ['-Wl,-dead_strip' if platform.system() == 'Darwin' else '-Wl,--gc-sections',
                        '-o', str(executable)]
            (out/(kind+'-compile-command.json')).write_text(json.dumps(command, indent=2)+'\n')
            with (out/(kind+'-compile.log')).open('w') as stream:
                subprocess.run(command, check=True, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
            if args.compile_only:
                continue
            for case in CASES:
                image.write_bytes(seed)
                mode = 'sync-content-capture-'+case
                process = subprocess.run([str(executable), str(image), mode], capture_output=True,
                                         text=True, timeout=60, cwd=ROOT)
                (out/(kind+'-'+case+'.log')).write_text(process.stdout+process.stderr)
                assert process.returncode == 0, process.stdout+process.stderr
                proof = re.search(r'capture-media: case=(\S+) result=(\d+) rename_attempts=(\d+) '
                                  r'rename_successes=(\d+) fresh_calls=(\d+) fresh_successes=(\d+) '
                                  r'compared=(\d+) ordinary_shared_reader_preserved=1 allocation_clean=1', process.stdout)
                assert proof and proof[1] == mode, process.stdout
                media = image.read_bytes()
                state = ExfatFixture(media)
                assert state.read('work/anchor.txt') == b'unchanged'
                assert not exists(state, 'work/Screenshots/.capture-777-12345.tmp')
                if int(proof[4]):
                    final = state.read('work/Screenshots/proof.bmp')
                    assert final == (corrupted if 'corrupt' in case else expected)
                else:
                    assert not exists(state, 'work/Screenshots/proof.bmp')
                check_clean_image(image)
                if kind == 'baseline' and case == 'final-corrupt-retained':
                    assert int(proof[2]) == 0 and final == corrupted, 'Historical wrapper must reproduce false success'
                if kind == 'current' and case == 'final-corrupt-retained':
                    assert int(proof[2]) == 3 and final == corrupted, 'Fresh proof must reject while preserving destination'
                results.append(dict(build=kind, case=case, proof=list(proof.groups()),
                                    image_sha256=hashlib.sha256(media).hexdigest(), stdout=process.stdout))
    finally:
        image.unlink(missing_ok=True)
    record = dict(status='COMPILED' if args.compile_only else 'PASS',
                  scope='Complete save wrapper, real process open/read/write/flush/close cores and faithful checked rename, real exFAT/VFS; not syscall admission/native timing',
                  baseline_save_sha256=hashlib.sha256(args.baseline_save.read_bytes()).hexdigest() if args.baseline_save else None,
                  cases=results, sha256={p: hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in dependencies})
    (out/'result.json').write_text(json.dumps(record, indent=2)+'\n')
    print('capture-media integration:', record['status'], len(results), 'cases; disposable image removed')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
