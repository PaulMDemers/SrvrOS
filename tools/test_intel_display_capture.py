import os
from pathlib import Path
import subprocess
import unittest
from intel_display_capture import capture_result


class CaptureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root=Path(__file__).resolve().parents[1]
        cls.text=subprocess.check_output([str(root/'build/tests'/('intel-display-probe'+('.exe' if os.name=='nt' else ''))),'--report'],text=True)

    def test_actual_producer_receipt(self):
        result=capture_result(self.text)
        self.assertEqual(result['status'],'observed')
        self.assertTrue(result['observation_complete'])
        self.assertFalse(result['scanout_ownership'])
        self.assertFalse(result['physical_tearfree'])
        self.assertEqual(result['rows']['sample'][0]['reads'],58)

    def test_absent_and_truncated(self):
        self.assertIsNone(capture_result('unrelated boot log\n'))
        lines=self.text.splitlines()
        for index,line in enumerate(lines):
            if ': v=' in line:
                with self.subTest(index=index),self.assertRaises(ValueError):
                    capture_result('\n'.join(lines[:index]+lines[index+1:]))

    def test_duplicates_versions_and_ownership(self):
        summary=next(line for line in self.text.splitlines() if line.startswith('intel-display-observe: v='))
        for extra in (summary,summary.replace('v=1','v=2'),summary.replace('maps=9','maps=9 maps=9')):
            with self.assertRaises(ValueError):capture_result(self.text+'\n'+extra)
        for old,new in [('capture_writes=0','capture_writes=1'),('ownership=none','ownership=owned'),('ggtt_backing=unproven','ggtt_backing=owned')]:
            with self.subTest(old=old),self.assertRaises(ValueError):capture_result(self.text.replace(old,new))

    def test_inconsistent_or_invalid_positive(self):
        rows=self.text.splitlines()
        mutations={
            'intel-display-identity: v=1 sample=1':('command=0x6','command=0x4'),
            'intel-display-sample: v=1 sample=1':('reads=58','reads=57'),
            'intel-display-pipe: v=1 sample=1 pipe=0':('stride=0x1800','stride=0x1000'),
            'intel-display-framebuffer:':('bpp=32','bpp=24'),
            'intel-display-relation:':('active_planes=1','active_planes=0'),
        }
        for prefix,(old,new) in mutations.items():
            changed=[line.replace(old,new) if line.startswith(prefix) else line for line in rows]
            self.assertNotEqual(rows,changed)
            with self.subTest(prefix=prefix),self.assertRaises(ValueError):capture_result('\n'.join(changed))
        for value in ['18446744073709551616','-1','0x10000000000000000']:
            with self.assertRaises(ValueError):capture_result(self.text.replace('maps=9','maps='+value))

    def test_refusal_stays_incomplete(self):
        refused=self.text.replace('status=observed','status=identity-refused')
        result=capture_result(refused)
        self.assertEqual(result['status'],'identity-refused')
        self.assertFalse(result['observation_complete'])
        self.assertFalse(result['physical_tearfree'])
