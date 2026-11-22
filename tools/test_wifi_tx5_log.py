import re,unittest
from unittest.mock import patch
import wifi_tx5_log as c
class Tests(unittest.TestCase):
 def rows(self):
  ring=0x30000
  r=['srvros.wifi-tx5-owner=1','wifi-handoff5: old_ring=65536 new_ring=131072','wifi-peer5: complete=1']
  values=[0x2f2f1313,0x04050000,0x02020004,0x3e02,0x02020000,0x3e02,0x003e0000,0x3c3c0000,0x3e]
  for i,(a,v) in enumerate(zip(c.ADDRESSES,values)):r.append(f'wifi-power5-usb: cleanup=0 write=1 address={a} value={v}')
  r.append(c.PROGRESS)
  def context(op,w):r.append('wifi-tx5-context: op='+str(op)+' '+' '.join('w'+str(i)+'='+str(v) for i,v in enumerate(w)))
  def command(op,t):r.append(f'wifi-tx5-command: op={op} type={t} slot=7 dci=8 ok=1')
  context(0,[0]*8);command(1,12);w=[1,(3<<1)|(2<<3)|(512<<16),ring|1,0,124,0,0,0];context(2,w)
  for i in range(15):
   g=i if i<3 else i-12 if i>=12 else 3;a,v=((0x1004,0),(0x2000,0xf000f200),(0x208,0x40000050))[g] if g<3 else (c.ADDRESSES[i-3],values[i-3]);r.append(f'wifi-tx5-usb: op={i} address={a} value={v} ok=1')
  r.append(f'wifi-tx5-stop-event: phase=3 slot=7 dci=8 pointer={ring} cc=27 residue=0 ed=0 ok=1')
  command(3,15);w[0]=3;context(4,w);command(5,12);w[0]=0;context(6,w)
  r+= [f'wifi-tx5: ok=1 configured=1 stopped=1 disabled=1 input_restored=1 semantic=0 commands=3 controls=15 phase=7 ring={ring} stop_events=1 others=4 retained=1 posted=0 doorbells=0 network_out=0','wifi-power5-usb: cleanup=1'];return r
 def check(self,r):
  with patch.object(c,'parent',return_value={'peer5_complete':True}):return c.check('\n'.join(r))
 def test_good_and_disabled_reserved(self):
  r=self.rows();self.assertTrue(self.check(r)['tx5_disabled'])
  i=next(i for i,s in enumerate(r) if s.startswith('wifi-tx5-context: op=6'))
  r[i]=re.sub(r'w[1-7]=\d+',lambda m:m[0].split('=')[0]+'=0',r[i]);self.assertTrue(self.check(r)['tx5_disabled'])
 def test_missing_duplicate_order(self):
  r=self.rows();start=r.index(c.PROGRESS)
  for i in range(start,len(r)-1):
   for b in (r[:i]+r[i+1:],r[:i]+[r[i]]+r[i:]):
    with self.assertRaises((ValueError,AssertionError)):self.check(b)
   if i+1<len(r)-1:
    b=r.copy();b[i],b[i+1]=b[i+1],b[i]
    with self.assertRaises((ValueError,AssertionError)):self.check(b)
 def test_bound_fields_and_schema(self):
  r=self.rows()
  for i,s in enumerate(r):
   if not s.startswith('wifi-tx5'):continue
   for m in re.finditer(r'\b([a-z_][a-z_0-9]*)=(\d+)',s):
    if s.startswith('wifi-tx5-context: op=6') and m[1] not in ('op','w0'):continue
    b=r.copy();b[i]=s[:m.start(2)]+str(int(m[2])^1)+s[m.end(2):]
    with self.subTest(i=i,k=m[1]),self.assertRaises((ValueError,AssertionError)):self.check(b)
   if ': ' in s:
    b=r.copy();b[i]+=' unexpected=1'
    with self.assertRaises((ValueError,AssertionError)):self.check(b)
 def test_controller_only_requires_separate_checker(self):
  r=self.rows();r[0]+=' srvros.wifi-nop5=1'
  with self.assertRaises((ValueError,AssertionError)):self.check(r)
  with patch.object(c,'parent',return_value={'peer5_complete':True}):
   self.assertTrue(c.check('\n'.join(r),nop=True)['tx5_complete'])
if __name__=='__main__':unittest.main()
