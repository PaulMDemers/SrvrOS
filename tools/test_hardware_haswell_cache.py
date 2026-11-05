import base64
import struct
import unittest
from unittest.mock import patch

import hardware_boot as boot
from hardware_haswell_cache import cache_result, effective, UNIT, WINDOW
from test_hardware_haswell_firmware import fixture as fw_fixture


def fixture():
    pre,pin=fw_fixture()
    pre=pre.replace('laptop-capture: completed=yes\n','')
    post=pre.replace('hsw-firmware','hcache-firmware-post')
    for line in post.splitlines():
        if not line.startswith('hcache-firmware-post-meta:'):continue
        v=list(struct.unpack('<6I3Q',base64.b64decode(line.split('data=')[1])));v[7]+=10000;v[8]+=10000
        post=post.replace(line,line.split('data=')[0]+'data='+base64.b64encode(struct.pack('<6I3Q',*v)).decode())
    text=pre+'hsw-cache: v=1 status=1 samples=2 msr_cap=49 pte_cap=16 limit_us=100000 mappings=0 writes=0 ownership=0\n'
    for phase in (0,1):
        t=phase*4000
        values=[0x756e6547,0x49656e69,0x6c65746e,0x40651,0x11020,39,0,0x0007040600070406,0x100,0x806,2100+t,4900+t,0x100000,0,0xffff800000000000,2000+t,5000+t,17,0,11,10]+[0]*43
        text+=f'hsw-cache-cpu: v=1 phase={phase} data='+base64.b64encode(struct.pack('<6I11Q4I43Q',*values)).decode()+'\n'
        for edge in (0,1):
            for alias in (0,1):
                values=([WINDOW,0,0x100000,0]+[0]*6+[1,0,1]) if alias else [0xffff800000000000+UNIT,UNIT,0x100000,0x101003,0x101000,0x102003,0x102000,0x103003,0x103000,UNIT|0x13,4,12,2]
                text+=f'hsw-cache-walk: v=1 phase={phase} edge={edge} alias={alias} data='+base64.b64encode(struct.pack('<10Q3I',*values)).decode()+'\n'
    return text+post+'laptop-capture: completed=yes\n',pin


def mutate(text,prefix,fmt,index,value):
    line=next(x for x in text.splitlines() if x.startswith(prefix));v=list(struct.unpack(fmt,base64.b64decode(line.split('data=')[1])));v[index]=value
    return text.replace(line,line.split('data=')[0]+'data='+base64.b64encode(struct.pack(fmt,*v)).decode())


class CacheTests(unittest.TestCase):
    def test_full_context_and_raw_max_retention_never_grant_mapping(self):
        text,pin=fixture()
        self.assertIsNone(cache_result(text))
        with patch('hardware_haswell_firmware.PIN',pin):
            p=cache_result(text);self.assertIsNotNone(p)
            self.assertEqual(p['status'],'OBSERVED_CONTEXT');self.assertEqual(p['hhdm_alias_state'],'present')
            self.assertEqual(p['hhdm_effective']['effective'],0)
            for f in ('all_aliases_proven','native_mapping_authorized','hardware_authority','ownership'):self.assertFalse(p[f])
            tail=('x'*131072+'\n'+text+'y'*32768)[-65536:];self.assertEqual(cache_result(tail),p)
    def test_every_raw_record_is_required_and_duplicate_refuses(self):
        text,pin=fixture()
        with patch('hardware_haswell_firmware.PIN',pin):
            for line in text.splitlines():
                if line.startswith(('hsw-cache','hsw-firmware','hcache-firmware-post','laptop-capture:')):
                    self.assertIsNone(cache_result(text.replace(line+'\n','')))
                    if line.startswith(('hsw-cache-cpu:','hsw-cache-walk:')):self.assertIsNone(cache_result(text+line+'\n'))
    def test_cpu_identity_clock_cap_and_raw_change_refuse(self):
        text,pin=fixture()
        with patch('hardware_haswell_firmware.PIN',pin):
            for index,value in ((3,0x306d4),(4,0x11000),(5,40),(6,1<<30),(7,0xffffffffffffffff),(12,0x100001),(13,1<<12),(15,0),(16,102001),(17,50),(18,17),(20,17),(21,1)):
                with self.subTest(index=index):self.assertIsNone(cache_result(mutate(text,'hsw-cache-cpu: v=1 phase=1','<6I11Q4I43Q',index,value)))
    def test_walk_wrong_source_alias_flags_and_target_refuse(self):
        text,pin=fixture()
        with patch('hardware_haswell_firmware.PIN',pin):
            for index,value in ((0,WINDOW),(1,UNIT+4096),(2,UNIT),(3,0xfed90003),(9,UNIT|0x33),(10,5),(11,21),(12,1)):
                self.assertIsNone(cache_result(mutate(text,'hsw-cache-walk: v=1 phase=1 edge=1 alias=0','<10Q3I',index,value)))
            for old,new in (('mappings=0','mappings=1'),('writes=0','writes=1'),('ownership=0','ownership=1'),('completed=yes','completed=NO')):self.assertIsNone(cache_result(text.replace(old,new)))
    def test_uc_minus_wc_and_disabled_mtrr(self):
        c=dict(variables=[(0,1<<39,1)],default=0x806,pat=7)
        self.assertEqual(effective(c,0)['effective'],1)
        c['pat']=0;self.assertEqual(effective(c,0)['effective'],0)
        c['default']=0;c['pat']=1;self.assertEqual(effective(c,0)['effective'],0)
        c['default']=0x806;c['variables']=[(0,1<<39,4),(0,1<<39,5)]
        self.assertEqual(effective(c,0)['status'],'UNRESOLVED')
    def test_profile_requires_firmware_and_preserves_version1(self):
        config=boot.limine_config('x','ram-usb-v2-hfw-laptop-probe-1234abcd-auto-return')
        self.assertIn('srvros.hsw-cache=context-only',config)
        self.assertNotIn('srvros.hsw-cache=',boot.limine_config('x','ram-usb-v1-hfw-laptop-probe-1234abcd-auto-return'))
        with self.assertRaises(ValueError):boot.stage(haswell_cache_context=True,laptop_probe_volume='1234abcd')
        with self.assertRaises(ValueError):boot.stage(haswell_cache_context=True,haswell_firmware_capture=True,haswell_backing_capture=True,laptop_probe_volume='1234abcd')

if __name__=='__main__':unittest.main()
