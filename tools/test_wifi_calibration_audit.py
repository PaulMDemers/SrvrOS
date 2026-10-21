import unittest
from wifi_calibration_audit import audit

class AuditTests(unittest.TestCase):
    def setUp(self):
        self.data = bytearray(b'\xff'*512)
        for offset, value in ((0,0x7610),(2,0x0201),(0x34,0xfd11),(0x36,0x1004)):
            self.data[offset:offset+2] = value.to_bytes(2,'little')
    def test_metadata(self):
        result = audit(self.data, 2)
        self.assertEqual((result['version'],result['fae']),(2,1))
        self.assertFalse(result['radio_authorized'])
        self.assertNotIn('mac',result)
    def test_refusals(self):
        for offset, value in ((0,0),(1,0),(3,13),(0x34,0x21),(0x34,0x10)):
            with self.subTest(offset=offset,value=value):
                data=self.data.copy();data[offset]=value
                with self.assertRaises(ValueError):audit(data,0)
        for data,mask in ((self.data[:-1],0),(self.data,1<<32),(self.data,-1),(self.data,1),(self.data,8)):
            with self.assertRaises(ValueError):audit(data,mask)
        self.data[16]=0
        with self.assertRaises(ValueError):audit(self.data,2)
if __name__ == '__main__':unittest.main()
