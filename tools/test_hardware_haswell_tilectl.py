import base64
from pathlib import Path
import struct
import unittest

import hardware_boot as boot
from hardware_haswell_backing import META, backing_result
from test_hardware_haswell_backing import fixture


def fixture_v2(maximum=False):
    text=fixture(maximum).replace('v=1','v=2').replace('maps=8','maps=10')
    for line in text.splitlines():
        if not line.startswith('hsw-backing-meta:'):continue
        data=line.split('data=')[1];values=list(struct.unpack(META,base64.b64decode(data)))
        values[69]+=1
        text=text.replace(line,line.split('data=')[0]+'data='+base64.b64encode(struct.pack('<50Q16Q3Q5I',*(values+[8]))).decode())
    return text


def change_v2(text,index,value,phase=0):
    line=next(x for x in text.splitlines() if x.startswith(f'hsw-backing-meta: v=2 phase={phase}'))
    values=list(struct.unpack('<50Q16Q3Q5I',base64.b64decode(line.split('data=')[1])))
    values[index]=value
    return text.replace(line,line.split('data=')[0]+'data='+base64.b64encode(struct.pack('<50Q16Q3Q5I',*values)).decode())


class TilectlReceiptTests(unittest.TestCase):
    def test_v1_is_preserved_and_v2_is_raw_only(self):
        self.assertEqual(backing_result(fixture())['maps'],8)
        r=backing_result(fixture_v2());self.assertEqual(r['source_version'],2)
        self.assertEqual(r['tilectl_raw'],[8,8]);self.assertEqual(r['maps'],10)
        self.assertFalse(r['ownership']);self.assertFalse(r['unique_fence_proven'])
        self.assertEqual(backing_result(fixture_v2(True))['count'],1536)
    def test_changed_unavailable_truncated_and_mixed_versions_refuse(self):
        text=fixture_v2()
        for bad in (change_v2(text,73,9,1),change_v2(text,73,0xffffffff),change_v2(text,69,1152),
                    text.replace('maps=10','maps=8'),text.replace('meta: v=2 phase=1','meta: v=1 phase=1'),
                    text.replace('v=2','v=1'),text.replace('ownership=0','ownership=1')):
            self.assertIsNone(backing_result(bad))
        line=next(x for x in text.splitlines() if x.startswith('hsw-backing-meta:'))
        self.assertIsNone(backing_result(text.replace(line,line[:-4])))
    def test_raw_fields_are_not_overinterpreted(self):
        for raw in (0,1,2,3,0xfffffffe):
            text=fixture_v2()
            for phase in (0,1):text=change_v2(text,73,raw,phase)
            r=backing_result(text);self.assertEqual(r['tilectl_raw'],[raw,raw]);self.assertFalse(r['ownership'])
    def test_versioned_config_and_modifier_refusal(self):
        conf=boot.limine_config('x','ram-usb-v2-hsw-laptop-probe-1234abcd-auto-return')
        self.assertIn('srvros.capture=haswell-backing',conf)
        self.assertIn('srvros.hsw-cpu-control=read-only',conf)
        self.assertIn('srvros.capture-failure=reboot',conf)
        old=boot.limine_config('x','ram-usb-v1-hsw-laptop-probe-1234abcd-auto-return')
        self.assertNotIn('srvros.hsw-cpu-control',old)
        for token in ('srvros.power=','srvros.aml=','srvros.battery=','srvros.ap=','srvros.idle='):self.assertNotIn(token,conf)
        with self.assertRaises(ValueError):boot.stage(haswell_tilectl=True,laptop_probe_volume='1234abcd')
        with self.assertRaises(ValueError):boot.stage(haswell_tilectl=True,haswell_backing_capture=True,laptop_probe_volume='1234abcd',power_telemetry=True)


if __name__=='__main__':unittest.main()
