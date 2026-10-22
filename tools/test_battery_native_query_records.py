import unittest
from battery_native_query_records import check
from test_battery_query_records import PROOF as STUDY

PROOF=(b'laptop-capture: battery media parked=yes fault-reset=armed\n'+
       STUDY[:STUDY.index(b'battery-query: state=')].replace(b'policy=1',b'policy=2')+
       b'battery-native-query: outcome=returned-failure code=255 generation=2 state=6 error=5 fresh=0 valid=1; retired=yes parked=1\n'
       b'battery-query-capture: returned=yes clean=yes; physical accuracy NOT_TESTED\n'
       b'laptop-capture: completed=yes\n')

class NativeQueryProof(unittest.TestCase):
    def test_vm_and_native_profile(self):
        self.assertEqual(check(PROOF,vm=True)['code'],255)
        native=PROOF.replace(b'0000062f',b'0000189c').replace(b'0000062e',b'0000189e')
        self.assertEqual(check(native)['generation'],2)
        with self.assertRaises(AssertionError):check(PROOF)
        with self.assertRaises(AssertionError):check(native,vm=True)
    def test_early_and_policy(self):
        early=PROOF.replace(b'generation=2',b'generation=0').replace(b'valid=1',b'valid=0')
        self.assertFalse(check(early,vm=True)['cache_valid'])
        with self.assertRaises(AssertionError):check(early.replace(b'valid=0',b'valid=1'),vm=True)
        for code,kind in ((16,5),(128,6)):
            proof=PROOF.replace(b'code=255',b'code='+str(code).encode()).replace(b'class=4',b'class='+str(kind).encode())
            self.assertEqual(check(proof,vm=True)['code'],code)
    def test_missing_duplicate_and_torn(self):
        for row in PROOF.splitlines(keepends=True):
            with self.subTest(row=row):
                with self.assertRaises(AssertionError):check(PROOF.replace(row,b''),vm=True)
                with self.assertRaises(AssertionError):check(PROOF+row,vm=True)
        with self.assertRaises(AssertionError):check(PROOF.rstrip(b'\n'),vm=True)
    def test_invalid_receipts(self):
        for a,b in ((b'policy=2',b'policy=1'),(b'code=255',b'code=32'),(b'class=4',b'class=5'),
                    (b'transfer=1',b'transfer=0'),(b'aml_returns=1',b'aml_returns=0'),(b'effects=0',b'effects=1'),
                    (b'completed_after=10',b'completed_after=8'),(b'notifications_after=4',b'notifications_after=2'),
                    (b'blocked=0',b'blocked=96'),(b'fresh=0',b'fresh=1'),(b'heap=0',b'heap=1'),
                    (b'detached=1',b'detached=0'),(b'state=6',b'state=4'),(b'parked=1',b'parked=0'),
                    (b'outcome=returned-failure',b'outcome=not-exercised'),(b'clean=yes',b'clean=NO')):
            with self.subTest(a=a),self.assertRaises(AssertionError):check(PROOF.replace(a,b),vm=True)
    def test_order_and_failure(self):
        rows=PROOF.splitlines(keepends=True)
        with self.assertRaises(AssertionError):check(b''.join([rows[1],rows[0],*rows[2:]]),vm=True)
        for bad in (b'battery: FAILED test',b'battery: HALTED test',b'battery-quarantine: retained=yes'):
            with self.assertRaises(AssertionError):check(PROOF+bad+b'\n',vm=True)

if __name__=='__main__':unittest.main()
