from pathlib import Path
import subprocess,unittest,re
from intel_display_hda import hda_result,native_hda_result,summary_result
ROOT=Path(__file__).resolve().parents[1]
CORE=('normal','active','changed','max','admission','d3','bar','id','decode','reserved','cache','late-cache','power-change','map','retire','over','float','read','zero','frozen','backward','deadline')
ADAPTER=('normal','active','changed','max','loader','efi','recipe','irq','id','revision','subsystem','bar','high','d3','decode','cache','late-cache','power-change','map','retire','over','float','read','slow','ram','cr3','alias','stolen')
def adapter(mode):return subprocess.check_output([str(ROOT/'build/tests/intel-hda-adapter'),mode],text=True)
class HdaTests(unittest.TestCase):
 def test_core_protocol(self):
  for mode in CORE:
   with self.subTest(mode=mode):subprocess.check_call([str(ROOT/'build/tests/intel-hda-core'),mode],stdout=subprocess.DEVNULL)
 def test_adapter_observes_active_and_changing_without_claiming_retirement(self):
  for mode in ADAPTER:
   with self.subTest(mode=mode):
    t=adapter(mode);r=hda_result(t);good=mode in ('normal','active','changed','max')
    self.assertEqual(r['complete'],good);self.assertFalse(r['dma_retirement'] or r['reader_retirement'] or r['managed_range'])
    self.assertEqual(bool(r['summary']['retained']),mode in ('map','retire','cr3','stolen'))
    if good:self.assertTrue(r['bus_master_enabled']);self.assertEqual(r['stable'],mode!='changed')
    if mode=='active':self.assertEqual((r['run_mask'],r['command_dma_mask']),(1,7))
    if mode=='max':self.assertEqual((r['summary']['streams'],r['summary']['reads']),(8,118))
 def test_strict_schema_derived_flags_and_widths(self):
  t=adapter('normal')
  for old,new in [('dma_retirement=0','dma_retirement=1'),('writes=0','writes=1'),('reserved=1','reserved=0'),('bar=0xc1610004','bar=0xc1620004'),('pm=0','pm=0x3'),('reads=46','reads=45'),('run=0','run=1'),('dma=0','dma=1'),('stable=1','stable=0'),('retained=0','retained=1'),('serialized=1','serialized=0'),('gcap=0x1101','gcap=0x9001'),('control=0','control=0xff0000'),('lvi=0','lvi=0x10000'),('v=1','v=2')]:
   with self.subTest(old=old),self.assertRaises(ValueError):hda_result(t.replace(old,new))
  for bad in (t+t,t+'intel-hda-mystery: v=1\n',re.sub(r'^intel-hda-stream:.*\n','',t,count=1,flags=re.M)):
   with self.assertRaises(ValueError):hda_result(bad)
 def test_native_chain_and_checkpoint(self):
  cache=subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe'),'fresh-uc'],text=True)
  handoff=subprocess.check_output([str(ROOT/'build/tests/intel-handoff-probe'),'0'],text=True)
  t='bootloader: Limine 11.4.1\n'+cache+'intel-boot-claim: v=1 policy=2 client=2 immutable=1 hardware_ownership=0 managed_pages=0\n'+handoff+adapter('normal')
  self.assertTrue(native_hda_result(t)['native_observation_complete'])
  from intel_display_hda import verify_hda_checkpoint
  pre=t[:t.index('intel-hda: preflight checkpoint saved')]+'workspace-log: snapshot=session-a.log\n'
  self.assertTrue(verify_hda_checkpoint(pre,t))
  for bad in (t.replace('preflight checkpoint saved','checkpoint missing'),t.replace('Limine 11.4.1','Limine 11.4.2'),t.replace('mask=0x0000007ff0000800','mask=0')):
   self.assertNotEqual(t,bad)
   with self.assertRaises(ValueError):native_hda_result(bad)
  with self.assertRaises(ValueError):verify_hda_checkpoint(t, t)
 def test_cache_qualification(self):
  for mode in ('hda-uc','hda-pat-only'):subprocess.check_call([str(ROOT/'build/tests/intel-cache-probe'),mode],stdout=subprocess.DEVNULL)
 def test_attention_and_strict_recipe(self):
  from hardware_desktop import needs_attention
  line=next(l for l in adapter('normal').splitlines() if l.startswith('intel-hda-summary:'))
  self.assertFalse(needs_attention(line))
  for bad in (line+' mystery=0',line.replace('reason=0','reason=1'),line.replace('complete=1','complete=0')):self.assertTrue(needs_attention(bad))
  from hardware_boot import stage,limine_config
  config=limine_config('0123456789abcdef','ram-usb-v1-intel-hda-laptop-probe-37d8c5d0-auto-return');cmd=next(l for l in config.splitlines() if 'cmdline:' in l)
  self.assertEqual(len(cmd.split('cmdline:')[1].split()),12);self.assertIn('srvros.intel-hda=read-only',cmd)
  for banned in ('srvros.audio=hda','srvros.intel-wake=trial','srvros.intel-blitter=retire','srvros.intel-engines=read-only'):self.assertNotIn(banned,cmd)
  for kw in ({'intel_blitter_trial':True},{'intel_wake_trial':True},{'desktop_audio':True},{'haswell_ecam_boot':True},{'hold_on_failure':True}):
   with self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_hda_capture=True,**kw)
if __name__=='__main__':unittest.main()
