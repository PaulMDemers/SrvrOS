import base64
import struct
import unittest

import hardware_boot as boot
from hardware_haswell_backing import backing_result
from test_hardware_haswell_tilectl import fixture_v2


def fixture_v3(maximum=False):
    text=fixture_v2(maximum).replace('v=2','v=3').replace('fences=16 upper=unknown','fences=32 upper=linux-model-32')
    for line in text.splitlines():
        if not line.startswith('hsw-backing-meta:'):continue
        values=list(struct.unpack('<50Q16Q3Q5I',base64.b64decode(line.split('data=')[1])))
        values=values[:66]+[0]*16+values[66:];values[85]+=96
        text=text.replace(line,line.split('data=')[0]+'data='+base64.b64encode(struct.pack('<50Q32Q3Q5I',*values)).decode())
    return text


def change_v3(text,index,value,phase=0):
    line=next(x for x in text.splitlines() if x.startswith(f'hsw-backing-meta: v=3 phase={phase}'))
    values=list(struct.unpack('<50Q32Q3Q5I',base64.b64decode(line.split('data=')[1])))
    values[index]=value
    return text.replace(line,line.split('data=')[0]+'data='+base64.b64encode(struct.pack('<50Q32Q3Q5I',*values)).decode())


class LinuxFenceReceiptTests(unittest.TestCase):
    def test_32_rows_are_model_coverage_not_authority(self):
        for maximum,count in ((False,1056),(True,1536)):
            r=backing_result(fixture_v3(maximum));self.assertEqual(r['count'],count)
            self.assertEqual(r['source_version'],3);self.assertEqual(len(r['samples'][0]['fences']),32)
            self.assertEqual(r['samples'][0]['window_reads'],count+193)
            self.assertEqual(r['linux_model_unique_candidate'],not maximum) # Max fixture intentionally has mismatched fence pitch/extent.
            self.assertFalse(r['reference_count_discrepancy_resolved'])
            self.assertFalse(r['unique_fence_proven']);self.assertFalse(r['ownership']);self.assertFalse(r['hardware_authority'])
            self.assertEqual(r['maps'],10)
    def test_upper_change_all_ones_and_missing_rows_refuse(self):
        text=fixture_v3()
        for bad in (change_v3(text,81,1,1),change_v3(text,66,0xffffffff),change_v3(text,85,1153),
                    text.replace('upper=linux-model-32','upper=unknown'),text.replace('fences=32','fences=16'),
                    text.replace('meta: v=3 phase=1','meta: v=2 phase=1'),text.replace('v=3','v=2')):
            self.assertIsNone(backing_result(bad))
    def test_upper_overlap_removes_model_candidate_uniqueness(self):
        text=fixture_v3();duplicate=(0x41f02b<<32)|1
        for phase in (0,1):text=change_v3(text,81,duplicate,phase)
        r=backing_result(text)
        self.assertEqual(r['lower_fence_candidates'],[0]);self.assertEqual(r['linux_model_fence_candidates'],[0,31])
        self.assertFalse(r['linux_model_unique_candidate']);self.assertFalse(r['unique_fence_proven'])
    def test_config_is_separate_and_requires_cpu_modifier(self):
        conf=boot.limine_config('x','ram-usb-v3-hsw-laptop-probe-1234abcd-auto-return')
        for token in ('srvros.hsw-fences=linux-model-32','srvros.hsw-cpu-control=read-only','srvros.capture=haswell-backing'):self.assertIn(token,conf)
        for version in (1,2):self.assertNotIn('srvros.hsw-fences=',boot.limine_config('x',f'ram-usb-v{version}-hsw-laptop-probe-1234abcd-auto-return'))
        for extra in ({},{'haswell_backing_capture':True},{'haswell_tilectl':True}):
            with self.assertRaises(ValueError):boot.stage(haswell_linux_fences=True,laptop_probe_volume='1234abcd',**extra)
        with self.assertRaises(ValueError):boot.stage(haswell_linux_fences=True,haswell_tilectl=True,haswell_backing_capture=True,laptop_probe_volume='1234abcd',power_telemetry=True)
        for token in ('srvros.power=','srvros.aml=','srvros.battery=','srvros.ap=','srvros.idle='):self.assertNotIn(token,conf)


if __name__=='__main__':unittest.main()
