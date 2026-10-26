import unittest
from serial_records import search_complete, desktop_progress_end


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

    def test_desktop_progress_exact_offsets_and_complete_trace_only(self):
        marker=b'desktools: starting role=text'
        prefix=b'prior message\n'
        for trace in (b'deskd: opened pid=7\r\n',b'desktop-launch: app=2 pid=7 exec_us=12 async=yes\r\n'):
            raw=prefix+marker[:12]+trace+marker[12:]
            for end in range(len(raw)):
                self.assertIsNone(desktop_progress_end(raw[:end],marker,len(prefix)))
            self.assertEqual(desktop_progress_end(bytearray(raw),marker,len(prefix)),len(raw))
            for bad in (trace[:-1],b'unknown trace\n',b'exception: bad\n'):
                self.assertIsNone(desktop_progress_end(prefix+marker[:12]+bad+marker[12:],marker,len(prefix)))
        self.assertEqual(desktop_progress_end(prefix+marker,marker,len(prefix)),len(prefix)+len(marker))
        self.assertIsNone(desktop_progress_end(prefix+marker,marker,len(prefix)+1))
        self.assertIsNone(desktop_progress_end(b'deskdesktop-launch: x\nd: ready',b'deskd: ready'))
        self.assertIsNone(desktop_progress_end(b'desktop-deskd: x\nlaunch: ready',b'desktop-launch: ready'))
        with self.assertRaises(ValueError):desktop_progress_end(b'anything',b'')


if __name__=='__main__': unittest.main()
