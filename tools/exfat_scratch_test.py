#!/usr/bin/env python3
"""Verify scratch extent admission and metadata isolation on disposable exFAT."""
from pathlib import Path
import re
import struct
import tempfile
from exfat_hardening_test import command, format_image, DRIVER
from exfat_test_image import ExfatFixture, check_clean_image


def checksum(data, off):
    n = (data[off + 1] + 1) * 32
    value = 0
    for i, byte in enumerate(data[off:off + n]):
        if i in (2, 3):
            continue
        value = ((((value & 1) << 15) | (value >> 1)) + byte) & 65535
    struct.pack_into('<H', data, off + 2, value)


def main():
    with tempfile.TemporaryDirectory(prefix='srvros-scratch-') as directory:
        image = Path(directory) / 'scratch.img'
        for cluster in ('4K', '128K'):
            format_image(image, cluster)
            command(DRIVER, image, 'scratch-seed')
            check_clean_image(image)
            baseline = image.read_bytes()
            output = command(DRIVER, image, 'scratch-range')
            offset = int(re.fullmatch(r'scratch offset=(\d+) bytes=4096\n', output)[1])
            assert image.read_bytes() == baseline
            fs = ExfatFixture(baseline)
            entries = dict(fs.entries(fs.chain(fs.root)))
            scratch = entries['scratch.bin']
            assert offset == fs.offset(struct.unpack_from('<I', scratch, 52)[0])
            assert baseline[offset:offset+4096] == bytes(i & 255 for i in range(4096))
            # A reset at any sector boundary can only tear this scratch file.
            for length in range(0, 4097, 512):
                changed = bytearray(baseline)
                changed[offset:offset+length] = b'\x72' * length
                image.write_bytes(changed)
                check_clean_image(image)
                assert ExfatFixture(changed).read('anchor.txt') == b'unchanged'
                assert changed[:offset] == baseline[:offset]
                assert changed[offset+4096:] == baseline[offset+4096:]
            scratch_at = baseline.index(scratch, fs.offset(fs.root))
            anchor_at = baseline.index(entries['anchor.txt'], fs.offset(fs.root))
            for fault in ('size', 'multi-cluster', 'alias', 'directory-overlap', 'bitmap', 'dirty'):
                changed = bytearray(baseline)
                if fault in ('size', 'multi-cluster'):
                    struct.pack_into('<Q', changed, scratch_at+56, 1024 if fault == 'size' else 8192)
                    checksum(changed, scratch_at)
                elif fault == 'alias':
                    changed[anchor_at+52:anchor_at+60] = changed[scratch_at+52:scratch_at+60]
                    struct.pack_into('<Q', changed, anchor_at+40, 4096)
                    checksum(changed, anchor_at)
                elif fault == 'directory-overlap':
                    struct.pack_into('<I', changed, scratch_at+52, fs.root)
                    checksum(changed, scratch_at)
                elif fault == 'dirty':
                    struct.pack_into('<H', changed, 106, 2)
                else:
                    root = fs.chain(fs.root)
                    bitmap = next(root[i:i+32] for i in range(0, len(root), 32) if root[i] == 0x81)
                    bitmap_offset = fs.offset(struct.unpack_from('<I', bitmap, 20)[0])
                    bit = struct.unpack_from('<I', scratch, 52)[0] - 2
                    changed[bitmap_offset+bit//8] &= ~(1 << (bit % 8))
                image.write_bytes(changed)
                command(DRIVER, image, 'scratch-refuse')
                assert image.read_bytes() == changed
            print(f'exfat-scratch: {cluster} extent, refusal, read-only admission and torn-data isolation PASS')


if __name__ == '__main__':
    main()
