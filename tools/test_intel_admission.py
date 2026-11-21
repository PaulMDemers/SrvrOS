from pathlib import Path
import subprocess,unittest
from intel_admission import admission_result
from intel_engine_held_reset import held_reset_result

def line(**kw):
    s=dict(v=1,queries=0,code=0,detail=0,pages=0,phase=0,reason=0,level=0,checked=0,value=0);s.update(kw)
    return 'intel-admission: '+' '.join(f'{k}={v}' for k,v in s.items())+'\n'

class AdmissionTests(unittest.TestCase):
    def test_actual_native_adapter_first_failure_is_sticky(self):
        text=subprocess.check_output(['build/tests/intel-held-reset-adapter','--admission-witness'],text=True)
        s=admission_result(text)['summary']
        self.assertEqual(s,dict(v=1,queries=2,code=7,detail=0,pages=1350,phase=1,reason=7,level=0,checked=0,value=3))

    def test_success_is_not_authority_and_old_logs_remain_optional(self):
        text=subprocess.check_output(['build/tests/intel-held-reset-adapter','--wake-engine-report'],text=True)
        r=held_reset_result(text);self.assertTrue(r['complete']);a=r['admission'];self.assertGreater(a['summary']['queries'],0)
        self.assertFalse(any(a[k] for k in ('hardware_ownership','managed_range','physical_tearfree')))
        old='\n'.join(l for l in text.splitlines() if not l.startswith('intel-admission:'))
        self.assertIsNone(held_reset_result(old)['admission'])
        with self.assertRaises(ValueError):held_reset_result(text.replace('code=0 detail=0 pages=0 phase=0 reason=0 level=0 checked=0 value=0','code=7 detail=0 pages=1350 phase=1 reason=7 level=0 checked=0 value=3'))

    def test_all_root_and_walk_reasons(self):
        for phase in (1,3):
            for reason in range(1,9):
                value=512 if reason==1 else 3 if reason==7 else 8 if reason==8 else 0
                self.assertEqual(admission_result(line(queries=1,code=7,pages=1350,phase=phase,reason=reason,checked=0 if phase==1 else 1350,value=value))['summary']['reason'],reason)
        for reason,levels in {10:[1],11:[1],12:[2,3,4],13:[2],14:[3],15:[4],16:[4],17:[0]}.items():
            for level in levels:self.assertIsNotNone(admission_result(line(queries=1,code=7,pages=1350,phase=2,reason=reason,level=level,checked=1349)))
        for value in range(1,8):self.assertIsNotNone(admission_result(line(queries=1,code=7,pages=1350,reason=9,value=value)))

    def test_malformed_or_unbound_diagnostics_refuse(self):
        good=dict(queries=1,code=7,pages=1350,phase=1,reason=7,value=3)
        for changes in ({'v':2},{'queries':0},{'queries':2000001},{'code':8},{'detail':1},{'pages':1537},{'phase':4},{'reason':0},{'reason':18},{'level':1},{'checked':1},{'value':0},{'value':65}):
            with self.subTest(changes=changes),self.assertRaises(ValueError):admission_result(line(**(good|changes)))
        for text in (line()+line(),line().replace('v=1','v=01'),line()+'intel-admission-unknown: v=1\n',line().replace(' queries=0',''),line().replace('value=0','value=18446744073709551616'),line(code=1,reason=7),line(code=304,detail=32),line(queries=1,code=7,pages=1350,phase=3,reason=7,checked=1349,value=1),line(queries=1,code=7,pages=1350,phase=2,reason=15,level=4,checked=1350)):
            with self.subTest(text=text),self.assertRaises(ValueError):admission_result(text)
