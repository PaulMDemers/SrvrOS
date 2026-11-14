import unittest,subprocess,re
from unittest.mock import patch
import wifi_wpa5_log as base
import wifi_provider5_log as historical
import wifi_reuse5_log as c
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.trace=subprocess.check_output(['build/tests/xhci-reuse5-host','--trace'],text=True).splitlines()
 def rows(self):
  end=[x for x in self.trace if x.startswith(('wifi-reuse5:','wifi-provider5:'))]
  return ['srvros.wifi-wpa5=1 srvros.wifi-data5=1 srvros.wifi-provider5=1 srvros.wifi-reuse5=1','wifi-handoff5: old_ring=40960 new_ring=77824','wifi-tx5: ring=81920']+[x for x in self.trace if x not in end]+['wifi-channel-vco5: status=0']+end+['wifi-firmware-capture: completed=yes','wifi-power5-usb: cleanup=1']
 def check(self,rows):
  with patch.object(base,'parent',return_value={}):return c.check('\n'.join(rows))
 def test_trace(self):
  r=self.check(self.rows());self.assertTrue(r['reuse5_complete']);self.assertFalse(r['reuse5_descriptor_wrap']);self.assertFalse(r['reuse5_ordinary_rx'])
  with patch.object(base,'parent',return_value={}),self.assertRaises((AssertionError,ValueError)):historical.check('\n'.join(self.rows()))
 def test_refusals(self):
  rows=self.rows();i=next(i for i,x in enumerate(rows) if x.startswith('wifi-reuse5:'))
  for key in c.fields(rows[i],'wifi-reuse5'):
   bad=rows.copy();bad[i]=re.sub(r'\b'+key+r'=\d+',key+'=999',bad[i])
   with self.subTest(key=key),self.assertRaises((AssertionError,ValueError)):self.check(bad)
  for i,line in enumerate(rows):
   if not line.startswith('wifi-reuse5'):continue
   for bad in (rows[:i]+rows[i+1:],rows[:i]+[line]+rows[i:]):
    with self.assertRaises((AssertionError,ValueError)):self.check(bad)
  for token in ('srvros.wifi-reuse5=1','srvros.wifi-provider5=1'):
   bad=rows.copy();bad[0]=bad[0].replace(token,'')
   with self.assertRaises((AssertionError,ValueError)):self.check(bad)
if __name__=='__main__':unittest.main()
