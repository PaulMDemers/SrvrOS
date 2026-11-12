"""Actual entry/reset/requester adapter and strict joined wire contracts."""
from pathlib import Path
import base64,json,os,re,struct,subprocess,sys,tempfile,unittest
import test_intel_ring_reset as old
from intel_entry_requester import requester_result,native_requester_result,requester_recipe,verify_requester_checkpoint
from intel_gt_wake import wake_result
from intel_gpu_hold import gpu_hold_capture,FIELDS as GPU_FIELDS
from intel_ring_transition import FIELDS as ENTRY_FIELDS
from hardware_boot import limine_config,stage
ROOT=Path(__file__).resolve().parents[1]
EXE=Path(os.environ.get('INTEL_ENTRY_REQUESTER_EXE',ROOT/'build/tests/intel-entry-requester-adapter'))
def run(mode):
 p=subprocess.run([str(EXE),'--wake-engine-'+mode],capture_output=True,text=True,timeout=20)
 if p.returncode:raise AssertionError((mode,p.stdout[-1600:],p.stderr))
 return p.stdout

def stats(t,prefix):return {k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',t.split(prefix+':')[1].splitlines()[0])}
def packed(t,prefix,keys,key,value):
 line=next(l for l in t.splitlines() if l.startswith(prefix+':'));b=line.split(' b=',1)[1];raw=base64.b64decode(b);vs=list(struct.unpack('<'+'Q'*len(keys),raw));vs[keys.index(key)]=value
 return t.replace(line,line.split(' b=',1)[0]+' b='+base64.b64encode(struct.pack('<'+'Q'*len(keys),*vs)).decode())

class EntryRequesterTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.addClassCleanup(setattr,old,'EXE',old.EXE);old.EXE=EXE
  cls.text=run('report');cls.pending=run('gpu-pending');cls.stats=stats(cls.text,'ring-reset-fixture');cls.gpu=stats(cls.text,'gpu-fixture');cls.pending_gpu=stats(cls.pending,'gpu-fixture')
 def failure(self,mode,retained=False,no_reset=False,gpu_retained=True):
  t=old.RingResetTests.failure(self,mode,retained,no_reset,gpu_retained)
  try:r=requester_result(t)
  except ValueError:r=None
  if r:self.assertFalse(r['complete'])
  if no_reset:self.assertEqual(gpu_hold_capture(t)['summary']['attempts'],0)
  return t
 def test_positive_original_owner_and_pending_restoration(self):
  for t,polls in ((self.text,0),(self.pending,3)):
   r=requester_result(t);g=r['requester'];e=r['entry_reset'];self.assertTrue(r['complete']);self.assertFalse(r['native_complete'] or r['managed_pages'] or r['hardware_ownership'] or r['physical_tearfree'] or r['enabled_writer_complete'])
   self.assertEqual([p['words'][1]&65535 for p in g['phases']],[7,3,7]);self.assertEqual((g['summary']['polls'],g['summary']['issued'],g['summary']['claim_borrows'],g['summary']['claim_returns']),(polls,2,1,1));self.assertEqual(e['entry']['summary']['kind'],2);self.assertEqual(e['entry']['summary']['bcs_writes'],0)
   self.assertEqual(g['joint']['wake']['summary']['v'],27);self.assertEqual(g['joint']['wake']['summary']['engine_reads'],775+7*polls);self.assertEqual(g['joint']['held']['summary']['live_checks'],62+polls)
 def test_every_requester_io_and_original_source_boundary(self):
  count=0
  native=('gpu-last-time','gpu-last-zero','gpu-last-backward','gpu-last-alias','gpu-last-original','gpu-last-claim')
  source=('gpu-last-entry','gpu-last-census','gpu-last-takeover','gpu-last-bound','gpu-last-provider','gpu-last-entry-return','gpu-last-handoff-clock','gpu-last-parent')
  for mode,nmax,start in [(m,self.gpu['reads'],1) for m in ('gpu-read','gpu-drift','gpu-source')]+[(m,self.gpu['admissions'],1) for m in native+source]:
   for n in range(start,nmax+1):
    with self.subTest(mode=mode,n=n):
     t=self.failure(f'{mode}:{n}',retained=True);self.assertTrue(gpu_hold_capture(t)['summary']['borrow_active'])
    count+=1
  # Pending read/return suffix: earlier common prefixes are covered above.
  for mode,nmax in [(m,self.pending_gpu['reads']) for m in ('gpu-read','gpu-drift','gpu-source')]+[(m,self.pending_gpu['admissions']) for m in native+source]:
   for n in range(19,nmax+1):
    with self.subTest(pending=mode,n=n):self.failure(f'requester-pending-{mode}:{n}',retained=True)
    count+=1
  for pending in ('','requester-pending-'):
   for mode in ('gpu-none','gpu-uncertain'):
    for n in (1,2):self.failure(f'{pending}{mode}:{n}',retained=True);count+=1
  t=self.failure('gpu-timeout',retained=True);self.assertEqual(gpu_hold_capture(t)['summary']['polls'],32);count+=1
  print('composed requester I/O/source boundaries:',count)
 def test_entry_and_nonzero_geometry_never_enter_requester(self):
  for mode in ('reset-enabled','reset-nonzero-disabled','ring-return-zero','reset-clock-zero:19'):
   t=self.failure(mode,no_reset=True);self.assertFalse(gpu_hold_capture(t)['summary']['attempts'])
 def test_all_original_entry_census_reader_clocks(self):old.RingResetTests.test_all_entry_census_readers_and_clocks(self)
 def test_original_entry_source_provider_words(self):old.RingResetTests.test_entry_source_and_post_word_binding(self)
 def test_original_handoff_source_guard_and_clock(self):old.RingResetTests.test_every_handoff_guard_clock_and_source_boundary(self)
 def test_original_hda_boundaries_and_late_restoration(self):old.RingResetTests.test_every_original_hda_boundary(self)
 def test_original_reset_claim_return_boundaries(self):old.RingResetTests.test_existing_nested_reset_claim_and_return_faults(self)
 def fixture(self):
  from test_intel_shutdown import ShutdownTests
  conf=limine_config('0123456789abcdef','ram-usb-v1-intel-entry-requester-laptop-probe-37d8c5d0-auto-return');cmd=next(l.split('cmdline:',1)[1].strip() for l in conf.splitlines() if 'cmdline:' in l)
  native=ShutdownTests.native(self.text);before,after=native.split('intel-joint-hda:',1);prefix='init: deferred=entry-requester-preinit; terminal automatic capture, no user services\nlaptop-capture: boot options='+cmd+'\n'
  return prefix+before+'workspace-log: snapshot=session-a.log\n',prefix+before+'intel-wake: preflight checkpoint saved\nintel-joint-hda:'+after+'laptop-capture: completed=yes\n'
 def test_modeled_native_collection_and_exact_provenance(self):
  pre,t=self.fixture();self.assertTrue(native_requester_result(t)['native_complete']);self.assertTrue(verify_requester_checkpoint(pre,t,native=True))
  from hardware_desktop import collect,needs_attention
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);work=root/'srvros-work';logs=work/'Logs/session-0000000000000001-00';logs.mkdir(parents=True);(work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
   for name,text in [('boot.log',pre),('session-a.log',pre),('exit.log',t)]:(logs/name).write_text(text)
   _,report=collect(root,'37d8c5d0',root/'positive');row,=report['sessions'];self.assertEqual(row['attention'],[]);self.assertTrue(row['intel_entry_requester']['native_complete']);self.assertIsNone(row['intel_gpu_hold']);self.assertIsNone(row['intel_ring_reset'])
   bad=packed(t,'intel-gpu-hold-summary',GPU_FIELDS,'complete',0);(logs/'exit.log').write_text(bad);_,report=collect(root,'37d8c5d0',root/'negative');self.assertTrue(report['sessions'][0]['attention'])
  for prefix in ('intel-gpu-hold-summary:','intel-reset-entry-summary:'):
   line=next(l for l in t.splitlines() if l.startswith(prefix));self.assertFalse(needs_attention(line));self.assertTrue(needs_attention(line+' extra=0'))
  for bad in (t.replace('entry-requester-preinit','ring-reset-preinit'),t+'init: started pid=1\n',t.replace(' srvros.intel-gpu-requester=hold','')):
   with self.assertRaises(ValueError):verify_requester_checkpoint(pre,bad,native=True)
 def test_strict_lossless_raw_wire_and_old_image_refusal(self):
  t=self.text;r=requester_result(t);e=r['entry_reset']['entry']['summary'];g=r['requester']['summary']
  for prefix,keys,k,v in [('intel-reset-entry-summary',ENTRY_FIELDS+['kind'],'kind',1),('intel-reset-entry-summary',ENTRY_FIELDS+['kind'],'bcs_writes',1),('intel-gpu-hold-summary',GPU_FIELDS,'origin',g['origin']+1),('intel-gpu-hold-summary',GPU_FIELDS,'returned',0),('intel-gpu-hold-summary',GPU_FIELDS,'admit0',3),('intel-gpu-hold-summary',GPU_FIELDS,'assurances',31),('intel-gpu-hold-summary',GPU_FIELDS,'managed_pages',1)]:
   with self.subTest(k=k),self.assertRaises(ValueError):requester_result(packed(t,prefix,keys,k,v))
  vectors=[l for l in t.splitlines() if l.startswith('intel-entry-census:')];self.assertEqual(len(vectors),2)
  for prefix in ('intel-entry-census:','intel-reset-entry-summary:','intel-gpu-hold-summary:'):
   line=next(l for l in t.splitlines() if l.startswith(prefix))
   for bad in (t+'\n'+line,t.replace(line,line+' extra=0'),t.replace(line,line[:-1]),t.replace(line,'')):
    with self.subTest(prefix=prefix),self.assertRaises(ValueError):requester_result(bad)
  swapped=t.replace(vectors[0],'SWAP').replace(vectors[1],vectors[0]).replace('SWAP',vectors[1])
  with self.assertRaises(ValueError):requester_result(swapped)
  for extra in ('intel-completion-row: v=2 s=0 e=0 d=0,0,0,0,1,0,0,0','intel-compose-entry-summary: v=1 complete=1'):
   with self.assertRaises(ValueError):requester_result(t+'\n'+extra)
  from intel_ring_reset import reset_result
  from intel_gpu_hold import gpu_hold_result
  from intel_ring_entry import entry_result
  for decode in (reset_result,gpu_hold_result,entry_result):
   try:legacy=decode(t)
   except ValueError:continue
   self.assertFalse(legacy and legacy.get('complete'),decode.__name__)
 def test_exact23_recipe_c_gate_and_mutual_exclusivity(self):
  conf=limine_config('0123456789abcdef','ram-usb-v1-intel-entry-requester-laptop-probe-37d8c5d0-auto-return');cmd=next(l.split('cmdline:',1)[1].strip() for l in conf.splitlines() if 'cmdline:' in l);self.assertEqual(len(cmd.split()),23);self.assertEqual(requester_recipe(cmd),dict(volume='37d8c5d0',release='0123456789abcdef'))
  parser=str(ROOT/'build/tests/intel-gt-boot');self.assertEqual(subprocess.run([parser,'entry-requester',cmd]).returncode,0)
  ts=cmd.split()
  for i in range(len(ts)):
   for bad in (' '.join(ts[:i]+ts[i+1:]),cmd+' '+ts[i]):
    with self.assertRaises(ValueError):requester_recipe(bad)
    self.assertEqual(subprocess.run([parser,'entry-requester',bad]).returncode,1)
  for mode in ('ring-reset','gpu-hold','joint','ring-entry','ring-transition'):self.assertEqual(subprocess.run([parser,mode,cmd]).returncode,1)
  for kwargs in ({'intel_ring_reset':True},{'intel_gpu_hold':True},{'storage_only_capture':True},{'desktop_volume':'37d8c5d0'},{'hold_on_failure':True},{'repeat':2},{'desktop_scale':2}):
   with self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_entry_requester=True,**kwargs)
  with self.assertRaises(ValueError):stage(intel_entry_requester=True)
  p=subprocess.run([sys.executable,'tools/hardware_boot.py','stage','--help'],capture_output=True,text=True);self.assertIn('--intel-entry-requester',p.stdout)
  self.assertEqual(subprocess.run([sys.executable,'tools/hardware_boot.py','install','--intel-entry-requester'],capture_output=True,text=True).returncode,2)
if __name__=='__main__':unittest.main()
