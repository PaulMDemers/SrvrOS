#!/usr/bin/env python3
"""Create a disposable nested exFAT fixture; never open a device or replace a file."""
import argparse
import os
from pathlib import Path
import subprocess
from exfat_test_image import ExfatFixture, check_clean_image

SIZE = 16 * 1024 * 1024
MARKER = b'SRVROS-BATTERY-FS-V1\n'
ANCHOR = b'Unchanged isolated filesystem anchor\n'


def create(path):
    path = Path(path)
    with path.open('xb') as stream:
        stream.truncate(SIZE)
    subprocess.run(['/usr/sbin/mkfs.exfat', '-c', '4K', str(path)], check=True,
                   stdout=subprocess.DEVNULL)
    subprocess.run(['/usr/sbin/tune.exfat', '-I', '0x42415431', str(path)], check=True,
                   stdout=subprocess.DEVNULL)
    fs = ExfatFixture(path.read_bytes())
    fs.add_directory_files('fixture', {'.battery-test': MARKER, 'anchor.txt': ANCHOR})
    with path.open('r+b') as stream:
        stream.write(fs.data)
        stream.flush()
        os.fsync(stream.fileno())
    check_clean_image(path)


def verify(data, cycles):
    fs = ExfatFixture(data)
    assert fs.read('fixture/.battery-test') == MARKER
    assert fs.read('fixture/anchor.txt') == ANCHOR
    folder = 'fixture/fs-proof'
    assert {name for name, _ in fs.entries(fs.read(folder))} == {'payload.bin', 'empty.bin'}
    assert fs.read(folder+'/payload.bin') == bytes((i*37+(i>>8)+cycles-1)&255 for i in range(262181))
    assert fs.read(folder+'/empty.bin') == b''


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    create(parser.parse_args().output)
