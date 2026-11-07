import unittest
from unittest.mock import patch
from wifi_trim5_log import check,prior,plan,receipt,PROGRESS,field_trace,summary
from test_wifi_held_log import fixture as parent_fixture


def fixture(lna=12,polls=(1,1)):
    n=sum(polls)
    rows=[PROGRESS]+field_trace(polls)+[f'wifi-trim5-fields: status=0 valid=1 reads={22+n} writes=2 polls={n} blocks=2 submitted={24+n} phase=15 failed=0 raw_block_logged=0',f'wifi-trim5-value: trim=68 lna={lna} channel=161 bandwidth=20 raw_fields_logged=0']
    rows += [f'wifi-trim5-field-guard: index={i} mac=0x0000000000000000 core=0x00000000f000f200' for i in range(4)]
    rows += [f'wifi-trim5-field-dma: index={i} value=0x0000000040000050' for i in range(2)]
    rows += [receipt(op,row) for op,row in enumerate(plan(lna))]+[summary(lna)]
    return parent_fixture(lna).replace('srvros.wifi-held-channel=1','srvros.wifi-held-channel=1 srvros.wifi-trim5-channel=1')+'\n'.join(rows)+'\n'


class ProofTests(unittest.TestCase):
    def test_pinned_outer_plan(self):
        import struct
        from pathlib import Path
        blob=(Path(__file__).with_name('fixtures')/'mt7610-5ghz-trim-cycle.bin').read_bytes()
        actual=[(row[3],row[4],int(row[2])) for row in plan(12) if row[0]==13 and row[5] is not None]
        self.assertEqual(actual,list(struct.iter_unpack('<III',blob)))
    def test_complete(self):
        self.assertEqual(check(fixture())['operations'],11578)
        self.assertTrue(check(fixture(0,(2,3)))['trim5_restored'])
    def test_every_receipt(self):
        good=fixture();parent=prior(good)
        with patch('wifi_trim5_log.prior',return_value=parent):
            for row in good.splitlines():
                if not row.startswith('wifi-trim5'):continue
                for replacement in ('',row+'\n'+row+'\n'):
                    with self.assertRaises(ValueError):check(good.replace(row+'\n',replacement,1))
            with self.assertRaises(ValueError):check(good+'wifi-trim5-unknown: ok=1\n')
    def test_malformed(self):
        good=fixture();parent=prior(good)
        for a,b in [('phase=13','phase=12'),('trim=68','trim=67'),('submitted=26','submitted=25'),('phase=15 failed=0','phase=15 failed=1'),('raw_fields_logged=0','raw_fields_logged=1'),('restored=1','restored=0'),('cc=13','cc=1'),('residue=1008','residue=0'),('ok=1','ok=0'),('value=0x0000000040000050','value=0x00000000ffffffff')]:
            # Mutate the new proof alone; original parent still validates independently.
            before,after=good.split(PROGRESS,1)
            with patch('wifi_trim5_log.prior',return_value=parent):
                with self.assertRaises(ValueError):check(before+PROGRESS+after.replace(a,b))
        with self.assertRaises(ValueError):check(good.replace(PROGRESS+'\n','').replace('wifi-held-progress:',PROGRESS+'\nwifi-held-progress:',1))
    def test_parent_not_optional(self):
        with self.assertRaises(ValueError):check(fixture().replace('wifi-held: status=0','wifi-held: status=1'))
