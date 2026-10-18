import binascii
import unittest
from desktop_io_result import validate, validate_timing, timing_gate, validate_text, text_gate, validate_timeline, BYTES

class DesktopIOTests(unittest.TestCase):
    run_id='0123456789abcdef'
    def record(self,kind,fields):
        data=f'format=srvros-desktop-{kind}-v1\nrun={self.run_id}\n'.encode()
        data+=''.join(f'{k}={v}\n' for k,v in fields.items()).encode()
        return data+f'crc32={binascii.crc32(data)&0xffffffff}\n'.encode()
    def test_durable_payload_and_coverage(self):
        worker=self.record('io',dict(state=3,rounds=16,bytes=16*BYTES))
        activity=dict(state=3,rounds=16,keys=8,motions=1,wheels=1,focus_changes=2,cancels=1)
        ui=self.record('ui',activity)
        payload=bytes((i*17+16*29+ord(self.run_id[i%16]))&255 for i in range(BYTES))
        self.assertEqual(validate(self.run_id,worker,ui,payload)['status'],'EXERCISED')
        for key in ('keys','motions','wheels','focus_changes','cancels'):
            self.assertEqual(validate(self.run_id,worker,self.record('ui',dict(activity,**{key:0})),payload)['status'],'INCOMPLETE')
        with self.assertRaises(ValueError): validate(self.run_id,worker,ui,payload[:-1]+b'\0')
        with self.assertRaises(ValueError): validate(self.run_id,worker[:-1],ui,payload)
        with self.assertRaises(ValueError): validate(self.run_id,worker,self.record('ui',dict(activity,state=4)),payload)

    def test_text_evidence(self):
        target=dict(clock=1,sent=32,presented=32,overflow=0,p95_us=50000,p99_us=100000,max_us=250000)
        self.assertEqual(validate_text(self.run_id,self.record('text',target)),target)
        self.assertTrue(text_gate(target))
        for miss in (dict(sent=31,presented=31),dict(presented=31),dict(overflow=1),
                     dict(p95_us=50001),dict(p99_us=100001),dict(max_us=250001)):
            self.assertFalse(text_gate(dict(target,**miss)))
        for invalid in (dict(clock=0),dict(presented=33),dict(overflow=33),dict(p99_us=1)):
            with self.assertRaises(ValueError): validate_text(self.run_id,self.record('text',dict(target,**invalid)))
        with self.assertRaises(ValueError): validate_text(self.run_id,self.record('text',target)[:-1])

    def test_timeline(self):
        work=dict(start_us=200,end_us=800)
        activity=dict(clock=1,start_request_us=100,first_key_us=300,first_pointer_us=900,
                      last_input_us=950,stop_request_us=0,recorded_us=1000)
        def check(w=work,a=activity):
            return validate_timeline(self.run_id,self.record('workclock',w),self.record('inputclock',a))
        value=check()
        self.assertEqual(value['workload_seconds'],0.0006)
        self.assertEqual(value['first_key_phase'],'during')
        self.assertEqual(value['first_pointer_phase'],'after')
        self.assertEqual(check(a=dict(activity,first_key_us=150))['first_key_phase'],'before')
        self.assertEqual(check(a=dict(activity,first_key_us=0,first_pointer_us=0,last_input_us=0))['first_key_phase'],'none')
        for invalid in (dict(clock=0),dict(start_request_us=201),dict(first_key_us=99),
                        dict(last_input_us=400),dict(stop_request_us=1001),dict(recorded_us=799)):
            with self.assertRaises(ValueError): check(a=dict(activity,**invalid))
        with self.assertRaises(ValueError): check(w=dict(work,end_us=199))
        with self.assertRaises(ValueError): validate_timeline(self.run_id,self.record('workclock',work)[:-1],self.record('inputclock',activity))

    def test_timing_evidence(self):
        fields=dict(clock=1,active_count=51,active_p95_us=50000,active_p99_us=100000,active_max_us=150000,
                    render_count=25,render_p95_us=8000,render_p99_us=10000,render_max_us=10000)
        self.assertEqual(validate_timing(self.run_id,self.record('timing',fields)),fields)
        target=dict(fields,active_count=100,active_max_us=250000)
        self.assertTrue(timing_gate(target))
        for miss in (dict(active_count=99),dict(active_p95_us=50001),dict(active_p99_us=100001),dict(active_max_us=250001),dict(render_p95_us=16668)):
            self.assertFalse(timing_gate(dict(target,**miss)))
        for invalid in (dict(clock=0),dict(active_count=0),dict(active_p95_us=200000),dict(render_max_us=1)):
            with self.assertRaises(ValueError): validate_timing(self.run_id,self.record('timing',dict(fields,**invalid)))
        empty=dict(clock=1,active_count=0,active_p95_us=0,active_p99_us=0,active_max_us=0,
                   render_count=0,render_p95_us=0,render_p99_us=0,render_max_us=0)
        self.assertEqual(validate_timing(self.run_id,self.record('timing',empty)),empty)
        self.assertFalse(timing_gate(empty))
        self.assertFalse(timing_gate(dict(target,render_count=0,render_p95_us=0,render_p99_us=0,render_max_us=0)))

if __name__=='__main__': unittest.main()
