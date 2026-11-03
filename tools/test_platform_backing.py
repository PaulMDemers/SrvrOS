import base64
import re
import struct
import unittest
import test_hardware_platform as previous
from hardware_platform import (BACKING_FORMAT,backing_decode,platform_backing_result,
                               platform_result,dry_profile_words,dry_profile_digest)

class BackingTests(unittest.TestCase):
    def values(self,phase=0):
        us=40+phase*92000000
        return [1,9,us,us+1,0xa000000c,0,0xa000000c,0,6,6,*([0x40000000]*8),
                0x98000000,5632,0,0x100000,0,(1365<<16)|767,0x100000,0x98000000,0x100000,
                1,0xffff8000a0100000,0xa0100000,1366,768,5632,5632*768,
                32,1,8,16,8,8,8,0,1,1056,1,0xa0000000,0x10000000,7,1,0,0]
    def encode(self,v):return base64.b64encode(struct.pack(BACKING_FORMAT,*v)).decode()
    def fixture(self):
        s=previous.PlatformTests().dry_fixture().replace('ap-platform: v=8','ap-platform: v=9')
        def change(m):
            v=list(map(int,m[2].split(',')));v[8]=0x98000000
            return m[1]+','.join(map(str,v))
        s=re.sub(r'(ap-platform-resource: phase=[01] count=26 raw=)([0-9,]+)',change,s)
        digest=dry_profile_digest(dry_profile_words(s,0))
        data=base64.b64encode(struct.pack('<QQH10B',1,digest,127,1,4,1,0,0,0,0,0,0,0)).decode()
        s=re.sub(r'ap-platform-dry: v=1 data=\S+',f'ap-platform-dry: v=1 data={data}',s)
        for phase in (0,1):s+=f'ap-platform-backing: v=1 phase={phase} data={self.encode(self.values(phase))}\n'
        return s
    def test_complete_raw_binding_and_legacy(self):
        r=platform_backing_result(self.fixture());self.assertEqual(r['status'],'OBSERVED');self.assertTrue(r['stable'])
        self.assertEqual(r['samples'][0]['relation'],'compatible-linear-aperture')
        self.assertFalse(r['backing_adopted']);self.assertFalse(r['ggtt_proven']);self.assertFalse(r['hardware_authority'])
        self.assertIsNotNone(platform_result(self.fixture()))
        self.assertIsNone(platform_backing_result(previous.PlatformTests().dry_fixture()))
        bad=self.fixture().replace('rc6=100,1000','rc6=100,72000000')
        self.assertIsNotNone(platform_backing_result(bad));self.assertIsNone(platform_result(bad))
    def test_malformed_or_missing_never_coverage(self):
        s=self.fixture();last=s.splitlines()[-1]
        for bad in (s+last+'\n',s.replace(last,''),s.replace('backing: v=1','backing: v=2'),s.replace('v=9 fault','v=10 fault'),s.replace('phase=1 data=','phase=0 data='),s.replace(last,last[:-1]),s.replace('ap-platform-dry:','missing-dry:')):
            with self.subTest(bad=bad[-50:]):self.assertIsNone(platform_result(bad))
        v=self.values();v[2]=1
        s=s.replace(self.encode(self.values()),self.encode(v));self.assertIsNone(platform_backing_result(s))
    def test_wire_bounds_changed_context_and_claims(self):
        for index,val in [(0,0),(1,8),(3,0),(4,0xa000100c),(5,1),(6,0xb000000c),(8,0),(9,2),(10,0),(14,0),(18,0xffffffff),(25,1),(26,0),(27,2),(28,0),(29,0),(30,4097),(31,0),(32,1),(33,1),(34,24),(35,0),(37,0),(42,4097),(43,0),(44,4),(45,0),(46,0),(47,1),(48,0),(49,1),(50,1)]:
            v=self.values();v[index]=val
            with self.subTest(index=index):
                with self.assertRaises(ValueError):backing_decode(self.encode(v))
        v=self.values();v[0]=3;v[1]=0;v[48]=0
        self.assertEqual(backing_decode(self.encode(v))['relation'],'unavailable')
        s=self.fixture().replace(self.encode(self.values()),self.encode(v));self.assertEqual(platform_backing_result(s)['status'],'UNAVAILABLE');self.assertIsNone(platform_result(s))
    def test_unknown_mappings_and_formats_stay_unbound(self):
        for index,val in [(18,0x98000400),(18,0x98008000),(18,0x90000000),(22,1),(24,0),(29,0xa0200000)]:
            v=self.values();v[index]=val
            if index==18:v[25]=val
            with self.subTest(index=index):self.assertEqual(backing_decode(self.encode(v))['relation'],'unbound-or-unsupported')
        # Unknown classification is represented by zero fields, no reservation claim.
        v=self.values();v[44]=0;v[45:48]=[0,0,0];self.assertEqual(backing_decode(self.encode(v))['classification'],0)
        for kind,classification in ((1,2),(0,3)):
            v=self.values();v[47]=kind;v[44]=classification;self.assertEqual(backing_decode(self.encode(v))['classification'],classification)
    def test_endpoint_identity_and_route_crosscheck(self):
        s=self.fixture();v=self.values(1);v[19]=5696
        changed=s.replace(self.encode(self.values(1)),self.encode(v));r=platform_backing_result(changed)
        self.assertFalse(r['stable']);self.assertEqual(r['samples'][1]['relation'],'unbound-or-unsupported')
        v=self.values(1);v[18]=v[25]=0x90000000
        self.assertIsNone(platform_backing_result(s.replace(self.encode(self.values(1)),self.encode(v))))
    def test_fixed_schema_size(self):
        self.assertEqual(struct.calcsize(BACKING_FORMAT),203);self.assertEqual(len(self.encode(self.values())),272)
        from pathlib import Path
        source=Path('kernel/diagnostics/platform_backing.inc').read_text()
        self.assertIn('{0x70180,0x70188,0x70184,0x7019c,0x701a4,0x6001c,0x701ac,0x70180,0x7019c}',source)

if __name__=='__main__':unittest.main()
