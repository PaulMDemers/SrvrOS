import unittest
from unittest.mock import patch
from intel_handoff import handoff_result,native_handoff_result
GOOD='intel-handoff-summary: v=1 selected=1 acquired=1 idle=1 blocked=127 snapshot=1 preserved=1 missing=31 managed_pages=0 pte_writes=0 aperture_mappings=0 released=1 complete=1 hardware_ownership=0'
REFUSED='intel-handoff-summary: v=1 selected=0 acquired=0 idle=0 blocked=0 snapshot=0 preserved=0 missing=95 managed_pages=0 pte_writes=0 aperture_mappings=0 released=0 complete=0 hardware_ownership=0'
class HandoffTest(unittest.TestCase):
    def test_results(self):
        self.assertIsNone(handoff_result('unrelated'))
        self.assertFalse(handoff_result(REFUSED)['qualification_complete'])
        proof=handoff_result(GOOD)
        self.assertTrue(proof['qualification_complete'])
        for k in ('native_qualification_complete','managed_range','hardware_ownership','physical_tearfree'):self.assertFalse(proof[k])
        with self.assertRaises(ValueError):native_handoff_result(GOOD)
        claim="intel-boot-claim: v=1 policy=2 client=2 immutable=1 hardware_ownership=0 managed_pages=0\n"
        with patch('intel_ggtt_window.native_window_result',return_value={'native_observation_complete':True}):
            self.assertTrue(native_handoff_result(claim+GOOD)['native_qualification_complete'])
            for text in (claim+claim+GOOD,claim.replace('client=2','client=1')+GOOD):
                with self.assertRaises(ValueError):native_handoff_result(text)
    def test_malformed_or_authority(self):
        for text in (GOOD+'\n'+GOOD,GOOD+' extra=1',GOOD+' v=1',GOOD.replace('v=1','v=2'),GOOD.replace('blocked=127','blocked=128'),GOOD.replace('complete=1','complete=0'),GOOD.replace('missing=31','missing=0'),GOOD.replace('missing=31','missing=95'),GOOD.replace('preserved=1','preserved=0'),GOOD.replace('selected=1','selected=0'),GOOD.replace('idle=1','idle=0'),GOOD.replace('released=1','released=2'),GOOD.replace('acquired=1','acquired=01x'),GOOD.replace('managed_pages=0','managed_pages=16'),GOOD.replace('pte_writes=0','pte_writes=1'),GOOD.replace('aperture_mappings=0','aperture_mappings=1'),GOOD.replace('hardware_ownership=0','hardware_ownership=1'),GOOD.replace('summary:','other:')):
            with self.subTest(text=text),self.assertRaises(ValueError):handoff_result(text)
if __name__=='__main__':unittest.main()
