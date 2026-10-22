import unittest,re
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


def home_fixture(data):
    data=data.replace(b'native=disabled',b'native=explicit')
    data=data.replace(b'battery-control-files: begin',b'battery-home-session: limit=512 per_session=yes startup=disabled\nbattery-control-files: begin')
    receipt=b'battery-home-return: idle=1 held=1; sources_masked=1\n'
    data=data.replace(b'battery-recovery-resources:',receipt+b'battery-recovery-resources:')
    restores=(b'battery: foreign-event restored enable=0x62f original=0x40 protected_writes=1\n'
          b'battery: foreign-event restored enable=0x62e original=0x4 protected_writes=1\n')
    data=data.replace(b'battery-service: retired',restores+b'battery-service: retired')
    data=re.sub(rb'battery-smbus: event=(\d+) generation=(\d+) status=(\d+) alarm=(\d+) address=(\d+) data=(\d+) acknowledged=(\d+) refresh=(\d+); aml=0 serialized=1',rb'smbus-e:\1,\2,\3,\4,\5,\6,\7,\8',data)
    return data


class Records(unittest.TestCase):
    def test_complete(self):
        result=check(fixture());self.assertEqual((result['requests'],result['lifetimes'],result['worker_operations']),(5,3,20))

    def test_missing_duplicate_and_torn(self):
        data=fixture()
        for row in data.splitlines(keepends=True):
            if not row.strip():continue
            for bad in (data.replace(row,b'',1),data+row,data.replace(row,row[:-2]+b'\n',1)):
                with self.subTest(row=row),self.assertRaises(AssertionError):check(bad)

    def test_cancel_requires_explicit_child_retirement(self):
        data=fixture();start=data.index(b'text-battery: request=2 serial=2')
        end=data.index(b'desktools: file-result',start)
        events=data.index(b'battery-smbus: event=1',end)
        retired=data.index(b'battery-service: retired epoch=1',events)
        retired=data.index(b'\n',retired)+1
        prefix=data[:end]+b'text-battery: cancel serial=2 pending_file=1 submitted=1\ntext-files: cycles=0 pending=1 storage=workspace\ntext-battery: completed=NO\n'
        suffix=data[events:retired]+b'battery-controls: FAIL requests=2 descendants=3 epoch=1; revoked=yes retired=yes\nbattery-workspace-controls: complete=NO epoch=1 state=4; revoked=yes retired=yes mounted=yes native=disabled\nbattery-workspace-hold: released token=1\n'
        for status in (1,143):
            child=f'deskd: child-exit pid=5 status={status} tick=900\n'.encode()
            exit_log=b'desktools: exited role=text failure=1\n' if status==1 else b''
            proof=prefix+exit_log+child+suffix
            self.assertEqual(check(proof,'cancel')['clients_retired'],3)
            with self.assertRaises(AssertionError):check(proof.replace(child,b''),'cancel')
            with self.assertRaises(AssertionError):check(proof.replace(child,b'deskd: child-exit pid=5 status=0 tick=900\n'),'cancel')
        with self.assertRaises(AssertionError):check(prefix+b'deskd: child-exit pid=5 status=1 tick=900\n'+suffix,'cancel')

    def test_explicit_home_requires_each_return_boundary(self):
        data=home_fixture(fixture())
        receipt=b'battery-home-return: idle=1 held=1; sources_masked=1\n'
        self.assertTrue(check(data,home=True)['final_active_owner_retired'])
        with self.assertRaises(AssertionError):check(data)
        with self.assertRaises(AssertionError):check(fixture(),home=True)
        rows=[row for row in data.splitlines(keepends=True) if row.startswith((b'battery-home-',b'battery: foreign-event restored',b'smbus-e:'))]
        for row in rows:
            for bad in (data.replace(row,b'',1),data+row,data.replace(row,row[:-2]+b'\n',1)):
                with self.subTest(row=row),self.assertRaises(AssertionError):check(bad,home=True)
        for bad in (data.replace(b'held=1',b'held=0'),data.replace(b'sources_masked=1',b'sources_masked=0'),
                    data.replace(b'original=0x40',b'original=0x0'),data.replace(b'limit=512',b'limit=0'),
                    data.replace(b'startup=disabled',b'startup=enabled'),
                    data.replace(receipt,b'',1).replace(b'battery-workspace-hold: released',receipt+b'battery-workspace-hold: released',1)):
            with self.assertRaises(AssertionError):check(bad,home=True)

    def test_refused_query_retains_failed_workload_and_return_proof(self):
        data=fixture();prefix=data[:data.index(b'text-battery: request=2')]
        start=data.index(b'battery-smbus: event=1');end=data.index(b'\n',data.index(b'battery-service: retired epoch=1'))+1
        core=data[start:end]
        core=re.sub(rb'^battery-smbus: event=3.*\n',b'',core,flags=re.M)
        core=core.replace(b'events=3',b'events=2').replace(b'queries=3',b'queries=2')
        tail=(b'text-files: cycles=0 pending=1 storage=workspace\ntext-battery: completed=NO\n'
              b'desktools: exited role=text failure=1\ndeskd: child-exit pid=5 status=1 tick=900\n'
              b'battery-controls: FAIL requests=1 descendants=3 epoch=1; revoked=yes retired=yes\n'
              b'battery-workspace-controls: complete=NO epoch=1 state=6; revoked=yes retired=yes mounted=yes native=disabled\n'
              b'battery-workspace-hold: released token=1\n')
        proof=home_fixture(prefix+core+tail)
        receipt=(b'battery-query-policy: policy=2 code=255 class=4; dispatch=refused\n'
                 b'battery-query-return: sequence=3 generation=2 code=255 transfer=1 aml_calls=0 aml_returns=0 effects=0 completed_before=6 completed_after=7 notifications_before=1 notifications_after=1 idle=1; refused=yes\n'
                 b'battery-recovery-resources: known=1 lease=0 ec_phase=6 poison=0 sample=0 waiting=0 smbus=0 api=0 aml=0 mutex=0 pending=0 facs=0 blocked=0\n')
        proof=proof.replace(b'battery-home-return:',receipt+b'battery-home-return:')
        result=check(proof,'query-refusal',home=True)
        self.assertFalse(result['workload_complete']);self.assertEqual(result['events'],2)
        for bad in (proof.replace(b'effects=0',b'effects=1'),proof.replace(b'transfer=1',b'transfer=0'),
                    proof.replace(b'completed_after=7',b'completed_after=6'),proof.replace(b'notifications_after=1',b'notifications_after=2'),
                    proof.replace(b'state=6',b'state=4'),proof.replace(b'complete=NO',b'complete=yes'),
                    proof.replace(b'status=1 tick=',b'status=0 tick='),proof.replace(b'pending=1 storage=',b'pending=0 storage=')):
            with self.assertRaises(AssertionError):check(bad,'query-refusal',home=True)
        with self.assertRaises(AssertionError):check(proof,'normal',home=True)

    def test_false_overlap_and_retirement(self):
        data=fixture()
        for bad in (data.replace(b'submitted=1',b'submitted=0'),data.replace(b'pending_file=1',b'pending_file=0'),
                    data.replace(b'file_op=2',b'file_op=1'),data.replace(b'status=0 bytes=2',b'status=1 bytes=2'),
                    data.replace(b'epoch=3 state=2',b'epoch=3 state=4'),data.replace(b'heap=0',b'heap=64'),
                    data.replace(b'descendants=3',b'descendants=2'),data.replace(b'epoch=3 attempts=',b'epoch=2 attempts='),
                    data.replace(b'presented=60',b'presented=59'),data.replace(b'end_us=61000000',b'end_us=60000000')):
            with self.subTest(bad=bad),self.assertRaises(AssertionError):check(bad)

if __name__=='__main__':unittest.main()
