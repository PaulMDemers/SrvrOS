import re,subprocess,unittest
from unittest.mock import patch
import wifi_wpa5_log as c
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.trace=subprocess.check_output(['build/tests/xhci-wpa5-host','--trace'],text=True).splitlines()
 def rows(self):return ['srvros.wifi-wpa5=1','wifi-handoff5: old_ring=40960 new_ring=77824','wifi-tx5: ring=81920']+self.trace+['wifi-power5-usb: cleanup=1']
 def check(self,rows):
  with patch.object(c,'parent',return_value={}):return c.check('\n'.join(rows))
 def test_trace(self):self.assertTrue(self.check(self.rows())['wpa5_software_keys_erased'])
 def test_refusals(self):
  rows=self.rows()
  for i,line in enumerate(rows):
   if not line.startswith('wifi-wpa5'):continue
   for bad in (rows[:i]+rows[i+1:],rows[:i]+[line]+rows[i:]):
    with self.subTest(i=i),self.assertRaises((AssertionError,ValueError)):self.check(bad)
  for key,value in [('ok',0),('m3_mic',0),('m4',0),('activated',0),('erased',0),('unsafe',1),('sends',6),('posts',49),('hardware_keys',1),('ip_data',1),('restored',0),('commands',5),('toggle',0)]:
   bad=rows.copy();i=next(i for i,x in enumerate(bad) if x.startswith('wifi-wpa5:'));bad[i]=re.sub(r'\b'+key+r'=\d+',key+'='+str(value),bad[i])
   with self.subTest(key=key),self.assertRaises((AssertionError,ValueError)):self.check(bad)
  for key,value in [('type',0),('request',0),('value',1),('index',5),('length',1),('actual',1),('ok',0)]:
   bad=rows.copy();i=next(i for i,x in enumerate(bad) if x.startswith('wifi-wpa5-toggle:'));bad[i]=re.sub(r'\b'+key+r'=\d+',key+'='+str(value),bad[i])
   with self.subTest(toggle=key),self.assertRaises((AssertionError,ValueError)):self.check(bad)
  bad=rows.copy();i=next(i for i,x in enumerate(bad) if x.startswith('wifi-wpa5-send: kind=3'));line=bad.pop(i);bad.insert(next(i for i,x in enumerate(bad) if x.startswith('wifi-wpa5-send: kind=2')),line)
  with self.assertRaises((AssertionError,ValueError)):self.check(bad)
  with self.assertRaises((AssertionError,ValueError)):self.check(rows+['wifi-wpa5-unknown: x=1'])
if __name__=='__main__':unittest.main()
