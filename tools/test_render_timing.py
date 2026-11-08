import unittest
import subprocess,os
from pathlib import Path
from render_timing import RENDER_FIELDS,CAPTURE_FIELDS,validate_render_timing
from loop_timing import SCHED_FIELDS
from test_loop_timing import receipt


def line(prefix, fields, row):
    return prefix+b' '.join(f'{k}={row[k]}'.encode() for k in fields)+b'\n'


def fixture(final=1, **changes):
    loop=receipt(render_us=540000,post_us=10000,final=final)
    r=dict(zip(RENDER_FIELDS,(2,1,1,1101,1200,540000,540100,541000,1,1,2,1,530000,7000,1,1,100,100,final)))
    c=dict(zip(CAPTURE_FIELDS,(2,1,1,541101,551000,2,2,1,0,0,0,final)))
    rs=dict.fromkeys(SCHED_FIELDS,0);rs.update(seq=2,version=1,start_us=1110,end_us=540990,end_tick=60,
        blocked_us=2000,ready_us=1000,blocks=1,signals=1,dispatches=1,max_ready_us=1000,wake_us=2000,run_us=3000,wake_tick=1,reason=1,final=final)
    cs=dict(rs);cs.update(start_us=541110,end_us=550990,blocked_us=0,blocks=0,signals=0,wake_us=542000,run_us=543000)
    for key,value in changes.items():
        target,field=key.split('__');{'r':r,'c':c,'rs':rs,'cs':cs}[target][field]=value
    return loop+line(b'desktop-loop-render: ',RENDER_FIELDS,r)+line(b'desktop-loop-render-sched: ',SCHED_FIELDS,rs)+line(b'desktop-loop-capture: ',CAPTURE_FIELDS,c)+line(b'desktop-loop-capture-sched: ',SCHED_FIELDS,cs)


class RenderTiming(unittest.TestCase):
    def test_actual_c_writer(self):
        exe=Path(__file__).resolve().parents[1]/('build/tests/desktop-session-host'+('.exe' if os.name=='nt' else ''))
        raw=subprocess.check_output([str(exe),'--render-proof'])
        d=validate_render_timing([raw],559000)
        self.assertEqual(d['render']['compose_us'],530000)
        self.assertEqual(d['render_scheduler']['ready_us'],1000)
        self.assertEqual(d['capture']['id_after'],2)

    def test_retained_binding_and_subphase_conservation(self):
        d=validate_render_timing([fixture(0),fixture(),fixture()])
        self.assertEqual(d['sequence'],2)
        self.assertEqual(sum(d['phases'].values()),541000-1101)
        self.assertEqual(d['render']['compose_us'],530000)
        self.assertEqual(d['capture_scheduler']['ready_us'],1000)
        self.assertEqual(validate_render_timing([fixture(0)],require_final=False)['render']['final'],0)

    def test_missing_truncated_conflicting_and_legacy_refused(self):
        raw=fixture()
        for bad in (receipt(),raw[:-2],fixture(0)):
            with self.assertRaises(AssertionError):validate_render_timing([bad])
        for prefix in (b'desktop-loop-render:',b'desktop-loop-render-sched:',b'desktop-loop-capture:',b'desktop-loop-capture-sched:'):
            rows=raw.splitlines(keepends=True);target=next(q for q in rows if q.startswith(prefix))
            with self.assertRaises(AssertionError):validate_render_timing([raw.replace(target,b'')])
            with self.assertRaises(AssertionError):validate_render_timing([raw+target.replace(b'seq=2',b'seq=2 extra=1')])
        with self.assertRaises(AssertionError):validate_render_timing([fixture(0),fixture(r__compose_us=529999)])

    def test_geometry_clock_counts_and_scheduler_faults_refused(self):
        cases=dict(r__seq=3,r__version=2,r__valid=0,r__begin_us=1,r__end_us=541101,
            r__present_begin_us=540001,r__damage=17,r__scale=3,r__capture_busy=2,
            r__compose_us=540000,r__upload_calls=0,r__compose_calls=2,
            c__begin_us=541000,c__end_us=551101,c__id_after=4,c__busy_before=0,c__pending_after=2,
            rs__invalid=1,rs__start_us=1100,rs__end_us=541001,rs__blocked_us=540000,
            rs__signals=2,rs__dispatches=0,rs__run_us=3001,rs__reason=3,
            cs__seq=3,cs__wake_us=540000,cs__version=0,r__compose_pixels=1<<64)
        for k,v in cases.items():
            with self.subTest(k=k),self.assertRaises(AssertionError):validate_render_timing([fixture(**{k:v})])

    def test_no_reached_stage_has_explicit_zero_companions(self):
        raw=receipt(render_us=0,post_us=0,control_us=551000)
        r=dict.fromkeys(RENDER_FIELDS,0);r.update(seq=2,version=1,final=1)
        c=dict.fromkeys(CAPTURE_FIELDS,0);c.update(seq=2,version=1,final=1)
        q=dict.fromkeys(SCHED_FIELDS,0);q.update(seq=2,final=1)
        raw+=line(b'desktop-loop-render: ',RENDER_FIELDS,r)+line(b'desktop-loop-render-sched: ',SCHED_FIELDS,q)
        raw+=line(b'desktop-loop-capture: ',CAPTURE_FIELDS,c)+line(b'desktop-loop-capture-sched: ',SCHED_FIELDS,q)
        self.assertEqual(validate_render_timing([raw])['phases'],{})


if __name__=='__main__':unittest.main()
