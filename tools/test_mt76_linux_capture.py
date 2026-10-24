import unittest
from mt76_linux_capture import submission, completion, consume

class CaptureTests(unittest.TestCase):
    def test_write_and_completion(self):
        r=submission('abc 100 S Co:1:009:0 s 40 06 0000 0800 0004 4 = 01000000')
        self.assertEqual(r['data'],'01000000')
        pending={'abc':r}
        completion('abc 151 C Co:1:009:0 0 4 >',pending)
        self.assertEqual((r['status'],r['actual'],r['elapsed_us']),(0,4,51))
        self.assertFalse(pending)
    def test_read_failure_and_address_mismatch(self):
        r=submission('abc 100 S Ci:1:009:0 s c0 07 0000 0080 0004 4 <')
        pending={'abc':r}
        completion('abc 110 C Ci:1:010:0 0 4 = 020000ff',pending)
        self.assertIsNone(r['status'])
        completion('abc 300100 C Ci:1:009:0 -110 0',pending)
        self.assertEqual(r['status'],-110)
        self.assertEqual(r['elapsed_us'],300000)
    def test_read_payload(self):
        r=submission('abc 100 S Ci:1:009:0 s c0 07 0000 0020 0004 4 <')
        completion('abc 110 C Ci:1:009:0 0 4 = ff17c000',{'abc':r})
        self.assertEqual(r['data'],'ff17c000')
    def test_unselected_tag_reuse(self):
        records, pending = [], {}
        consume('abc 100 S Ci:1:009:0 s c0 07 0000 0080 0004 4 <', records, pending)
        consume('abc 110 S Ci:1:009:0 s c0 07 0000 9999 0004 4 <', records, pending)
        consume('abc 120 C Ci:1:009:0 0 4 = ff17c000', records, pending)
        self.assertIsNone(records[0]['data'])
        self.assertIsNone(records[0]['status'])
        self.assertIn('tag reused',records[0]['correlation'])
        self.assertFalse(pending)

    def test_filter(self):
        for line in ('abc 100 S Bo:1:009:8 -115 8 = deadbeef',
                     'abc 100 S Co:1:009:0 s 40 06 0000 9999 0004 4 = deadbeef',
                     'abc 100 S Co:1:009:0 s 40 01 0012 0000 0040 64 = deadbeef',
                     'broken'):
            self.assertIsNone(submission(line))
        self.assertIsNotNone(submission('abc 100 S Co:1:009:0 s 40 01 0001 0000 0000 0'))
if __name__=='__main__': unittest.main()
