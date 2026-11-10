"""Strict reset-capable lifetime mutations using the actual native adapter."""
from pathlib import Path
import re
import subprocess
import unittest
from intel_engine_held_reset import held_reset_result,reset_capture,reset_summary,native_held_reset_result
from intel_held_requester import native_held_result
from hardware_desktop import needs_attention
ADAPTER=Path('build/tests/intel-held-reset-adapter')

def output(mode='report'):
 return subprocess.check_output([str(ADAPTER),'--wake-engine-'+mode],text=True,timeout=15)

def change(text,prefix,key,value):
 line=next(l for l in text.splitlines() if l.startswith(prefix))
 changed=re.sub(r'(?<= )'+key+r'=[0-9]+',key+'='+str(value),line)
 assert changed!=line,(prefix,key,value)
 return text.replace(line,changed,1)

class HeldResetReceiptTests(unittest.TestCase):
 def test_complete_composition_without_memory_authority(self):
  r=held_reset_result(output());s=r['summary']
  self.assertTrue(r['complete'] and r['core_complete'])
  self.assertFalse(r['native_complete'] or r['dma_drained'] or r['hardware_ownership'] or r['managed_range'] or r['physical_tearfree'])
  self.assertEqual((s['attempts'],s['writes'],s['acks'],s['reads'],s['polls']),(2,2,2,20,2))
  self.assertEqual((s['epoch_before'],s['epoch_after']),(0,1))
  self.assertEqual(r['prepare']['summary']['snapshots'],5)
  self.assertEqual(r['prepare']['rows'][3]['words'][:5],[3,3,3,3,0])
  self.assertEqual(r['prepare']['rows'][4]['words'][:5],[0]*5)
  with self.assertRaises(ValueError):native_held_result(output())

 def test_required_receipts_exactly_once_and_in_order(self):
  text=output();lines=[l for l in text.splitlines() if l.startswith('intel-held-reset-')]
  self.assertEqual(len(lines),5)
  for line in lines:
   with self.subTest(missing=line),self.assertRaises(ValueError):held_reset_result(text.replace(line+'\n','',1))
   with self.subTest(duplicate=line),self.assertRaises(ValueError):held_reset_result(text+'\n'+line)
  for unknown in ('intel-held-reset-unknown: v=1','intel-held-reset-row: v=2 s=0 n=0 d=-'):
   with self.assertRaises(ValueError):held_reset_result(text+'\n'+unknown)
  row=lines[0]
  with self.assertRaises(ValueError):held_reset_result(text.replace(row+'\n','',1)+'\n'+row)

 def test_unknown_preparation_reason_bit_refuses(self):
  from intel_engine_prepare import prepare_summary
  text=change(output(),'intel-prepare-summary:','reason',4096)
  line=next(l for l in text.splitlines() if l.startswith('intel-prepare-summary:'))
  with self.assertRaises(ValueError):prepare_summary(line)

 def test_summary_mutations(self):
  text=output();prefix='intel-held-reset-summary:'
  for key,value in [('origin',1),('begin',1),('end',2),('epoch_before',1),('epoch_after',2),
                    ('attempts',1),('writes',1),('reads',19),('polls',1),('snapshots',1),
                    ('snapshot_attempts',1),('n0',8),('n1',8),('acks',1),('advanced',0),
                    ('attempted',0),('entered',0),('active',1),('provider_enter',0),('provider_exit',0),
                    ('uncertain',1),('stage',4),('settle_begin',1),('settle_end',2),
                    ('clock_at',1),('clock_prev',1),('reason',2),('complete',0),
                    ('obligations',0),('pte_writes',1),('managed_pages',1),('hardware_ownership',1)]:
   with self.subTest(key=key),self.assertRaises(ValueError):held_reset_result(change(text,prefix,key,value))
  line=next(l for l in text.splitlines() if l.startswith(prefix))
  for bad in (line+' extra=0',line+' writes=2',line.replace(' v=1',' v=2'),line.replace(' reads=20',' reads=18446744073709551616')):
   with self.assertRaises(ValueError):held_reset_result(text.replace(line,bad))

 def test_raw_prefix_and_ack_mutations(self):
  text=output();rows=[l for l in text.splitlines() if l.startswith('intel-held-reset-row:')]
  for row in rows:
   for bad in (row.replace('n=9','n=8'),row.replace('v=1','v=2'),row.replace('d=0,','d=00,',1),row.replace('d=0,','d=4294967295,',1),row.replace(',512,',',0,',1)):
    self.assertNotEqual(row,bad)
    with self.subTest(bad=bad),self.assertRaises(ValueError):held_reset_result(text.replace(row,bad,1))
  for line in [l for l in text.splitlines() if l.startswith('intel-held-reset-ack:')]:
   for bad in (line.replace('d=0,0,0','d=8,0,0'),line.replace('d=0,0,0','d=0,8,0'),line.replace('d=0,0,0','d=0,0,1'),line.replace('v=1','v=2'),line+' extra=0'):
    self.assertNotEqual(line,bad)
    with self.subTest(bad=bad),self.assertRaises(ValueError):held_reset_result(text.replace(line,bad,1))

 def test_reset_and_prepare_cannot_be_spliced(self):
  text=output()
  for prefix,key,value in [('intel-prepare-summary:','epoch_after',0),('intel-prepare-summary:','reset_writes',0),
                           ('intel-prepare-summary:','epoch',1),('intel-prepare-summary:','sample_attempts',4),
                           ('intel-prepare-summary:','snapshots',4),('intel-prepare-summary:','v',1),
                           ('intel-held-requester-summary:','epoch',1),('intel-held-requester-summary:','origin',1),
                           ('intel-wake-summary:','v',16),('intel-wake-summary:','engine_reads',273)]:
   with self.subTest(prefix=prefix,key=key),self.assertRaises(ValueError):held_reset_result(change(text,prefix,key,value))
  mixed=next(l for l in text.splitlines() if l.startswith('intel-prepare-row: v=1 s=3'))
  for bad in (mixed.replace('d=3,3,3,3,0,0','d=3,3,3,3,3,0'),mixed.replace('d=3,3,3,3,0,0','d=0,3,3,3,0,0')):
   self.assertNotEqual(mixed,bad)
   with self.assertRaises(ValueError):held_reset_result(text.replace(mixed,bad,1))

 def test_original_phase_and_settle_deadlines(self):
  text=output();s=reset_capture(text)['summary']
  with self.assertRaises(ValueError):held_reset_result(change(text,'intel-held-reset-summary:','settle_end',s['settle_begin']+49))
  with self.assertRaises(ValueError):held_reset_result(change(text,'intel-held-reset-summary:','end',s['origin']+100001))
  ack=next(l for l in text.splitlines() if l.startswith('intel-held-reset-ack:'))
  begin=int(re.search(r' begin=(\d+)',ack)[1])
  bad=re.sub(r' end=\d+',' end='+str(begin+10001),ack)
  with self.assertRaises(ValueError):held_reset_result(text.replace(ack,bad))

 def test_incomplete_snapshot_cannot_read_past_floating_word(self):
  text=output('hreset-float:1');r=reset_capture(text)
  self.assertEqual(r['summary']['n0'],1)
  self.assertFalse(r['core_complete'])
  row=next(l for l in text.splitlines() if l.startswith('intel-held-reset-row: v=1 s=0'))
  # Retain an incomplete/READ/STATE failure and repair its count fields. The
  # native provider must still stop at the first floating word, not fabricate
  # eight later loads under the failure flags.
  text=text.replace(row,'intel-held-reset-row: v=1 s=0 n=9 d=4294967295,0,0,0,1,0,512,0,0',1)
  for key,value in [('n0',9),('snapshots',1),('reads',9)]:
   text=change(text,'intel-held-reset-summary:',key,value)
  with self.assertRaises(ValueError):reset_capture(text)

 def test_line_triage_is_strict(self):
  text=output();line=next(l for l in text.splitlines() if l.startswith('intel-held-reset-summary:'))
  self.assertFalse(needs_attention(line))
  for bad in (line.replace(' complete=1',' complete=0'),line+' unknown=0',line.replace(' active=0',' active=1'),line.replace(' obligations=255',' obligations=0')):
   self.assertTrue(needs_attention(bad))
  self.assertIsNone(reset_capture('ordinary boot\n'))

if __name__=='__main__':unittest.main()
