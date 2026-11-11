"""Actual original-owner HDA/BCS adapter, exact recipe and evidence isolation."""
from pathlib import Path
import os
import re
import subprocess
import unittest
from intel_hda_shutdown import paired_recipe,paired_result,native_paired_result
from intel_gt_wake import wake_result
from hardware_boot import limine_config,stage

ROOT=Path(__file__).resolve().parents[1]
EXE=Path(os.environ.get('INTEL_HDA_SHUTDOWN_EXE',ROOT/'build/tests/intel-hda-shutdown-adapter'))
def run(mode):
 p=subprocess.run([str(EXE),'--wake-engine-'+mode],capture_output=True,text=True,timeout=15)
 if p.returncode:raise AssertionError((mode,p.stdout[-2200:],p.stderr))
 return p.stdout

def field(text,prefix,key,value):
 s,n=re.subn(r'(^'+re.escape(prefix)+r':.*?\b'+key+r'=)[^\s]+',lambda m:m[1]+str(value),text,count=1,flags=re.M)
 assert n==1
 return s

class PairedShutdownTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.text=run('blit-report')
  cls.stats={k:int(v) for k,v in re.findall(r'(hda_reads|endpoints|cache|attempts|stores)=(\d+)',cls.text.split('hda-bcs-fixture:')[1])}
 def test_both_branches_and_no_claim_or_native_inference(self):
  for mode,enabled in [('blit-report',True),('blit-off-report',False)]:
   with self.subTest(mode=mode):
    t=run(mode);r=paired_result(t);h=r['pair']['summary']
    self.assertTrue(r['complete']);self.assertEqual(r['enabled_writer_complete'],enabled)
    self.assertEqual([p['endpoint']['command'] for p in r['pair']['phases']],[6,2,2,6])
    self.assertEqual((h['root_maps'],h['root_retired'],h['borrows'],h['returns'],h['hda_writes']),(1,1,4,4,2))
    self.assertEqual((h['complete'],h['gpu_complete'],h['attached']),(0,0,0))
    self.assertEqual((r['io']['entry_attempts'],r['io']['entry_matches'],r['io']['store_checks']),(2,3,int(enabled)))
    self.assertEqual(r['shutdown']['summary']['epoch_after'],int(enabled))
    self.assertFalse(r['native_complete'] or r['physical_tearfree'] or r['hardware_ownership'] or r['managed_pages'] or r['reset'])
    with self.assertRaises(ValueError):native_paired_result(t)
    from intel_shutdown import shutdown_result
    with self.assertRaises(ValueError):shutdown_result(t)
    from intel_joint import joint_result
    self.assertIsNone(joint_result(t))
 def test_independent_fixture_chain_and_preinit_checkpoint(self):
  from test_intel_shutdown import ShutdownTests
  from intel_hda_shutdown import verify_paired_checkpoint
  config=limine_config('0123456789abcdef','ram-usb-v1-intel-paired-shutdown-laptop-probe-37d8c5d0-auto-return')
  cmd=next(l.split('cmdline:',1)[1].strip() for l in config.splitlines() if 'cmdline:' in l)
  prefix='init: deferred=hda-bcs-preinit; terminal automatic capture, no user services\nlaptop-capture: boot options='+cmd+'\n'
  text=prefix+ShutdownTests.native(self.text)
  self.assertTrue(native_paired_result(text)['native_complete']) # Independent model chain, not physical evidence.
  pre=text.split('intel-wake: preflight checkpoint saved',1)[0]+'workspace-log: snapshot=session-a.log\n'
  self.assertTrue(verify_paired_checkpoint(pre,text,native=True))
  for bad in (text.replace('deferred=hda-bcs-preinit','deferred=other'),text+'\ninit: started pid=1\n',text.replace('srvros.intel-hda-bcs=joint-owner-v1','srvros.intel-joint=hda-claim')):
   with self.assertRaises(ValueError):verify_paired_checkpoint(pre,bad,native=True)
  with self.assertRaises(ValueError):verify_paired_checkpoint(pre.replace('deferred=hda-bcs-preinit','deferred=other'),text,native=True)
 def test_prewrite_refusals_and_source_binding(self):
  modes=('blit-notidle','blit-execlist','blit-ppgtt','blit-busy','blit-start','blit-change','blit-float','blit-read-fail','blit-unissued','blit-reader-fail','blit-reader-primary','active','pending','changed','float','read-fail','cache-last','paired-entry-1','paired-entry-2','paired-source-owner','paired-source-result','paired-source-census','paired-source-trial')
  for mode in modes:
   with self.subTest(mode=mode):
    t=run(mode);w=wake_result(t)
    self.assertFalse(w['lease_complete'])
    from intel_blitter import blitter_result
    self.assertEqual(blitter_result(t)['summary']['writes'],0)
    try:r=paired_result(t)
    except ValueError:continue
    self.assertFalse(r['complete'])
 def test_final_store_clock_refusal_without_relaxing_deadlines(self):
  for mode in ('zero','backward','core','owner','phase','hda','outer'):
   with self.subTest(mode=mode):
    t=run('paired-clock-'+mode)
    from intel_blitter import blitter_result
    self.assertEqual(blitter_result(t)['summary']['writes'],0)
    self.assertFalse(wake_result(t)['lease_complete'])
    try:r=paired_result(t)
    except ValueError:continue
    self.assertFalse(r['complete']);self.assertEqual((r['io']['store_checks'],r['io']['fault']),(1,1))
 def test_uncertain_store_and_postconditions_keep_original_pins(self):
  for mode in ('blit-uncertain','blit-readback','blit-mode-change','blit-late-foreign','blit-reader-change','blit-audio-change','blit-post-read','blit-post-change','blit-post-enabled','blit-post-pending','blit-post-float','blit-post-foreign','blit-post-clock','blit-post-backward','blit-link-change','blit-ltr-change'):
   with self.subTest(mode=mode):
    t=run(mode);w=wake_result(t)['summary']
    self.assertTrue(w['retained']);self.assertEqual((w['released'],w['retired']),(0,0))
    from intel_joint import joint_capture
    h=joint_capture(t.replace('intel-hda-bcs-hda:','intel-joint-hda:').replace('intel-hda-bcs-summary:','intel-joint-summary:'))['summary']
    self.assertTrue(h['pinned'] and h['retained']);self.assertEqual(h['hda_writes'],1)
    try:r=paired_result(t)
    except ValueError:continue
    self.assertFalse(r['complete'])
 def test_every_hda_load_endpoint_alias_cache_and_clock_boundary(self):
  cases=0
  for mode,count in (('joint-read',self.stats['hda_reads']),('joint-drift',self.stats['hda_reads']),('joint-read-time',self.stats['hda_reads']),('joint-alias',self.stats['hda_reads']),('joint-endpoint',self.stats['endpoints']),('joint-cache',self.stats['cache'])):
   for point in range(1,count+1):
    cases+=1
    with self.subTest(mode=mode,point=point):
     t=run(f'{mode}:{point}');self.assertFalse(wake_result(t)['lease_complete'])
     try:r=paired_result(t)
     except ValueError:continue
     self.assertFalse(r['complete'])
  for mode in ('joint-map:1','joint-retire:1','joint-none:1','joint-none:2','joint-uncertain:1','joint-uncertain:2','joint-store-time:1','joint-store-time:2','joint-upper:1'):
   with self.subTest(mode=mode):self.assertFalse(wake_result(run(mode))['lease_complete'])
  print('original-owner HDA/BCS fault prefixes:',cases)
 def test_strict_raw_versions_coverage_clock_and_authority(self):
  for prefix in ('intel-hda-bcs-hda','intel-hda-bcs-summary','intel-hda-bcs-entry','intel-hda-bcs-io','intel-clock','intel-shutdown-summary'):
   line=next(l for l in self.text.splitlines() if l.startswith(prefix+':'))
   for t in (self.text.replace(line+'\n','',1),self.text+'\n'+line,self.text.replace(line,line+' extra=0',1)):
    with self.subTest(prefix=prefix),self.assertRaises(ValueError):paired_result(t)
  for prefix,k,v in [('intel-wake-summary','v',8),('intel-wake-summary','v',23),('intel-hda-bcs-summary','complete',1),('intel-hda-bcs-summary','attached',1),('intel-hda-bcs-summary','hda_writes',1),('intel-hda-bcs-summary','managed_pages',1),('intel-hda-bcs-io','entry_matches',1),('intel-hda-bcs-io','owner',1),('intel-hda-bcs-io','fault',1),('intel-hda-bcs-io','previous',1<<64)]:
   with self.subTest(k=k,prefix=prefix),self.assertRaises(ValueError):paired_result(field(self.text,prefix,k,v))
  for prefix in ('intel-claim-summary: v=1','intel-joint-summary: v=1','intel-held-reset-summary: v=1','intel-hda-bcs-future: v=1'):
   with self.subTest(prefix=prefix),self.assertRaises(ValueError):paired_result(self.text+'\n'+prefix)
 def test_exact_sixteen_token_recipe_and_profile_isolation(self):
  c=limine_config('0123456789abcdef','ram-usb-v1-intel-paired-shutdown-laptop-probe-37d8c5d0-auto-return')
  tokens=next(l.split('cmdline:',1)[1].split() for l in c.splitlines() if 'cmdline:' in l)
  cmd=' '.join(tokens);self.assertEqual(len(tokens),16);self.assertEqual(paired_recipe(cmd)['release'],'0123456789abcdef')
  exe=str(ROOT/'build/tests/intel-gt-boot')
  self.assertEqual(subprocess.run([exe,'hda-shutdown',cmd]).returncode,0)
  self.assertEqual(subprocess.run([exe,cmd]).returncode,1)
  for n in range(16):
   for t in (tokens[:n]+tokens[n+1:],tokens+[tokens[n]]):
    bad=' '.join(t)
    with self.assertRaises(ValueError):paired_recipe(bad)
    self.assertEqual(subprocess.run([exe,'hda-shutdown',bad]).returncode,1)
  for key in ('intel_shutdown_trial','intel_joint','intel_gpu_hold','intel_blitter_trial','intel_bcs_reset_trial','intel_engine_capture','haswell_register_sample','desktop_audio','hold_on_failure'):
   with self.subTest(key=key),self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_hda_shutdown=True,**{key:True})
 def test_collector_attention_is_separate_from_claim_success(self):
  from hardware_desktop import needs_attention
  line=next(l for l in self.text.splitlines() if l.startswith('intel-hda-bcs-summary:'))
  self.assertFalse(needs_attention(line));self.assertTrue(needs_attention(line+' unknown=0'))
  self.assertTrue(needs_attention(field(line,'intel-hda-bcs-summary','hda_complete',0)))
  self.assertTrue(needs_attention(line.replace('intel-hda-bcs-summary:','intel-joint-summary:')))
if __name__=='__main__':unittest.main()
