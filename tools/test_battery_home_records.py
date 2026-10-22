#!/usr/bin/env python3
"""Sanitized receipts: reject incomplete, duplicated and contradictory cleanup."""
import unittest
from battery_home_records import check, check_capture
RESOURCE=b'battery-recovery-resources: known=1 lease=0 ec_phase=6 poison=0 sample=0 waiting=0 smbus=0 api=0 aml=0 mutex=0 pending=0 facs=0 blocked=0'

def fixture(case='normal'):
    query=case=='query-refusal';alarm=case=='alarm';gen=2 if query else 3
    rows=[b'battery-workspace-hold: acquired token=1',b'battery-home: begin limit=128 mounted=yes clients=0; startup=disabled']
    for i in range(gen):
        if alarm and i==0:rows.append(b'battery-smbus: invalidate generation=0 cached=0 valid=0 fresh=0')
        fields='status=192 alarm=1 address=11 data=4660 acknowledged=1 refresh=1' if alarm and i==0 else 'status=128 alarm=0 address=0 data=0 acknowledged=0 refresh=0'
        rows.append(f'battery-smbus: event={i+1} generation={i} {fields}; aml=0 serialized=1'.encode())
        if not i:rows.append(b'battery-workspace-hold: guarded token=1 unmount=blocked scratch=blocked image=blocked')
    if query:
        rows += [b'battery-query-policy: policy=2 code=255 class=4; dispatch=refused',b'battery-query-return: sequence=3 generation=2 code=255 transfer=1 aml_calls=0 aml_returns=0 effects=0 completed_before=5 completed_after=6 notifications_before=1 notifications_after=1 idle=1; refused=yes',RESOURCE]
    rows += [f'battery-home: snapshot generation={gen} events={gen} valid=1 fresh=1'.encode(),b'battery-home-return: idle=1 held=1; sources_masked=1',RESOURCE,
        b'battery: foreign-event restored enable=0x62f original=0x40 protected_writes=1',b'battery: foreign-event restored enable=0x62e original=0x4 protected_writes=1',
        f'battery-service: retired epoch=1 attempts={gen} rejected=0 queries={gen} notifications=1 attached=1 detached=1 heap=0 errors=0 denied=0'.encode(),
        f'battery-home: outcome={"query-refused" if query else "complete"} epoch=1 generation={gen} events={gen} alarms={int(alarm)} acknowledgements={int(alarm)} state={6 if query else 4} error={5 if query else 0} fresh=0; retired=yes mounted=yes'.encode(),b'battery-workspace-hold: released token=1']
    return b'\n'.join(rows)+b'\n'

class Proof(unittest.TestCase):
    def test_complete_and_corrupt(self):
        for case in ('normal','alarm','query-refusal'):
            data=fixture(case);self.assertEqual(check(data,case)['status'],'PASS')
            lines=data.splitlines(keepends=True)
            for i in range(len(lines)):
                for altered in (lines[:i]+lines[i+1:],lines[:i]+[lines[i]]+lines[i:],lines[:i]+[lines[i][:-2]+b'\n']+lines[i+1:]):
                    with self.subTest(case=case,row=i),self.assertRaises(AssertionError):check(b''.join(altered),case)
            for before,after in ((b'held=1',b'held=0'),(b'effects=0',b'effects=1'),(b'heap=0',b'heap=1'),(b'released token=1',b'released token=2'),(b'fresh=0; retired',b'fresh=1; retired')):
                if before in data:
                    with self.subTest(case=case,mutation=before),self.assertRaises(AssertionError):check(data.replace(before,after),case)
    def test_saved_native_capture(self):
        from hardware_desktop import battery_home_result
        boot=b'laptop-capture: boot options=srvros.capture=battery-home\n'
        media=b'laptop-capture: battery Home mounted=yes logging-paused=yes fault-reset=disabled\n'
        end=b'battery-home-capture: returned=yes clean=yes; physical accuracy NOT_TESTED\nlaptop-capture: completed=yes\n'
        data=boot+media+fixture().replace(b'enable=0x62f',b'enable=0x189c').replace(b'enable=0x62e',b'enable=0x189e')+end
        self.assertEqual(check_capture(data)['status'],'PASS')
        self.assertEqual(check_capture(boot+media+fixture('alarm')+end,vm=True,case='alarm')['alarms'],1)
        self.assertIsNotNone(battery_home_result(data.decode()))
        for row in (media,)+tuple(end.splitlines(keepends=True)):
            with self.assertRaises(AssertionError):check_capture(data.replace(row,b''))
            with self.assertRaises(AssertionError):check_capture(data+row)
        for before,after in ((b'fault-reset=disabled',b'fault-reset=armed'),(b'clean=yes',b'clean=NO'),(b'held=1',b'held=0'),(b'capture=battery-home',b'capture=battery-home-other')):
            self.assertIsNone(battery_home_result(data.replace(before,after).decode()))

    def test_refusals(self):
        self.assertEqual(check(b'battery-home: REFUSED mounted ownership\n','unmounted-refusal')['runtime_started'],False)
        for case in ('profile-refusal','native-refusal'):
            refusal=b'battery-native-smbus: REFUSED captured controller profile' if case=='profile-refusal' else b'battery: REFUSED platform/resource ownership preflight'
            data=b'\n'.join((b'battery-workspace-hold: acquired token=1',b'battery-home: begin limit=128 mounted=yes clients=0; startup=disabled',refusal,b'battery-home: snapshot generation=0 events=0 valid=0 fresh=0',b'battery-home: outcome=unavailable epoch=1 generation=0 events=0 alarms=0 acknowledgements=0 state=5 error=3 fresh=0; retired=yes mounted=yes',b'battery-workspace-hold: released token=1'))+b'\n'
            self.assertEqual(check(data,case)['outcome'],'unavailable')
            with self.assertRaises(AssertionError):check(data.replace(b'state=5',b'state=4'),case)

if __name__=='__main__':unittest.main()
