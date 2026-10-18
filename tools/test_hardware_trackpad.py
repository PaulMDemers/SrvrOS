import binascii
import subprocess
import unittest
from unittest.mock import patch
import hardware_trackpad as capture

class CaptureTests(unittest.TestCase):
    def setUp(self):
        self.metadata={'run':'0123456789abcdef','release':'fedcba9876543210'}
        self.data=subprocess.check_output([str(capture.boot.ROOT/'build/tests/trackpad-capture-host'),'--dump'])
    def completion(self,data):
        return f"COMPLETE {self.metadata['run']} {binascii.crc32(data)&0xffffffff:08x}\n".encode()
    def test_kernel_report_roundtrip(self):
        fields=capture.validate_result(self.metadata,self.data,self.completion(self.data))
        self.assertEqual(fields['status'],'CAPTURED')
        self.assertEqual(len(fields['sample_multi']),972)
    def test_truncation_corruption_missing_completion(self):
        for data,done in ((self.data[:-1],self.completion(self.data)),
                          (self.data.replace(b'AaZz09',b'AaZz08'),self.completion(self.data)),
                          (self.data,b'')):
            with self.assertRaises(ValueError): capture.validate_result(self.metadata,data,done)
    def test_inner_checksum_identity_and_coverage(self):
        corrupted=self.data.replace(b'AaZz09',b'AaZz08')
        with self.assertRaises(ValueError): capture.validate_result(self.metadata,corrupted,self.completion(corrupted))
        with self.assertRaises(ValueError): capture.validate_result(dict(self.metadata,release='0'*16),self.data,self.completion(self.data))
        payload=self.data[:self.data.index(b'crc32=')].replace(b'max_contacts=2',b'max_contacts=1')
        data=payload+f"crc32={binascii.crc32(payload)&0xffffffff}\ncomplete={self.metadata['run']}\n".encode()
        with self.assertRaises(ValueError): capture.validate_result(self.metadata,data,self.completion(data))
    def test_root_required_for_launch(self):
        with patch.object(capture.os,'geteuid',return_value=1000):
            with self.assertRaises(RuntimeError): capture.launch(None)

if __name__=='__main__': unittest.main()
