import re,subprocess,unittest
from pathlib import Path
from unittest.mock import patch
import wifi_join5_log as c
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.trace=subprocess.check_output([str(Path('build/tests/xhci-join5-host')),'--trace'],text=True).splitlines()
 def rows(self):
  old=[3,0x2000036,10*4096+129,0,4096,0,0,0]
  rows=['srvros.wifi-join5=1','wifi-handoff5: old_ring=40960 old_dequeue=41089 new_ring=77824','wifi-handoff5-context: op=6 '+' '.join(f'w{i}={v}' for i,v in enumerate(old)),'wifi-tx5: ring=81920','wifi-tx5-command: slot=7','wifi-power5-usb: cleanup=0 write=1 address=5040 value=791614227']
  return rows+self.trace+['wifi-power5-usb: cleanup=1']
 def check(self,rows):
  with patch.object(c,'parent',return_value={'tx5_complete':True}):return c.check('\n'.join(rows))
 def test_native_production_trace(self):self.assertTrue(self.check(self.rows())['join5_complete'])
 def test_missing_duplicate_order(self):
  r=self.rows();start=r.index(c.PROGRESS)
  for i in range(start,len(r)-1):
   for b in (r[:i]+r[i+1:],r[:i]+[r[i]]+r[i:]):
    with self.subTest(i=i),self.assertRaises((ValueError,AssertionError)):self.check(b)
   if i+1<len(r)-1:
    b=r.copy();b[i],b[i+1]=b[i+1],b[i]
    with self.subTest(swap=i),self.assertRaises((ValueError,AssertionError)):self.check(b)
 def test_fields(self):
  r=self.rows()
  # Exact schema everywhere;bounded variable counters are separately tested at
  # invalid endpoints rather than asserting that all legitimate changes fail.
  for i,s in enumerate(r):
   if not s.startswith('wifi-join5') or ': ' not in s:continue
   b=r.copy();b[i]+=' unexpected=1'
   with self.subTest(extra=i),self.assertRaises((ValueError,AssertionError)):self.check(b)
  mutations=[('wifi-join5:','auth',0),('wifi-join5:','assoc',0),('wifi-join5:','leave',0),('wifi-join5:','restored',0),('wifi-join5:','unsafe',1),('wifi-join5:','posts',49),('wifi-join5:','aid',2008),('wifi-join5:','polls',16385),('wifi-join5:','authenticated',1),('wifi-join5:','ip_data',1),('wifi-join5-mac:','disabled',0),('wifi-join5-mac:','filter_restored',0),('wifi-join5-mac:','alc',0),('wifi-join5-command:','slot',8),('wifi-join5-send:','completed',0),('wifi-join5-stop-event:','ok',0)]
  for prefix,k,v in mutations:
   b=r.copy();i=next(i for i,s in enumerate(b) if s.startswith(prefix));b[i]=re.sub(r'\b'+k+r'=(?:0x[0-9a-f]+|\d+)',k+'='+str(v),b[i])
   with self.subTest(prefix=prefix,k=k),self.assertRaises((ValueError,AssertionError)):self.check(b)
if __name__=='__main__':unittest.main()
