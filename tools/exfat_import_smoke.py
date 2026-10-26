#!/usr/bin/env python3
"""Run imported exFAT data-length tests on a disposable copy of the built disk."""
import argparse
import hashlib
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
from exfat_test_image import ExfatFixture

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--disk', type=Path, default=ROOT/'build/srvros.exfat')
    parser.add_argument('--iso', type=Path, default=ROOT/'build/srvros-x86_64.iso')
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--corrupt-entry', action='store_true', help='Require mount refusal after corrupting an entry checksum')
    args = parser.parse_args()
    raw = args.disk.read_bytes()
    fixture = ExfatFixture(raw)
    valid = {'partial': 4103, 'zero': 0, 'boundary': 4096}
    fixture.add_directory_files('import-test', {
        name: bytes(i % 251 + 1 if i < size else 0xa5 for i in range(8192))
        for name, size in valid.items()})
    root = fixture.chain(fixture.root)
    entry = next(e for name, e in fixture.entries(root) if name == 'import-test')
    start = fixture.offset(struct.unpack_from('<I', entry, 52)[0])
    directory = fixture.chain(struct.unpack_from('<I', entry, 52)[0])
    cursor = start
    for name, entry in fixture.entries(directory):
        assert fixture.data[cursor:cursor+len(entry)] == entry
        struct.pack_into('<Q', fixture.data, cursor+40, valid[name])
        checksum = 0
        for i, byte in enumerate(fixture.data[cursor:cursor+len(entry)]):
            if i not in (2, 3):
                checksum = (((checksum >> 1) | ((checksum & 1) << 15)) + byte) & 65535
        struct.pack_into('<H', fixture.data, cursor+2, checksum)
        cursor += len(entry)
    if args.corrupt_entry:
        fixture.data[start+2] ^= 1
    with tempfile.TemporaryDirectory(prefix='srvros-import-') as folder:
        disk = Path(folder)/'fixture.exfat'
        disk.write_bytes(fixture.data)
        print('source disk sha256:', hashlib.sha256(raw).hexdigest(), flush=True)
        print('fixture disk sha256:', hashlib.sha256(fixture.data).hexdigest(), flush=True)
        result = subprocess.run([sys.executable, str(ROOT/'tools/thread_smoke.py'),
            '--root', str(ROOT), '--disk', str(disk), '--iso', str(args.iso.resolve()),
            '--qemu', args.qemu, '--test', 'exfat-refused' if args.corrupt_entry else 'exfat-import',
            '--boot-wait', '60', '--run-wait', '120'],
            capture_output=True, text=True)
        print(result.stdout + result.stderr, end='')
        if args.corrupt_entry:
            from thread_smoke import has_fatal_exception
            passed = (result.returncode == 0 and not has_fatal_exception(result.stdout) and
                      'exfat: failed to scan block root directory' in result.stdout and
                      'exfat: mounted /fat from initramfs-exfat' in result.stdout and
                      'threadstress: exfat-refused ok' in result.stdout)
            print('exfat-import: corrupt-entry refusal', 'PASS' if passed else 'FAIL')
            return int(not passed)
        return result.returncode or int('threadstress: exfat-import ok' not in result.stdout)

if __name__ == '__main__':
    raise SystemExit(main())
