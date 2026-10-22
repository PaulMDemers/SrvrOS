import unittest
from battery_native_smbus_records import check
from test_battery_smbus_records import proof,RESOURCE,RETIRE

PROOF=(b'laptop-capture: battery media parked=yes fault-reset=armed\n'+
       proof('late-alarm').split(RESOURCE)[0]+
       b'battery-native-smbus: snapshot generation=3 events=3 valid=1 fresh=1\n'
       b'battery-smbus-return: idle=1 parked=1; sources_masked=1\n'+RESOURCE+b'\n'+
       b'battery: foreign-event restored enable=0x000000000000062f original=0x0000000000000040 protected_writes=1\n'
       b'battery: foreign-event restored enable=0x000000000000062e original=0x0000000000000004 protected_writes=1\n'+RETIRE+b'\n'+
       b'battery-native-smbus: outcome=completed events=3 alarms=1 acknowledgements=1 unknown=0 invalid_through=2 generation=3 state=4 error=0 fresh=0 valid=1; retired=yes parked=1\n'
       b'battery-smbus-capture: returned=yes clean=yes; physical accuracy NOT_TESTED\n'
       b'laptop-capture: completed=yes\n')

class Proof(unittest.TestCase):
    def test_profile(self):
        self.assertEqual(check(PROOF,vm=True)['events'],3)
        native=PROOF.replace(b'0000062f',b'0000189c').replace(b'0000062e',b'0000189e')
        self.assertEqual(check(native)['generation'],3)
        with self.assertRaises(AssertionError):check(native,vm=True)
        with self.assertRaises(AssertionError):check(PROOF)
    def test_missing_duplicate_torn(self):
        for row in PROOF.splitlines(keepends=True):
            with self.subTest(row=row):
                with self.assertRaises(AssertionError):check(PROOF.replace(row,b'',1),vm=True)
                with self.assertRaises(AssertionError):check(PROOF+row,vm=True)
        with self.assertRaises(AssertionError):check(PROOF.rstrip(b'\n'),vm=True)
    def test_false_completion(self):
        for old,new in ((b'outcome=completed',b'outcome=not-exercised'),(b'events=3',b'events=4'),
                        (b'alarms=1',b'alarms=0'),(b'acknowledged=1',b'acknowledged=0'),
                        (b'unknown=0',b'unknown=1'),(b'invalid_through=2',b'invalid_through=3'),
                        (b'snapshot generation=3',b'snapshot generation=2'),(b'fresh=1',b'fresh=0'),
                        (b'cached=2',b'cached=4'),(b'queries=3',b'queries=2'),(b'poison=0',b'poison=1'),
                        (b'sources_masked=1',b'sources_masked=0'),(b'detached=1',b'detached=0'),
                        (b'aml=0',b'aml=1'),(b'parked=1',b'parked=0'),(b'retired=yes',b'retired=NO')):
            with self.subTest(old=old):
                self.assertIn(old,PROOF)
                with self.assertRaises(AssertionError):check(PROOF.replace(old,new),vm=True)
        for bad in (b'battery: FAILED protocol',b'battery: HALTED lifecycle',b'battery-query-policy: refused',b'battery-smbus: FAILED ack'):
            with self.assertRaises(AssertionError):check(PROOF+bad+b'\n',vm=True)
    def test_order(self):
        rows=PROOF.splitlines(keepends=True)
        with self.assertRaises(AssertionError):check(b''.join([rows[1],rows[0],*rows[2:]]),vm=True)
        event=next(row for row in rows if row.startswith(b'battery-smbus: event=3'))
        with self.assertRaises(AssertionError):check(PROOF.replace(event,b'')+event,vm=True)

if __name__=='__main__':unittest.main()
