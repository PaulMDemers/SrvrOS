from pathlib import Path
import re,subprocess,unittest
from intel_hda_owner import owner_result,owner_summary,native_owner_result,verify_owner_checkpoint
ROOT=Path(__file__).resolve().parents[1]
CORE=('normal','facts','zero','frozen','backward','guard','late-guard','inspect-fail','inspect-slow','active','late-active','buffer','corb','intctl','reset','topology','changed','self-copy','map-retained','resource-change','power-change','unissued','uncertain','restore-fail','write-slow','readback')
ADAPTER=('normal','active','changed','max','loader','efi','recipe','irq','id','revision','subsystem','bar','high','d3','decode','cache','late-cache','power-change','map','retire','over','float','read','slow','ram','cr3','alias','stolen','late-active','late-retire','store-none','store-uncertain','readback','restore-none','write-slow','store-cache','store-power','store-cr3','store-alias','store-parent','restore-readback','final-active')
def adapter(mode):return subprocess.check_output([str(ROOT/'build/tests/intel-hda-owner-adapter'),mode],text=True)
class OwnerTests(unittest.TestCase):
 def test_successful_phase_reasons_do_not_warn_but_failures_still_do(self):
  from hardware_desktop import needs_attention
  text=adapter('normal')
  rows=[l for l in text.splitlines() if l.startswith('intel-hda-owner-phase:')]
  self.assertEqual(len(rows),3)
  for line in rows:
   self.assertFalse(needs_attention(line))
   for old,new in [('reason=0','reason=1'),('complete=1','complete=0'),('retained=0','retained=1'),('reads=58','reads=57'),('stable=1','stable=0'),('run=0','run=1'),('dma=0','dma=1'),('samples=2','samples=1'),('retired=1','retired=0'),('bar=0xc1610004','bar=0xc1620004'),('state=1','state=0')]:
    with self.subTest(field=old):self.assertTrue(needs_attention(line.replace(old,new)))
   for bad in (line+' reason=0',line+' error=1',line.replace('v=1','v=2')):self.assertTrue(needs_attention(bad))
  # A quiet header alone cannot suppress invalid raw controller/stream proof.
  for old,new in [('g2=0','g2=0x1'),('w3=0','w3=0x1000')]:
   bad=text.replace(old,new);self.assertNotEqual(text,bad)
   with self.assertRaises(ValueError):owner_result(bad)
 def test_core(self):
  for m in CORE:
   with self.subTest(mode=m):subprocess.check_call([str(ROOT/'build/tests/intel-hda-owner-core'),m],stdout=subprocess.DEVNULL)
 def test_actual_adapter_faults(self):
  for m in ADAPTER:
   with self.subTest(mode=m):
    r=owner_result(adapter(m));self.assertEqual(r['complete'],m=='normal');self.assertFalse(r['dma_retirement'] or r['firmware_retirement'] or r['managed_range'])
    if m=='normal':self.assertEqual([p['command'] for p in r['phases']],[6,2,6]);self.assertEqual(sum(p['reads'] for p in r['phases']),174)
 def test_strict_receipts(self):
  text=adapter('normal')
  for old,new in [('policy=1','policy=2'),('attempts=2','attempts=1'),('writes=2','writes=1'),('mmio_writes=0','mmio_writes=1'),('status_writes=0','status_writes=1'),('dma_retirement=0','dma_retirement=1'),('retained=0','retained=1'),('recipe=1','recipe=0'),('inhibited=1','inhibited=0'),('restored=1','restored=0'),('g0=0x3001','g0=0x3101'),('w0=0x40000','w0=0x40002'),('bar=0xc1610004','bar=0xc1620004'),('command=0x2','command=0x6'),('reads=58','reads=57'),('cache_checks=20','cache_checks=19'),('guards=202','guards=201')]:
   with self.subTest(field=old),self.assertRaises(ValueError):owner_result(text.replace(old,new))
  for bad in (text+text,text+'intel-hda-owner-mystery: v=1\n',re.sub('^intel-hda-owner-stream:.*\n','',text,count=1,flags=re.M)):
   with self.assertRaises(ValueError):owner_result(bad)
  lines=text.splitlines();a=next(i for i,l in enumerate(lines) if l.startswith('intel-hda-owner-stream:'));lines[a],lines[a+1]=lines[a+1],lines[a]
  with self.assertRaises(ValueError):owner_result('\n'.join(lines))
 def test_native_and_saved_checkpoint(self):
  cache=subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe'),'fresh-uc'],text=True)
  handoff=subprocess.check_output([str(ROOT/'build/tests/intel-handoff-probe'),'0'],text=True)
  t='bootloader: Limine 11.4.1\n'+cache+'intel-boot-claim: v=1 policy=2 client=2 immutable=1 hardware_ownership=0 managed_pages=0\n'+handoff+adapter('normal')
  self.assertTrue(native_owner_result(t)['native_trial_complete'])
  pre=t[:t.index('intel-hda-owner: preflight checkpoint saved')]+'workspace-log: snapshot=session-a.log\n'
  self.assertTrue(verify_owner_checkpoint(pre,t))
  for old,new in [('preflight checkpoint saved','checkpoint failed'),('Limine 11.4.1','Limine 11.4.2'),('mask=0x0000007ff0000800','mask=0')]:
   bad=t.replace(old,new);self.assertNotEqual(t,bad)
   with self.assertRaises(ValueError):native_owner_result(bad)
  with self.assertRaises(ValueError):verify_owner_checkpoint(t,t)
 def test_stage_recipe_and_attention(self):
  from hardware_boot import limine_config,stage
  from hardware_desktop import needs_attention
  text=limine_config('0123456789abcdef','ram-usb-v1-intel-hda-owner-laptop-probe-37d8c5d0-auto-return')
  cmd=next(l for l in text.splitlines() if 'cmdline:' in l).split('cmdline:')[1]
  self.assertEqual(len(cmd.split()),13);self.assertIn('srvros.intel-hda=owner-trial',cmd);self.assertIn('srvros.intel-hda-policy=cooperative-efi-v1',cmd)
  for banned in ('srvros.audio=hda','srvros.intel-wake=trial','srvros.intel-blitter=retire','srvros.intel-hda=read-only'):self.assertNotIn(banned,cmd)
  for kw in ({'intel_hda_capture':True},{'intel_blitter_trial':True},{'desktop_audio':True},{'haswell_ecam_boot':True},{'hold_on_failure':True}):
   with self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_hda_owner_trial=True,**kw)
  line=next(l for l in adapter('normal').splitlines() if l.startswith('intel-hda-owner-summary:'));self.assertFalse(needs_attention(line))
  for old,new in [('complete=1','complete=0'),('reason=0','reason=1'),('writes=2','writes=3')]:self.assertTrue(needs_attention(line.replace(old,new)))
if __name__=='__main__':unittest.main()
