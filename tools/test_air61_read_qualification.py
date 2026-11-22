"""Real fresh bridge/encoder, lossless reconstruction, strict nonpromoting proof."""
import base64
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import hardware_air61_read_qualification as q
from hardware_air61_window import decode as old_decode
from test_hardware_air61_window import PIN
SESSION='session-0000000000000001-00'
EXE=Path('build/tests/air61-read-qualification')

class Qualification(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory();cls.blobs={}
        for mode in ('normal','maximum','ancestry'):
            p=Path(cls.tmp.name)/mode
            subprocess.run([str(EXE),mode,str(p)],check=True,capture_output=True,timeout=30)
            cls.blobs[mode]=p.read_bytes()
    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()
    def decode(self,data,session=SESSION):
        with patch('hardware_haswell_firmware.PIN',PIN):return q.decode(data,session)
    def test_real_producer_and_exact_roundtrip(self):
        for mode,blob in self.blobs.items():
            with self.subTest(mode=mode):
                p=self.decode(blob)
                self.assertEqual(q.pack(p['raw_payload']),blob)
                self.assertEqual(len(p['passes']),2)
                self.assertTrue(20000<=p['gap_us']<=100000)
                self.assertTrue(all(not p[k] for k in ('native_accepted','source_completed','register_read_authorized','hardware_authority','ownership')))
                self.assertEqual((p['target_reads'],p['target_writes']),(0,0))
                text=''.join(f'hsw-read-data: v=1 chunk={i//512} data='+base64.b64encode(blob[i:i+512]).decode()+'\n' for i in range(0,len(blob),512))
                self.assertLessEqual(len(text)+1024,65536)
    def test_bridge_refusal_poison_and_bounded_output(self):
        for mode in ('foreign','ap','wp','nx','clock','delay','borrowed-A','boot','cache-change','source-change','poison0','poison1','small','invalid-count','oversized-census'):
            with self.subTest(mode=mode):subprocess.run([str(EXE),mode],check=True,capture_output=True,timeout=30)
    def test_session_series_and_lifetime_tampering(self):
        raw=bytearray(q.unpack(self.blobs['normal']))
        for offset in (0,8,35,43,51,59,67,75,83,87,94,95):
            b=raw.copy();b[offset]^=128
            with self.subTest(offset=offset),self.assertRaises((ValueError,struct.error)):self.decode(q.pack(b))
        with self.assertRaises(ValueError):self.decode(self.blobs['normal'],SESSION[:-1]+'1')
    def test_exact_paths_all_bits_both_passes(self):
        original=q.unpack(self.blobs['normal']);r=q.Reader(original);r.raw(95)
        locations=[]
        for _ in range(2):
            n,=r.take('I');r.raw(n)
            r.raw(24+28+24+8)
            # All64 bits of each of the four pre-path words. Post paths are
            # independently compared to the same exact expected tuple.
            locations.append(r.at+4)
            r.raw(12*68)
        self.assertEqual(r.at,len(original))
        for where in locations:
            for word in range(4):
                for bit in range(64):
                    raw=bytearray(original);raw[where+word*8+bit//8]^=1<<(bit%8)
                    with self.assertRaises(ValueError):self.decode(q.pack(raw))
    def test_coherent_forbidden_root_flags(self):
        # Change the full raw baseline, all path copies and census run first/
        # negative-step fields together: graph binding alone cannot detect it.
        raw=q.unpack(self.blobs['normal']);old=0x2001023;mask=(1<<64)-1
        positive=struct.pack('<Q',old);negative=struct.pack('<Q',(-old)&mask)
        self.assertGreater(raw.count(positive),0);self.assertGreater(raw.count(negative),0)
        for bit in (6,8,9,10,11,*range(39,63)):
            new=old|(1<<bit)
            changed=raw.replace(positive,struct.pack('<Q',new)).replace(negative,struct.pack('<Q',(-new)&mask))
            with self.subTest(bit=bit),self.assertRaisesRegex(ValueError,'forbidden borrowed root flags'):
                self.decode(q.pack(changed))
    def test_guard_accounting_and_old_wire_separation(self):
        raw=q.unpack(self.blobs['normal']);header=struct.pack('<7I',2,3996,20,40,2,49,49)
        self.assertEqual(raw.count(header),2)
        for at in (raw.index(header),raw.rindex(header)):
            b=bytearray(raw);struct.pack_into('<I',b,at+4,1840)
            with self.assertRaises(ValueError):self.decode(q.pack(b))
        n,=struct.unpack_from('<I',raw,95)
        with self.assertRaises(ValueError):old_decode(raw[99:99+n])
    def test_wire_truncation_duplicates_and_compression_bounds(self):
        blob=self.blobs['normal']
        for end in (*range(32),*range(512,len(blob),512),len(blob)-1):
            with self.assertRaises((ValueError,struct.error)):self.decode(blob[:end])
        for b in (blob+b'\0',blob+blob,b'Z1AIR610'+struct.pack('<I',70001),b'Z1AIR610'+struct.pack('<I',1)+b'\0\0',b'Z1AIR610'+struct.pack('<I',1)+struct.pack('<H',0x8001)):
            with self.assertRaises((ValueError,struct.error)):self.decode(b)
        self.assertEqual(q.unpack(q.pack(bytes(70000))),bytes(70000))
        for raw in (bytes(70001),b'\xff'*45000):
            with self.assertRaises(ValueError):q.pack(raw)

if __name__=='__main__':unittest.main()
