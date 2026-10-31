import unittest
from file_service_records import file_service_receipts

class Receipts(unittest.TestCase):
    def setUp(self):
        self.row=b'file-service: stopped version=3 pid=42 role=1 requested=1 opened=1 failures=0 cancelled=0 abandoned=0 jobs_cancelled=0 retired=1 retire_failed=0 pending=0 join_error=0 max_start_us=5000000 max_retire_us=2500000 async=yes quiesced=yes process_owned=no joined=yes\n'
    def test_complete_only_and_overlap(self):
        for split in range(len(self.row)):
            with self.assertRaises(AssertionError):file_service_receipts([self.row[:split]],{42:1})
        self.assertEqual(len(file_service_receipts([self.row+b'\r\n',self.row+b'\n'],{42:1})),1)
        owned=self.row.replace(b'process_owned=no joined=yes',b'process_owned=yes joined=no')
        self.assertTrue(file_service_receipts([owned],{42:1})[0]['process_owned'])
        for bad in (self.row.replace(b'process_owned=no',b'process_owned=yes'),owned.replace(b'quiesced=yes',b'quiesced=no')):
            with self.assertRaises(AssertionError):file_service_receipts([bad],{42:1})
        for prefix in (b'bad',b'x\r'):
            with self.assertRaises(AssertionError):file_service_receipts([prefix+self.row],{42:1})
    def test_cancelled_construction_and_late_child_accounting(self):
        base=self.row.replace(b'process_owned=no joined=yes',b'process_owned=yes joined=no').replace(b'opened=1',b'opened=0').replace(b'jobs_cancelled=0',b'jobs_cancelled=1')
        for key in (b'cancelled',b'abandoned'):
            clean=base.replace(key+b'=0',key+b'=1')
            self.assertEqual(len(file_service_receipts([clean],{42:1})),1)
            with self.assertRaises(AssertionError):file_service_receipts([clean.replace(b'jobs_cancelled=1',b'jobs_cancelled=0')],{42:1})
    def test_refuse_failed_unjoined_missing_or_conflicting(self):
        for key in (b'failures',b'retire_failed',b'pending',b'join_error'):
            with self.assertRaises(AssertionError):file_service_receipts([self.row.replace(key+b'=0',key+b'=1')],{42:1})
        for old,new in ((b'joined=yes',b'joined=no'),(b'opened=1',b'opened=0'),(b'retired=1',b'retired=0')):
            with self.assertRaises(AssertionError):file_service_receipts([self.row.replace(old,new)],{42:1})
        with self.assertRaises(AssertionError):file_service_receipts([self.row],{42:2})
        with self.assertRaises(AssertionError):file_service_receipts([self.row+b'\n'+self.row],{42:1})
        with self.assertRaises(AssertionError):file_service_receipts([self.row,self.row.replace(b'max_start_us=5000000',b'max_start_us=6000000')],{42:1})
if __name__=='__main__':unittest.main()
