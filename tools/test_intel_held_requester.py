"""Strict held-action receipt mutations, backed by the actual native adapter fixture."""
from pathlib import Path
import subprocess
import unittest
from intel_held_requester import held_result,held_summary,action_summary
from hardware_desktop import needs_attention
ADAPTER=Path('build/tests/intel-held-requester-adapter')
def output():return subprocess.check_output([str(ADAPTER),'--wake-engine-report'],text=True,timeout=15)
class HeldReceiptTests(unittest.TestCase):
 def test_complete_raw_capture_without_authority(self):
  r=held_result(output())
  self.assertTrue(r['complete']);self.assertFalse(r['native_complete'] or r['dma_drained'] or r['hardware_ownership'] or r['managed_range'] or r['physical_tearfree'])
  self.assertEqual((r['summary']['reads'],r['summary']['n0'],r['summary']['n1']),(18,9,9))
 def test_strict_missing_duplicate_unknown(self):
  text=output()
  lines=[l for l in text.splitlines() if l.startswith(('intel-held-requester-','intel-prepare-action:'))]
  self.assertEqual(len(lines),4)
  for line in lines:
   with self.subTest(missing=line),self.assertRaises(ValueError):held_result(text.replace(line+'\n','',1))
   with self.subTest(duplicate=line),self.assertRaises(ValueError):held_result(text+'\n'+line)
  with self.assertRaises(ValueError):held_result(text+'\nintel-held-requester-unknown: v=1')
 def test_raw_summary_mutations(self):
  text=output();line=next(l for l in text.splitlines() if l.startswith('intel-held-requester-summary:'))
  for key,value in [('reads',17),('attempts',17),('profiles',1),('n0',8),('samples',1),('epoch',1),('bus_master',0),('pending',1),('flr',1),('obligations',0),('config_writes',1),('reset_writes',1),('pte_writes',1),('managed_pages',1),('hardware_ownership',1),('stable',0),('complete',0),('origin',1),('begin',1),('end',2)]:
   import re
   changed=re.sub(r'(?<= )'+key+r'=\d+',key+'='+str(value),line)
   self.assertNotEqual(changed,line)
   with self.subTest(key=key),self.assertRaises(ValueError):held_result(text.replace(line,changed))
  for bad in (line+' unknown=0',line+' reads=18',line.replace(' v=1',' v=2'),line.replace(' reads=18',' reads=18446744073709551616')):
   with self.assertRaises(ValueError):held_result(text.replace(line,bad))
 def test_action_binding_mutations(self):
  text=output();line=next(l for l in text.splitlines() if l.startswith('intel-prepare-action:'))
  import re
  for key,value in [('attempted',0),('completed',0),('active',1),('provider_enter',0),('provider_exit',0),('begin',1),('end',18446744073709551615)]:
   changed=re.sub(r'(?<= )'+key+r'=\d+',key+'='+str(value),line)
   with self.subTest(key=key),self.assertRaises(ValueError):held_result(text.replace(line,changed))
 def test_raw_rows_and_image_binding(self):
  text=output();row=next(l for l in text.splitlines() if l.startswith('intel-held-requester-row:'))
  for changed in (row.replace('s=0','s=1'),row.replace('n=9','n=8'),row.replace('371622022','4294967295'),row.replace('371622022','0371622022')):
   self.assertNotEqual(row,changed)
   with self.assertRaises(ValueError):held_result(text.replace(row,changed,1))
  with self.assertRaises(ValueError):held_result(text.replace('intel-wake-summary: v=16','intel-wake-summary: v=14'))
 def test_compact_post_vectors(self):
  from intel_engine_prepare import post_vector
  for n in (8,16,24,32,40,46):
   line='intel-prepare-post-vector: v=3 s=0 n='+str(n)+' h='+'00000001'*n
   sample,normalized=post_vector(line)
   self.assertEqual(sample,0);self.assertEqual(len(normalized),min(n//8,5)+int(n==46))
   for bad in (line.replace('v=3','v=2'),line+'0',line.replace('n='+str(n),'n=0'+str(n)),line.replace('h=00000001','h=ABCDEF01')):
    with self.subTest(n=n,bad=bad),self.assertRaises(ValueError):post_vector(bad)
  text=output();vectors=[l for l in text.splitlines() if l.startswith('intel-prepare-post-vector:')]
  self.assertEqual(len(vectors),2)
  for line in vectors:
   for changed in (line.replace('v=3','v=2'),line.replace('n=46','n=40'),line+'0',line.replace('h=','h=A',1)):
    with self.assertRaises(ValueError):held_result(text.replace(line,changed))
   with self.assertRaises(ValueError):held_result(text.replace(line+'\n','',1))
   with self.assertRaises(ValueError):held_result(text.replace(line,line+'\n'+line,1))
  with self.assertRaises(ValueError):held_result(text.replace(vectors[0],'VECTOR_PLACEHOLDER').replace(vectors[1],vectors[0]).replace('VECTOR_PLACEHOLDER',vectors[1]))
  _,oldrows=post_vector(vectors[0])
  old=oldrows[0].replace('intel-completion-','intel-prepare-post-',1)
  with self.assertRaises(ValueError):held_result(text.replace(vectors[0],vectors[0]+'\n'+old))
 def test_expired_action_retains_last_valid_clocks(self):
  import re
  for n in (1,18):
   text=subprocess.check_output([str(ADAPTER),'--wake-engine-held-time:'+str(n)],text=True,timeout=15)
   r=held_result(text)
   self.assertFalse(r['complete']);self.assertEqual(r['summary']['reads'],n)
   self.assertTrue(r['summary']['reason']&2 and r['prepare']['summary']['retained'])
   # Without both failed-clock witnesses and pinned failure, containment may
   # not be relaxed. Mutations target only this exceptional failed receipt.
   for prefix,key,value in [('intel-held-requester-summary:','reason',0),
                            ('intel-held-requester-summary:','clock_at',1),
                            ('intel-prepare-summary:','reason',2560),
                            ('intel-prepare-summary:','clock_at',1),
                            ('intel-prepare-summary:','clock_prev',1),
                            ('intel-prepare-summary:','uncertain',0)]:
    line=next(l for l in text.splitlines() if l.startswith(prefix))
    changed=re.sub(r'(?<= )'+key+r'=\d+',key+'='+str(value),line)
    self.assertNotEqual(line,changed)
    with self.subTest(n=n,prefix=prefix,key=key),self.assertRaises(ValueError):held_result(text.replace(line,changed))
 def test_line_attention(self):
  text=output()
  summary=next(l for l in text.splitlines() if l.startswith('intel-held-requester-summary:'))
  action=next(l for l in text.splitlines() if l.startswith('intel-prepare-action:'))
  self.assertFalse(needs_attention(summary));self.assertFalse(needs_attention(action))
  for line in (summary.replace(' complete=1',' complete=0'),summary+' unknown=0',action.replace(' completed=1',' completed=0'),action.replace(' active=0',' active=1')):self.assertTrue(needs_attention(line))
if __name__=='__main__':unittest.main()
