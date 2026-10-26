#!/usr/bin/env python3
"""Check generated entry contracts independently, optionally across a built image."""
import argparse
from pathlib import Path
import struct
import unittest
from exfat_test_image import ExfatFixture
from mk_exfat_image import file_entry, ascii_upcase_table, checksum32, write_boot_regions


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


def verify_boot(data):
    sector=1<<data[108]
    for base in (0,12*sector):
        assert data[base:base+3]==b'\xeb\x76\x90'
        assert not any(data[base+11:base+64])
        assert data[base+510:base+512]==b'\x55\xaa'
        for number in range(1,9):
            assert struct.unpack_from('<I',data,base+(number+1)*sector-4)[0]==0xaa550000
        expected=folded_sum((data[base+i] for i in range(11*sector) if i not in (106,107,112)),32)
        assert all(struct.unpack_from('<I',data,base+i)[0]==expected for i in range(11*sector,12*sector,4))


def verify_upcase(table, expected):
    assert folded_sum(table,32)==expected
    words=struct.unpack('<'+'H'*(len(table)//2),table)
    mapping=[];i=0
    while i<len(words):
        value=words[i];i+=1
        if value==0xffff and len(mapping)!=65535:
            assert i<len(words)
            count=words[i];i+=1
            assert count and len(mapping)+count<=65536
            mapping.extend(range(len(mapping),len(mapping)+count))
        else:mapping.append(value)
    assert len(mapping)==65536
    assert mapping[:128]==[c-32 if 97<=c<=122 else c for c in range(128)]


class GeneratorTests(unittest.TestCase):
    def test_boot_regions(self):
        for shift in range(9,13):
            data=bytearray(24*(1<<shift));data[:3]=b'\xeb\x76\x90'
            data[108]=shift;data[510:512]=b'\x55\xaa';write_boot_regions(data)
            verify_boot(data)
            data[106]=2;data[112]=255;verify_boot(data)
            data[11*(1<<shift)+4]^=1
            with self.assertRaises(AssertionError):verify_boot(data)

    def test_upcase(self):
        table=ascii_upcase_table();verify_upcase(table,checksum32(table))
        with self.assertRaises(AssertionError):verify_upcase(table[:-2],checksum32(table[:-2]))

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
    verify_boot(fixture.data)
    root=fixture.chain(fixture.root)
    table_entry=next(root[i:i+32] for i in range(0,len(root),32) if root[i]==0x82)
    table=fixture.chain(struct.unpack_from('<I',table_entry,20)[0],struct.unpack_from('<Q',table_entry,24)[0])
    verify_upcase(table,struct.unpack_from('<I',table_entry,4)[0])
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
