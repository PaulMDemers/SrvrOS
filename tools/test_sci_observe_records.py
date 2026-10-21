import unittest
from acpi_battery_smoke import check_sci_records

BASE = b'''battery-sci: snapshot=retired epoch=2 phase=setup delivered=0 handled=0 unclaimed=0 in_flight=0 first_unclaimed_us=0 last_unclaimed_us=0
battery-sci: snapshot=retired epoch=2 phase=active delivered=2 handled=1 unclaimed=1 in_flight=0 first_unclaimed_us=110 last_unclaimed_us=110
battery-sci: snapshot=retired epoch=2 phase=retire delivered=0 handled=0 unclaimed=0 in_flight=0 first_unclaimed_us=0 last_unclaimed_us=0
battery-sci-observe: snapshot=retired epoch=2 total=2 retained=2 overwritten=0 ec_mask=0x10
battery-sci-event: snapshot=retired epoch=2 seq=1 phase=1 handled=1 at_us=100 duration_us=4 previous_valid=0 previous_handled=0 gap_us=0 query_before=0 query_after=1 valid=0xf fixed_status=0x8001 fixed_enable=0x20 ec_status=0x10 ec_enable=0x10 ec_raw_status=0x10 ec_raw_enable=0x10
battery-sci-event: snapshot=retired epoch=2 seq=2 phase=1 handled=0 at_us=110 duration_us=2 previous_valid=1 previous_handled=1 gap_us=10 query_before=1 query_after=1 valid=0xf fixed_status=0x8001 fixed_enable=0x20 ec_status=0x0 ec_enable=0x0 ec_raw_status=0x0 ec_raw_enable=0x0
'''

class ObserverRecords(unittest.TestCase):
    def test_complete(self):
        self.assertEqual(len(check_sci_records(BASE)),3)

    def test_missing_reads_are_not_rejected_as_zero(self):
        self.assertEqual(len(check_sci_records(BASE.replace(b'valid=0xf',b'valid=0x0'))),3)

    def test_corrupt_or_truncated_observation(self):
        bad = [BASE.replace(b'total=2',b'total=3'),
               BASE.replace(b'retained=2',b'retained=9'),
               BASE.replace(b'overwritten=0',b'overwritten=1'),
               BASE.replace(b'seq=2',b'seq=1'),
               BASE.replace(b'valid=0xf',b'valid=0x10'),
               BASE.replace(b'query_before=1',b'query_before=2'),
               BASE[:BASE.rfind(b'battery-sci-event:')],
               b'\n'.join(line for line in BASE.splitlines() if not line.startswith(b'battery-sci-observe:'))]
        for value in bad:
            with self.subTest(value=value):
                with self.assertRaises(AssertionError):check_sci_records(value)

if __name__=='__main__':unittest.main()
