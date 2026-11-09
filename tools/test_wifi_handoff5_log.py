import re
import unittest
from unittest.mock import patch
import wifi_handoff5_log as c
class Tests(unittest.TestCase):
 def rows(self):
  oldring,newring,olddeq=0x10000,0x20000,0x10081
  old=[3,(3<<1)|(6<<3)|(512<<16),olddeq,0,4096,0,0,0];fresh=old.copy();fresh[2]=newring|1
  values=[0x2f2f1313,0x04050000,0x02020004,0x3e02,0x02020000,0x3e02,0x003e0000,0x3c3c0000,0x3e]
  rows=['srvros.wifi-handoff5=1',f'wifi-cal5-stop: ok=1 posts=8 state=3 dequeue={olddeq} ring={oldring} retained=1']
  for i,(a,v) in enumerate(zip(c.ADDRESSES,values)):rows.append(f'wifi-power5-usb: op={39+5*i} cleanup=0 write=1 address={a} value={v} ok=1')
  rows.extend([c.SELECTED,c.PROGRESS])
  def context(op,w):rows.append('wifi-handoff5-context: op='+str(op)+' '+' '.join('w'+str(i)+'='+str(v) for i,v in enumerate(w)))
  def command(op,d):rows.append(f'wifi-handoff5-command: op={op} dequeue={d} slot=7 dci=9 ok=1')
  context(0,old);command(1,newring|1);context(2,fresh)
  for i in range(15):
   g=i if i<3 else i-12 if i>=12 else 3;a,v=((0x1004,0),(0x2000,0xf000f200),(0x208,0x40000050))[g] if g<3 else (c.ADDRESSES[i-3],values[i-3])
   rows.append(f'wifi-handoff5-usb: op={i} address={a} value={v} ok=1')
  context(4,fresh);command(5,olddeq);context(6,old)
  rows.append(f'wifi-handoff5: ok=1 complete=1 retargeted=1 inspected=1 restored=1 unsafe=0 reads=4 commands=2 controls=15 steps=7 generation=1 old_ring={oldring} old_dequeue={olddeq} new_ring={newring} protected=17 retained=1 posted=0 network_out=0')
  rows.append('wifi-power5-usb: op=84 cleanup=1 write=0 address=4100 value=0 ok=1');return rows
 def check(self,rows):
  with patch.object(c,'parent',return_value={'power5_complete':True,'cal5_completions':8}):return c.check('\n'.join(rows))
 def test_good(self):self.assertTrue(self.check(self.rows())['handoff5_original_context_restored'])
 def test_cleanup_frontier_protection_and_slot_binding(self):
  rows=[s.replace(str(0x10081),str(0x100a1)).replace('protected=17','protected=25') for s in self.rows()]
  rows[1]+=' cleanup=2'
  parent={'power5_complete':True,'cal5_completions':8,'cal5_cleanup_enabled':True,'cal5_cleanup_posts':2,'cal5_cleanup_slot':7}
  with patch.object(c,'parent',return_value=parent):
   self.assertTrue(c.check('\n'.join(rows))['handoff5_original_context_restored'])
   for before,after in [('protected=25','protected=17'),('cleanup=2','cleanup=1'),('slot=7','slot=8'),(str(0x100a1),str(0x100b1))]:
    with self.assertRaises(ValueError):c.check('\n'.join(rows).replace(before,after))
 def test_every_handoff_row_once_in_order(self):
  rows=self.rows();start=rows.index(c.PROGRESS)
  for i in range(start,len(rows)-1):
   with self.subTest(i=i),self.assertRaises((AssertionError,ValueError)):self.check(rows[:i]+rows[i+1:])
   with self.assertRaises((AssertionError,ValueError)):self.check(rows[:i]+[rows[i]]+rows[i:])
   if i+1<len(rows)-1:
    b=rows.copy();b[i],b[i+1]=b[i+1],b[i]
    with self.assertRaises((AssertionError,ValueError)):self.check(b)
 def test_bound_numbers_and_schema(self):
  rows=self.rows()
  for i,line in enumerate(rows):
   if not line.startswith('wifi-handoff5-') and not line.startswith('wifi-handoff5:'):continue
   for m in re.finditer(r'\b([a-z_][a-z_0-9]*)=(\d+)',line):
    b=rows.copy();b[i]=line[:m.start(2)]+str(int(m[2])^1)+line[m.end(2):]
    with self.subTest(i=i,key=m[1]),self.assertRaises((AssertionError,ValueError)):self.check(b)
   if ': ' in line:
    with self.assertRaises((AssertionError,ValueError)):self.check(rows[:i]+[line+' extra=1']+rows[i+1:])
 def test_profile_lifetime_old_stop_and_alias(self):
  rows=self.rows()
  with self.assertRaises((AssertionError,ValueError)):self.check(rows[1:])
  b=rows.copy();b[1]=b[1].replace('posts=8','posts=7')
  with self.assertRaises((AssertionError,ValueError)):self.check(b)
  start=rows.index(c.PROGRESS);finish=len(rows)-2
  b=rows.copy();block=b[start:finish+1];del b[start:finish+1];b[2:2]=block
  with self.assertRaises((AssertionError,ValueError)):self.check(b)
  with self.assertRaises((AssertionError,ValueError)):self.check(rows[:-1])
  with self.assertRaises((AssertionError,ValueError)):self.check(rows[:-1]+['wifi-handoff5-unknown: x=1']+rows[-1:])
if __name__=='__main__':unittest.main()
