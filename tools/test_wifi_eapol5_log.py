import re,subprocess,unittest
from unittest.mock import patch
import wifi_eapol5_log as c
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.trace=subprocess.check_output(['build/tests/xhci-join5-host','--eapol-trace'],text=True).splitlines()
 def rows(self):return ['srvros.wifi-eapol5=1']+self.trace
 def check(self,rows):
  with patch.object(c,'parent',return_value={'join5_posts':4}):return c.check('\n'.join(rows))
 def test_production_trace(self):self.assertTrue(self.check(self.rows())['eapol5_m1_shape'])
 def test_receipts(self):
  rows=self.rows()
  for i,line in enumerate(rows):
   if not line.startswith('wifi-eapol5'):continue
   for bad in (rows[:i]+rows[i+1:],rows[:i]+[line]+rows[i:]):
    with self.subTest(i=i),self.assertRaises((AssertionError,ValueError)):self.check(bad)
  for prefix,key,value in [('wifi-eapol5:','complete',0),('wifi-eapol5:','matched',2),('wifi-eapol5:','frames',2),('wifi-eapol5:','polls',20001),('wifi-eapol5:','elapsed_us',1700000),('wifi-eapol5:','keys',1),('wifi-eapol5:','eapol_tx',1),('wifi-eapol5-rx:','rxinfo',0x100000000),('wifi-eapol5-rx:','index',0),('wifi-eapol5-rx:','m1',0),('wifi-eapol5-rx:','eapol',98),('wifi-eapol5-rx:','view',1),('wifi-eapol5-rx:','fc',0x88)]:
   bad=rows.copy();i=next(i for i,x in enumerate(bad) if x.startswith(prefix));bad[i]=re.sub(r'\b'+key+r'=(0x[0-9a-f]+|\d+)',key+'='+str(value),bad[i])
   with self.subTest(key=key),self.assertRaises((AssertionError,ValueError)):self.check(bad)
  with self.assertRaises((AssertionError,ValueError)):self.check(rows+['wifi-eapol5-unknown: x=1'])
  bad=rows.copy();line=next(x for x in bad if x.startswith('wifi-eapol5:'));bad.remove(line);bad.append(line)
  with self.assertRaises((AssertionError,ValueError)):self.check(bad)
if __name__=='__main__':unittest.main()
