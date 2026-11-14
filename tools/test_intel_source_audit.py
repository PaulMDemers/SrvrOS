"""Actual v35 held PCI/cache/display-reader adapter and strict raw receiver."""
import base64,os,re,resource,subprocess,tempfile,unittest
from pathlib import Path
from intel_source_audit import audit_recipe,audit_capture,audit_result,native_audit_result,verify_audit_checkpoint,audit_view
from intel_source_engines import engines_result
BIN=Path(os.environ.get('INTEL_SOURCE_AUDIT_TEST_BINARY',Path(__file__).resolve().parents[1]/'build/tests/intel-source-audit-adapter'))
def run(mode):
 def limit():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 p=subprocess.run([str(BIN),'--wake-engine-'+mode],text=True,capture_output=True,preexec_fn=limit)
 if p.returncode:raise AssertionError((mode,p.returncode,p.stderr,p.stdout[-1600:]))
 return p.stdout

def fixture(text):
 line=next(l for l in text.splitlines() if l.startswith('source-audit-fixture:'))
 return {k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)}
def native_chain(text):
 from test_intel_source_engines import native_chain as previous
 pre,final=previous(audit_view(text))
 # Earlier fixture builders supply native preflight, never replace raw v35 reset bytes.
 reset_line=next(l for l in text.splitlines() if l.startswith('intel-source-audit-reset:'))
 final=re.sub(r'(?m)^intel-nonzero-ack:.*\n',lambda m: reset_line+'\n' if ' n=0 ' in m[0] else '',final)
 final=re.sub(r'(?m)^intel-nonzero-pulse:.*\n','',final)
 return (pre.replace('source-engines-v1','source-audit-v1').replace('source-engines-preinit','source-audit-preinit'),
         final.replace('source-engines-v1','source-audit-v1').replace('source-engines-preinit','source-audit-preinit').replace('intel-wake-summary: v=34 ','intel-wake-summary: v=35 '))
class Audit(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.texts=[run('report'),run('entry-off-report')]
 def retained(self,text):
  f=fixture(text);self.assertEqual((f['actual'],f['command']),(5,3));self.assertIn('retained=1 success=0',text)
  try:r=audit_result(text)
  except ValueError:return
  self.assertFalse(r['complete'])
 def test_two_original_routes_live_owner_and_exact_raw_vectors(self):
  for off in ('','entry-off-'):
   for mode in ('report','nz-default','nz-poll','engines-poll','requester-pending'):
    text=run(off+mode);r=audit_result(text);self.assertTrue(r['complete']);self.assertFalse(r['native_complete']);self.assertEqual((r['summary']['pci_reads'],r['summary']['plane_reads'],r['summary']['cache_checks']),(18,54,2))
    self.assertEqual(r['samples'][0]['pci'],r['samples'][1]['pci']);self.assertEqual(r['samples'][0]['planes'],r['samples'][1]['planes']);self.assertEqual(r['summary']['obligations'],255);self.assertEqual(r['summary']['range_checks'],4);self.assertEqual([q['pages'] for q in r['samples'][0]['ranges']],[1356,1356])
    self.assertFalse(any(r['summary'][k] for k in ('assurances','managed_pages','pte_writes','hardware_ownership','registered')))
    with self.assertRaises(ValueError):engines_result(text)
    with self.assertRaises(ValueError):native_audit_result(text)
 def test_each_clock_source_counts_registration_and_original_deadlines(self):
  modes=('audit-zero','audit-backward','audit-time','audit-parent-time','audit-source','audit-provider','audit-epoch','audit-kind','audit-mode','audit-origin','audit-parent-origin','audit-count','audit-samples','audit-state','audit-registration','audit-root','audit-parent-registration','audit-io')
  for off,text in zip(('','entry-off-'),self.texts):
   for mode in modes:
    for n in range(1,fixture(text)['clocks']+1):
     with self.subTest(off=off,mode=mode,n=n):self.retained(run(f'{off}{mode}:{n}'))
 def test_every_raw_target_failure_and_drift_stops_later_targets(self):
  for off in ('','entry-off-'):
   for target,maximum in (('pci',18),('plane',54)):
    for mode in ('fail','float','drift'):
     for n in range(1,maximum+1):
      with self.subTest(off=off,target=target,mode=mode,n=n):
       text=run(f'{off}audit-{target}-{mode}:{n}');self.retained(text);self.assertLessEqual(fixture(text)['pci' if target=='pci' else 'planes'],n)
 def test_each_complete_context_failure_and_clock_metadata_retains(self):
  for off in ('','entry-off-'):
   for mode in ('fail','time','before','zero','hole','pat'):
    for n in (1,2):
     with self.subTest(off=off,mode=mode,n=n):self.retained(run(f'{off}audit-cache-{mode}:{n}'))
 def test_final_parent_restore_requires_original_completed_audit(self):
  for off in ('','entry-off-'):
   for mode in ('complete','parent','registered','count','pci','plane','cache','cache-raw','range-count','range-base','range-pages','range-index','range-type','range-checks'):
    with self.subTest(off=off,mode=mode):
     text=run(f'{off}audit-final-{mode}:2');f=fixture(text);self.assertEqual((f['actual'],f['command']),(10,3));self.assertIn('retained=1 success=0',text)
     row=next(l for l in text.splitlines() if l.startswith('source-requester-fixture:'));self.assertIn('actual=1',row);self.assertIn('complete=0',row)
     try:r=audit_result(text)
     except ValueError:continue
     self.assertFalse(r['complete'])
 def test_exact_recipe_image_preinit_coverage_and_each_summary_field(self):
  from hardware_boot import limine_config,stage
  config=limine_config('0123456789abcdef','ram-usb-v1-intel-source-audit-laptop-probe-37d8c5d0-auto-return');cmd=next(l.split('cmdline:',1)[1].strip() for l in config.splitlines() if 'cmdline:' in l)
  self.assertEqual(audit_recipe(cmd),dict(volume='37d8c5d0',release='0123456789abcdef'));tokens=cmd.split();self.assertEqual(len(tokens),15)
  for n in range(15):
   for bad in (' '.join(tokens[:n]+tokens[n+1:]),cmd+' '+tokens[n]):
    with self.assertRaises(ValueError):audit_recipe(bad)
  for extra in ({'intel_source_engines':True},{'intel_source_requester':True},{'repeat':2},{'hold_on_failure':True},{'desktop_scale':2},{'aml_capture':True},{'desktop_audio':True}):
   with self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_source_audit=True,**extra)
  for text in self.texts:
   line=next(l for l in text.splitlines() if l.startswith('intel-source-audit-summary:'))
   for key,value in re.findall(r' (\w+)=(\d+)',line):
    with self.subTest(key=key),self.assertRaises(ValueError):audit_result(text.replace(line,line.replace(f' {key}={value}',f' {key}={int(value)+200000}',1),1))
   for key in ('v','state','epoch','kind','mode','origin','parent','begin','last','end','samples','pci_reads','plane_reads','cache_checks','range_checks','complete'):
    with self.subTest(required_nonzero=key),self.assertRaises(ValueError):audit_result(text.replace(line,re.sub(rf' {key}=\d+',f' {key}=0',line),1))
   for prefix in ('intel-source-audit-data:','intel-source-audit-summary:','intel-source-audit-reset:'):
    row=next(l for l in text.splitlines() if l.startswith(prefix))
    for bad in (text.replace(row+'\n',''),text.replace(row,row+'\n'+row)):
     with self.assertRaises(ValueError):audit_result(bad)
   for bad in (text.replace('v=35 compiled=','v=34 compiled='),text.replace('source-audit-preinit','source-engines-preinit'),text.replace('source-audit-v1','source-engines-v1'),text+'intel-source-audit-extra: v=1\n'):
    with self.assertRaises(ValueError):audit_result(bad)
 def test_raw_vector_metadata_and_compact_reset_mutations(self):
  for text in self.texts:
   for prefix,size,offsets in (('intel-source-audit-data:',528,(0,4,32,36,140,144,152,160,168,176,180,184,196,200,208,212,216,220,224,228,232,240,244,248,252,256,260,264,527)),('intel-source-audit-reset:',528,(0,4,8,12,20,28,36,440,527))):
    line=next(l for l in text.splitlines() if l.startswith(prefix));raw=base64.b64decode(line.split('b=',1)[1]);self.assertEqual(len(raw),size)
    for offset in offsets:
     data=bytearray(raw)
     if prefix=='intel-source-audit-data:' and offset in (144,152,160,168):data[offset:offset+8]=bytes([255])*8
     elif prefix=='intel-source-audit-data:' and offset==176:data[offset:offset+4]=bytes([255])*4
     elif prefix=='intel-source-audit-reset:' and offset in (12,20,28):data[offset:offset+8]=bytes([255])*8
     else:data[offset]^=1
     bad=line.split('b=',1)[0]+'b='+base64.b64encode(data).decode()
     with self.subTest(prefix=prefix,offset=offset),self.assertRaises(ValueError):audit_result(text.replace(line,bad))
 def test_modeled_native_collection_preserves_original_raw_and_refuses_splicing(self):
  from hardware_desktop import collect
  for text in self.texts:
   pre,final=native_chain(text);self.assertTrue(native_audit_result(final)['native_complete']);self.assertTrue(verify_audit_checkpoint(pre,final,native=True))
   with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);work=root/'srvros-work';logs=work/'Logs'/'session-0000000000000001-00';logs.mkdir(parents=True);(work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
    for name,data in [('boot.log',pre),('session-a.log',pre),('exit.log',final)]:(logs/name).write_text(data)
    saved,report=collect(root,'37d8c5d0',root/'collected');row,=report['sessions'];self.assertTrue(row['intel_source_audit']['native_complete']);self.assertIsNone(row['intel_source_engines']);self.assertEqual(row['intel_gt_wake']['summary']['v'],35)
    self.assertEqual((saved/logs.name/'exit.log').read_text(),final);self.assertFalse(any('Invalid' in a or 'incomplete' in a for a in row['attention']))
    (logs/'exit.log').write_text(final.replace('cache_checks=2 range_checks=4','cache_checks=1 range_checks=4'))
    _,report=collect(root,'37d8c5d0',root/'rejected');row,=report['sessions'];self.assertFalse((row['intel_source_audit'] or {}).get('native_complete'));self.assertTrue(row['attention'])
 def test_cache_context_actual_fresh_read_comparison(self):
  subprocess.run(['build/tests/intel-cache-probe','context'],check=True,capture_output=True)
if __name__=='__main__':unittest.main()
