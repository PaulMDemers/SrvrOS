#!/usr/bin/env python3
"""Real-driver, read-only inspection namespaces on disposable synthetic media.

The optional native image is inspected and hashed only. No test invokes a
mutating mode on it. These are host admission tests, not native acceptance.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

from exfat_test_image import ExfatFixture
from test_storage_leases import empty_image


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as source:
        while chunk := source.read(1024 * 1024):
            result.update(chunk)
    return result.hexdigest()


def fixture(prefix='/work', malformed=False, duplicate=False, dirty=False):
    paths = [(prefix, True), (prefix + '/srvros-work', True),
             (prefix + '/srvros-work/anchor.txt', False),
             (prefix + '/srvros-work/battery-fs.img', False)]
    records = ['SRVROSMETA1']
    for i, (path, directory) in enumerate(paths):
        inode = 50000 if duplicate and i == 2 else 50000 + i
        records.append('\t'.join(map(str, [path, inode, 0o40755 if directory else 0o100644,
                                           2 if directory else 1, 42, 43, 1, 2, 3])))
    metadata = ('\n'.join(records) + '\n').encode('ascii')
    if malformed:
        metadata = b'SRVROSMETA1\n/work/srvros-work/anchor.txt\tbad\n'
    image = empty_image(32 * 1024 * 1024)
    image.add_directory_files('srvros-work', {
        'anchor.txt': b'unchanged', 'battery-fs.img': bytes(16 * 1024 * 1024)})
    image.add_directory_files('.srvros', {'meta': metadata})
    if dirty:
        for base in (0, 12 * image.sector):
            struct.pack_into('<H', image.data, base + 106, 2)
    entry = dict(image.entries(image.read('srvros-work')))['battery-fs.img']
    offset = image.offset(struct.unpack_from('<I', entry, 52)[0])
    return bytes(image.data), hashlib.sha256(metadata).hexdigest(), offset


class Matrix:
    def __init__(self, driver, output):
        self.driver = driver.resolve()
        self.output = output
        self.records = []

    def inspect(self, image, mode, expected, mountpoint=None, contains=()):
        before = digest(image)
        args = [str(self.driver), str(image.resolve()), mode]
        if mountpoint is not None:
            args += ['--mountpoint', mountpoint]
        result = subprocess.run(args, cwd=self.output, capture_output=True, text=True, timeout=30)
        after = digest(image)
        assert result.returncode == expected, (args, result.returncode, result.stdout, result.stderr)
        assert before == after, ('inspection changed media', args)
        assert not result.stderr, (args, result.stderr)
        assert not re.search(r'(?:writes|flushes)=[1-9][0-9]*', result.stdout), result.stdout
        if mountpoint is not None:
            assert result.stdout.count('inspection-mountpoint: ' + mountpoint + '\n') == 1
            assert result.stdout.count('inspection-io: writes=0 flushes=0\n') == 1
        else:
            # Legacy default output remains unchanged; the real C driver
            # asserts both I/O counters are zero before returning this result.
            assert 'inspection-mountpoint:' not in result.stdout and 'inspection-io:' not in result.stdout
        for text in contains:
            assert text in result.stdout, (args, text, result.stdout)
        self.records.append(dict(mode=mode, mountpoint=mountpoint, exit=result.returncode,
            image_sha256=before, image_unchanged=True, writes=0, flushes=0,
            counter_proof='explicit terminal receipt' if mountpoint is not None else 'real-driver zero-counter assertions',
            stdout=result.stdout))

    def invalid(self, fifo, extra, option_as_image=False):
        # Opening this FIFO read-only would block. Exit2 within the deadline,
        # without a mount context, proves invalid inspection args are rejected
        # before the normal inspection open path. Mutating flags also use only
        # this sentinel, never a disposable image or the optional native clone.
        args = [str(self.driver), *(['--mountpoint'] if option_as_image else []), str(fifo), *extra]
        result = subprocess.run(args, cwd=self.output, capture_output=True, text=True, timeout=3)
        assert result.returncode == 2 and result.stdout == '', (args, result.returncode, result.stdout, result.stderr)
        assert result.stderr == 'exfat-driver: invalid inspection arguments\n', result.stderr
        self.records.append(dict(invalid_arguments=extra, option_as_image=option_as_image,
                                 exit=2, preopen_refusal=True,
                                 writes=0, flushes=0))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--driver', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--native-image', type=Path)
    args = parser.parse_args()
    output = args.output.resolve() if args.output else Path(tempfile.mkdtemp(prefix='srvros-exfat-inspection-'))
    output.mkdir(parents=True, exist_ok=True)
    matrix = Matrix(args.driver, output)
    image = output / 'work.exfat'
    image_records = []
    for name, options in [('work', {}), ('legacy-vol', {'prefix': '/vol'}),
                          ('malformed', {'malformed': True}), ('duplicate-inode', {'duplicate': True}),
                          ('wrong-sidecar-prefix', {'prefix': '/other'}), ('dirty', {'dirty': True})]:
        data, metadata_sha, offset = fixture(**options)
        image.write_bytes(data)
        before = digest(image)
        if name == 'work':
            matrix.inspect(image, 'writable-check', 1, contains=('writable-check: REFUSED zero_writes=yes',))
            matrix.inspect(image, 'writable-check', 0, '/work', ('writable-check: PASS zero_writes=yes',))
            matrix.inspect(image, 'allocation-check', 0, '/work', ('allocation-check: PASS',))
            matrix.inspect(image, 'headroom-check', 0, '/work', ('headroom-check: PASS',))
            matrix.inspect(image, 'image-range', 0, '/work',
                           (f'image-range: PASS offset={offset} bytes=16777216',))
            for prefix in ('/vol', '/other', '/guest/work space'):
                matrix.inspect(image, 'writable-check', 1, prefix, ('writable-check: REFUSED',))
            matrix.inspect(image, 'image-range', 1, '/other', ('image-range: REFUSED',))
        elif name == 'legacy-vol':
            matrix.inspect(image, 'writable-check', 0, contains=('writable-check: PASS zero_writes=yes',))
            matrix.inspect(image, 'writable-check', 0, '/vol', ('writable-check: PASS',))
            matrix.inspect(image, 'writable-check', 1, '/work', ('writable-check: REFUSED',))
        else:
            matrix.inspect(image, 'writable-check', 1, '/work', ('writable-check: REFUSED',))
            matrix.inspect(image, 'allocation-check', 0, '/work', ('allocation-check: PASS',))
            matrix.inspect(image, 'image-range', 1, '/work', ('image-range: REFUSED',))
        assert before == digest(image)
        assert hashlib.sha256(ExfatFixture(image.read_bytes()).read('.srvros/meta')).hexdigest() == metadata_sha
        image_records.append(dict(kind=name, image_sha256=before, metadata_sha256=metadata_sha,
                                  image_unchanged=True, metadata_unchanged=True))

    fifo = output / 'must-not-open.fifo'
    os.mkfifo(fifo)
    try:
        matrix.invalid(fifo, ['--mountpoint', '/work'])
        matrix.invalid(fifo, ['allocation-check'], option_as_image=True)
        invalid = ['', '/', 'work', '/work/', '//work', '/a/./b', '/a/../b',
                   '/a//b', '/work\t', '/work\x7f', '/wörk', '/' + 'a' * 1023]
        for prefix in invalid:
            matrix.invalid(fifo, ['writable-check', '--mountpoint', prefix])
        # Canonical and shorter than SRV_PATH_MAX, but neither derived query
        # fits once its suffix is appended. Refuse before opening the image.
        long_prefix = '/' + '/'.join(['a' * 254] * 4)
        assert len(long_prefix) == 1020
        for mode in ('headroom-check', 'image-range'):
            matrix.invalid(fifo, [mode, '--mountpoint', long_prefix])
        for extra in [[], ['allocation-check', '--mountpoint'],
                      ['allocation-check', '--bad', '/work'],
                      ['allocation-check', '--mountpoint', '/work', 'extra'],
                      ['allocation-check', '--mountpoint', '/work', '--mountpoint', '/vol'],
                      ['append-seed', '--mountpoint', '/work'],
                      ['create', '--mountpoint', '/work'],
                      ['clean-check', 'accept', '--mountpoint', '/work'],
                      ['clean-check', '--mountpoint', '/work']]:
            matrix.invalid(fifo, extra)
    finally:
        fifo.unlink()

    native = None
    if args.native_image:
        source = args.native_image.resolve()
        assert source != image.resolve(), 'native input must not be a generated fixture output'
        before = digest(source)
        matrix.inspect(source, 'writable-check', 1, contains=('writable-check: REFUSED',))
        for mode, text in [('allocation-check', 'allocation-check: PASS'),
                           ('writable-check', 'writable-check: PASS'),
                           ('headroom-check', 'headroom-check: PASS')]:
            matrix.inspect(source, mode, 0, '/work', (text,))
        assert digest(source) == before
        native = dict(path=str(source), sha256=before, unchanged=True,
                      scope='read-only cloned-workspace host inspection; no native acceptance')
    report = dict(passed=True, driver=str(matrix.driver), driver_sha256=digest(matrix.driver),
                  tests=len(matrix.records), records=matrix.records, images=image_records,
                  optional_native=native, native_acceptance=False,
                  scope='real-driver namespace admission only; all inspection writes/flushes zero')
    (output / 'proof.json').write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    print(f'exfat-inspection-mount: PASS {len(matrix.records)} checks; images/metadata unchanged; writes=0 flushes=0')
    print(output / 'proof.json')


if __name__ == '__main__':
    main()
