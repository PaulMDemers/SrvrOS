import base64
import struct
import subprocess
import unittest
from unittest.mock import patch
from pathlib import Path

import hardware_boot as boot
from hardware_haswell_alias import alias_result, alias_preinit_result, decode_pages
from test_hardware_haswell_cache import fixture, mutate


def fixture_alias(compact=False):
    text, pin = fixture()
    # A sparse raw RAM root (one four-entry chain, target on the 4KiB leaf).
    pages = [(0x100000, 0, {256:0x101003}), (0x101000, 1, {3:0x102003}),
             (0x102000, 2, {502:0x103003}), (0x103000, 3, {400:0xfed90013})]
    data = bytearray()
    for index, (pa, level, nonzero) in enumerate(pages):
        data += struct.pack('<QB', pa | (compact and index == 0), level)
        words = [nonzero.get(i, 0) for i in range(512)]
        i = 0
        while i < 512:
            n = 1; step = 0
            if i+1 < 512:
                step = (words[i+1]-words[i]) & ((1<<64)-1);n = 2
                while i+n < 512 and words[i+n] == (words[i]+step*n) & ((1<<64)-1):n += 1
            if compact:
                first = words[i]
                widths = [(value.bit_length()+7)//8 for value in (first, step)]
                data += struct.pack('<H', (widths[0]*9+widths[1])*512+n-1)
                data += first.to_bytes(widths[0], 'little')+step.to_bytes(widths[1], 'little')
            else:
                data += struct.pack('<HQQ', n, words[i], step)
            i += n
    meta = f'hsw-alias: v=1 status=1 step=6 pages=4 reads=2048,2048 bytes={len(data)},{len(data)} target_leaves=1 root=0x0000000000100000 nx=0x0000000004100000,0x0000000004100000 max=0x0000000080000008,0x0000000080000008 efer=0x0000000000000d00,0x0000000000000d00 irq=0x0000000000000202,0x0000000000000202 begin=5100 end=5900 cursor=0x0000000000103000,511,0x0000000000000000,0x0000000000000000 table_cap=64 entry_cap=32768 data_cap=4096 limit_us=100000 scope=sole-managed-root writes=0 maps=0 ownership=0\n'
    for phase in (0,1):
        for index, at in enumerate(range(0,len(data),512)):
            meta += f'hsw-alias-data: v=1 phase={phase} chunk={index} data='+base64.b64encode(data[at:at+512]).decode()+'\n'
    return text+meta, pin


class AliasTests(unittest.TestCase):
    def test_compact_receipt_preserves_complete_legacy_proof(self):
        legacy, pin = fixture_alias()
        compact, _ = fixture_alias(compact=True)
        with patch('hardware_haswell_firmware.PIN', pin):
            old = alias_result(legacy)
            new = alias_result(compact)
            self.assertLess(new.pop('raw_encoded_bytes_per_pass'), old.pop('raw_encoded_bytes_per_pass'))
            self.assertEqual(old, new)
            chunk = next(line for line in compact.splitlines() if line.startswith('hsw-alias-data: v=1 phase=1'))
            prefix, raw = chunk.split('data=')
            data = bytearray(base64.b64decode(raw))
            data[0] ^= 1
            changed = prefix+'data='+base64.b64encode(data).decode()
            self.assertIsNone(alias_result(compact.replace(chunk, changed)))

    def test_large_producer_round_trip_stays_under_original_cap(self):
        exe = Path('build/tests/hsw-alias-core')
        if not exe.exists():
            self.skipTest('build actual encoder target first')
        both = subprocess.check_output([str(exe), '--dump-large'])
        size = len(both)//2
        self.assertLessEqual(size, 4096)
        self.assertEqual(both[:size], both[size:])
        expected = [[0]*512 for _ in range(42)]
        expected[0][0], expected[1][0] = 0x101003, 0x102003
        for page in range(3, 42):
            expected[2][page-3] = (0x100000+4096*page)|3
            base = 0x8000000020000000+page*0x200000
            for begin, end, flags in ((13, 76, 0x23), (91, 118, 0x63), (126, 502, 0x23)):
                expected[page][begin:end] = [base+4096*i+flags for i in range(begin, end)]
            expected[page][509:] = [0xfffffffffffffff0, 0, 0x10]
        pages = decode_pages(both[:size], 42)
        self.assertEqual(list(pages), [0x100000+4096*i for i in range(42)])
        self.assertEqual([page['raw'] for page in pages.values()], expected)
        self.assertEqual([page['level'] for page in pages.values()], [0, 1, 2]+[3]*39)

    def test_compact_decoder_refuses_malformed_runs(self):
        header = struct.pack('<QB', 0x100001, 0)
        zeros = struct.pack('<H', 511)
        invalid = (
            header+zeros+b'\0',                         # trailing bytes
            header+struct.pack('<H', 81*512+511),        # impossible operand width
            header+struct.pack('<H', 9*512+511),         # missing first operand
            header+struct.pack('<H', 9*512+511)+b'\0',   # nonminimal first operand
            header+struct.pack('<H', 512+511)+b'\0',     # nonminimal step
            header+struct.pack('<H', 512)+b'\x01',        # singleton with step
            header+b'\x00\x00'+zeros,                  # run crosses page boundary
            header.replace(b'\x01', b'\x03', 1)+zeros,  # unknown PA tag
        )
        for data in invalid:
            with self.subTest(data=data):
                self.assertRaises(ValueError, decode_pages, data, 1)
        self.assertRaises(ValueError, decode_pages, header+zeros+header+zeros, 2)

    def test_bounded_preparation_receipt_and_legacy_compatibility(self):
        text,pin=fixture_alias()
        prep='hsw-alias-preparation: v=1 reads=2048 pages=4 begin=5100 end=5200 table_cap=64 entry_cap=32768 limit_us=100000 shared_deadline=1 pte_writes=0 retries=0\n'
        with patch('hardware_haswell_firmware.PIN',pin):
            self.assertIsNone(alias_result(text)['preparation'])
            self.assertEqual(alias_result(text+prep)['preparation']['reads'],2048)
            for bad in (prep+prep,prep.replace('reads=2048','reads=32769'),prep.replace('pages=4','pages=65'),prep.replace('reads=2048','reads=2047'),prep.replace('begin=5100','begin=5101'),prep.replace('end=5200','end=5100'),prep.replace('end=5200','end=5900'),prep.replace('shared_deadline=1','shared_deadline=0'),prep.replace('pte_writes=0','pte_writes=1'),prep.replace('retries=0','retries=1')):
                self.assertIsNone(alias_result(text+bad),bad)

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
            for old,new in (('root=0x0000000000100000','root=100000'),('root=0x0000000000100000','root=0x100000'),('nx=0x0000000004100000,0x0000000004100000','nx=0x1000000004100000,0x1000000004100000'),('max=0x0000000080000008,0x0000000080000008','max=0x1000000080000008,0x1000000080000008'),('pages=4','pages=65'),('reads=2048,2048','reads=2049,2048'),('root=0x0000000000100000','root=0x0000000000101000'),('nx=0x0000000004100000,0x0000000004100000','nx=0x0000000004100000,0x0000000000000000'),('efer=0x0000000000000d00,0x0000000000000d00','efer=0x0000000000000d00,0x0000000000000500'),('irq=0x0000000000000202,0x0000000000000202','irq=0x0000000000000202,0x0000000000000002'),('begin=5100','begin=4999'),('target_leaves=1','target_leaves=0'),('completed=yes','completed=NO')):
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

    def test_preinit_epoch_requires_complete_containment_and_single_report(self):
        text,pin=fixture_alias()
        epoch='hsw-alias-epoch: v=1 phase=pre-user-init begin=1 end=20000 report=30000 attempted=1\n'
        with patch('hardware_haswell_firmware.PIN',pin):
            self.assertIsNone(alias_preinit_result(text))
            p=alias_preinit_result(text+epoch);self.assertEqual(p['capture_epoch'],'pre-user-init')
            self.assertFalse(p['live_post_init_admission'])
            for bad in (epoch+epoch,epoch.replace('begin=1','begin=30000'),epoch.replace('end=20000','end=5'),epoch.replace('report=30000','report=19999')):
                self.assertIsNone(alias_preinit_result(text+bad))
        conf=boot.limine_config('x','ram-usb-v4-hfw-laptop-probe-1234abcd-auto-return')
        self.assertIn('srvros.hsw-cache=alias-nx-preinit',conf)
        self.assertRaises(ValueError,boot.stage,haswell_alias_preinit=True)

    def test_profile_keeps_legacy_and_requires_all_explicit_flags(self):
        conf=boot.limine_config('x','ram-usb-v3-hfw-laptop-probe-1234abcd-auto-return')
        self.assertIn('srvros.hsw-cache=alias-nx',conf)
        self.assertNotIn('alias-nx',boot.limine_config('x','ram-usb-v2-hfw-laptop-probe-1234abcd-auto-return'))
        self.assertRaises(ValueError,boot.stage,haswell_alias_context=True)
        self.assertRaises(ValueError,boot.stage,haswell_alias_context=True,haswell_firmware_capture=True)
        self.assertRaises(ValueError,boot.stage,haswell_alias_context=True,haswell_firmware_capture=True,haswell_cache_context=True,laptop_probe_volume='1234abcd',aml_capture=True)


if __name__=='__main__':unittest.main()
