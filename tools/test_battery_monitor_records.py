import unittest
from battery_workspace_controls_records import check_hold, check_home_returns
from test_battery_control_files_records import fixture, home_fixture
from test_battery_workspace_controls_records import fixture as controls_fixture
from battery_home_monitor_smoke import check_rollups


def proof():
    return (b'battery-monitor-events: epoch=1 at_us=5000000 events=600 alarms=3 acknowledgements=3 unknown=1 invalidations=2 saturated=0 final=0\n'
            b'battery-monitor-events: epoch=1 at_us=10000000 events=1200 alarms=6 acknowledgements=6 unknown=2 invalidations=4 saturated=0 final=0\n'
            b'battery-monitor-events: epoch=1 at_us=10000001 events=1210 alarms=6 acknowledgements=6 unknown=2 invalidations=4 saturated=0 final=1\n'
            b'battery-controls-events: epoch=1 events=1210 alarms=6 acknowledgements=6 unknown=2 invalid_through=4 generation=5; drained=yes\n'
            b'battery-service: retired epoch=1 attempts=5 rejected=0 queries=1210 notifications=1 attached=1 detached=1 heap=0 errors=0 denied=0\n')


class Records(unittest.TestCase):
    def test_good_totals(self):
        self.assertEqual(len(check_rollups(proof(),1)),3)
        self.assertEqual(len(check_rollups(proof().replace(b'\n',b'\r\n'),1)),3)

    def test_modes_do_not_alias_diagnostic_evidence(self):
        diagnostic=home_fixture(fixture())
        old=b'battery-home-session: limit=512 per_session=yes startup=disabled\n'
        new=b'battery-home-monitor: begin continuous=yes log_interval_us=5000000 native=disabled\n'
        continuous=diagnostic.replace(old,new)
        check_home_returns(diagnostic,3)
        check_home_returns(continuous,3,continuous=True)
        for data,mode in ((diagnostic,True),(continuous,False),(continuous+old,True),(diagnostic+new,False),(continuous+new,True)):
            with self.assertRaises(AssertionError):check_home_returns(data,3,continuous=mode)
        rows=controls_fixture().splitlines(keepends=True)
        i=next(i for i,r in enumerate(rows) if r.startswith(b'battery-control: dispatched'))
        rows[i]=b'battery-monitor: auto_start=1 native=disabled\n'
        data=b''.join(r for r in rows if not r.startswith(b'battery-control: dispatched'))
        self.assertEqual(check_hold(data,automatic_start=True),1)
        with self.assertRaises(AssertionError):check_hold(data)
        with self.assertRaises(AssertionError):check_hold(data.replace(rows[i],b''),automatic_start=True)

    def test_reject_inconsistent_or_unbounded_records(self):
        for old,new in ((b'at_us=10000000',b'at_us=9999999'),(b'events=1200',b'events=599'),
                        (b'acknowledgements=6',b'acknowledgements=5'),(b'unknown=2',b'unknown=3'),
                        (b'saturated=0',b'saturated=1'),(b'final=1',b'final=0'),
                        (b'queries=1210',b'queries=1209'),(b'heap=0',b'heap=1'),(b'drained=yes',b'drained=no'),(b'epoch=1 events=1210',b'epoch=1 events=1200')):
            with self.subTest(old=old):
                with self.assertRaises(AssertionError):check_rollups(proof().replace(old,new),1)
        for extra in (proof().splitlines(keepends=True)[0],proof().splitlines(keepends=True)[-1],
                      b'battery-monitor-events: broken\n',b'smbus-e:1,2,3\n',b'battery: HALTED bad\n'):
            with self.subTest(extra=extra):
                with self.assertRaises(AssertionError):check_rollups(proof()+extra,1)
        with self.assertRaises(AssertionError):check_rollups(proof(),2)
        with self.assertRaises(AssertionError):check_rollups(proof().replace(b'final=1\n',b'final=1'),1)


if __name__=='__main__':unittest.main()
