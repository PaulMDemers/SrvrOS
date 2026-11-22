import re,subprocess,unittest
from unittest.mock import patch
import wifi_wpa5_log as parent
import wifi_data5_log as old
import wifi_provider5_log as c
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.trace=subprocess.check_output(['build/tests/xhci-provider5-host','--trace'],text=True).splitlines()
 def rows(self):return ['srvros.wifi-wpa5=1 srvros.wifi-data5=1 srvros.wifi-provider5=1','wifi-handoff5: old_ring=40960 new_ring=77824','wifi-tx5: ring=81920']+self.trace[:-1]+['wifi-channel-vco5: status=0',self.trace[-1],'wifi-firmware-capture: completed=yes','wifi-power5-usb: cleanup=1']
 def check(self,rows):
  with patch.object(parent,'parent',return_value={}):return c.check('\n'.join(rows))
 def test_trace(self):
  r=self.check(self.rows());self.assertTrue(r['provider5_complete']);self.assertFalse(r['provider5_ordinary_rx'])
  self.assertTrue(self.check([c.BOOT]+self.rows())['provider5_complete'])
  for rows in ([c.BOOT,c.BOOT]+self.rows(),self.rows()+[c.BOOT],[c.BOOT.replace('deferred=1','deferred=0')]+self.rows()):
   with self.assertRaises((AssertionError,ValueError)):self.check(rows)
  with patch.object(parent,'parent',return_value={}),self.assertRaises((AssertionError,ValueError)):old.check('\n'.join(self.rows()))
 def test_lifecycle_refusals(self):
  rows=self.rows();i=next(i for i,x in enumerate(rows) if x.startswith('wifi-provider5:'))
  for key in c.fields(rows[i],'wifi-provider5'):
   bad=rows.copy();bad[i]=re.sub(r'\b'+key+r'=\d+',key+'=99',bad[i])
   with self.subTest(key=key),self.assertRaises((AssertionError,ValueError)):self.check(bad)
  for i,line in enumerate(rows):
   if not line.startswith('wifi-provider5'):continue
   for bad in (rows[:i]+rows[i+1:],rows[:i]+[line]+rows[i:]):
    with self.assertRaises((AssertionError,ValueError)):self.check(bad)
  bad=rows.copy();i=next(i for i,x in enumerate(bad) if x.startswith('wifi-provider5:'));row=bad.pop(i);j=next(i for i,x in enumerate(bad) if x.startswith('wifi-channel-vco5:'));bad.insert(j,row)
  with self.assertRaises((AssertionError,ValueError)):self.check(bad)
  bad=rows.copy();bad[0]=bad[0].replace(' srvros.wifi-provider5=1','')
  with self.assertRaises((AssertionError,ValueError)):self.check(bad)
if __name__=='__main__':unittest.main()
