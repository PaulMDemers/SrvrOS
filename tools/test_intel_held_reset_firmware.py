"""Lossless native held-reset firmware vectors and strict profile binding."""
import re
import subprocess
import unittest
from intel_gt_firmware import firmware_result,OFFSETS

class HeldResetFirmwareTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.text=subprocess.check_output(['build/tests/intel-held-reset-adapter','--wake-engine-report'],text=True)
 def test_all_raw_words_equal_legacy_decoder(self):
  result=firmware_result(self.text);self.assertTrue(result['qualification_complete'])
  vectors=[l for l in self.text.splitlines() if l.startswith('intel-firmware-vector:')]
  self.assertEqual(len(vectors),2);legacy=self.text
  for n,line in enumerate(vectors):
   h=line.split(' h=',1)[1];self.assertEqual(len(h),104)
   words=[int(h[i:i+8],16) for i in range(0,len(h),8)]
   raw='\n'.join(f'intel-firmware-word: v=1 sample={n} index={i} offset={hex(o)} value={hex(words[i])}' for i,o in enumerate(OFFSETS))
   legacy=legacy.replace(line,raw)
   self.assertEqual([w['value'] for w in result['rows']['word'][n*13:(n+1)*13]],words)
  self.assertEqual(firmware_result(legacy)['rows'],result['rows'])
 def test_encoding_count_order_and_image_refuse(self):
  vectors=[l for l in self.text.splitlines() if l.startswith('intel-firmware-vector:')];a,b=vectors
  bad=[self.text.replace(a+'\n','',1),self.text+'\n'+a,self.text.replace(a,b,1),self.text.replace(a,a.replace('v=2','v=1'),1),self.text.replace(a,a.replace('n=13','n=12'),1),self.text.replace(a,a[:-1],1),self.text.replace(a,a.replace('h=','h=G',1),1),self.text.replace(a,a.upper(),1),self.text.replace('intel-wake-summary: v=17 ','intel-wake-summary: v=16 ',1)]
  bad += [self.text.replace(a,a+'\nintel-firmware-word: v=1 sample=0 index=0 offset=0 value=0',1)]
  for text in bad:
   with self.subTest(text=text[-120:]),self.assertRaises(ValueError):firmware_result(text)
  only='\n'.join(l for l in self.text.splitlines() if l.startswith('intel-firmware-'))
  with self.assertRaises(ValueError):firmware_result(only)

if __name__=='__main__':unittest.main()
