"""Read-only AF capture, prefixes, clocks, recipes and unsupported authority."""
from pathlib import Path
import re,subprocess,unittest
from intel_requester import requester_result,native_requester_result
ROOT=Path(__file__).resolve().parents[1]
CORE=ROOT/'build/tests/intel-requester-core'
ADAPTER=ROOT/'build/tests/intel-requester-adapter'
MODES='normal master-off pending pending-transition status-transition flr status-reserved frozen copy pre-epoch pre-count pre-time pre-census extra end-deadline end-epoch'.split()
POINTS={'read':range(1,19),'float':range(1,19),'profile':(0,2,3,4,5,6,7,8),'count':(1,2),'guard':range(1,5),'guard-time':range(1,5),'guard-epoch':range(1,5),'guard-detach':range(1,5),'sample-epoch':(1,2),'sample-time':(1,2),'zero':range(1,11),'backward':range(2,11),'clock-first':(2,3),'clock-epoch':(2,3)}
POSITIVE='report req-pending req-pending-transition take-busy take-mask take-reserved take-domain take-domain-reserved active pending flip-wait tlb-pending'.split()
ADAPTER_MODES=POSITIVE+['req-status-reserved']+[m+':'+str(n) for m in ('req-read','req-float','req-foreign','req-map') for n in range(1,19)]+['req-profile:'+str(n) for n in (0,2,3,4,5,6,7,8)]+[m+':'+str(n) for m in ('req-exit-endpoint','req-exit-backing') for n in (1,2)]
def output(exe,*args):return subprocess.check_output([str(exe),*map(str,args)],text=True,timeout=15)
def adapter(mode='report'):return output(ADAPTER,'--wake-engine-'+mode)
class RequesterTests(unittest.TestCase):
 def test_core_faults(self):
  for mode in MODES:
   with self.subTest(mode=mode):self.assertIn('PASS',output(CORE,mode))
  for mode,points in POINTS.items():
   for n in points:
    with self.subTest(mode=mode,n=n):self.assertIn('PASS',output(CORE,mode,n))
 def test_actual_adapter(self):
  for mode in ADAPTER_MODES:
   with self.subTest(mode=mode):
    t=adapter(mode);r=requester_result(t);self.assertEqual(r['complete'],mode in POSITIVE)
    self.assertFalse(any(r[k] for k in ('dma_drained','cache_ownership','hardware_ownership','managed_range','physical_tearfree')))
 def test_capability_prefix_gates_every_next_fixed_read(self):
  for i in (0,2,3,4,5,6,7,8):
   with self.subTest(i=i):
    r=requester_result(adapter('req-profile:'+str(i)))
    self.assertFalse(r['complete']);self.assertEqual(r['summary']['reads'],i+1)
    self.assertEqual(r['summary']['profiles'],0)
 def test_busy_and_pending_do_not_create_ownership(self):
  for mode,pending in [('report',0),('req-pending',3),('req-pending-transition',2)]:
   r=requester_result(adapter(mode));self.assertTrue(r['complete']);self.assertEqual(r['summary']['pending'],pending)
   self.assertEqual(r['summary']['stable'],mode!='req-pending-transition');self.assertFalse(r['dma_drained'])
 def test_exit_failure_retains_prefix_and_aliases(self):
  from intel_gt_wake import wake_result
  for kind in ('endpoint','backing'):
   for n in (1,2):
    with self.subTest(kind=kind,n=n):
     t=adapter('req-exit-'+kind+':'+str(n));r=requester_result(t);w=wake_result(t)['summary']
     self.assertEqual(r['summary']['reads'],9*n);self.assertEqual(r['summary']['profiles'],n-1)
     self.assertEqual(r['summary']['returned'],0);self.assertEqual(r['summary']['retained'],1)
     self.assertEqual((w['retired'],w['released'],w['retained']),(0,0,1))
 def test_strict_rows_schemas_masks_order_and_unsupported_authority(self):
  t=adapter();line=next(l for l in t.splitlines() if l.startswith('intel-requester-summary:'))
  for a,b in [('reads=18','reads=17'),('profiles=2','profiles=1'),('pending=0','pending=3'),('bus_master=3','bus_master=0'),('obligations=255','obligations=0'),('config_writes=0','config_writes=1'),('reset_writes=0','reset_writes=1'),('hardware_ownership=0','hardware_ownership=1'),('flr=0','flr=1'),('stable=1','stable=0'),('returned=1','returned=0'),('clock_at=0','clock_at=1')]:
   with self.subTest(a=a),self.assertRaises(ValueError):requester_result(t.replace(line,line.replace(a,b)))
  rows=[l for l in t.splitlines() if l.startswith('intel-requester-')]
  for row in rows:
   for text in (t.replace(row+'\n','',1),t+'\n'+row,t.replace(row,row+' extra=0')):
    with self.subTest(row=row),self.assertRaises(ValueError):requester_result(text)
  for old,new in [('intel-wake-summary: v=15','intel-wake-summary: v=13'),('engine_reads=122','engine_reads=104'),('371622022','371622023'),('50724883','50724882')]:
   with self.subTest(old=old),self.assertRaises(ValueError):requester_result(t.replace(old,new))
  with self.assertRaises(ValueError):requester_result(t+'intel-requester-unknown: v=1\n')
  a,b=rows[:2]
  with self.assertRaises(ValueError):requester_result(t.replace(a,'TEMP').replace(b,a).replace('TEMP',b))
 def test_native_contract_and_checkpoint(self):
  cache=output(ROOT/'build/tests/intel-cache-probe','fresh-uc');handoff=output(ROOT/'build/tests/intel-handoff-probe','0')
  for mode in ('report','req-pending','req-pending-transition'):
   a=adapter(mode).replace('intel-wake: bounded','intel-wake: preflight checkpoint saved\nintel-wake: bounded',1)
   text='bootloader: Limine 11.4.1\n'+cache+'intel-boot-claim: v=1 policy=2 client=2 immutable=1 hardware_ownership=0 managed_pages=0\n'+handoff+a
   self.assertTrue(native_requester_result(text)['native_complete'])
   for old,new in [('Limine 11.4.1','Limine 11.4.2'),('intel-wake: preflight checkpoint saved',''),('cache_checks=10','cache_checks=9'),('maps=10','maps=9')]:
    with self.subTest(old=old),self.assertRaises(ValueError):native_requester_result(text.replace(old,new))
 def test_recipe_and_noncomposition(self):
  from hardware_boot import limine_config,stage
  cfg=limine_config('0123456789abcdef','ram-usb-v1-intel-requester-laptop-probe-37d8c5d0-auto-return')
  tokens=next(l.split('cmdline:',1)[1].split() for l in cfg.splitlines() if 'cmdline:' in l);self.assertEqual(len(tokens),16)
  self.assertIn('srvros.intel-requester=read-only',tokens)
  cli=[str(ROOT/'build/tests/intel-gt-boot'),'requester'];subprocess.run(cli+[' '.join(tokens)],check=True)
  for n in range(len(tokens)):
   self.assertEqual(subprocess.run(cli+[' '.join(tokens[:n]+tokens[n+1:])]).returncode,1)
   self.assertEqual(subprocess.run(cli+[' '.join(tokens+[tokens[n]])]).returncode,1)
  for old in ('takeover','engine-prepare','bcs-shutdown'):self.assertEqual(subprocess.run([cli[0],old,' '.join(tokens)]).returncode,1)
  for extra in ({'intel_takeover_preflight':True},{'intel_engine_prepare':True},{'intel_bcs_reset_trial':True},{'hold_on_failure':True},{'desktop_audio':True}):
   with self.subTest(extra=extra),self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_requester=True,**extra)
 def test_attention(self):
  from hardware_desktop import needs_attention
  line=next(l for l in adapter().splitlines() if l.startswith('intel-requester-summary:'))
  self.assertFalse(needs_attention(line));self.assertTrue(needs_attention(line+' extra=0'))
if __name__=='__main__':unittest.main()
