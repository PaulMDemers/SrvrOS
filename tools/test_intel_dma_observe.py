import base64
import copy
import pathlib
import struct
import subprocess
import unittest
from unittest.mock import patch
import intel_dma_observe as dma

def fixture():
    b=bytearray(48);b[:4]=b'DMAR';struct.pack_into('<I',b,4,136);b[8]=1;b[10:16]=b'TEST  ';b[36:38]=bytes([38,3])
    b+=struct.pack('<HHBBHQ',0,24,0,0,0,0xfed90000)+bytes([1,8,0,0,0,0,2,0])
    b+=struct.pack('<HHBBHQ',0,32,1,0,0,0xfed91000)+bytes([3,8,0,0,2,240,31,0,4,8,0,0,0,240,15,0])
    b+=struct.pack('<HHHHQQ',1,32,0,0,0x8b800000,0x8fffffff)+bytes([1,8,0,0,0,0,2,0]);b[9]=(-sum(b))%256
    h=14695981039346656037
    for x in b:h=((h^x)*1099511628211)&((1<<64)-1)
    text=f'''intel-dma-summary: v=1 status=8 maps=2 retained=0 hash={hex(h)} unit=0xfed90000 other=0xfed91000 reserved_base=0x8b800000 reserved_end=0x90000000 translation=1 backing_preserved=1 guard_retained=0 writes=0 dma_assured=0 ownership=none
intel-dma-resource: v=1 base=0xfed90000 length=4096 type=1 overlaps=1
intel-dma-table: v=1 length=136 data={base64.b64encode(b).decode()}
intel-dma-sample: v=1 sample=0 begin_us=100 end_us=200 version=0x10 cap=0x100001234 ecap=0x200005678 root=0x800000 before=0xc0000000 after=0xc0000000 reads=12
intel-dma-sample: v=1 sample=1 begin_us=20200 end_us=20300 version=0x10 cap=0x100001234 ecap=0x200005678 root=0x800000 before=0xc0000000 after=0xc0000000 reads=12
'''
    return text,h

class DmaTests(unittest.TestCase):
    def setUp(self):
        self.text,self.pin=fixture()
        # A synthetic header must NEVER match the real production firmware pin.
        self.assertNotEqual(self.pin,dma.DMAR_HASH)
    def decode(self,text):
        with patch.object(dma,'DMAR_HASH',self.pin),patch.object(dma,'capture_result',return_value={'observation_complete':True}),patch.object(dma,'backing_result',return_value={'backing_relationship_observed':True}):return dma.dma_result(text)
    def test_synthetic_positive(self):
        r=self.decode(self.text);self.assertTrue(r['observation_complete']);self.assertFalse(r['dma_authority']);self.assertFalse(r['hardware_ownership'])
        r=self.decode(self.text.replace('translation=1','translation=0').replace('0xc0000000','0x0'));self.assertTrue(r['observation_complete']);self.assertFalse(r['dma_authority'])
    def test_production_pin_refuses_synthetic_table(self):
        with self.assertRaisesRegex(ValueError,'DMAR identity'):dma.dma_result(self.text)
    def test_identity_loss_and_duplicates(self):
        lines=self.text.splitlines()
        for i in range(len(lines)):
            with self.subTest(i=i):
                with self.assertRaises(ValueError):self.decode('\n'.join(lines[:i]+lines[i+1:]))
                with self.assertRaises(ValueError):self.decode(self.text+lines[i]+'\n')
    def test_fields_and_no_authority(self):
        for a,b in [('writes=0','writes=1'),('dma_assured=0','dma_assured=1'),('ownership=none','ownership=hardware'),('v=1 status','v=2 status'),('maps=2','maps=1'),('retained=0','retained=1'),('guard_retained=0','guard_retained=1'),('backing_preserved=1','backing_preserved=0'),('length=4096','length=4095'),('overlaps=1','overlaps=2'),('type=1','type=0'),('translation=1','translation=0'),('reads=12','reads=11'),('begin_us=20200','begin_us=19999'),('end_us=200','end_us=99'),('hash=0x','hash=0xffff'),('root=0x800000','root=0xfffffffffffffffff')]:
            with self.subTest(a=a):
                with self.assertRaises(ValueError):self.decode(self.text.replace(a,b,1))
    def test_raw_table_and_changed_snapshot(self):
        for a,b in [('data=','data=!'),('length=136','length=135'),('sample=1','sample=0'),('cap=0x100001234','cap=0x100001235')]:
            with self.assertRaises(ValueError):self.decode(self.text.replace(a,b,1))
    def test_prerequisites(self):
        with patch.object(dma,'DMAR_HASH',self.pin),patch.object(dma,'capture_result',return_value=None):
            with self.assertRaisesRegex(ValueError,'prerequisites'):dma.dma_result(self.text)
    def test_actual_adapter_refusal_formatter(self):
        p=pathlib.Path('build/tests/intel-display-probe');self.assertTrue(p.exists())
        r=dma.dma_result(subprocess.check_output([str(p),'--dma-refused'],text=True));self.assertFalse(r['observation_complete']);self.assertEqual(r['summary']['maps'],0);self.assertFalse(r['dma_authority'])
    def test_absent_and_bounded_report(self):
        self.assertIsNone(dma.dma_result('ordinary desktop'))
        self.assertLess(len(self.text),2048)
        backing=subprocess.check_output(['build/tests/intel-display-probe','--backing-report'],text=True)
        self.assertLess(len(backing)+len(self.text)+16384,65536)

if __name__=='__main__':unittest.main()
