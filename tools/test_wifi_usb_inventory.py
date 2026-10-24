import unittest
from pathlib import Path
from wifi_usb_inventory import inspect


class CapturedWifiTests(unittest.TestCase):
    data = (Path(__file__).parent/'fixtures/edimax-7392-a711-usb.bin').read_bytes()

    def test_captured_topology(self):
        result = inspect(self.data)
        self.assertEqual(result['bulk_in'], ['84','85'])
        self.assertEqual(result['bulk_out'], ['08','04','05','06','07','09'])

    def test_all_truncations(self):
        for length in range(len(self.data)):
            with self.subTest(length=length), self.assertRaises(ValueError):
                inspect(self.data[:length])

    def test_identity_and_topology_refusals(self):
        for offset in [7,8,10,12,17,20,22,23,29,30,31,32,33,34,35,38,39,40,41,42,45]:
            mutated = bytearray(self.data)
            mutated[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                inspect(mutated)
        with self.assertRaises(ValueError):
            inspect(self.data+b'\x00')


if __name__ == '__main__':
    unittest.main()
