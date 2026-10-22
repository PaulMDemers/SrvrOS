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
    def test_receive_decoding(self):
        for offset,value in ((0x3a,68),(0xdb,0xfe),(0xd1,0xf6),
                             (0x44,5),(0x45,12),(0x49,0),(0x4d,255),
                             (0x46,0xf6),(0x47,10),(0x4a,11),(0x4b,0xf5)):
            self.data[offset]=value
        r=audit(self.data,0)['receive_calibration']
        self.assertEqual(r['frequency_offset_decoded'],70)
        self.assertEqual(r['temperature_offset_decoded'],-10)
        self.assertEqual(r['lna_gain_linux_fallback_2ghz_low_mid_high_5ghz'],[5,12,12,12])
        self.assertEqual(r['rssi_offset_linux_clamped_2ghz_5ghz'],[-10,10,0,0])
        self.assertEqual(r['band_hint'],'both')
        self.assertTrue(r['external_pa_5ghz'])
        self.assertFalse(r['hardware_programming_validated'])
    def test_missing_receive_fields_are_unknown(self):
        r=audit(self.data,1<<13)['receive_calibration']
        self.assertIsNone(r['frequency_offset_decoded'])
        self.assertIsNone(r['temperature_offset_decoded'])
        self.assertFalse(r['receive_fields_complete'])
        self.assertIn('0xdb',r['missing_offsets'])
    def test_fallback_and_band_hints(self):
        for board,expected in ((1,'2GHz'),(2,'5GHz'),(3,'both')):
            self.data[0x35]=board<<4
            r=audit(self.data,0)['receive_calibration']
            self.assertEqual(r['band_hint'],expected)
            self.assertFalse(r['band_hint_is_channel_policy'])
            self.assertEqual(r['frequency_offset_decoded'],0)
            self.assertEqual(r['temperature_offset_decoded'],-10)

if __name__ == '__main__':unittest.main()
