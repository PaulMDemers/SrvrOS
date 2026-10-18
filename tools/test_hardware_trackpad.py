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
    def test_physical_capture(self):
        fixtures=capture.boot.ROOT/'tools/fixtures'
        fields=capture.validate_result(
            {'run':'69fe1189e88fb65d','release':'9640bd00eeb0ed49'},
            (fixtures/'apple-05ac-0291-capture.txt').read_bytes(),
            (fixtures/'apple-05ac-0291-capture.done').read_bytes())
        self.assertEqual(fields['status'],'CAPTURED')
        self.assertEqual(fields['keyboard'],'PASS')
        self.assertEqual(fields['reports'],'493')
        self.assertEqual(fields['errors'],'0')
    def test_pointer_trial_requires_measurements_and_desktop_exit(self):
        metadata=dict(self.metadata,pointer=True)
        with self.assertRaises(ValueError): capture.validate_result(metadata,self.data,self.completion(self.data))
        data=subprocess.check_output([str(capture.boot.ROOT/'build/tests/trackpad-capture-host'),'--dump-pointer'])
        fields=capture.validate_result(metadata,data,self.completion(data))
        self.assertEqual(fields['pointer_trial'],'EXERCISED')
        for old,new in ((b'desktop_exit=PASS',b'desktop_exit=FAIL'),
                        (b'pointer_queue_overflows=0',b'pointer_queue_overflows=1')):
            payload=data[:data.index(b'crc32=')].replace(old,new)
            altered=payload+f"crc32={binascii.crc32(payload)&0xffffffff}\ncomplete={self.metadata['run']}\n".encode()
            with self.assertRaises(ValueError): capture.validate_result(metadata,altered,self.completion(altered))
    def test_gesture_measurements(self):
        metadata=dict(self.metadata,pointer=True,gestures=True)
        with self.assertRaises(ValueError): capture.validate_result(metadata,self.data,self.completion(self.data))
        data=subprocess.check_output([str(capture.boot.ROOT/'build/tests/trackpad-capture-host'),'--dump-gestures'])
        self.assertEqual(capture.validate_result(metadata,data,self.completion(data))['gesture_trial'],'EXERCISED')
        payload=data[:data.index(b'crc32=')].replace(b'gesture_scroll_events=12',b'gesture_scroll_events=0')
        altered=payload+f"crc32={binascii.crc32(payload)&0xffffffff}\ncomplete={self.metadata['run']}\n".encode()
        with self.assertRaises(ValueError): capture.validate_result(metadata,altered,self.completion(altered))
    def test_physical_pointer_trial(self):
        fixtures=capture.boot.ROOT/'tools/fixtures'
        fields=capture.validate_result(
            {'run':'0133f0fdf2364d7b','release':'fecfea827320e7c3','pointer':True},
            (fixtures/'apple-05ac-0291-pointer.txt').read_bytes(),
            (fixtures/'apple-05ac-0291-pointer.done').read_bytes())
        self.assertEqual(fields['pointer_trial'],'EXERCISED')
        self.assertEqual(fields['desktop_exit'],'PASS')
        self.assertEqual(fields['pointer_motion'],'1905')
        self.assertEqual(fields['pointer_buttons'],'18')
        for key in ('pointer_cancels','pointer_queue_overflows','keyboard_dropped','errors','malformed'):
            self.assertEqual(fields[key],'0')
    def test_physical_scroll_trial(self):
        fixtures=capture.boot.ROOT/'tools/fixtures'
        fields=capture.validate_result(
            {'run':'c2bd37adbee95638','release':'a39982cf4c4499be','pointer':True,'gestures':True},
            (fixtures/'apple-05ac-0291-scroll.txt').read_bytes(),
            (fixtures/'apple-05ac-0291-scroll.done').read_bytes())
        self.assertEqual(fields['gesture_trial'],'EXERCISED')
        self.assertEqual(fields['gesture_scroll_events'],'1325')
        # Exercised scrolling does not imply exercised accidental-touch filters.
        self.assertEqual(fields['gesture_typing_suppressed'],'0')
        self.assertEqual(fields['gesture_palm_suppressed'],'0')
        for key in ('pointer_cancels','pointer_queue_overflows','keyboard_dropped','errors','malformed'):
            self.assertEqual(fields[key],'0')
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
