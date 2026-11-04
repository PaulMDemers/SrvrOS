import base64
import copy
import re
import struct
import unittest
from collections import Counter
import hardware_boot as boot
from hardware_haswell_backing import META,backing_result


def digest(words):
    v=14695981039346656037
    for word in words:
        for b in struct.pack('<I',word):v=((v^b)*1099511628211)&((1<<64)-1)
    return v


def fixture(maximum=False):
    w,h,pitch=(384,4096,1536) if maximum else (1366,768,5632)
    count=pitch*h//4096
    e=[0x0a268086,0x03000009,0x011a106b,0xb0000004,0,0xa000000c,0,7,0,0x0a048086,0x209,0x8da00001,0x8d800001,0x8fa00001,0x209,0x8da00001,0x9c438086,0x2000,
       0x98004400,pitch,0,0,0,((w-1)<<16)|(h-1),0,0x98004400,0,*([0x40000000]*4),1,0xffff8000a0000000,0xa0000000,w,h,pitch,pitch*h,0xa0000000,pitch*h,7,count,3,32,0x0000080808100801,1,1,1,1,1]
    words=[(0x8da00000+i*4096)|0x805 for i in range(count)]
    fences=[(0x41f02b<<32)|1]+[0]*15
    text=f'hsw-backing: v=1 status=1 count={count} cap=1536 fences=16 upper=unknown swizzle=partial maps=8 retained=0 offset=0 bytes={pitch*h} table=2097152 stolen=2376073216,2409627648 limit_us=100000 writes=0 ownership=0\n'
    for phase in range(2):
        values=e+fences+[100+1000*phase,200+1000*phase,digest(words),count+96,count,count,count]
        meta=base64.b64encode(struct.pack(META,*values)).decode();ptes=base64.b64encode(struct.pack('<'+'I'*count,*words)).decode()
        text+=f'hsw-backing-meta: v=1 phase={phase} data={meta}\nhsw-backing-ptes: v=1 phase={phase} data={ptes}\n'
    return text+'laptop-capture: completed=yes\n'


def change_meta(text,index,value,phase=0):
    line=next(x for x in text.splitlines() if x.startswith(f'hsw-backing-meta: v=1 phase={phase}'))
    data=line.split('data=')[1];v=list(struct.unpack(META,base64.b64decode(data)));v[index]=value
    return text.replace(line,line.split('data=')[0]+'data='+base64.b64encode(struct.pack(META,*v)).decode())


class HaswellReceiptTests(unittest.TestCase):
    def test_full_vectors_and_cache_bit11(self):
        r=backing_result(fixture());self.assertEqual(r['status'],'OBSERVED');self.assertEqual(r['count'],1056)
        self.assertEqual(r['samples'][0]['address_candidates'][0],0x8da00000)
        self.assertEqual(r['samples'][1]['stolen_candidates'],1056)
        self.assertFalse(r['unique_fence_proven']);self.assertFalse(r['hardware_authority']);self.assertFalse(r['ownership'])
    def test_metadata_bounds_identity_and_generation(self):
        for index,val in [(0,0x16268086),(1,0),(2,0x011b106b),(3,0xb0001004),(4,1),(5,0),(6,1),(7,0),(8,3),(9,0x16048086),(10,0x40),(14,0x40),(16,0),(27,0),(31,2),(32,0),(33,0),(34,4097),(35,0),(36,1),(37,1),(38,0xb0000000),(39,0),(40,0),(41,0),(42,4097),(43,24),(44,0),(45,3),(46,0),(47,0),(48,0),(49,2),(66,0),(67,100101),(68,0),(69,1151),(70,0),(71,0),(72,0)]:
            with self.subTest(index=index):self.assertIsNone(backing_result(change_meta(fixture(),index,val)))
    def test_duplicate_truncated_noncanonical_or_missing(self):
        s=fixture();lines=s.splitlines()
        for line in lines[:-1]:
            self.assertIsNone(backing_result(s+line+'\n'))
            self.assertIsNone(backing_result(s.replace(line+'\n','')))
        for old,new in [('v=1 status','v=2 status'),('writes=0','writes=1'),('ownership=0','ownership=1'),('fences=16','fences=32'),('upper=unknown','upper=owned'),('retained=0','retained=1'),('cap=1536','cap=4096'),('count=1056','count=1055'),('completed=yes','completed=NO')]:
            self.assertIsNone(backing_result(s.replace(old,new)))
        self.assertIsNone(backing_result(s.replace(lines[1],lines[1][:-1])))
    def test_raw_vector_mutation_never_covered_by_hash_only(self):
        s=fixture();line=next(x for x in s.splitlines() if x.startswith('hsw-backing-ptes:'))
        raw=bytearray(base64.b64decode(line.split('data=')[1]));raw[77]^=1
        bad=s.replace(line,line.split('data=')[0]+'data='+base64.b64encode(raw).decode())
        self.assertIsNone(backing_result(bad))
        self.assertIsNone(backing_result(s.replace(line,line.split('data=')[0]+'data=')))
    def test_endpoints_gap_fence_and_counts(self):
        for index,val in [(17,0),(50,0),(66,100),(66,100201),(67,1100),(69,1153)]:
            with self.subTest(index=index):self.assertIsNone(backing_result(change_meta(fixture(),index,val,1)))
        s=fixture();bad=s.replace('status=1','status=6').replace('retained=0','retained=1')
        r=backing_result(bad);self.assertEqual(r['status'],'FAILED');self.assertFalse(r['hardware_authority'])
    def test_refusal_is_not_observation(self):
        s=fixture().replace('status=1','status=2').replace('completed=yes','completed=NO')
        self.assertEqual(backing_result(s)['status'],'REFUSED')
    def test_unchanged_tail_capacity_keeps_maximum_raw_vectors(self):
        s=fixture(True);self.assertIsNotNone(backing_result(s))
        # Separate profile: 32KiB reserved after the receipt for checkpoint/report
        # output, plus 128KiB pre-final chatter. No sample/vector omission.
        stressed='pre-final:'+'x'*131072+'\n'+s+'post-final:'+'x'*32768+'\n'
        tail=stressed.encode()[-65536:].decode()
        self.assertIsNotNone(backing_result(tail))
        critical=lambda t:Counter(x for x in t.splitlines() if x.startswith(('hsw-backing:','hsw-backing-meta:','hsw-backing-ptes:','laptop-capture: completed=')))
        self.assertEqual(critical(s),critical(tail))
    def test_stage_isolated_config_and_conflicts(self):
        conf=boot.limine_config('x','ram-usb-v1-hsw-laptop-probe-1234abcd-auto-return')
        self.assertIn('srvros.capture=haswell-backing',conf);self.assertIn('srvros.capture-failure=reboot',conf)
        for token in ('srvros.power=','srvros.aml','srvros.battery','srvros.audio','srvros.idle=','srvros.ap=c3-test','srvros.capture=cpu-load'):self.assertNotIn(token,conf)
        for extra in ({},{'cpu_load_capture':True},{'ap_ownership_test':True},{'power_telemetry':True},{'battery_air61':True},{'repeat':2}):
            with self.assertRaises(ValueError):boot.stage(haswell_backing_capture=True,**({'laptop_probe_volume':'1234abcd'} if extra else {}),**extra)

if __name__=='__main__':unittest.main()
