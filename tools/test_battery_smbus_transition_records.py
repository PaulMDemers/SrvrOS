import unittest
from battery_smbus_transition_records import check
from test_battery_native_smbus_records import PROOF as SHORT

HEADER=b'laptop-capture: battery media parked=yes fault-reset=armed\nbattery-smbus-transitions: begin phases=3 stable=3 phase_seconds=120 event_limit=4096; USB=parked\n'
RETIRE=SHORT[SHORT.index(b'battery-smbus-return:'):SHORT.index(b'battery-native-smbus: outcome=')].replace(b'attempts=3',b'attempts=9').replace(b'queries=3',b'queries=144')
PHASES=b''.join((f'battery-smbus-transitions: accepted={i} stable=3 first_gen={i*3+1} last_gen={i*3+3} first_us={i*6000000+1} ac_us={i*6000000+4000001} start_us={i*6000000+4000002} end_us={i*6000000+4000100} ac={int(i!=1)} current_ua={-250000 if i==1 else 0} relative_soc=86 voltage_uv=8000000 capacity=3500000 full=4000000 design=5000000 unit=1 valid=0x000000000000000f\n').encode() for i in range(3))
PROOF=HEADER+RETIRE+PHASES+b'battery-smbus-transitions: outcome=completed phases=3 events=144 alarms=0 acknowledgements=0 unknown=0 generation=9 invalid_through=0 retired=yes parked=1\nbattery-smbus-transitions-capture: returned=yes clean=yes; physical accuracy NOT_TESTED\nlaptop-capture: completed=yes\n'

class Proof(unittest.TestCase):
    def test_valid_and_collector(self):
        self.assertEqual(check(PROOF,vm=True)['generation'],9)
        native=PROOF.replace(b'0000062f',b'0000189c').replace(b'0000062e',b'0000189e')
        from hardware_desktop import battery_smbus_transitions_result
        boot=b'laptop-capture: boot options=srvros.capture=battery-smbus-transitions\n'
        self.assertEqual(battery_smbus_transitions_result((boot+native).decode())['events'],144)
        self.assertIsNone(battery_smbus_transitions_result((boot+native).replace(b'current_ua=-250000',b'current_ua=0').decode()))
        self.assertIsNone(battery_smbus_transitions_result((boot+native).replace(b'capture=battery-smbus-transitions',b'capture=battery-smbus-transitions-other').decode()))
    def test_missing_duplicate_truncated(self):
        for row in PROOF.splitlines(keepends=True):
            if row.startswith(b'battery: foreign-event'):continue
            with self.subTest(row=row):
                with self.assertRaises(AssertionError):check(PROOF.replace(row,b'',1),vm=True)
                with self.assertRaises(AssertionError):check(PROOF.replace(row,row+row,1),vm=True)
        with self.assertRaises(AssertionError):check(PROOF[:-1],vm=True)
    def test_invalid_evidence(self):
        for before,after in ((b'last_gen=6',b'last_gen=7'),(b'current_ua=-250000',b'current_ua=0'),(b'ac=0',b'ac=1'),(b'full=4000000',b'full=0'),(b'valid=0x000000000000000f',b'valid=0x0000000000000007'),(b'events=144',b'events=4097'),(b'acknowledgements=0',b'acknowledgements=1'),(b'invalid_through=0',b'invalid_through=9'),(b'retired=yes',b'retired=NO'),(b'errors=0',b'errors=1')):
            with self.subTest(before=before):
                with self.assertRaises(AssertionError):check(PROOF.replace(before,after),vm=True)
    def test_incomplete_not_accepted(self):
        raw=(HEADER+RETIRE+PHASES.splitlines(keepends=True)[0]+PROOF[PROOF.index(b'battery-smbus-transitions: outcome='):]).replace(b'outcome=completed phases=3',b'outcome=incomplete phases=1').replace(b'clean=yes',b'clean=NO').replace(b'completed=yes',b'completed=NO')
        self.assertEqual(check(raw,vm=True,complete=False)['status'],'INCOMPLETE')
        with self.assertRaises(AssertionError):check(raw,vm=True)

if __name__=='__main__':unittest.main()
