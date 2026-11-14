import os,re,subprocess,unittest
from pathlib import Path
from unittest.mock import patch
import wifi_join5_log as parent
import wifi_join5_cleanup_log as c
import test_wifi_join5_log as historical

class CleanupTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  saved=os.environ.get('WIFI_JOIN5_CLEANUP_TRACE')
  cls.actual=[Path(saved).read_text().splitlines()] if saved else [subprocess.check_output([str(Path('build/tests/xhci-join5-host')),mode],text=True).splitlines() for mode in ('--cleanup-trace','--cleanup-zero-trace','--cleanup-pending-trace','--cleanup-stop-normal-trace')]
  cls.trace=subprocess.check_output([str(Path('build/tests/xhci-join5-host')),'--trace'],text=True).splitlines()
 def rows(self):
  t=historical.Tests();t.trace=self.trace
  return ['srvros.wifi-cal5-cleanup=1']+t.rows()
 def check(self,rows,required=True):return c.check('\n'.join(rows),require_cleanup=required)
 def render(self,d):return c.PREFIX+': '+' '.join(k+'='+(f'0x{d[k]:016x}' if k in c.HEX else str(d[k])) for k in c.KEYS)
 def synthetic(self,n=0,completed=0,forward=0):
  # Explicit parser-only ledger extensions; actual-source traces have a separate
  # test. Existing management and all original retirement receipts are retained.
  rows=self.rows();p=c.fields(next(x for x in rows if x.startswith('wifi-join5-plan:')),'wifi-join5-plan');s=c.fields(next(x for x in rows if x.startswith('wifi-join5:')),'wifi-join5');first=s['posts'];total=first+n;entry=first-1;normal=entry+forward+completed
  self.assertIn(normal,(total-1,total))
  pointer=p['rx']+16*(total if normal==total else total-1);code=27 if normal==total else 26;residue=0 if code==27 else 4096;control=(32<<10)|(9<<16)|(p['slot']<<24)
  d=dict(version=1,first=first,entry=entry,posted=n,completed=completed,forward=forward,forward_p=p['rx']+16*(first-1) if forward else 0,forward_s=(13<<24)|4000 if forward else 0,forward_c=control if forward else 0,copied=total,codes=sum(13<<(8*i) for i in range(completed)),residues0=sum(4000<<(16*i) for i in range(min(completed,4))),residues1=sum(4000<<(16*i) for i in range(max(completed-4,0))),stop_p=pointer,stop_s=(code<<24)|residue,stop_c=control,stop_q=pointer+1,reads=21,rounds=7,busy0=0x22,busy1=0,busy2=0,held=1,failed=0)
  for i,line in enumerate(rows):
   if line.startswith('wifi-join5:'):rows[i]=re.sub(r'\bposts=\d+',f'posts={total}',line)
   elif line.startswith('wifi-join5-stop-event: dci=9 '):rows[i]=f'wifi-join5-stop-event: dci=9 cc={code} residue={residue} pointer=0x{pointer:016x} ok=1'
   elif line.startswith(('wifi-join5-context: phase=5 dci=9 ','wifi-join5-context: phase=6 dci=9 ')):
    rows[i]=re.sub(r'\bw2=(?:0x[0-9a-f]+|\d+)',f'w2=0x{(pointer+1)&0xffffffff:016x}',line);rows[i]=re.sub(r'\bw3=(?:0x[0-9a-f]+|\d+)',f'w3=0x{(pointer+1)>>32:016x}',rows[i])
  rows.insert(len(rows)-1,self.render(d));return rows,d
 def mutate(self,rows,d,key,value):
  changed=dict(d);changed[key]=value
  return [self.render(changed) if x.startswith(c.PREFIX+':') else x for x in rows]
 def test_absence_compatibility_and_requirement(self):
  self.assertFalse(self.check(self.rows(),False)['join5_cleanup_enabled'])
  with self.assertRaises(ValueError):self.check(self.rows())
  rows,d=self.synthetic()
  with self.assertRaises(ValueError):self.check([x for x in rows if x!='srvros.wifi-cal5-cleanup=1'])
 def test_synthetic_zero_and_bounded_pending_complete(self):
  for n,completed,forward in [(0,0,0),(0,0,1),(1,0,1),(1,1,1),(8,7,1),(8,8,1)]:
   rows,d=self.synthetic(n,completed,forward);result=self.check(rows)
   self.assertEqual(result['join5_cleanup_posts'],n)
   with patch.object(parent,'parent',return_value={'tx5_complete':True}):self.assertTrue(parent.check('\n'.join(rows))['join5_complete'])
 def test_required_cardinality_schema_canonical_order(self):
  rows,d=self.synthetic();at=next(i for i,x in enumerate(rows) if x.startswith(c.PREFIX+':'))
  for bad in [rows[:at]+rows[at+1:],rows[:at]+[rows[at]]+rows[at:],rows[:at]+[rows[at]+' extra=1']+rows[at+1:],rows[:at]+[rows[at].replace('version=1','version=01')]+rows[at+1:],rows[:at]+[rows[at].replace('version=1 first=', 'first=3 version=1 dummy=')]+rows[at+1:]]:
   with self.assertRaises((ValueError,AssertionError)):self.check(bad)
  bad=rows.copy();bad[at],bad[-1]=bad[-1],bad[at]
  with self.assertRaises(ValueError):self.check(bad)
 def test_mutations(self):
  rows,d=self.synthetic(2,1,1)
  bads=[('version',2),('first',0),('first',49),('first',48),('entry',d['first']-2),('entry',d['first']+1),('entry',d['first']),('posted',9),('completed',3),('forward',0),('forward',2),('forward_p',d['forward_p']+16),('forward_s',25<<24),('forward_s',(13<<24)|4097),('forward_c',d['forward_c']|4),('forward_c',d['forward_c']^(1<<24)),('copied',d['copied']-1),('codes',25),('codes',13|(13<<8)),('residues0',4097),('residues1',1),('stop_p',d['stop_p']+16),('stop_s',27<<24),('stop_s',(26<<24)|4097),('stop_c',d['stop_c']|4),('stop_c',d['stop_c']^(1<<16)),('stop_c',d['stop_c']^(1<<24)),('stop_q',d['stop_q']+16),('stop_q',d['stop_q']&~1),('reads',20),('rounds',6),('rounds',201),('busy0',1<<16),('busy0',1<<32),('busy1',1),('busy2',1),('held',0),('failed',1)]
  for key,value in bads:
   with self.subTest(key=key,value=value),self.assertRaises((ValueError,AssertionError)):self.check(self.mutate(rows,d,key,value))
 def test_forward_packet_and_context_confusion(self):
  rows,d=self.synthetic(2,1,1)
  bad=rows.copy();at=next(i for i,x in enumerate(bad) if x.startswith('wifi-join5-rx:'));bad[at]=re.sub(r'\bindex=\d+','index=2',bad[at])
  with self.assertRaises(ValueError):self.check(bad)
  for prefix,key,value in [('wifi-join5-stop-event: dci=9 ','pointer',d['stop_p']+16),('wifi-join5-context: phase=5 dci=9 ','w0',1),('wifi-join5-context: phase=6 dci=9 ','w2',d['stop_q']+16),('wifi-join5:','posts',d['first']),('wifi-join5-mac:','rx_drained',0),('wifi-join5-plan:','tx',122880),('wifi-join5-plan:','rx',40960),('wifi-join5-plan:','old',0)]:
   bad=rows.copy();at=next(i for i,x in enumerate(bad) if x.startswith(prefix));bad[at]=re.sub(r'\b'+key+r'=(?:0x[0-9a-f]+|\d+)',key+'='+str(value),bad[at])
   with self.subTest(prefix=prefix,key=key),self.assertRaises(ValueError):self.check(bad)
 def test_actual_private_source_trace(self):
  for trace in self.actual:
   rows=self.rows();start=rows.index(parent.PROGRESS);rows=rows[:start]+trace+[rows[-1]]
   self.assertTrue(self.check(rows)['join5_cleanup_enabled'])
   with patch.object(parent,'parent',return_value={'tx5_complete':True}):self.assertTrue(parent.check('\n'.join(rows))['join5_complete'])
if __name__=='__main__':unittest.main()
