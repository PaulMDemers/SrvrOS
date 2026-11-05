import base64
import struct
import subprocess
import unittest
from unittest.mock import patch
from pathlib import Path

import hardware_boot as boot
from hardware_haswell_alias import alias_result, decode_pages
from test_hardware_haswell_cache import fixture, mutate


def fixture_alias():
    text, pin = fixture()
    # A sparse raw RAM root (one four-entry chain, target on the 4KiB leaf).
    pages = [(0x100000, 0, {256:0x101003}), (0x101000, 1, {3:0x102003}),
             (0x102000, 2, {502:0x103003}), (0x103000, 3, {400:0xfed90013})]
    data = bytearray()
    for pa, level, nonzero in pages:
        data += struct.pack('<QB', pa, level)
        words = [nonzero.get(i, 0) for i in range(512)]
        i = 0
        while i < 512:
            n = 1; step = 0
            if i+1 < 512:
                step = (words[i+1]-words[i]) & ((1<<64)-1);n = 2
                while i+n < 512 and words[i+n] == (words[i]+step*n) & ((1<<64)-1):n += 1
            data += struct.pack('<HQQ', n, words[i], step);i += n
    meta = f'hsw-alias: v=1 status=1 step=6 pages=4 reads=2048,2048 bytes={len(data)},{len(data)} target_leaves=1 root=0x0000000000100000 nx=0x0000000004100000,0x0000000004100000 max=0x0000000080000008,0x0000000080000008 efer=0x0000000000000d00,0x0000000000000d00 irq=0x0000000000000202,0x0000000000000202 begin=5100 end=5900 cursor=0x0000000000103000,511,0x0000000000000000,0x0000000000000000 table_cap=64 entry_cap=32768 data_cap=4096 limit_us=100000 scope=sole-managed-root writes=0 maps=0 ownership=0\n'
    for phase in (0,1):
        for index, at in enumerate(range(0,len(data),512)):
            meta += f'hsw-alias-data: v=1 phase={phase} chunk={index} data='+base64.b64encode(data[at:at+512]).decode()+'\n'
    return text+meta, pin


class AliasTests(unittest.TestCase):
    def test_synthetic_pin_full_raw_graph_and_scope(self):
        text,pin=fixture_alias();self.assertIsNone(alias_result(text))
        with patch('hardware_haswell_firmware.PIN',pin):p=alias_result(text)
        self.assertIsNotNone(p);self.assertEqual(p['status'],'OBSERVED_MANAGED_ROOT_CONTEXT')
        self.assertEqual(p['pages'],4);self.assertEqual(len(p['raw_pages'][0]['raw']),512)
        self.assertTrue(p['sole_managed_root_coverage']);self.assertTrue(p['nx_enablement_observed'])
        self.assertEqual(len(p['target_leaves']),1)
        for k in ('all_hardware_aliases_proven','resource_lease_proven','native_mapping_authorized','hardware_authority','ownership'):self.assertFalse(p[k])

    def test_control_clock_count_and_every_chunk(self):
        text,pin=fixture_alias()
        with patch('hardware_haswell_firmware.PIN',pin):
            for old,new in (('root=0x0000000000100000','root=100000'),('root=0x0000000000100000','root=0x100000'),('pages=4','pages=65'),('reads=2048,2048','reads=2049,2048'),('root=0x0000000000100000','root=0x0000000000101000'),('nx=0x0000000004100000,0x0000000004100000','nx=0x0000000004100000,0x0000000000000000'),('efer=0x0000000000000d00,0x0000000000000d00','efer=0x0000000000000d00,0x0000000000000500'),('irq=0x0000000000000202,0x0000000000000202','irq=0x0000000000000202,0x0000000000000002'),('begin=5100','begin=4999'),('target_leaves=1','target_leaves=0'),('completed=yes','completed=NO')):
                self.assertIsNone(alias_result(text.replace(old,new)),(old,new))
            for line in text.splitlines():
                if line.startswith('hsw-alias'):
                    self.assertIsNone(alias_result(text.replace(line+'\n','')))
                    self.assertIsNone(alias_result(text+line+'\n'))

    def test_raw_encoding_refuses_missing_extra_cyclic_or_changed_tables(self):
        text,pin=fixture_alias()
        with patch('hardware_haswell_firmware.PIN',pin):
            for prefix in ('hsw-alias-data: v=1 phase=0','hsw-alias-data: v=1 phase=1'):
                line=next(x for x in text.splitlines() if x.startswith(prefix))
                data=bytearray(base64.b64decode(line.split('data=')[1]));data[0]^=16
                self.assertIsNone(alias_result(text.replace(line,line.split('data=')[0]+'data='+base64.b64encode(data).decode())))
        for data in (b'',struct.pack('<QB',0x100000,0)+struct.pack('<HQQ',513,0,0),struct.pack('<QB',0x100001,0)+struct.pack('<HQQ',512,0,0)):
            self.assertRaises(ValueError,decode_pages,data,1)

    def test_actual_adapter_refusal_and_native_fixture(self):
        exe=Path('build/tests/hsw-alias-adapter')
        if not exe.exists():self.skipTest('build actual adapter target first')
        refused=subprocess.check_output([str(exe)],text=True)
        self.assertEqual(alias_result(refused)['status'],'REFUSED')

    def test_profile_keeps_legacy_and_requires_all_explicit_flags(self):
        conf=boot.limine_config('x','ram-usb-v3-hfw-laptop-probe-1234abcd-auto-return')
        self.assertIn('srvros.hsw-cache=alias-nx',conf)
        self.assertNotIn('alias-nx',boot.limine_config('x','ram-usb-v2-hfw-laptop-probe-1234abcd-auto-return'))
        self.assertRaises(ValueError,boot.stage,haswell_alias_context=True)
        self.assertRaises(ValueError,boot.stage,haswell_alias_context=True,haswell_firmware_capture=True)
        self.assertRaises(ValueError,boot.stage,haswell_alias_context=True,haswell_firmware_capture=True,haswell_cache_context=True,laptop_probe_volume='1234abcd',aml_capture=True)


if __name__=='__main__':unittest.main()
