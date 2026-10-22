#!/usr/bin/env python3
import unittest
from battery_smbus_workspace_records import check

def stopped(phase='read'):
    lines=[]
    for i in range(5):
        if i==4:lines.append(f'battery-vm-smbus-stop: phase={phase} generation=4 state=3 synchronous=0')
        alarm=int(i in (2,4))
        if alarm:lines.append(f'battery-smbus: invalidate generation={i} cached={i} valid=1 fresh=0')
        lines.append(f'battery-smbus: event={i+1} generation={i} status={192 if alarm else 128} alarm={alarm} address={11 if alarm else 0} data={4660 if alarm else 0} acknowledged={alarm} refresh={alarm}; aml=0 serialized=1')
    lines += [
        'battery-recovery-resources: known=1 lease=0 ec_phase=6 poison=0 sample=0 waiting=0 smbus=0 api=0 aml=0 mutex=0 pending=0 facs=0 blocked=0',
        'battery-service: retired epoch=1 attempts=4 rejected=0 queries=5 notifications=1 attached=1 detached=1 heap=0 errors=0 denied=0',
        'battery-desktop: FAIL outcome=service progress epoch=1',
        'battery-workspace: FAIL descendants=3; retired=yes mounted=yes',
        'battery-smbus-workspace: outcome=interrupted events=5 alarms=2 acknowledgements=2 unknown=0 invalid_through=4 generation=4 state=4 error=0 fresh=0; retired=yes native=disabled']
    return ('\n'.join(lines)+'\n').encode()

class ProofTests(unittest.TestCase):
    def test_retired_but_incomplete(self):
        for phase in ('read','ack'):
            result=check(stopped(phase),'smbus-stop-'+phase)
            self.assertFalse(result['workload_complete']);self.assertTrue(result['runtime_retired'])
    def test_missing_duplicate_and_torn(self):
        data=stopped();lines=data.splitlines(keepends=True)
        for i in range(len(lines)):
            with self.subTest(i=i):
                with self.assertRaises(AssertionError):check(b''.join(lines[:i]+lines[i+1:]),'smbus-stop-read')
        for prefix in (b'battery-smbus-workspace:',b'battery-vm-smbus-stop:',b'battery-smbus: event=',b'battery-service: retired '):
            row=next(r for r in lines if r.startswith(prefix))
            with self.assertRaises(AssertionError):check(data+row,'smbus-stop-read')
        with self.assertRaises(AssertionError):check(data[:-1],'smbus-stop-read')
    def test_false_retirement_freshness_and_counts(self):
        for old,new in ((b'lease=0',b'lease=1'),(b'fresh=0; retired',b'fresh=1; retired'),
                        (b'acknowledgements=2',b'acknowledgements=1'),(b'unknown=0',b'unknown=1'),
                        (b'queries=5',b'queries=4'),(b'generation=4 state=4',b'generation=5 state=4'),
                        (b'outcome=interrupted',b'outcome=complete')):
            with self.subTest(old=old):
                with self.assertRaises(AssertionError):check(stopped().replace(old,new),'smbus-stop-read')
    def test_stop_after_ack_is_not_race_proof(self):
        data=stopped();row=next(r for r in data.splitlines(keepends=True) if r.startswith(b'battery-vm-smbus-stop:'))
        with self.assertRaises(AssertionError):check(data.replace(row,b'')+row,'smbus-stop-read')
    def test_uncertain_is_not_retirement(self):
        data=b'battery-vm-smbus-stop: phase=ack generation=4 state=3 synchronous=0\nbattery-smbus: FAILED phase=7 status=192 ack_attempted=1 acknowledged=0\nbattery: HALTED SMBus event errors=0\n'
        result=check(data,'smbus-stop-uncertain');self.assertFalse(result['runtime_retired'])
        with self.assertRaises(AssertionError):check(data+b'battery-service: retired epoch=1\n','smbus-stop-uncertain')
    def test_native_refusal(self):
        data=b'battery-smbus-workspace: REFUSED VM-only mounted study\n'
        check(data,'smbus-refusal')
        with self.assertRaises(AssertionError):check(data+b'battery-service: started\n','smbus-refusal')

if __name__=='__main__':unittest.main()
