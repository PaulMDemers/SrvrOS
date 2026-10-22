import unittest
from battery_workspace_controls_records import check
from test_battery_controls_records import BASE


def fixture():
    data=BASE
    for epoch in (1,2):
        row=next(r for r in data.splitlines(keepends=True) if r.startswith(f'battery-service: retired epoch={epoch} '.encode()))
        events=b''.join(f'battery-smbus: event={n+1} generation={n} status=128 alarm=0 address=0 data=0 acknowledged=0 refresh=0; aml=0 serialized=1\n'.encode() for n in range(3))
        resources=b'battery-recovery-resources: known=1 lease=0 ec_phase=6 poison=0 sample=0 waiting=0 smbus=0 api=0 aml=0 mutex=0 pending=0 facs=0 blocked=0\n'
        summary=f'battery-controls-events: epoch={epoch} events=3 alarms=0 acknowledgements=0 unknown=0 invalid_through=0 generation=2; drained=yes\n'.encode()
        data=data.replace(row,events+resources+summary+row)
    return b'battery-workspace-controls: begin mounted=yes native=disabled\n'+data+b'battery-workspace-controls: complete=yes epoch=2 state=4; revoked=yes retired=yes mounted=yes native=disabled\n'


class Records(unittest.TestCase):
    def test_complete_lifetimes(self):
        result=check(fixture())
        self.assertEqual((result['events'],result['mounted_lifetimes']),(6,2))

    def test_alarm_receipts(self):
        data=fixture().replace(b'event=3 generation=2 status=128 alarm=0 address=0 data=0 acknowledged=0 refresh=0',
            b'event=3 generation=2 status=192 alarm=1 address=11 data=4660 acknowledged=1 refresh=1')
        data=data.replace(b'battery-smbus: event=3',b'battery-smbus: invalidate generation=2 cached=2 valid=1 fresh=0\n' + b'battery-smbus: event=3')
        data=data.replace(b'alarms=0 acknowledgements=0 unknown=0 invalid_through=0',b'alarms=1 acknowledgements=1 unknown=0 invalid_through=2')
        self.assertEqual(check(data)['alarms'],2)
        for bad in (data.replace(b'acknowledged=1',b'acknowledged=0'),data.replace(b'refresh=1',b'refresh=0'),
                    data.replace(b'invalid_through=2',b'invalid_through=1'),data.replace(b'address=11',b'address=48')):
            with self.assertRaises(AssertionError):check(bad)

    def test_missing_duplicate_torn_receipts(self):
        data=fixture()
        for row in data.splitlines(keepends=True):
            if not row.startswith((b'battery-workspace-controls:',b'battery-controls-events:',b'battery-smbus:',b'battery-recovery-resources:')):continue
            for bad in (data.replace(row,b'',1),data+row,data.replace(row,row[:-2]+b'\n',1)):
                with self.subTest(row=row),self.assertRaises(AssertionError):check(bad)

    def test_epoch_event_resource_and_order_corruption(self):
        data=fixture()
        for bad in (data.replace(b'epoch=2 events=',b'epoch=1 events='),data.replace(b'event=3 ',b'event=2 '),
                    data.replace(b'events=3 ',b'events=4 '),data.replace(b'lease=0',b'lease=1'),
                    data.replace(b'generation=2; drained=',b'generation=1; drained='),
                    data.replace(b'complete=yes',b'complete=NO'),data+b'workspace: unmounted\n',
                    data+b'battery: HALTED bad transfer\n'):
            with self.subTest(bad=bad),self.assertRaises(AssertionError):check(bad)
        resource=next(r for r in data.splitlines(keepends=True) if r.startswith(b'battery-recovery-resources:'))
        bad=data.replace(resource,b'',1).replace(b'battery-smbus: event=1',resource+b'battery-smbus: event=1',1)
        with self.assertRaises(AssertionError):check(bad)

if __name__=='__main__':unittest.main()
