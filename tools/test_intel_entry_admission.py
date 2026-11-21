"""Actual original-owner disabled-entry child and unchanged reset pipeline."""
from pathlib import Path
import os,re,subprocess,sys,unittest
from intel_entry_admission import entry_capture,entry_result,entry_summary,native_entry_result,verify_entry_checkpoint
from intel_gt_wake import wake_result
from intel_joint import joint_recipe,joint_result
from hardware_boot import limine_config,stage
ROOT=Path(__file__).resolve().parents[1]
EXE=Path(os.environ.get('INTEL_ENTRY_ADAPTER_EXE',ROOT/'build/tests/intel-joint-admit-adapter'))
def run(mode):
 p=subprocess.run([str(EXE),'--wake-engine-'+mode],capture_output=True,text=True,timeout=15)
 if p.returncode:raise AssertionError((mode,p.stdout[-2300:],p.stderr))
 return p.stdout

def field(t,p,k,v):
 s,n=re.subn(r'(^'+re.escape(p)+r':.*?\b'+k+r'=)[^\s]+',lambda m:m[1]+str(v),t,count=1,flags=re.M)
 assert n==1
 return s

class EntryTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.text=run('report')
  cls.stats={k:int(v) for k,v in re.findall(r'(reads|endpoints|cache|attempts|stores|claims)=(\d+)',cls.text.split('joint-fixture:')[1].split('entry-fixture:')[0])}
 def test_same_original_owner_child_returns_before_reset_admission(self):
  r=entry_result(self.text);s=r['summary'];j=r['joint']
  self.assertTrue(r['complete'] and j['complete']);self.assertFalse(r['native_complete'])
  self.assertEqual([p['endpoint']['command'] for p in j['phases']],[6,2,2,6])
  self.assertEqual((s['borrowed'],s['returned'],s['blocked'],s['bound'],s['retained']),(1,1,1,1,0))
  self.assertEqual((s['samples'],s['reads'],s['epoch'],s['epoch_after'],s['bcs_attempts'],s['bcs_writes']),(4,32,0,0,0,0))
  self.assertEqual(j['wake']['summary']['v'],23);self.assertEqual(j['wake']['summary']['engine_reads'],619)
  self.assertEqual(len(r['rows']),4)
  from intel_takeover import takeover_result
  self.assertLess(s['source_end'],s['begin']);self.assertLess(s['end'],takeover_result(self.text)['summary']['begin_us'])
  self.assertFalse(r['hardware_ownership'] or r['managed_pages'] or r['physical_tearfree'])
  with self.assertRaises(ValueError):native_entry_result(self.text)
 def test_native_fixture_chain_and_preinit_provenance(self):
  from test_intel_shutdown import ShutdownTests
  config=limine_config('0123456789abcdef','ram-usb-v1-intel-joint-admit-laptop-probe-37d8c5d0-auto-return')
  cmd=next(l.split('cmdline:',1)[1].strip() for l in config.splitlines() if 'cmdline:' in l)
  native=ShutdownTests.native(self.text)
  marker='intel-wake: preflight checkpoint saved\n'
  self.assertNotIn(marker,native)
  prefix='init: deferred=joint-admit-preinit; terminal automatic capture, no user services\nlaptop-capture: boot options='+cmd+'\n'
  text=prefix+marker+native
  self.assertTrue(native_entry_result(text)['native_complete']) # Modeled independent chain only.
  pre=prefix+native.split('intel-joint-hda:',1)[0]+'workspace-log: snapshot=session-a.log\n'
  self.assertTrue(verify_entry_checkpoint(pre,text,native=True))
  for bad in (text.replace('joint-admit-preinit','other'),text+'\ninit: started pid=1\n',text.replace('srvros.intel-admission=disabled-bcs-v1','unknown=yes')):
   with self.assertRaises(ValueError):verify_entry_checkpoint(pre,bad,native=True)
 def test_refuse_enabled_geometry_and_failed_census_before_child_or_reset(self):
  for mode in ('entry-enabled','entry-geometry','active','pending','changed','float','read-fail','context-change','context-float','context-read'):
   with self.subTest(mode=mode):
    t=run(mode);r=entry_capture(t)
    self.assertFalse(r['complete']);self.assertFalse(r['summary']['borrowed']);self.assertFalse(r['summary']['reads'])
    self.assertIn('attempts=0',next(l for l in t.splitlines() if l.startswith('intel-held-reset-summary:')))
    self.assertFalse(wake_result(t)['lease_complete'])
 def test_each_child_callback_fault_cannot_advance_reset(self):
  cases=0
  for mode in ('entry-read','entry-drift','entry-busy','entry-float','entry-time','entry-backward','entry-epoch','entry-source-time','entry-source-begin','entry-source-last','entry-extra-borrow','entry-source-word','entry-copy-census','entry-copy-owner','entry-borrow','entry-alias'):
   for n in range(1,5):
    cases+=1
    with self.subTest(mode=mode,n=n):
     t=run(f'{mode}:{n}');line=next(l for l in t.splitlines() if l.startswith('intel-compose-entry-summary:'));s=entry_summary(line)
     self.assertFalse(s['complete']);self.assertEqual((s['bcs_attempts'],s['bcs_writes']),(0,0))
     take=next(l for l in t.splitlines() if l.startswith('intel-takeover-summary:'));self.assertIn('attempts=0',take)
     if mode in ('entry-copy-census','entry-copy-owner','entry-source-time','entry-source-begin','entry-source-last','entry-extra-borrow','entry-source-word','entry-epoch','entry-borrow','entry-alias'):self.assertTrue(s['retained'])
     try:r=entry_result(t)
     except ValueError:continue
     self.assertFalse(r['complete'])
  print('new original-child callback fault prefixes:',cases)
 def test_all_original_census_words_are_immutable_during_child(self):
  for n in range(1,93):
   with self.subTest(word=n):
    t=run(f'entry-word:{n}');s=entry_summary(next(l for l in t.splitlines() if l.startswith('intel-compose-entry-summary:')))
    self.assertFalse(s['complete']);self.assertTrue(s['retained']);self.assertTrue(s['reason']&4)
    self.assertIn('attempts=0',next(l for l in t.splitlines() if l.startswith('intel-takeover-summary:')))
 def test_each_hda_boundary_keeps_old_owner_and_refusal_contracts(self):
  cases=0
  for mode,count in (('joint-read',self.stats['reads']),('joint-drift',self.stats['reads']),('joint-read-time',self.stats['reads']),('joint-alias',self.stats['reads']),('joint-endpoint',self.stats['endpoints']),('joint-cache',self.stats['cache'])):
   for n in range(1,count+1):
    cases+=1
    with self.subTest(mode=mode,n=n):
     t=run(f'{mode}:{n}');self.assertFalse(wake_result(t)['lease_complete'])
     try:r=entry_result(t)
     except ValueError:continue
     self.assertFalse(r['complete'])
  print('composed original-HDA fault prefixes:',cases)
 def test_strict_wire_source_order_epoch_and_false_completion(self):
  for prefix in ('intel-compose-entry-row','intel-compose-entry-summary'):
   line=next(l for l in self.text.splitlines() if l.startswith(prefix+':'))
   for t in (self.text.replace(line+'\n','',1),self.text+'\n'+line,self.text.replace(line,line+' extra=0',1)):
    with self.subTest(prefix=prefix),self.assertRaises(ValueError):entry_result(t)
  s=entry_result(self.text)['summary']
  for k,v in [('returned',0),('bound',0),('retained',1),('reason',1),('checks',13),('epoch_after',1),('source_end',s['source_end']+1),('source_last',s['source_end']+1),('end',s['source_end']+100001),('parent_attempts_after',3),('bcs_writes',1),('reset_writes',1),('managed_pages',1)]:
   with self.subTest(k=k),self.assertRaises(ValueError):entry_result(field(self.text,'intel-compose-entry-summary',k,v))
  with self.assertRaises(ValueError):entry_result(self.text.replace('intel-wake-summary: v=23 ','intel-wake-summary: v=20 '))
  with self.assertRaises(ValueError):entry_result(self.text+'\nintel-compose-entry-future: v=1\n')
  line=next(l for l in self.text.splitlines() if l.startswith('intel-compose-entry-row:'))
  with self.assertRaises(ValueError):entry_result(self.text.replace(line,line.replace('512,0','512,1')))
 def test_actual_cli_exact_recipe_and_independent_legacy_profiles(self):
  exe=[sys.executable,str(ROOT/'tools/hardware_boot.py')]
  p=subprocess.run(exe+['--help'],capture_output=True,text=True);self.assertEqual(p.returncode,0);self.assertIn('--intel-joint-admit',p.stdout)
  for action in ('stage','install'):
   p=subprocess.run(exe+[action,'--intel-joint-admit'],capture_output=True,text=True);self.assertEqual(p.returncode,2);self.assertNotIn('Traceback',p.stderr);self.assertNotIn('unrecognized arguments',p.stderr)
  config=limine_config('0123456789abcdef','ram-usb-v1-intel-joint-admit-laptop-probe-37d8c5d0-auto-return')
  cmd=next(l.split('cmdline:',1)[1].split() for l in config.splitlines() if 'cmdline:' in l);self.assertEqual(len(cmd),22)
  self.assertTrue(joint_recipe(' '.join(cmd),entry_admit=True))
  parser=str(ROOT/'build/tests/intel-gt-boot');self.assertEqual(subprocess.run([parser,'joint-admit',' '.join(cmd)]).returncode,0)
  for i in range(len(cmd)):
   for tokens in (cmd[:i]+cmd[i+1:],cmd+[cmd[i]]):
    with self.assertRaises(ValueError):joint_recipe(' '.join(tokens),entry_admit=True)
    self.assertEqual(subprocess.run([parser,'joint-admit',' '.join(tokens)]).returncode,1)
  for mode in ('joint','gpu-hold','rehold','held-reset','hda-shutdown'):self.assertEqual(subprocess.run([parser,mode,' '.join(cmd)]).returncode,1)
  for key in ('intel_joint','intel_gpu_hold','intel_hda_shutdown','intel_shutdown_trial','intel_engine_prepare','intel_requester','desktop_audio','hold_on_failure'):
   with self.subTest(key=key),self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_joint_admit=True,**{key:True})
  from hardware_desktop import needs_attention
  line=next(l for l in self.text.splitlines() if l.startswith('intel-compose-entry-summary:'))
  self.assertFalse(needs_attention(line));self.assertTrue(needs_attention(field(line,'intel-compose-entry-summary','complete',0)))
if __name__=='__main__':unittest.main()
