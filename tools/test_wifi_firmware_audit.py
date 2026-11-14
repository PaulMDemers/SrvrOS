import struct
import unittest
from wifi_firmware_audit import inspect, layout, MAX_BYTES


class FirmwareAuditTests(unittest.TestCase):
    def image(self):
        return struct.pack('<IIHH20x', 80, 16, 0x7640, 0x100) + bytes(96)

    def test_layout(self):
        result = layout(self.image())
        self.assertEqual((result['ivb_offset'], result['ivb_size']), (32,64))
        self.assertEqual((result['ilm_offset'], result['ilm_size']), (96,16))
        self.assertEqual((result['dlm_offset'], result['dlm_size']), (112,16))

    def test_every_truncation_and_extra_bytes(self):
        data = self.image()
        for n in range(len(data)):
            with self.subTest(n=n), self.assertRaises(ValueError):
                layout(data[:n])
        with self.assertRaises(ValueError): layout(data+b'\0')
        with self.assertRaises(ValueError): layout(bytes(MAX_BYTES+1))

    def test_hostile_lengths(self):
        for ilm,dlm in [(0,96),(64,32),(0xffffffff,16),(80,0xffffffff)]:
            with self.subTest(ilm=ilm,dlm=dlm), self.assertRaises(ValueError):
                layout(struct.pack('<II',ilm,dlm)+self.image()[8:])

    def test_structure_does_not_authorize_a_firmware(self):
        for name in ['mt7610e.bin','mt7610u.bin','unknown.bin']:
            with self.subTest(name=name), self.assertRaises(ValueError):
                inspect(name,self.image())


if __name__ == '__main__':
    unittest.main()
