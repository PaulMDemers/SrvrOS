"""Actual dual-route adapter; observed entry cannot impersonate an exercised writer."""
from pathlib import Path
import json,os,re,resource,subprocess,sys,tempfile,unittest
from hardware_boot import limine_config,stage
from intel_ring_entry import entry_result,entry_summary,entry_recipe,native_entry_result,verify_entry_checkpoint
from intel_ring_transition import ring_result
from intel_gt_wake import wake_result
ROOT=Path(__file__).resolve().parents[1]
EXE=Path(os.environ.get('INTEL_RING_ENTRY_EXE',ROOT/'build/tests/intel-ring-entry-adapter'))
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def run(kind,mode):
 p=subprocess.run([str(EXE),'--wake-engine-'+('entry-off-' if kind==2 else '')+mode],capture_output=True,text=True,timeout=20)
 if p.returncode:raise AssertionError((kind,mode,p.stdout[-1800:],p.stderr))
 return p.stdout
def summary(t):return entry_summary(next(l for l in t.splitlines() if l.startswith('intel-ring-entry-summary:')))
def field(t,p,k,v):
 s,n=re.subn(r'(^'+re.escape(p)+r':.*?\b'+k+r'=)[^\s]+',lambda m:m[1]+str(v),t,count=1,flags=re.M);assert n==1
 return s
class EntryTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.text={k:run(k,'report') for k in (1,2)}
  cls.stats={k:{a:int(b) for a,b in re.findall(r'(\w+)=(\d+)',t.split('ring-fixture:')[1])} for k,t in cls.text.items()}
 def test_two_routes_original_source_epoch_and_no_writer_inference(self):
  for k,t in self.text.items():
   with self.subTest(kind=k):
    r=entry_result(t);s=r['summary'];h=r['owner']['summary'];w=r['wake']['summary']
    self.assertTrue(r['complete']);self.assertFalse(r['native_complete'] or r['reset'] or r['managed_pages'] or r['physical_tearfree'])
    self.assertEqual((s['kind'],s['epoch_before'],s['epoch_after'],s['bcs_writes']),(k,0,int(k==1),int(k==1)))
    self.assertEqual(r['enabled_writer_complete'],k==1);self.assertEqual((w['v'],w['maps'],w['retired'],w['engine_reads']),(25,14,14,216))
    self.assertEqual((h['complete'],h['gpu_complete'],h['attached'],h['borrows'],h['returns']),(0,0,0,4,4))
    self.assertEqual([p['endpoint']['command'] for p in r['owner']['phases']],[6,2,2,6])
    self.assertEqual((r['post']['rows'][4]['w2'],r['post']['rows'][4]['w3']),(0x3eef000,0xf000) if k==1 else (0,0))
    if k==2:self.assertEqual([r['pre']['contexts'][0]['c'+str(i)] for i in range(6)],[512,24704,10240,512,0,0])
    with self.assertRaises(ValueError):ring_result(t)
    with self.assertRaises(ValueError):native_entry_result(t)
 def native_fixture(self,k):
  from test_intel_shutdown import ShutdownTests
  conf=limine_config('0123456789abcdef','ram-usb-v1-intel-ring-entry-laptop-probe-37d8c5d0-auto-return')
  cmd=next(l.split('cmdline:',1)[1].strip() for l in conf.splitlines() if 'cmdline:' in l)
  prefix='init: deferred=ring-entry-preinit; terminal automatic capture, no user services\nlaptop-capture: boot options='+cmd+'\n'
  native=ShutdownTests.native(self.text[k]);before,after=native.split('intel-ring-entry-hda:',1)
  return prefix+before+'workspace-log: snapshot=session-a.log\n',prefix+before+'intel-wake: preflight checkpoint saved\nintel-ring-entry-hda:'+after+'laptop-capture: completed=yes\n'
 def test_full_collector_both_native_fixture_chains(self):
  from hardware_desktop import collect,needs_attention
  for k in (1,2):
   with self.subTest(kind=k):
    pre,t=self.native_fixture(k);r=native_entry_result(t)
    self.assertTrue(r['native_complete']);self.assertEqual(r['native_enabled_writer_complete'],k==1) # Synthetic only.
    self.assertTrue(verify_entry_checkpoint(pre,t,native=True))
    with tempfile.TemporaryDirectory() as tmp:
     root=Path(tmp);work=root/'srvros-work';logs=work/'Logs/session-0000000000000001-00';logs.mkdir(parents=True)
     (work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
     for name,data in (('boot.log',pre),('session-a.log',pre),('exit.log',t)):(logs/name).write_text(data)
     _,report=collect(root,'37d8c5d0',root/'positive');row,=report['sessions'];self.assertEqual(row['attention'],[])
     self.assertTrue(row['intel_ring_entry']['native_complete']);self.assertEqual(row['intel_ring_entry']['native_enabled_writer_complete'],k==1)
     (logs/'exit.log').write_text(field(t,'intel-ring-entry-summary','complete',0))
     _,report=collect(root,'37d8c5d0',root/'negative');self.assertTrue(report['sessions'][0]['attention'])
     self.assertFalse((report['sessions'][0]['intel_ring_entry'] or {}).get('native_complete',False))
    for prefix in ('intel-ring-entry-owner','intel-ring-entry-post-summary','intel-ring-entry-summary'):
     l=next(l for l in t.splitlines() if l.startswith(prefix+':'));self.assertFalse(needs_attention(l));self.assertTrue(needs_attention(l+' extra=0'))
    for bad in (t.replace('ring-entry-preinit','other'),t+'\ninit: started pid=1\n',t.replace('srvros.intel-ring=entry-v1','srvros.intel-ring=handoff-v1')):
     with self.assertRaises(ValueError):verify_entry_checkpoint(pre,bad,native=True)
 def failure(self,k,mode,retained=False,child_may_complete=False):
  t=run(k,mode)
  try:s=summary(t)
  except ValueError:
   # Corrupted branch metadata must be rejected by the decoder; still inspect
   # the actual C incomplete/retention result, never promote it to authority.
   s={key:int(value) for key,value in re.findall(r'(\w+)=(\d+)',next(l for l in t.splitlines() if l.startswith('intel-ring-entry-summary:')))}
  w=wake_result(t)['summary'];self.assertFalse(w['complete'])
  if not child_may_complete:self.assertFalse(s['complete'])
  if k==2:self.assertFalse(s['store_calls'] or s['bcs_attempts'] or s['bcs_writes'] or s['epoch_advanced'])
  if retained:self.assertTrue(s['uncertain'] and w['retained']);self.assertFalse(w['released']);self.assertEqual(w['retired'],0)
  try:r=entry_result(t)
  except ValueError:return t
  self.assertFalse(r['complete']);return t
 def test_every_source_provider_branch_and_immutable_word(self):
  count=0
  sources=('ring-epoch','ring-borrow','ring-alias','ring-source-time','ring-source-begin','ring-source-last','ring-extra-borrow','ring-copy-census','ring-copy-owner','ring-copy-provider','ring-kind','ring-kind-mask')
  for k in (1,2):
   for mode in ('ring-read','ring-drift','ring-busy','ring-float','ring-time')+sources:
    for n in range(1,5):
     count+=1
     with self.subTest(kind=k,mode=mode,n=n):
      t=self.failure(k,f'{mode}:{n}',mode in sources or k==1 and n>=3)
      if mode in sources:
       reason=int(re.search(r'^intel-ring-entry-summary:.* reason=(\d+) ',t,re.M)[1]);self.assertTrue(reason&4)
   for mode in ('ring-source-word','ring-post-word'):
    for n in range(1,93):
     count+=1
     with self.subTest(kind=k,mode=mode,n=n):self.failure(k,f'{mode}:{n}',True)
  for k in (1,2):
   for n in (1,2,3):self.failure(k,f'ring-post-stamp:{n}',True)
  print('dual-route source/provider/branch/word faults:',count)
 def test_both_full_census_reader_and_core_clock_boundaries(self):
  count=0
  for k in (1,2):
   for mode,nmax in [('ring-census-'+m,self.stats[k]['census_reads']) for m in ('read','drift','time')]+[('ring-reader-'+m,self.stats[k]['reader_reads']) for m in ('read','drift','time')]+[('ring-core-'+m,self.stats[k]['core_clocks']) for m in ('zero','backward','time')]:
    for n in range(1,nmax+1):
     count+=1
     with self.subTest(kind=k,mode=mode,n=n):self.failure(k,f'{mode}:{n}',retained=mode.startswith('ring-census-') and n>92)
  print('dual-route census/reader/core clocks:',count)
 def test_original_hda_boundaries_disabled_route_and_enabled_phase_edges(self):
  count=0;k=2
  for mode,nmax in [('joint-'+m,self.stats[k]['hda_reads']) for m in ('read','drift','read-time','alias')]+[('joint-endpoint',self.stats[k]['endpoints']),('joint-cache',self.stats[k]['cache'])]:
   for n in range(1,nmax+1):
    count+=1
    with self.subTest(mode=mode,n=n):self.failure(k,f'{mode}:{n}',child_may_complete=True)
  for k in (1,2):
   for mode in ('joint-map:1','joint-retire:1','joint-none:1','joint-none:2','joint-uncertain:1','joint-uncertain:2','joint-store-time:1','joint-store-time:2','joint-upper:1'):
    with self.subTest(kind=k,mode=mode):self.failure(k,mode,child_may_complete=True)
  print('disabled-route original HDA boundaries:',count)
 def test_last_store_and_terminal_clock_preserve_original_root(self):
  for mode in ('zero','backward','core','phase','owner','age','source','copy'):
   with self.subTest(mode=mode):self.assertFalse(summary(self.failure(1,'ring-store-'+mode))['bcs_writes'])
  self.failure(1,'ring-store-uncertain',True)
  self.failure(2,'ring-observer-tag',True)
  for k in (1,2):
   for mode in ('ring-return-zero','blit-audio-change'):
    with self.subTest(kind=k,mode=mode):self.failure(k,mode,True)
   for m in ('zero','backward','time'):
    t=self.failure(k,f'ring-core-{m}:{self.stats[k]["core_clocks"]}',True)
    self.assertTrue(summary(t)['returned']);self.assertFalse(summary(t)['retained'])
   for n in (1,2):self.failure(k,f'ring-source-age:{n}')
 def test_ineligible_sources_and_disabled_nonzero_geometry_refuse(self):
  for k in (1,2):
   for mode in ('active','pending','changed','float','read-fail','context-change','context-float','context-read','flip-wait','tlb-pending'):
    with self.subTest(kind=k,mode=mode):self.assertFalse(summary(self.failure(k,mode))['borrowed'])
  t=run(1,'entry-nonzero-disabled');s=summary(t);self.assertEqual(s['kind'],2);self.assertFalse(s['borrowed'] or s['store_calls'] or s['complete'])
 def test_strict_wire_and_no_disabled_to_writer_promotion(self):
  for k,t in self.text.items():
   for prefix in ('intel-ring-entry-summary','intel-ring-entry-bcs','intel-ring-entry-post-row','intel-ring-entry-post-context','intel-ring-entry-post-summary','intel-ring-entry-reader','intel-ring-entry-audio','intel-ring-entry-owner','intel-ring-entry-hda'):
    line=next(l for l in t.splitlines() if l.startswith(prefix+':'))
    for bad in (t.replace(line+'\n','',1),t+'\n'+line,t.replace(line,line+' extra=0',1)):
     with self.subTest(kind=k,prefix=prefix),self.assertRaises(ValueError):entry_result(bad)
   s=summary(t)
   for key,val in [('kind',3-k),('kind',0),('returned',0),('post_bound',0),('readers_bound',0),('reason',1),('source_end',s['source_end']+1),('post_last',s['post_end']+1),('epoch_after',int(k==2)),('store_calls',int(k==2)),('bcs_writes',int(k==2)),('end',s['begin']+150001),('reset_writes',1),('managed_pages',1)]:
    with self.subTest(kind=k,key=key),self.assertRaises(ValueError):entry_result(field(t,'intel-ring-entry-summary',key,val))
   for bad in (t.replace('intel-wake-summary: v=25 ','intel-wake-summary: v=24 '),t+'\nintel-ring-summary: v=1\n',t+'\nintel-ring-entry-future: v=1\n'):
    with self.assertRaises(ValueError):entry_result(bad)
 def test_exact_cli_boot_recipe_and_old_contracts(self):
  cmd=next(l.split('cmdline:',1)[1].split() for l in limine_config('0123456789abcdef','ram-usb-v1-intel-ring-entry-laptop-probe-37d8c5d0-auto-return').splitlines() if 'cmdline:' in l)
  self.assertEqual(len(cmd),15);self.assertTrue(entry_recipe(' '.join(cmd)))
  parser=str(ROOT/'build/tests/intel-gt-boot');self.assertEqual(subprocess.run([parser,'ring-entry',' '.join(cmd)]).returncode,0)
  for i in range(15):
   for tokens in (cmd[:i]+cmd[i+1:],cmd+[cmd[i]]):
    with self.assertRaises(ValueError):entry_recipe(' '.join(tokens))
    self.assertEqual(subprocess.run([parser,'ring-entry',' '.join(tokens)]).returncode,1)
  for mode in ('ring-transition','joint','joint-admit','gpu-hold','rehold','held-reset','hda-shutdown'):self.assertEqual(subprocess.run([parser,mode,' '.join(cmd)]).returncode,1)
  for key in ('intel_ring_transition','intel_joint','intel_gpu_hold','intel_hda_shutdown','intel_shutdown_trial','intel_engine_prepare','desktop_audio','hold_on_failure'):
   with self.subTest(key=key),self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_ring_entry=True,**{key:True})
  exe=[sys.executable,str(ROOT/'tools/hardware_boot.py')]
  p=subprocess.run(exe+['--help'],capture_output=True,text=True);self.assertIn('--intel-ring-entry',p.stdout)
  for action in ('stage','install'):
   p=subprocess.run(exe+[action,'--intel-ring-entry'],capture_output=True,text=True);self.assertEqual(p.returncode,2);self.assertNotIn('Traceback',p.stderr);self.assertNotIn('unrecognized arguments',p.stderr)
if __name__=='__main__':unittest.main()
