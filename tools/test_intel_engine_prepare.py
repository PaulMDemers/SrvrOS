"""Actual five-engine prepare/hold/cancel core fault boundaries."""
import resource
import subprocess
import unittest
from pathlib import Path
import re
from intel_engine_prepare import prepare_result,native_prepare_result
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
CORE=Path('build/tests/intel-engine-prepare-core')
ADAPTER=Path('build/tests/intel-engine-prepare-adapter')
ADAPTER_MODES=['report','prep-plane','prep-plane-read','prep-audio']+[m+':'+str(n) for m in ('prep-none','prep-uncertain','prep-foreign','prep-ack-float','prep-map') for n in range(1,11)]
ADAPTER_MODES += [m+':'+str(n) for m in ('prep-exit-endpoint','prep-exit-backing') for n in range(1,11)]
from test_intel_takeover import ADAPTER_MODES as TAKE_MODES
ADAPTER_MODES += [m for m in TAKE_MODES if m not in ADAPTER_MODES]
def output(exe,*args):return subprocess.check_output([str(exe),*args],text=True,timeout=10)
MODES=['normal','wait','copy','policy','pre-epoch','pre-active','pre-domain','pre-batches','pre-count','pre-time','pre-context','extra','preserve','preserve-epoch','preserve-time','end-deadline','end-epoch']
POINTS={**{m:range(1,11) for m in ('write-none','write-uncertain','write-epoch','write-detach','ack-float','ack-reserved','ack-foreign','poll-read','poll-epoch','phase-time','first-ack-time','never-ready')},**{m:range(1,25) for m in ('sample-read','sample-float')},**{m:range(1,5) for m in ('sample-count','sample-epoch')},**{m:range(5) for m in ('pre-geometry','pre-wait','pre-busy','hold-drift','hold-mask-drift')},**{m:range(1,9) for m in ('guard','guard-time','guard-epoch','clock-epoch','zero','backward')}}
class PrepareTests(unittest.TestCase):
 def test_preservation_summary_attention(self):
  from hardware_desktop import needs_attention
  text=output(ADAPTER,'--wake-engine-report')
  line=next(l for l in text.splitlines() if l.startswith('intel-prepare-post-summary:'))
  self.assertFalse(needs_attention(line))
  for changed in (line.replace('complete=1','complete=0'),
                  line.replace('reads=92','reads=91'),
                  line.replace('reason=0','reason=1'),
                  line.replace('managed_pages=0','managed_pages=1'),
                  line.replace(' v=1',' v=2'),line+' unknown=0',
                  line.replace(' complete=1','')):
   with self.subTest(line=changed):self.assertTrue(needs_attention(changed))
 def test_completed_store_prefix_on_exit_failure(self):
  for mode in ('prep-exit-endpoint','prep-exit-backing'):
   for n in range(1,11):
    text=output(ADAPTER,'--wake-engine-'+mode+':'+str(n));r=prepare_result(text)['summary']
    self.assertEqual((r['attempts'],r['writes'],r['acks']),(n,n,n))
    self.assertTrue(r['retained'] and r['uncertain'] and r['reason']&8)
    self.assertFalse(r['complete'] or r['returned'])
 def test_bounded_backing_provider(self):
  self.assertIn('backing-batch-provider: PASS',output(ADAPTER,'--backing-batch-provider'))
 def test_fused_guard_endpoint(self):
  self.assertIn('guard-endpoint: PASS',output(ADAPTER,'--guard-endpoint'))
 def test_actual_core(self):
  cases=[(m,[]) for m in MODES]+[(m,[str(p)]) for m,points in POINTS.items() for p in points]
  for mode,args in cases:
   with self.subTest(mode=mode,args=args):
    p=subprocess.run([str(CORE),mode,*args],capture_output=True,text=True,timeout=5)
    self.assertEqual(p.returncode,0,p.stdout+p.stderr)
 def test_actual_adapter(self):
  for mode in ADAPTER_MODES:
   with self.subTest(mode=mode):
    result=prepare_result(output(ADAPTER,'--wake-engine-'+mode))
    self.assertEqual(result['complete'],mode=='report')
    self.assertFalse(result['hardware_ownership'] or result['reset_admitted'] or result['managed_range'])
 def test_receipt_mutations(self):
  text=output(ADAPTER,'--wake-engine-report');self.assertTrue(prepare_result(text)['complete'])
  summary=next(l for l in text.splitlines() if l.startswith('intel-prepare-summary:'))
  for old,new in [('attempts=10','attempts=9'),('writes=10','writes=9'),('reads=34','reads=33'),('snapshots=4','snapshots=3'),('sample_attempts=4','sample_attempts=3'),('acks=10','acks=9'),('ready=31','ready=30'),('cancelled=31','cancelled=30'),('held=1','held=0'),('preserved=1','preserved=0'),('returned=1','returned=0'),('retained=0','retained=1'),('epoch=0','epoch=1'),('obligations=255','obligations=0'),('reset_writes=0','reset_writes=1'),('hardware_ownership=0','hardware_ownership=1')]:
   with self.subTest(old=old),self.assertRaises(ValueError):prepare_result(text.replace(summary,summary.replace(old,new)))
  for old,new in [('n=0 d=3,3,0','n=0 d=3,0,0'),('s=1 n=6 d=3,3,3,3,3,0','s=1 n=6 d=3,3,3,3,0,0'),('intel-wake-summary: v=14','intel-wake-summary: v=13'),('engine_reads=230','engine_reads=229'),('intel-prepare-post-row: v=2 s=0 e=0 d=0,0,0,0,1,0,0,0','intel-prepare-post-row: v=2 s=0 e=0 d=1,0,0,0,1,0,0,0')]:
   self.assertIn(old,text)
   with self.subTest(old=old),self.assertRaises(ValueError):prepare_result(text.replace(old,new))
  for line in [l for l in text.splitlines() if l.startswith('intel-prepare-')]:
   with self.subTest(missing=line),self.assertRaises(ValueError):prepare_result(text.replace(line+'\n','',1))
   with self.subTest(duplicate=line),self.assertRaises(ValueError):prepare_result(text+'\n'+line)
  with self.assertRaises(ValueError):prepare_result(text+'\nintel-prepare-unknown: v=1')
 def test_native_contract(self):
  cache=output(Path('build/tests/intel-cache-probe'),'fresh-uc')
  handoff=output(Path('build/tests/intel-handoff-probe'),'0')
  adapter=output(ADAPTER,'--wake-engine-report').replace('intel-wake: bounded','intel-wake: preflight checkpoint saved\nintel-wake: bounded',1)
  text='bootloader: Limine 11.4.1\n'+cache+'intel-boot-claim: v=1 policy=2 client=2 immutable=1 hardware_ownership=0 managed_pages=0\n'+handoff+adapter
  self.assertTrue(native_prepare_result(text)['native_complete'])
  for old,new in [('Limine 11.4.1','Limine 11.4.2'),('intel-wake: preflight checkpoint saved',''),('maps=13','maps=12'),('cache_checks=13','cache_checks=12')]:
   with self.subTest(old=old),self.assertRaises(ValueError):native_prepare_result(text.replace(old,new))
 def test_recipe(self):
  from hardware_boot import limine_config,stage
  cfg=limine_config('0123456789abcdef','ram-usb-v1-intel-engine-prepare-laptop-probe-37d8c5d0-auto-return')
  cmd=next(l.split('cmdline:',1)[1].strip() for l in cfg.splitlines() if 'cmdline:' in l)
  self.assertEqual(len(cmd.split()),16);self.assertIn('srvros.intel-engines-prepare=request-cancel',cmd)
  subprocess.run(['build/tests/intel-gt-boot','engine-prepare',cmd],check=True)
  for extra in ({'desktop_audio':True},{'intel_bcs_reset_trial':True},{'intel_takeover_preflight':True},{'hold_on_failure':True}):
   with self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_engine_prepare=True,**extra)
if __name__=='__main__':unittest.main()

