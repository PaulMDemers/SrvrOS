import unittest
from battery_query_records import check_query, check_query_finish

PROOF = b'''battery-query-policy: policy=1 code=255 class=4; dispatch=refused
battery-query-return: sequence=3 generation=2 code=255 transfer=1 aml_calls=1 aml_returns=1 effects=0 completed_before=8 completed_after=10 notifications_before=3 notifications_after=4 idle=1; refused=yes
battery-recovery-resources: known=1 lease=0 ec_phase=6 poison=0 sample=0 waiting=0 smbus=0 api=0 aml=0 mutex=0 pending=0 facs=0 blocked=0
battery: foreign-event restored enable=0x000000000000062f original=0x0000000000000040 protected_writes=0
battery: foreign-event restored enable=0x000000000000062e original=0x0000000000000004 protected_writes=0
battery-service: retired epoch=1 attempts=2 rejected=0 queries=3 notifications=4 attached=1 detached=1 heap=0 errors=0 denied=0
battery-query: state=6 error=5 generation=2 fresh=0 before=1 after=1 io_owned=1; retired=yes outcome=query-refused
'''

class ReturnedQuery(unittest.TestCase):
    def test_valid_notify_and_no_notify(self):
        self.assertEqual(check_query(PROOF)['notifications_drained'],1)
        quiet=PROOF.replace(b'aml_calls=1 aml_returns=1',b'aml_calls=0 aml_returns=0').replace(b'notifications_before=3',b'notifications_before=4')
        self.assertEqual(check_query(quiet)['notifications_drained'],0)
        self.assertEqual(check_query(quiet.replace(b'code=255',b'code=128'))['code'],128)
    def test_missing_duplicate_and_torn(self):
        for line in PROOF.splitlines(keepends=True):
            with self.assertRaises(AssertionError):check_query(PROOF.replace(line,b''))
            with self.assertRaises(AssertionError):check_query(PROOF+line)
        with self.assertRaises(AssertionError):check_query(PROOF.rstrip(b'\n'))
    def test_uncertain_or_false_retirement(self):
        for a,b in ((b'transfer=1',b'transfer=0'),(b'aml_returns=1',b'aml_returns=0'),
                    (b'effects=0',b'effects=1'),(b'completed_after=10',b'completed_after=8'),
                    (b'notifications_after=4',b'notifications_after=2'),(b'code=255',b'code=32'),
                    (b'blocked=0',b'blocked=96'),(b'fresh=0',b'fresh=1'),(b'heap=0',b'heap=1'),
                    (b'detached=1',b'detached=0'),(b'io_owned=1',b'io_owned=0'),
                    (b'state=6',b'state=4'),(b'after=1 io_owned',b'after=0 io_owned'),
                    (b'original=0x0000000000000040',b'original=0x0000000000000000')):
            with self.subTest(a=a),self.assertRaises(AssertionError):check_query(PROOF.replace(a,b))
        for text in (b'battery: HALTED test',b'battery-quarantine: retained=yes',b'battery-desktop: PASS'):
            with self.assertRaises(AssertionError):check_query(PROOF+text+b'\n')
    def test_live_completion(self):
        finish=(b'workspace: unmounted; USB writes disabled\n'
                b'battery-query: saved=yes unmounted=yes; runtime_retired=yes\n'
                b'battery-query: returning after storage drain; test outcome=returned-failure\n')
        check_query_finish(PROOF+finish)
        for line in finish.splitlines(keepends=True):
            with self.assertRaises((AssertionError,ValueError)):check_query_finish(PROOF+finish.replace(line,b''))
        for line in finish.splitlines(keepends=True)[1:]:
            with self.assertRaises(AssertionError):check_query_finish(PROOF+finish+line)
        with self.assertRaises(AssertionError):check_query_finish(PROOF+finish.replace(b'saved=yes',b'saved=NO'))
        with self.assertRaises(AssertionError):check_query_finish((PROOF+finish).rstrip(b'\n'))

    def test_native_policy(self):
        native=PROOF.replace(b'policy=1 code=255 class=4',b'policy=2 code=255 class=4')
        self.assertEqual(check_query(native)['policy'],2)
        for code,kind in ((16,5),(128,6)):
            proof=native.replace(b'code=255',b'code='+str(code).encode()).replace(b'class=4',b'class='+str(kind).encode())
            self.assertEqual(check_query(proof)['code'],code)
        with self.assertRaises(AssertionError):check_query(native.replace(b'class=4',b'class=6'))
        with self.assertRaises(AssertionError):check_query(native.replace(b'policy=2',b'policy=0'))

    def test_order(self):
        lines=PROOF.splitlines(keepends=True)
        with self.assertRaises(AssertionError):check_query(b''.join([lines[1],lines[0],*lines[2:]]))

if __name__=='__main__':unittest.main()
