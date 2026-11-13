import os,subprocess,unittest
from pathlib import Path
from control_timing import validate_control_timing

class ControlTiming(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        exe=Path(__file__).resolve().parents[1]/('build/tests/desktop-session-host'+('.exe' if os.name=='nt' else ''))
        cls.raw=subprocess.check_output([str(exe),'--control-proof'])
        cls.wide=subprocess.check_output([str(exe),'--control-wide'])
        cls.late=subprocess.check_output([str(exe),'--control-late'])

    def test_actual_writer_and_async_retention(self):
        d=validate_control_timing([self.raw,self.raw],419979)
        self.assertEqual(d['sequence'],1)
        self.assertEqual(sum(d['phases'].values()),411886)
        self.assertEqual(d['phases']['audio_us'],410000)
        self.assertEqual(d['inner_us']['wait'],409400)
        self.assertEqual(d['inner_us']['snapshot'],200)
        self.assertEqual(d['inner_us']['submit'],300)
        self.assertEqual(d['scheduler']['audio']['ready_us'],280000)
        self.assertEqual(d['audio']['wait_result'],0)

    def test_four_rows_and_existing_byte_limits(self):
        rows=[r for r in self.raw.splitlines() if r.startswith(b'desktop-loop-control')]
        self.assertEqual(len(rows),4)
        self.assertLess(sum(len(r)+1 for r in rows),1024)
        wide=self.wide.splitlines();self.assertEqual(len(wide),4)
        self.assertTrue(all(len(r)+1<1024 for r in wide))
        self.assertNotIn(b'record-overflow',self.wide)
        late=[r for r in self.late.splitlines() if r.startswith(b'desktop-loop-control')]
        self.assertLess(sum(len(r)+1 for r in late),1024)
        d=validate_control_timing([self.late],419979)
        self.assertEqual(d['sequence'],50000)
        self.assertEqual(d['inner_us']['wait'],409400)

    def test_missing_truncated_wrong_sequence_and_conflict(self):
        lines=self.raw.splitlines(keepends=True)
        rows=[r for r in lines if r.startswith(b'desktop-loop-control')]
        for r in rows:
            for bad in [self.raw.replace(r,b''),self.raw.replace(r,r[:-1]),self.raw.replace(r,r.replace(b'seq=1',b'seq=9'))]:
                with self.assertRaises(AssertionError):validate_control_timing([bad])
        header=rows[0]
        with self.assertRaises(AssertionError):validate_control_timing([self.raw+header.replace(b'state=0',b'state=1')])
        with self.assertRaises(AssertionError):validate_control_timing([self.raw],419980)

    def test_clock_identity_execution_and_scheduler_corruption(self):
        cases=[(b'version=1 valid=1',b'version=2 valid=1'),(b'valid=1 base_us=100',b'valid=0 base_us=100'),
            (b'base_us=100 edges=',b'base_us=101 edges='),(b'edges=100,1100,411100,411886',b'edges=100,1100,411100,411887'),
            (b'edges=100,1100,411100,411886',b'edges=100,1100,900,411886'),
            (b'flags=831',b'flags=827'),(b'flags=831',b'flags=2879'),(b'state=0 snapshot=',b'state=1 snapshot='),
            (b'wait=1400,410800',b'wait=410801,410800'),(b'child=8,8',b'child=8,9'),
            (b'waited=8',b'waited=9'),(b'wait_flags=1',b'wait_flags=2'),(b' result=0',b' result=8'),
            (b'snapshot=150,350',b'snapshot=150,4500'),(b'submit=400,700',b'submit=300,700'),
            (b'notice=410850,410900',b'notice=410700,410900'),(b'region=2',b'region=3'),
            (b'data=1,1210,411190',b'data=1,1199,411190'),
            (b'data=1,1210,411190,0,42,120000,280000',b'data=1,1210,411190,0,42,130000,290000'),
            (b',280000,130000,410000,13,0,1,0,0',b',280000,130000,410001,13,0,1,0,0'),
            (b'waited=8',b'waited=18446744073709551616'),(b' result=0',b' result=-9223372036854775809')]
        for before,after in cases:
            with self.subTest(before=before):
                self.assertIn(before,self.raw)
                with self.assertRaises(AssertionError):validate_control_timing([self.raw.replace(before,after)])

    def test_periodic_final_and_signed_wait_result(self):
        periodic=self.raw.replace(b'final=1',b'final=0')
        validate_control_timing([periodic,self.raw])
        self.assertEqual(validate_control_timing([periodic],require_final=False)['control']['final'],0)
        with self.assertRaises(AssertionError):validate_control_timing([periodic])
        negative=self.raw.replace(b' result=0',b' result=-4')
        self.assertEqual(validate_control_timing([negative])['audio']['wait_result'],-4)
        with self.assertRaises(AssertionError):validate_control_timing([periodic,negative])

if __name__=='__main__':unittest.main()
