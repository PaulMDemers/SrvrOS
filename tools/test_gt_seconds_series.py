"""Bounded three-boot state machine with no real reboot or service effects."""
import copy,json,tempfile,unittest
from pathlib import Path
from gt_seconds_series import advance,next_action,save_plan

class Fake:
    def __init__(self):self.states={'a-r1':dict(phase='QUALIFIED',intent=None)};self.reports={};self.bootid='one';self.actions=[];self.disabled=False;self.fail=None;self.changed=False
    def integrity(self,p):
        if self.changed:raise ValueError('source changed')
    def read(self,n):return self.states.get(n),self.reports.get(n)
    def boot_id(self):return self.bootid
    def effect(self,a,n):
        self.actions.append((a,n))
        if self.fail==a:raise ValueError('injected '+a)
    def create(self,n):self.effect('create',n);self.states[n]=dict(phase='NEW',intent=None)
    def qualify(self,n):self.effect('qualify',n);self.states[n]=dict(phase='QUALIFIED',intent=None)
    def boot(self,n):self.effect('boot',n);self.states[n]=dict(phase='BOOT_REQUESTED',intent=None,host_boot_id=self.bootid)
    def collect(self,n):self.effect('collect',n);self.states[n]=dict(phase='COLLECTED',intent=None);self.reports[n]=dict(diagnostic_complete=True,offline_integrity='PASS',native_acceptance=False)
    def diagnose(self,n):self.effect('diagnose',n);self.states[n]=dict(phase='DIAGNOSED',intent=None)
    def bank(self,n):self.effect('bank',n);return dict(run=n,source='source',native_acceptance=False,original_timing_acceptance=False,return_integrity='PASS')
    def disable(self):self.disabled=True

class Series(unittest.TestCase):
    def setUp(self):
        self.plan=dict(status='RUNNING',runs=['a-r1','a-r2','a-r3'],index=0,results=[],intent=None)
        self.ops=Fake();self.saved=[]
    def run_step(self):advance(self.plan,self.ops,lambda p:self.saved.append(copy.deepcopy(p)))
    def test_three_complete_returns_then_disable_without_fourth_boot(self):
        self.run_step();self.assertEqual(self.ops.actions,[('boot','a-r1')])
        for i in range(3):
            self.ops.bootid=str(i);self.run_step()
        self.assertEqual(self.plan['status'],'COMPLETE');self.assertEqual(len(self.plan['results']),3);self.assertTrue(self.ops.disabled)
        self.assertEqual([n for a,n in self.ops.actions if a=='boot'],self.plan['runs'])
        actions=list(self.ops.actions);self.run_step();self.assertEqual(self.ops.actions,actions)
    def test_same_boot_never_requests_reboot_twice(self):
        self.run_step();self.run_step();self.assertEqual(self.ops.actions,[('boot','a-r1')])
    def test_boot_interruption_is_collected_after_new_host_boot(self):
        self.run_step();self.plan['intent']='BOOT';self.ops.bootid='new';self.run_step()
        self.assertIn(('collect','a-r1'),self.ops.actions);self.assertEqual([n for a,n in self.ops.actions if a=='boot'],['a-r1','a-r2'])
    def test_incomplete_failure_or_dirty_media_never_advances(self):
        for report in (None,dict(diagnostic_complete=False,offline_integrity='PASS'),dict(diagnostic_complete=True,offline_integrity='FAIL'),dict(diagnostic_complete=True,offline_integrity='PASS',native_acceptance=True)):
            self.setUp();self.ops.states['a-r1']=dict(phase='COLLECTED',intent=None);self.ops.reports['a-r1']=report
            self.run_step();self.assertEqual(self.plan['status'],'STOPPED');self.assertEqual(self.ops.actions,[])
    def test_every_effect_failure_stops_series(self):
        for effect in ('boot','collect','diagnose','bank','create','qualify'):
            self.setUp();self.ops.fail=effect
            try:
                self.run_step();self.ops.bootid='next';self.run_step()
            except ValueError:pass
            self.assertEqual(self.plan['status'],'STOPPED',effect);self.assertTrue(self.ops.disabled)
            self.assertLessEqual(sum(a=='boot' for a,n in self.ops.actions),1)
    def test_unsafe_intermediate_intent_and_source_changes_stop(self):
        for state in (dict(phase='ARMED',intent=None),dict(phase='QUALIFIED',intent='reboot'),dict(phase='NEW',intent='validate'),dict(phase='QUALIFIED',intent=None,status='FAILED')):
            self.setUp();self.ops.states['a-r1']=state;self.run_step();self.assertEqual(self.plan['status'],'STOPPED');self.assertFalse(self.ops.actions)
        self.setUp();self.ops.changed=True
        with self.assertRaises(ValueError):self.run_step()
        self.assertEqual(self.plan['status'],'STOPPED');self.assertFalse(self.ops.actions)
    def test_explicit_stop_cannot_be_overwritten_by_running_worker(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'plan.json';p.write_text(json.dumps(dict(status='STOPPED')))
            with self.assertRaises(ValueError):save_plan(p,dict(status='RUNNING'))
            self.assertEqual(json.loads(p.read_text())['status'],'STOPPED')

if __name__=='__main__':unittest.main()
