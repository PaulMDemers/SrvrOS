"""Actual adapter faults; model evidence never promotes native acceptance."""
from pathlib import Path
import os,re,subprocess,sys,unittest,resource
from intel_ring_transition import ring_result,ring_summary,ring_recipe,native_ring_result,verify_ring_checkpoint
from intel_gt_wake import wake_result
from hardware_boot import limine_config,stage
ROOT=Path(__file__).resolve().parents[1]
EXE=Path(os.environ.get('INTEL_RING_TRANSITION_EXE',ROOT/'build/tests/intel-ring-transition-adapter'))
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def run(mode):
 p=subprocess.run([str(EXE),'--wake-engine-'+mode],capture_output=True,text=True,timeout=20)
 if p.returncode:raise AssertionError((mode,p.stdout[-2000:],p.stderr))
 return p.stdout
def summary(t):return ring_summary(next(l for l in t.splitlines() if l.startswith('intel-ring-summary:')))
def field(t,p,k,v):
 s,n=re.subn(r'(^'+re.escape(p)+r':.*?\b'+k+r'=)[^\s]+',lambda m:m[1]+str(v),t,count=1,flags=re.M)
 assert n==1
 return s
class RingTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.text=run('report')
  cls.stats={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',cls.text.split('ring-fixture:')[1])}
 def test_original_owner_epoch_fresh_census_and_retirement(self):
  r=ring_result(self.text);s=r['summary'];h=r['owner']['summary'];w=r['wake']['summary']
  self.assertTrue(r['complete']);self.assertFalse(r['native_complete'] or r['reset'] or r['managed_pages'] or r['physical_tearfree'])
  self.assertEqual((s['epoch_before'],s['epoch_after'],s['bcs_writes'],s['returned']),(0,1,1,1))
  self.assertEqual((w['v'],w['maps'],w['retired'],w['engine_reads']),(24,14,14,216))
  self.assertEqual((h['borrows'],h['returns'],h['hda_writes'],h['complete'],h['attached']),(4,4,2,0,0))
  self.assertEqual([p['endpoint']['command'] for p in r['owner']['phases']],[6,2,2,6])
  self.assertEqual(r['post']['rows'][4]['w2'],0x3eef000);self.assertEqual(r['post']['rows'][4]['w3'],0xf000)
  with self.assertRaises(ValueError):native_ring_result(self.text)
 def test_modeled_native_chain_and_checkpoint_not_physical_acceptance(self):
  from test_intel_shutdown import ShutdownTests
  cmd=next(l.split('cmdline:',1)[1].strip() for l in limine_config('0123456789abcdef','ram-usb-v1-intel-ring-transition-laptop-probe-37d8c5d0-auto-return').splitlines() if 'cmdline:' in l)
  prefix='init: deferred=ring-transition-preinit; terminal automatic capture, no user services\nlaptop-capture: boot options='+cmd+'\n'
  t=prefix+'intel-wake: preflight checkpoint saved\n'+ShutdownTests.native(self.text)
  self.assertTrue(native_ring_result(t)['native_complete'])
  pre=prefix+ShutdownTests.native(self.text).split('intel-ring-hda:',1)[0]+'workspace-log: snapshot=session-a.log\n'
  self.assertTrue(verify_ring_checkpoint(pre,t,native=True))
  for bad in (t.replace('ring-transition-preinit','other'),t+'\ninit: started pid=1\n',t.replace('srvros.intel-ring=handoff-v1','foreign=yes')):
   with self.assertRaises(ValueError):verify_ring_checkpoint(pre,bad,native=True)
 def test_source_ineligible_and_known_unissued_cannot_pass(self):
  for mode in ('ring-off','ring-zero','active','pending','changed','float','read-fail','context-change','context-float','context-read','flip-wait','tlb-pending','blit-link-change','blit-ltr-change','ring-store-none'):
   with self.subTest(mode=mode):
    t=run(mode);s=summary(t);self.assertFalse(s['complete'] or s['bcs_writes'] or wake_result(t)['lease_complete'])
    if mode not in ('ring-store-none','blit-link-change','blit-ltr-change'):self.assertFalse(s['borrowed'])
    elif mode!='ring-store-none':self.assertFalse(s['bcs_writes'])
    else:self.assertEqual((s['epoch_after'],s['returned'],s['uncertain']),(1,1,0))
 def check_failure(self,mode,retained=False,child_may_complete=False):
  t=run(mode);s=summary(t);w=wake_result(t)['summary'];self.assertFalse(w['complete'])
  if not child_may_complete:self.assertFalse(s['complete'])
  if retained:self.assertTrue(s['uncertain'] and w['retained']);self.assertFalse(w['released']);self.assertEqual(w['retired'],0)
  try:r=ring_result(t)
  except ValueError:return t
  self.assertFalse(r['complete'])
  return t
 def test_every_source_read_provider_and_immutable_word(self):
  count=0
  for mode in ('ring-read','ring-drift','ring-busy','ring-float','ring-time','ring-epoch','ring-borrow','ring-alias','ring-source-time','ring-source-begin','ring-source-last','ring-extra-borrow','ring-copy-census','ring-copy-owner','ring-copy-provider'):
   for n in range(1,5):
    count+=1
    with self.subTest(mode=mode,n=n):self.check_failure(f'{mode}:{n}',retained=n>=3 or mode in ('ring-epoch','ring-borrow','ring-alias','ring-source-time','ring-source-begin','ring-source-last','ring-extra-borrow','ring-copy-census','ring-copy-owner','ring-copy-provider'))
  for n in (1,2):self.check_failure(f'ring-source-age:{n}')
  for mode in ('ring-source-word','ring-post-word'):
   for n in range(1,93):
    count+=1
    with self.subTest(mode=mode,n=n):self.check_failure(f'{mode}:{n}',True)
  print('source/provider/immutable word faults:',count)
 def test_post_census_reader_and_core_clock_boundaries(self):
  count=0
  for mode,nmax in [('ring-census-'+m,self.stats['census_reads']) for m in ('read','drift','time')]+[('ring-reader-'+m,self.stats['reader_reads']) for m in ('read','drift','time')]+[('ring-core-'+m,self.stats['core_clocks']) for m in ('zero','backward','time')]:
   for n in range(1,nmax+1):
    count+=1
    with self.subTest(mode=mode,n=n):self.check_failure(f'{mode}:{n}')
  print('full census/reader/core clock faults:',count)
 def test_actual_last_store_clock_and_late_return_retention(self):
  for mode in ('zero','backward','core','phase','owner','age','source','copy'):
   with self.subTest(mode=mode):
    t=self.check_failure('ring-store-'+mode);s=summary(t);self.assertEqual(s['bcs_writes'],0)
    if mode in ('zero','backward','core','phase','owner','age'):self.assertEqual(s['store_fault'],1)
  for mode in ('ring-store-uncertain','ring-return-zero','blit-audio-change'):
   with self.subTest(mode=mode):t=self.check_failure(mode,True)
  s=summary(run('ring-return-zero'));self.assertTrue(s['returned']);self.assertEqual(s['retained'],0);self.assertTrue(s['uncertain']);self.assertEqual(s['clock_at'],0)
 def test_original_hda_boundaries(self):
  count=0
  for mode,nmax in [('joint-'+m,self.stats['hda_reads']) for m in ('read','drift','read-time','alias')]+[('joint-endpoint',self.stats['endpoints']),('joint-cache',self.stats['cache'])]:
   for n in range(1,nmax+1):
    count+=1
    with self.subTest(mode=mode,n=n):self.check_failure(f'{mode}:{n}',child_may_complete=True)
  for mode in ('joint-map:1','joint-retire:1','joint-none:1','joint-none:2','joint-uncertain:1','joint-uncertain:2','joint-store-time:1','joint-store-time:2','joint-upper:1'):
   with self.subTest(mode=mode):self.check_failure(mode,child_may_complete=True)
  print('original HDA fault boundaries:',count)
 def test_strict_wire_and_false_completion(self):
  for prefix in ('intel-ring-summary','intel-ring-bcs','intel-ring-post-row','intel-ring-post-context','intel-ring-post-summary','intel-ring-reader','intel-ring-audio','intel-ring-owner','intel-ring-hda'):
   line=next(l for l in self.text.splitlines() if l.startswith(prefix+':'))
   for t in (self.text.replace(line+'\n','',1),self.text+'\n'+line,self.text.replace(line,line+' extra=0',1)):
    with self.subTest(prefix=prefix),self.assertRaises(ValueError):ring_result(t)
  s=summary(self.text)
  for k,v in [('returned',0),('post_bound',0),('readers_bound',0),('retained',1),('reason',1),('checks',19),('epoch_after',0),('source_end',s['source_end']+1),('source_last',s['source_end']+1),('post_end',s['post_end']+1),('end',s['begin']+150001),('parent_attempts',3),('store_at',s['source_end']+100001),('reset_writes',1),('managed_pages',1)]:
   with self.subTest(k=k),self.assertRaises(ValueError):ring_result(field(self.text,'intel-ring-summary',k,v))
  for t in (self.text.replace('intel-wake-summary: v=24 ','intel-wake-summary: v=23 '),self.text+'\nintel-ring-future: v=1\n',self.text+'\nintel-held-reset-summary: v=1\n'):
   with self.assertRaises(ValueError):ring_result(t)
  from hardware_desktop import needs_attention
  line=next(l for l in self.text.splitlines() if l.startswith('intel-ring-summary:'))
  self.assertFalse(needs_attention(line));self.assertTrue(needs_attention(field(line,'intel-ring-summary','complete',0)))
 def test_retired_owner_and_fresh_census_attention(self):
  from hardware_desktop import needs_attention
  for prefix in ('intel-ring-owner','intel-ring-post-summary'):
   line=next(l for l in self.text.splitlines() if l.startswith(prefix+':'))
   self.assertFalse(needs_attention(line))
   for text in (line+' extra=0',field(line,prefix,'reason',1)):
    self.assertTrue(needs_attention(text))
  owner=next(l for l in self.text.splitlines() if l.startswith('intel-ring-owner:'))
  for k,v in (('state',3),('pinned',1),('retained',1),('hda_complete',0),('complete',1),('gpu_complete',1),('attached',1),('assurances',1)):
   with self.subTest(k=k):self.assertTrue(needs_attention(field(owner,'intel-ring-owner',k,v)))
  post=next(l for l in self.text.splitlines() if l.startswith('intel-ring-post-summary:'))
  self.assertTrue(needs_attention(field(post,'intel-ring-post-summary','complete',0)))
  self.assertTrue(needs_attention(field(post,'intel-ring-post-summary','reset_writes',1)))
  self.assertFalse(needs_attention(owner.replace('intel-ring-owner:','intel-hda-bcs-summary:')))
  # Full-chain acceptance still requires source/raw/epoch/clock/preservation proof.
  with self.assertRaises(ValueError):ring_result(owner+'\n'+post)
 def test_full_collector_accepts_modeled_complete_chain_only(self):
  import tempfile
  from hardware_desktop import collect
  from test_intel_shutdown import ShutdownTests
  volume='37d8c5d0';session='session-0000000000000001-00'
  conf=limine_config('0123456789abcdef','ram-usb-v1-intel-ring-transition-laptop-probe-'+volume+'-auto-return')
  cmd=next(l.split('cmdline:',1)[1].strip() for l in conf.splitlines() if 'cmdline:' in l)
  prefix='init: deferred=ring-transition-preinit; terminal automatic capture, no user services\nlaptop-capture: boot options='+cmd+'\n'
  native=ShutdownTests.native(self.text);before,after=native.split('intel-ring-hda:',1)
  pre=prefix+before+'workspace-log: snapshot=session-a.log\n'
  final=prefix+before+'intel-wake: preflight checkpoint saved\nintel-ring-hda:'+after+'laptop-capture: completed=yes\n'
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);work=root/'srvros-work';logs=work/'Logs'/session;logs.mkdir(parents=True)
   (work/'.workspace').write_text('SRVROS-WORKSPACE-V1 '+volume+'\n')
   for name,data in (('boot.log',pre),('session-a.log',pre),('exit.log',final)):(logs/name).write_text(data)
   _,report=collect(root,volume,root/'collected')
   row,=report['sessions'];self.assertEqual(row['attention'],[])
   self.assertTrue(row['intel_ring_transition']['native_complete']) # Synthetic chain, not hardware evidence.
   (logs/'exit.log').write_text(field(final,'intel-ring-summary','complete',0))
   _,report=collect(root,volume,root/'rejected')
   self.assertTrue(report['sessions'][0]['attention'])
   self.assertFalse((report['sessions'][0].get('intel_ring_transition') or {}).get('native_complete',False))
 def test_exact_recipe_real_cli_and_legacy_isolation(self):
  exe=[sys.executable,str(ROOT/'tools/hardware_boot.py')]
  p=subprocess.run(exe+['--help'],capture_output=True,text=True);self.assertEqual(p.returncode,0);self.assertIn('--intel-ring-transition',p.stdout)
  for action in ('stage','install'):
   p=subprocess.run(exe+[action,'--intel-ring-transition'],capture_output=True,text=True);self.assertEqual(p.returncode,2);self.assertNotIn('Traceback',p.stderr);self.assertNotIn('unrecognized arguments',p.stderr)
  conf=limine_config('0123456789abcdef','ram-usb-v1-intel-ring-transition-laptop-probe-37d8c5d0-auto-return');cmd=next(l.split('cmdline:',1)[1].split() for l in conf.splitlines() if 'cmdline:' in l)
  self.assertEqual(len(cmd),15);self.assertTrue(ring_recipe(' '.join(cmd)))
  parser=str(ROOT/'build/tests/intel-gt-boot');self.assertEqual(subprocess.run([parser,'ring-transition',' '.join(cmd)]).returncode,0)
  for i in range(15):
   for tokens in (cmd[:i]+cmd[i+1:],cmd+[cmd[i]]):
    with self.assertRaises(ValueError):ring_recipe(' '.join(tokens))
    self.assertEqual(subprocess.run([parser,'ring-transition',' '.join(tokens)]).returncode,1)
  for mode in ('joint','joint-admit','gpu-hold','rehold','held-reset','hda-shutdown'):self.assertEqual(subprocess.run([parser,mode,' '.join(cmd)]).returncode,1)
  for key in ('intel_joint','intel_gpu_hold','intel_joint_admit','intel_hda_shutdown','intel_shutdown_trial','intel_engine_prepare','intel_requester','desktop_audio','hold_on_failure'):
   with self.subTest(key=key),self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_ring_transition=True,**{key:True})
if __name__=='__main__':unittest.main()
