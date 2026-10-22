import unittest
from battery_control_files_records import check


def fixture():
    r=['battery-workspace-hold: acquired token=1','battery-workspace-controls: begin mounted=yes native=disabled',
       'battery-control-files: begin pending-application-jobs=yes native=disabled']
    def request(op,serial,epoch,cycle):
        pending=int(serial>1)
        r.extend([f'text-battery: request={op} serial={serial} epoch={epoch} cycle={cycle} pending_file={pending} file_op={2*pending} submitted={pending}',
                  f'battery-control: dispatched serial={serial} operation={op} epoch={epoch} result=0'])
    def observed(op,serial,epoch):
        r.append(f'text-battery: observed={op} serial={serial} epoch={epoch} state={2 if op==1 else 4} generation=2 pending_file=0')
    def retire(epoch):
        r.extend([f'battery-smbus: event={n+1} generation={n} status=128 alarm=0 address=0 data=0 acknowledged=0 refresh=0; aml=0 serialized=1' for n in range(3)])
        r.extend(['battery-recovery-resources: known=1 lease=0 ec_phase=6 poison=0 sample=0 waiting=0 smbus=0 api=0 aml=0 mutex=0 pending=0 facs=0 blocked=0',
                  f'battery-controls-events: epoch={epoch} events=3 alarms=0 acknowledgements=0 unknown=0 invalid_through=0 generation=2; drained=yes',
                  f'battery-service: retired epoch={epoch} attempts=2 rejected=0 queries=3 notifications=1 attached=1 detached=1 heap=0 errors=0 denied=0'])
    request(1,1,0,0);observed(1,1,1)
    r.append('battery-workspace-hold: guarded token=1 unmount=blocked scratch=blocked image=blocked')
    for cycle in range(5):
        if cycle<4:
            op=2 if cycle%2==0 else 1;epoch=(cycle+2)//2
            request(op,cycle+2,epoch,cycle)
        r.extend([f'desktools: file-result role=text op={op} status=0 bytes={n}' for op,n in ((2,0),(8,32),(5,0),(1,2))])
        if cycle<4:
            op=2 if cycle%2==0 else 1;epoch=(cycle+2)//2
            if op==2:retire(epoch)
            observed(op,cycle+2,epoch+int(op==1))
    r.extend([f'text-coexist: checkpoint serial={n} length=2 caret=0 anchor=0 clipboard_busy=0 hex=4141' for n in (12,24,36,48,60)])
    r.extend(['text-coexist: final serial=60 length=2 caret=0 anchor=0 clipboard_busy=0 hex=4141',
              'text-coexist: file_worker=enabled checkpoints=5','text-files: cycles=5 pending=0 storage=workspace',
              'text-battery: completed=yes requests=5 waiting=0 epoch=3 state=2; expected_owner=active',
              'desktools: exited role=text failure=0',
              'desktop-text: generated=60 presented=60 keyboard_sent=0 keyboard_presented=0 storage=workspace',
              'desktop-coexist: completed=yes start_us=1000000 end_us=61000000 loops=6000 max_gap_us=12000 generated=60 presented=60 frames=100 storage=workspace',
              'desktop-battery: reads=50 first_gen=1 last_gen=12 cached=yes'])
    retire(3)
    r.extend(['battery-controls: PASS requests=5 descendants=3 epoch=3; revoked=yes retired=yes',
              'battery-workspace-controls: complete=yes epoch=3 state=4; revoked=yes retired=yes mounted=yes native=disabled',
              'battery-workspace-hold: released token=1',''])
    return '\n'.join(r).encode()


class Records(unittest.TestCase):
    def test_complete(self):
        result=check(fixture());self.assertEqual((result['requests'],result['lifetimes'],result['worker_operations']),(5,3,20))

    def test_missing_duplicate_and_torn(self):
        data=fixture()
        for row in data.splitlines(keepends=True):
            if not row.strip():continue
            for bad in (data.replace(row,b'',1),data+row,data.replace(row,row[:-2]+b'\n',1)):
                with self.subTest(row=row),self.assertRaises(AssertionError):check(bad)

    def test_false_overlap_and_retirement(self):
        data=fixture()
        for bad in (data.replace(b'submitted=1',b'submitted=0'),data.replace(b'pending_file=1',b'pending_file=0'),
                    data.replace(b'file_op=2',b'file_op=1'),data.replace(b'status=0 bytes=2',b'status=1 bytes=2'),
                    data.replace(b'epoch=3 state=2',b'epoch=3 state=4'),data.replace(b'heap=0',b'heap=64'),
                    data.replace(b'descendants=3',b'descendants=2'),data.replace(b'epoch=3 attempts=',b'epoch=2 attempts='),
                    data.replace(b'presented=60',b'presented=59'),data.replace(b'end_us=61000000',b'end_us=60000000')):
            with self.subTest(bad=bad),self.assertRaises(AssertionError):check(bad)

if __name__=='__main__':unittest.main()
