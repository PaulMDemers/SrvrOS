import unittest
from battery_smbus_smoke import check

RESOURCE=b'battery-recovery-resources: known=1 lease=0 ec_phase=6 poison=0 sample=0 waiting=0 smbus=0 api=0 aml=0 mutex=0 pending=0 facs=0 blocked=0'
RETIRE=b'battery-service: retired epoch=1 attempts=3 rejected=0 queries=3 notifications=1 attached=1 detached=1 heap=0 errors=0 denied=0'

def proof(case):
    alarm=case!='completion';unknown=case=='unknown';late=case=='late-alarm'
    lines=[]
    for event in range(1,4):
        hit=alarm and event==(3 if late else 1)
        gen=event-1
        if hit and not unknown:
            lines.append(f'battery-smbus: invalidate generation={gen} cached={gen} valid={int(late)} fresh=0'.encode())
        fields=(event,gen,192 if hit else 128,int(hit),(48 if unknown else 11) if hit else 0,4660 if hit else 0,int(hit),int(hit and not unknown))
        lines.append(('battery-smbus: event=%d generation=%d status=%d alarm=%d address=%d data=%d acknowledged=%d refresh=%d; aml=0 serialized=1'%fields).encode())
    lines.extend([RESOURCE,RETIRE,('battery-smbus-study: PASS events=3 alarms=%d acknowledgements=%d unknown=%d generation=3; fresh_before_stop=1 retired=yes native=disabled'%(alarm,alarm,unknown)).encode()])
    return b'\n'.join(lines)+b'\n'

class Records(unittest.TestCase):
    def test_success_cases(self):
        for case in ('completion','alarm-darwin','alarm-other','unknown','late-alarm'):
            with self.subTest(case=case):
                data=proof(case)
                self.assertEqual(check(data,case)['status'],'PASS')
                self.assertEqual(check(data.replace(b'\n',b'\r\n'),case)['status'],'PASS')
                for row in data.splitlines(keepends=True):
                    with self.assertRaises(AssertionError):check(data.replace(row,b'',1),case)
                    with self.assertRaises(AssertionError):check(data+row,case)
                with self.assertRaises(AssertionError):check(data.rstrip(b'\n'),case)

    def test_corrupt_success(self):
        data=proof('late-alarm')
        for old,new in ((b'fresh=0',b'fresh=1'),(b'cached=2',b'cached=0'),
                        (b'generation=3;',b'generation=2;'),(b'acknowledged=1',b'acknowledged=0'),
                        (b'alarms=1',b'alarms=2'),(b'unknown=0',b'unknown=1'),
                        (b'serialized=1',b'serialized=0'),(b'aml=0',b'aml=1'),
                        (b'poison=0',b'poison=1'),(b'detached=1',b'detached=0'),
                        (b'notifications=1',b'notifications=2'),(b'events=3',b'events=4'),
                        (b'refresh=1',b'refresh=0')):
            with self.subTest(old=old):
                self.assertIn(old,data)
                with self.assertRaises(AssertionError):check(data.replace(old,new),'late-alarm')
        for failure in (b'battery: HALTED unexpected',b'battery-smbus: FAILED unexpected',b'battery-query-policy: unexpected'):
            with self.assertRaises(AssertionError):check(data+failure+b'\n','late-alarm')

    def test_failure_and_refusal_are_not_passes(self):
        for case in ('busy','error','read-timeout','ack-timeout'):
            phase,status,attempt={'busy':(0,0,0),'error':(1,216,0),'read-timeout':(3,192,0),'ack-timeout':(7,192,1)}[case]
            data=(f'battery-smbus: FAILED phase={phase} status={status} ack_attempted={attempt} acknowledged=0\n'
                  'battery: HALTED SMBus event errors=0\n').encode()
            self.assertEqual(check(data,case)['outcome'],'expected containment')
            with self.assertRaises(AssertionError):check(data+RETIRE+b'\n',case)
            with self.assertRaises(AssertionError):check(data.replace(b'acknowledged=0',b'acknowledged=1'),case)
        data=b'battery-smbus-study: REFUSED VM-only idle study\n'
        self.assertEqual(check(data,'native-refusal')['native_admission'],'refused')
        with self.assertRaises(AssertionError):check(data+b'battery-native: active\n','native-refusal')

if __name__=='__main__':unittest.main()
