import unittest
from battery_controls_smoke import check

BASE=b'\n'.join([
    *(f'battery-ui: request={op} result=0 serial={serial} epoch={epoch}'.encode() for op,serial,epoch in ((1,1,0),(2,2,1),(1,3,1),(2,4,2))),
    *(f'battery-control: dispatched serial={serial} operation={op} epoch={epoch} result=0'.encode() for op,serial,epoch in ((1,1,0),(2,2,1),(1,3,1),(2,4,2))),
    *(f'battery-ui: sampled cycle={n} epoch={n} generation=2'.encode() for n in (1,2)),
    *(f'battery-ui: stopped cycle={n} epoch={n}'.encode() for n in (1,2)),
    b'battery-ui: PASS cycles=2 requests=4 stopped=yes',
    b'desktop-controls: completed=yes reaped=1 errors=0 cached_reads=5',
    *(f'battery-service: retired epoch={n} attempts=2 rejected=0 queries=3 notifications=4 attached=1 detached=1 heap=0 errors=0 denied=0'.encode() for n in (1,2)),
    b'battery-controls: PASS requests=4 descendants=2 epoch=2; revoked=yes retired=yes',b''])

class Records(unittest.TestCase):
    def test_complete(self):
        self.assertEqual(check(BASE)['lifetimes'],2)
    def test_missing_duplicate_or_malformed_proof(self):
        for row in BASE.splitlines(keepends=True):
            with self.subTest(row=row):
                with self.assertRaises(AssertionError):check(BASE.replace(row,b'',1))
                with self.assertRaises(AssertionError):check(BASE+row)
        for changed in (BASE.replace(b'epoch=2;',b'epoch=3;'),BASE.replace(b'generation=2',b'generation=1'),
                        BASE.replace(b'serial=3',b'serial=2'),BASE.replace(b'result=0',b'result=-1'),
                        BASE.replace(b'detached=1',b'detached=0'),BASE.replace(b'revoked=yes',b'revoked=NO'),
                        BASE+b'battery-controls: malformed\n',BASE+b'battery-ui: FAIL phase=5\n'):
            with self.assertRaises(AssertionError):check(changed)

if __name__=='__main__':unittest.main()
