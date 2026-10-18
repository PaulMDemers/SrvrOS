#!/usr/bin/env python3
"""Check runtime packaging against program-header semantics and real built apps."""
from pathlib import Path
import struct
import unittest
from pack_runtime_elf import runtime_elf


class RuntimeElfTests(unittest.TestCase):
    def fixture(self):
        data = bytearray(0x1000)
        ident = b'\x7fELF\x02\x01\x01' + bytes(9)
        struct.pack_into('<16sHHIQQQIHHHHHH',data,0,ident,2,62,1,0x500000,64,0x800,0,64,56,3,64,4,3)
        struct.pack_into('<IIQQQQQQ',data,64,1,5,0x200,0x500000,0,32,64,0x1000)
        struct.pack_into('<IIQQQQQQ',data,120,7,4,0x300,0x501000,0,16,48,16)
        struct.pack_into('<IIQQQQQQ',data,176,4,4,0x400,0,0,16,16,4)
        data[0x200:0x220] = bytes(range(32))
        data[0x300:0x310] = b'TLS template1234'
        data[0x400:0x410] = b'notes preserved!'
        return bytes(data)

    def test_segments_and_original(self):
        original = self.fixture()
        packed = runtime_elf(original)
        self.assertEqual(len(original),0x1000)
        self.assertEqual(len(packed),0x410)
        self.assertEqual(packed[64:],original[64:len(packed)])
        self.assertEqual(struct.unpack_from('<Q',packed,40),(0,))
        self.assertEqual(struct.unpack_from('<HHH',packed,58),(0,0,0))
        self.assertEqual(runtime_elf(packed),packed)
        self.assertEqual(runtime_elf(b'ordinary file'),b'ordinary file')

    def test_reject_invalid(self):
        for offset,fmt,value in [(4,'B',1),(5,'B',2),(18,'H',3),(52,'H',63),(54,'H',55),(56,'H',0),(56,'H',65535),
                                 (32,'Q',0xffffffffffffffff),(72,'Q',0xffffffffffffffff),(96,'Q',0xffffffffffffffff),(104,'Q',1)]:
            data=bytearray(self.fixture());struct.pack_into('<'+fmt,data,offset,value)
            with self.subTest(offset=offset,value=value),self.assertRaises(ValueError): runtime_elf(bytes(data))
        with self.assertRaises(ValueError): runtime_elf(b'\x7fELF')

    def test_real_programs(self):
        root=Path(__file__).resolve().parents[1]
        files=list((root/'build/userspace').glob('*.elf'))
        self.assertGreater(len(files),100)
        before=after=0
        for path in files:
            original=path.read_bytes();packed=runtime_elf(original)
            before+=len(original);after+=len(packed)
            phoff=struct.unpack_from('<Q',original,32)[0]
            count=struct.unpack_from('<H',original,56)[0]
            for index in range(count):
                kind,_,offset,_,_,size,_,_=struct.unpack_from('<IIQQQQQQ',original,phoff+index*56)
                if size and offset>=64:
                    self.assertEqual(packed[offset:offset+size],original[offset:offset+size],(path,kind))
            self.assertEqual(path.read_bytes(),original)
        self.assertLess(after,before//2)
        print(f'runtime-elf: built program payload {before} -> {after} bytes')


if __name__=='__main__': unittest.main()
