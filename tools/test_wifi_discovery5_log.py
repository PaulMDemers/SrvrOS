import unittest
from unittest.mock import patch
import wifi_discovery5_log as c
# The complete cal5 parent has its own frozen-prefix suite. Isolate the new
# summary binding here;the run composition separately checks the entire prefix.
class Tests(unittest.TestCase):
 def log(self):return '\n'.join(['srvros.wifi-discovery5=1','wifi-cal5: ok=1',self.row,'inverse'])
 row='wifi-discovery5: complete=1 inputs=8 parsed=7 rows=3 matches=1 status=0 selected=1 channel=161 rsn_bytes=22 generation=1 retained=1 network_out=0 authenticated=0'
 def check(self,s):
  with patch.object(c,'parent',return_value=dict(cal5_completions=8,cal5_beacons=7,gain5_lna=12)),patch.object(c,'legacy_receipt',return_value='inverse'),patch.object(c,'trim_plan',return_value={c.HOOK:0}):return c.check(s)
 def test_good(self):
  r=self.check(self.log());self.assertTrue(r['demers5_selected']);self.assertFalse(r['authenticated'])
 def test_refusals(self):
  log=self.log()
  for key,value in [('complete',0),('inputs',7),('parsed',6),('rows',0),('matches',2),('status',1),('selected',0),('channel',1),('rsn_bytes',19),('generation',0),('retained',0),('network_out',1),('authenticated',1)]:
   import re
   with self.subTest(key=key),self.assertRaises((AssertionError,ValueError)):self.check(re.sub(r'\b'+key+r'=\d+',key+'='+str(value),log))
  for damaged in [log.replace(self.row,''),log+'\n'+self.row,log.replace('srvros.wifi-discovery5=1',''),log.replace(self.row+'\ninverse','inverse\n'+self.row),log.replace(self.row,self.row+' extra=1'),log.replace(self.row,self.row+' status=0')]:
   with self.assertRaises((AssertionError,ValueError)):self.check(damaged)
if __name__=='__main__':unittest.main()
