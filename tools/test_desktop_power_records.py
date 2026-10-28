import unittest
from desktop_power_records import desktop_power_result

class Records(unittest.TestCase):
    def fixture(self):
        logs = {}; final = ['battery-service: retired epoch=1 heap=0 errors=0 denied=0',
            'battery-controls: PASS revoked=yes retired=yes',
            'desktop-power-session: result=PASS cycles=6 errors=0 normal_exit=1 max_loop_us=10000 max_ready_us=20000 max_close_us=30000']
        for i,name in enumerate(('idle','apps','recovery')):
            start=31000000+i*60000000
            final.append(f'desktop-power-phase: name={name} start_us={start} end_us={start+60000000} battery_first={i+1} battery_last={i+2}')
            for j in range(5):
                t=start+5000000+j*10000000;n=t//1000000
                logs[f'checkpoint-{i*5+j:02x}.log'] = '\n'.join([
                    f'worker-snapshot: us={t} count=1 version=2',
                    f'worker: us={t} slot=0 id=1 name=worker state=3 ticks={n} switches={n*2} timeouts={n} signals={n}',
                    f'worker-snapshot-end: us={t} count=1',
                    f'workspace-devices: power_state=2 flags=3 energy_shift=14 nominal_mhz=2500 sample_us={t} interval_us=1000000 samples={n} rejected=0 energy={n*16384} aperf={n*1000} mperf={n*1000} monotonic_us={t+100}',
                    f'workspace-devices: thermal_state=2 flags=3 core_mc=50000 package_mc=51000 sample_us={t} monotonic_us={t+100}'])
        logs['exit.log']='\n'.join(final)+'\n';return logs
    def test_complete_overlapping_checkpoints(self):
        logs=self.fixture();logs['boot.log']=logs['checkpoint-00.log']
        result=desktop_power_result(logs)
        self.assertEqual(result['phases'][0]['power']['average_package_mw'],1000)
        self.assertEqual(result['phases'][1]['worker_activity']['workers'][0]['timeouts'],40)
    def test_missing_invalid_and_conflicting(self):
        for old,new in [('max_loop_us=10000','max_loop_us=500001'),('cycles=6','cycles=5'),('battery_last=2','battery_last=1'),('flags=3 energy','flags=0 energy'),('core_mc=50000','core_mc=90000'),('signals=36','signals=999')]:
            logs=self.fixture()
            if old=='signals=36':logs['boot.log']=logs['checkpoint-00.log']
            logs={k:v.replace(old,new) if k!='boot.log' else v for k,v in logs.items()}
            with self.subTest(old=old), self.assertRaises(AssertionError):desktop_power_result(logs)
        logs=self.fixture();del logs['exit.log']
        with self.assertRaises(AssertionError):desktop_power_result(logs)
        logs=self.fixture()
        for i in range(4):del logs[f'checkpoint-{i:02x}.log']
        with self.assertRaises(AssertionError):desktop_power_result(logs)
    def test_vm_no_fake_native_evidence(self):
        logs={k:'\n'.join(l for l in v.splitlines() if not l.startswith('workspace-devices:')) for k,v in self.fixture().items()}
        self.assertEqual(desktop_power_result(logs,native=False)['status'],'PASS')
        with self.assertRaises(AssertionError):desktop_power_result(logs)

if __name__=='__main__':unittest.main()
