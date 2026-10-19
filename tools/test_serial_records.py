import unittest
from serial_records import search_complete


class SerialRecordsTests(unittest.TestCase):
    def test_every_fragment_boundary_waits_for_complete_numeric_field(self):
        record=b'acpi-runtime: race=PASS value=10 waits=1 elapsed_us=207000\r\n'
        pattern=rb'acpi-runtime: race=PASS value=10 waits=(\d+) elapsed_us=(\d+)'
        for end in range(len(record)):
            self.assertIsNone(search_complete(pattern,record[:end]),end)
        match=search_complete(pattern,record)
        self.assertEqual(match.groups(),(b'1',b'207000'))
        self.assertEqual(search_complete(pattern,record+b'srv> ').groups(),match.groups())

    def test_complete_first_line_does_not_authorize_partial_second(self):
        record=b'first=35\nsecond=12000000\n'
        pattern=rb'first=(\d+)\nsecond=(\d+)'
        for end in range(len(record)):
            self.assertIsNone(search_complete(pattern,bytearray(record[:end])))
        self.assertEqual(search_complete(pattern,record).groups(),(b'35',b'12000000'))

    def test_earlier_records_can_be_read_while_next_is_incomplete(self):
        self.assertEqual(search_complete(rb'count=(\d+)',b'count=123\ncount=9').group(1),b'123')
        self.assertIsNone(search_complete(rb'HALTED.*heap=(\d+)',b'normal\nHALTED unsafe heap=1'))
        self.assertEqual(search_complete(rb'HALTED.*heap=(\d+)',b'HALTED unsafe heap=162283\n')[1],b'162283')


if __name__=='__main__': unittest.main()
