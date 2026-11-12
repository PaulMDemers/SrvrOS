"""Actual original entry/post-source handoff and existing reset pipeline."""
from pathlib import Path
import base64,struct,json,os,re,resource,subprocess,sys,tempfile,unittest
from intel_ring_reset import reset_result,native_reset_result,entry_capture,reset_recipe,verify_reset_checkpoint,admit_summary
from intel_gt_wake import wake_result
from intel_ring_transition import ring_summary
from hardware_boot import stage,limine_config
ROOT=Path(__file__).resolve().parents[1]
EXE=Path(os.environ.get('INTEL_RING_RESET_EXE',ROOT/'build/tests/intel-ring-reset-adapter'))
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def run(mode):
 p=subprocess.run([str(EXE),'--wake-engine-'+mode],capture_output=True,text=True,timeout=20)
 if p.returncode:raise AssertionError((mode,p.stdout[-1600:],p.stderr))
 return p.stdout
def field(t,p,k,v):
 s,n=re.subn(r'(^'+re.escape(p)+r':.*?\b'+k+r'=)[^\s]+',lambda m:m[1]+str(v),t,count=1,flags=re.M);assert n==1
 return s
class RingResetTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.text=run('report');cls.stats={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',cls.text.split('ring-reset-fixture:')[1])}
 def failure(self,mode,retained=False,no_reset=False,gpu_retained=True):
  t=run(mode);w=wake_result(t)['summary'];self.assertFalse(w['complete'])
  try:r=reset_result(t)
  except ValueError:r=None
  if r:self.assertFalse(r['complete'])
  if retained:
   self.assertTrue(w['retained']);self.assertFalse(w['released']);self.assertEqual(w['retired'],0 if gpu_retained else 14)
  if no_reset:
   s=next(l for l in t.splitlines() if l.startswith('intel-held-reset-summary:'))
   values={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',s)};self.assertFalse(values['attempted'] or values['writes']);self.assertRegex(t,r'ring-reset-fixture:.* prepare_calls=0 ')
  return t
 def test_positive_original_fresh_source_and_existing_reset(self):
  r=reset_result(self.text);e=r['entry'];s=r['summary'];j=r['joint'];self.assertTrue(r['complete']);self.assertFalse(r['native_complete'] or r['managed_pages'] or r['hardware_ownership'] or r['physical_tearfree'] or r['enabled_writer_complete'])
  self.assertEqual((e['summary']['kind'],e['summary']['epoch_after'],e['summary']['bcs_writes']),(2,0,0));self.assertEqual((s['eligible'],s['bound'],s['complete'],s['checks']),(1,1,1,10))
  self.assertEqual(j['wake']['summary']['v'],26);self.assertEqual(j['wake']['summary']['engine_reads'],711)
  self.assertEqual([p['endpoint']['command'] for p in j['phases']],[6,2,2,6])
  self.assertEqual((j['summary']['borrows'],j['summary']['returns'],j['summary']['root_retired']),(4,4,1));self.assertTrue(j['held']['complete'])
  from intel_takeover import takeover_result
  t=takeover_result(self.text);self.assertLess(e['summary']['end'],t['summary']['begin_us']);self.assertEqual(__import__('intel_completion_audit').takeover_source(self.text)['summary']['begin_us'],e['post']['summary']['begin_us'])
  with self.assertRaises(ValueError):native_reset_result(self.text)
 def test_equal_entry_end_and_handoff_begin_is_monotonic(self):
  r=reset_result(run('reset-tie'));self.assertTrue(r['complete']);self.assertEqual(r['summary']['begin'],r['summary']['entry_end'])
 def test_enabled_and_nonzero_geometry_refuse_reset(self):
  r=reset_result(self.failure('reset-enabled',no_reset=True));self.assertTrue(r['entry']['complete']);self.assertEqual(r['entry']['summary']['bcs_writes'],1);self.assertFalse(r['summary']['eligible']);self.assertFalse(r['enabled_writer_complete'])
  self.failure('reset-nonzero-disabled',no_reset=True)
 def test_every_handoff_guard_clock_and_source_boundary(self):
  count=0
  for mode in ('reset-epoch','reset-provider','reset-return','reset-copy-entry','reset-copy-census','reset-copy-parent','reset-copy-takeover','reset-extra-borrow'):
   for n in range(1,self.stats['guards']+1):
    count+=1
    with self.subTest(mode=mode,n=n):self.failure(f'{mode}:{n}',retained=True,no_reset=True)
  for mode in ('reset-post-word','reset-entry-source-word'):
   for n in range(1,93):
    count+=1
    with self.subTest(mode=mode,n=n):self.failure(f'{mode}:{n}',retained=True,no_reset=True)
  for mode in ('reset-clock-zero','reset-clock-backward','reset-clock-time','reset-clock-age'):
   for n in range(1,self.stats['clocks']+1):
    count+=1
    with self.subTest(mode=mode,n=n):
     t=self.failure(f'{mode}:{n}',retained=True,no_reset=True);parsed=entry_capture(t);self.assertFalse(parsed['complete'])
     if n==1 and mode in ('reset-clock-zero','reset-clock-backward','reset-clock-age'):
      s=parsed['summary'];self.assertEqual(s['clock_prev'],s['entry_end']);self.assertEqual(s['clock_at'],0 if mode=='reset-clock-zero' else 1 if mode=='reset-clock-backward' else s['source_end']+100001)
  for n in range(2,self.stats['guards']+1):self.failure(f'reset-takeover-source:{n}',retained=True,no_reset=True);count+=1
  for n in (1,2,3):self.failure(f'reset-takeover-stamp:{n}',retained=True,no_reset=True);count+=1
  for n in range(1,4):self.failure(f'reset-post-stamp:{n}',retained=True,no_reset=True);count+=1
  print('reset handoff guards/source/clocks:',count)
 def test_entry_source_and_post_word_binding(self):
  count=0
  for mode in ('ring-read','ring-drift','ring-busy','ring-float','ring-time','ring-epoch','ring-borrow','ring-alias','ring-source-time','ring-source-begin','ring-source-last','ring-extra-borrow','ring-copy-census','ring-copy-owner','ring-copy-provider','ring-kind','ring-kind-mask'):
   for n in range(1,5):
    count+=1
    with self.subTest(mode=mode,n=n):self.failure(f'{mode}:{n}',no_reset=True)
  for mode in ('ring-source-word','ring-post-word'):
   for n in range(1,93):
    count+=1
    with self.subTest(mode=mode,n=n):self.failure(f'{mode}:{n}',retained=True,no_reset=True)
  for mode in ('ring-observer-tag','ring-post-stamp:1','ring-post-stamp:2','ring-post-stamp:3','ring-return-zero'):self.failure(mode,retained=True,no_reset=True)
  print('composed entry source/provider/word faults:',count)
 def test_all_entry_census_readers_and_clocks(self):
  count=0
  for mode,nmax in [('ring-census-'+s,self.stats['census_reads']) for s in ('read','drift','time')]+[('ring-reader-'+s,self.stats['reader_reads']) for s in ('read','drift','time')]+[('ring-core-'+s,self.stats['core_clocks']) for s in ('zero','backward','time')]:
   for n in range(1,nmax+1):
    count+=1
    with self.subTest(mode=mode,n=n):self.failure(f'{mode}:{n}')
  print('composed census/reader/entry clocks:',count)
 def test_every_original_hda_boundary(self):
  count=0
  for mode,nmax in [('joint-'+s,self.stats['hda_reads']) for s in ('read','drift','read-time','alias')]+[('joint-endpoint',self.stats['endpoints']),('joint-cache',self.stats['cache'])]:
   for n in range(1,nmax+1):
    count+=1
    with self.subTest(mode=mode,n=n):self.failure(f'{mode}:{n}')
  for mode in ('joint-none','joint-uncertain','joint-store-time','joint-store-zero','joint-store-backward'):
   for n in (1,2):self.failure(f'{mode}:{n}',retained=True,gpu_retained=n==1)
  print('composed original HDA boundaries:',count)
 def test_existing_nested_reset_claim_and_return_faults(self):
  for mode in ('interval-read','interval-drift','interval-foreign','interval-map','interval-time','joint-drop-hda-child','joint-drop-hda-list','joint-drop-hda-count','joint-drop-claim'):
   for n in (1,2):
    with self.subTest(mode=mode,n=n):self.failure(f'{mode}:{n}')
  for mode in ('joint-return-deadline','joint-retire:1','joint-map:1','joint-leave','joint-upper:1','hreset-map-slot'):
   with self.subTest(mode=mode):self.failure(mode)
 def native_fixture(self):
  from test_intel_shutdown import ShutdownTests
  conf=limine_config('0123456789abcdef','ram-usb-v1-intel-ring-reset-laptop-probe-37d8c5d0-auto-return');cmd=next(l.split('cmdline:',1)[1].strip() for l in conf.splitlines() if 'cmdline:' in l)
  native=ShutdownTests.native(self.text);before,after=native.split('intel-joint-hda:',1);prefix='init: deferred=ring-reset-preinit; terminal automatic capture, no user services\nlaptop-capture: boot options='+cmd+'\n'
  return prefix+before+'workspace-log: snapshot=session-a.log\n',prefix+before+'intel-wake: preflight checkpoint saved\nintel-joint-hda:'+after+'laptop-capture: completed=yes\n'
 def test_full_modeled_native_collection_and_provenance(self):
  pre,t=self.native_fixture();self.assertTrue(native_reset_result(t)['native_complete']);self.assertTrue(verify_reset_checkpoint(pre,t,native=True))
  from hardware_desktop import collect,needs_attention
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);work=root/'srvros-work';logs=work/'Logs/session-0000000000000001-00';logs.mkdir(parents=True);(work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
   for name,text in [('boot.log',pre),('session-a.log',pre),('exit.log',t)]:(logs/name).write_text(text)
   _,report=collect(root,'37d8c5d0',root/'positive');row,=report['sessions'];self.assertEqual(row['attention'],[]);self.assertTrue(row['intel_ring_reset']['native_complete']);self.assertIsNone(row['intel_ring_entry'])
   (logs/'exit.log').write_text(field(t,'intel-reset-admit','complete',0));_,report=collect(root,'37d8c5d0',root/'negative');self.assertTrue(report['sessions'][0]['attention']);self.assertFalse((report['sessions'][0]['intel_ring_reset'] or {}).get('native_complete',False))
  for p in ('intel-reset-admit','intel-reset-entry-summary','intel-reset-entry-post-summary'):
   l=next(l for l in t.splitlines() if l.startswith(p+':'));self.assertFalse(needs_attention(l));self.assertTrue(needs_attention(l+' extra=0'))
  for bad in (t.replace('ring-reset-preinit','other'),t+'\ninit: started pid=1\n',t.replace('srvros.intel-ring=reset-v1','srvros.intel-ring=entry-v1')):
   with self.assertRaises(ValueError):verify_reset_checkpoint(pre,bad,native=True)
 def test_strict_join_clock_source_and_old_contract_rejection(self):
  t=self.text;s=entry_capture(t)['summary']
  for p,k,v in [('intel-reset-admit','source_begin',s['source_begin']+1),('intel-reset-admit','source_last',s['source_last']+1),('intel-reset-admit','entry_end',s['entry_end']+1),('intel-reset-admit','epoch',1),('intel-reset-admit','checks',9),('intel-reset-admit','take_last',s['take_last']+1),('intel-takeover-summary','last_us',s['take_last']+1),('intel-reset-admit','begin',s['entry_end']-1),('intel-reset-admit','end',s['begin']+100001),('intel-reset-entry-summary','kind',1),('intel-reset-entry-summary','epoch_after',1),('intel-reset-entry-summary','bcs_writes',1)]:
   with self.subTest(p=p,k=k),self.assertRaises(ValueError):reset_result(field(t,p,k,v))
  for line in ('intel-reset-admit:','intel-reset-entry-post-summary:','intel-reset-entry-summary:'):
   l=next(l for l in t.splitlines() if l.startswith(line))
   for bad in (t+'\n'+l,t.replace(l,l+' extra=0')):
    with self.assertRaises(ValueError):reset_result(bad)
  from intel_ring_entry import entry_result
  from intel_entry_admission import entry_result as old
  self.assertIsNone(entry_result(t));self.assertIsNone(old(t))
 def test_lossless_vectors_are_canonical_ordered_and_complete(self):
  t=self.text
  vectors=[l for l in t.splitlines() if l.startswith('intel-reset-entry-post-vector:')]
  readers=[l for l in t.splitlines() if l.startswith('intel-reset-entry-readers:')]
  self.assertEqual((len(vectors),len(readers)),(2,2))
  bads=[t+'\n'+vectors[0],t+'\n'+readers[0],t.replace(vectors[0],vectors[0]+' extra=0'),t.replace(readers[0],readers[0]+' extra=0'),t.replace(vectors[0],vectors[0].replace('n=46','n=40')),t.replace(readers[0],readers[0][:-1]),t.replace(vectors[0],vectors[0]+'\nintel-reset-entry-post-row: v=2 s=0 e=0 d=0,0,0,0,1,0,0,0')]
  for bad in bads:
   with self.assertRaises(ValueError):reset_result(bad)
  swapped=t.replace(vectors[0],'SWAP').replace(vectors[1],vectors[0]).replace('SWAP',vectors[1])
  with self.assertRaises(ValueError):reset_result(swapped)
  floating=t
  for l in readers:
   old=l.split(' b=',1)[1];w=list(struct.unpack('<37I',base64.b64decode(old)));w[6]=0xffffffff
   floating=floating.replace(l,l.split(' b=',1)[0]+' b='+base64.b64encode(struct.pack('<37I',*w)).decode())
  with self.assertRaisesRegex(ValueError,'floating reader'):reset_result(floating)
 def test_exact_cli_recipe_compiled_gate_and_legacy_refusal(self):
  conf=limine_config('0123456789abcdef','ram-usb-v1-intel-ring-reset-laptop-probe-37d8c5d0-auto-return');cmd=next(l.split('cmdline:',1)[1].strip() for l in conf.splitlines() if 'cmdline:' in l);self.assertEqual(reset_recipe(cmd),dict(volume='37d8c5d0',release='0123456789abcdef'));self.assertEqual(len(cmd.split()),22)
  parser=str(ROOT/'build/tests/intel-gt-boot');self.assertEqual(subprocess.run([parser,'ring-reset',cmd]).returncode,0)
  ts=cmd.split()
  for i in range(len(ts)):
   for bad in (' '.join(ts[:i]+ts[i+1:]),cmd+' '+ts[i]):
    with self.assertRaises(ValueError):reset_recipe(bad)
    self.assertEqual(subprocess.run([parser,'ring-reset',bad]).returncode,1)
  for mode in ('joint','joint-admit','ring-entry','ring-transition','gpu-hold','rehold'):self.assertEqual(subprocess.run([parser,mode,cmd]).returncode,1)
  for kwargs in ({'intel_ring_entry':True},{'intel_gpu_hold':True},{'desktop_volume':'37d8c5d0'},{'hold_on_failure':True}):
   with self.assertRaises(ValueError):stage(laptop_probe_volume='37D8-C5D0',intel_ring_reset=True,**kwargs)
  help=subprocess.run([sys.executable,'tools/hardware_boot.py','stage','--help'],capture_output=True,text=True);self.assertIn('--intel-ring-reset',help.stdout)
  self.assertEqual(subprocess.run([sys.executable,'tools/hardware_boot.py','install','--intel-ring-reset'],capture_output=True,text=True).returncode,2)
if __name__=='__main__':unittest.main()
