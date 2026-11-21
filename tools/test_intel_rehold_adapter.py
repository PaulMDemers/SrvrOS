"""Actual native provider + strict raw collector; modeled MMIO, not hardware."""
from pathlib import Path
import subprocess,re,unittest
from intel_engine_rehold import rehold_capture,rehold_result,native_rehold_result
from intel_display_backing import backing_result
from hardware_boot import stage,limine_config
ROOT=Path(__file__).resolve().parents[1];EXE=ROOT/'build/tests/intel-rehold-adapter'
def run(mode):
    p=subprocess.run([str(EXE),'--wake-engine-'+mode],capture_output=True,text=True,timeout=15)
    if p.returncode:raise AssertionError((mode,p.stdout[-3000:],p.stderr))
    return p.stdout
def field(text,key,value):
    return re.sub(r'(^intel-rehold-summary:.*?\b'+key+r'=)[^\s]+',lambda m:m[1]+str(value),text,count=1,flags=re.M)
class ReholdAdapterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.text=run('report')
    def test_complete_native_adapter_and_native_gate(self):
        for mode in ('report','hreset-delay'):
            r=rehold_result(run(mode));s=r['summary'];self.assertTrue(r['complete']);self.assertFalse(r['native_complete'])
            self.assertEqual((s['writes'],s['live_checks'],s['live_reads'],s['child_borrows'],s['child_returns']),(2,19,95,1,1))
            self.assertFalse(r['hardware_ownership']);self.assertEqual(r['managed_pages'],0)
        with self.assertRaises(ValueError):native_rehold_result(self.text)
    def test_all_actual_native_load_boundaries(self):
        for mode in ('rehold-read','rehold-value','rehold-value-foreign','rehold-foreign','rehold-map','rehold-time'):
            for point in range(1,120):
                with self.subTest(mode=mode,point=point):
                    text=run(f'{mode}:{point}');raw=rehold_capture(text);self.assertFalse(raw['complete'])
                    try:r=rehold_result(text)
                    except ValueError:continue
                    self.assertFalse(r['complete'])
    def test_actual_store_and_last_admission_boundaries(self):
        for name in ('rehold-none','rehold-uncertain','rehold-ack-float','rehold-store-foreign','rehold-store-time','rehold-admission-time','rehold-admission-zero','rehold-admission-backward','rehold-admission-owner'):
            for point in (1,2):
                with self.subTest(name=name,point=point):
                    text=run(f'{name}:{point}');raw=rehold_capture(text);s=raw['summary'];self.assertFalse(raw['complete'])
                    if name.startswith('rehold-admission-'):
                        self.assertEqual((s['attempts'],s['writes']),(point,point-1))
                    try:r=rehold_result(text)
                    except ValueError:continue
                    self.assertFalse(r['complete'])
    def test_child_failure_prevents_cancel(self):
        for kind in ('interval-read','interval-drift','interval-foreign','interval-map','interval-time'):
            for point in range(1,7):
                with self.subTest(kind=kind,point=point):
                    s=rehold_capture(run(f'{kind}:{point}'))['summary']
                    self.assertEqual((s['attempts'],s['writes'],s['child_borrows'],s['child_returns'],s['child_retained']),(1,1,1,0,1))
                    self.assertFalse(s['restored']);self.assertFalse(s['complete'])
    def test_lossless_all_pte_words_and_old_image_binding(self):
        old=subprocess.check_output([str(ROOT/'build/tests/intel-held-interval-adapter'),'--wake-engine-report'],text=True)
        a=backing_result(old);b=backing_result(self.text)
        self.assertEqual(a['ptes'],b['ptes']);self.assertEqual(len(b['ptes']),1356)
        self.assertEqual({x['v'] for x in a['rows']['pte']},{1});self.assertEqual({x['v'] for x in b['rows']['pte']},{2})
        self.assertEqual([x['count'] for x in b['rows']['pte']],[64]*21+[12])
        self.assertLess(len(b['rows']['pte']),len(a['rows']['pte']))
        with self.assertRaises(ValueError):backing_result(self.text.replace('intel-wake-summary: v=19 ','intel-wake-summary: v=18 '))
        row=next(l for l in self.text.splitlines() if l.startswith('intel-backing-pte:'))
        for bad in (self.text.replace(row+'\n',''),self.text.replace(row,row+'\n'+row),self.text.replace(row,row.replace('v=2','v=1')),self.text.replace(row,row[:-1])):
            with self.assertRaises(ValueError):backing_result(bad)
    def test_strict_receipt_mutations(self):
        changes=[('epoch',2),('origin',1),('owner_origin',0),('attempts',1),('writes',1),('reads',25),('child_borrows',0),('child_returns',0),('child_retained',1),('live_checks',18),('live_reads',94),('live_failed',1),('restored',0),('uncertain',1),('obligations',0),('assurances',31),('pte_writes',1),('managed_pages',1),('hardware_ownership',1),('complete','01'),('clock_at',1)]
        bad=[field(self.text,k,v) for k,v in changes]
        rows=[l for l in self.text.splitlines() if l.startswith('intel-rehold-row:')]
        bad += [self.text.replace(rows[0]+'\n',''),self.text.replace(rows[0],rows[0]+'\n'+rows[0]),self.text.replace(rows[1],rows[1][:-48]+'00000000'*6),self.text.replace('intel-wake-summary: v=19 ','intel-wake-summary: v=18 ')]
        for text in bad:
            with self.subTest(text=text[-200:]),self.assertRaises(ValueError):rehold_result(text)
    def test_exact20_token_boot_recipe_and_exclusivity(self):
        config=limine_config('0123456789abcdef','ram-usb-v1-intel-rehold-laptop-probe-37d8c5d0-auto-return')
        recipe=next(l.split('cmdline: ',1)[1] for l in config.splitlines() if 'cmdline: ' in l)
        self.assertEqual(len(recipe.split()),20);self.assertEqual(len(set(recipe.split())),20)
        self.assertEqual(subprocess.run(['build/tests/intel-gt-boot','rehold',recipe],capture_output=True).returncode,0)
        for old in ('held-interval','held-reset'):
            self.assertNotEqual(subprocess.run(['build/tests/intel-gt-boot',old,recipe],capture_output=True).returncode,0)
        for extra in ({'intel_held_interval':True},{'intel_held_reset':True},{'intel_held_requester':True},{'intel_memory_capture':True},{'repeat':2},{'hold_on_failure':True},{'desktop_volume':'37d8c5d0'},{'aml_capture':True},{'desktop_scale':2}):
            with self.subTest(extra=extra),self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_rehold=True,**extra)
        with self.assertRaises(ValueError):stage(intel_rehold=True)
if __name__=='__main__':unittest.main()
