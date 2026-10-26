#!/usr/bin/env python3
"""Check generated entry contracts independently, optionally across a built image."""
import argparse
from pathlib import Path
import struct
import unittest
from exfat_test_image import ExfatFixture
from mk_exfat_image import file_entry


def folded_sum(data, bits=16):
    total = 0
    for byte in data:
        total = (total // 2 + (total % 2) * (1 << (bits-1)) + byte) % (1 << bits)
    return total


def verify_entry(entry):
    assert entry[0] == 0x85 and len(entry) == (entry[1]+1)*32
    assert entry[32] == 0xc0 and entry[35]
    assert len(entry) == (2 + (entry[35]+14)//15)*32
    assert struct.unpack_from('<H', entry, 2)[0] == folded_sum(entry[:2]+entry[4:])
    units = bytearray()
    for off in range(64, len(entry), 32):
        assert entry[off] == 0xc1 and not entry[off+1] & 3
        units += entry[off+2:off+32]
    name = units[:entry[35]*2].decode('utf-16le')
    assert name.isascii() and name not in ('.', '..')
    assert struct.unpack_from('<H', entry, 36)[0] == folded_sum(name.upper().encode('utf-16le'))
    size = struct.unpack_from('<Q', entry, 56)[0]
    assert entry[33] & 1
    if not size:
        assert not entry[33] & 2 and struct.unpack_from('<I', entry, 52)[0] == 0
    return name


class GeneratorTests(unittest.TestCase):
    def test_fixed_vector(self):
        for name in ('Aa.txt', 'aA.TXT'):
            entry = file_entry(name, 5, b'x'*35)
            self.assertEqual(struct.unpack_from('<H', entry, 36)[0], 0x24d8)
            self.assertEqual(verify_entry(entry), name)

    def test_lengths(self):
        for length in (1, 15, 16, 255):
            with self.subTest(length=length):
                name = 'x'*length
                self.assertEqual(verify_entry(file_entry(name, 5, b'x')), name)

    def test_empty_and_directory(self):
        empty = file_entry('empty', 9, b'')
        self.assertEqual(verify_entry(empty), 'empty')
        directory = file_entry('dir', 5, b'', attributes=0x10, data_length=4096)
        self.assertEqual(verify_entry(directory), 'dir')
        self.assertEqual(struct.unpack_from('<Q', directory, 40)[0], 4096)
        self.assertEqual(verify_entry(ExfatFixture.checked_entry('empty', 0, b'')), 'empty')

    def test_invalid_names(self):
        for name in ('', '.', '..', 'x'*256, 'caf\u00e9', 'a/b', 'a\\b', 'a:b', '\0', 'a?b'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                file_entry(name, 5, b'x')


def verify_image(path):
    fixture = ExfatFixture(path.read_bytes())
    seen = set()
    total = 0

    def walk(cluster, size=None, contiguous=False):
        nonlocal total
        assert cluster not in seen, 'directory alias/cycle in generated image'
        seen.add(cluster)
        for _, entry in fixture.entries(fixture.chain(cluster, size, contiguous)):
            verify_entry(entry)
            total += 1
            if struct.unpack_from('<H', entry, 4)[0] & 0x10:
                walk(struct.unpack_from('<I', entry, 52)[0],
                     struct.unpack_from('<Q', entry, 56)[0], bool(entry[33] & 2))

    walk(fixture.root)
    assert total
    print(f'exfat-generator: {total} entry sets in {len(seen)} directories PASS')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path)
    args = parser.parse_args()
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(GeneratorTests))
    if not result.wasSuccessful():
        raise SystemExit(1)
    if args.image:
        verify_image(args.image)
