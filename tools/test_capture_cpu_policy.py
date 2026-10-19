import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from capture_linux_hardware import capture_cpu_policy


class CpuPolicyTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.sys = self.root / 'sys'
        self.cpu = self.sys / 'devices/system/cpu'
        self.out = self.root / 'out'
        self.out.mkdir()

    def put(self, relative, value):
        path = self.cpu / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(value)
        return path

    def capture(self):
        errors = []
        capture_cpu_policy(self.out, errors, self.sys)
        return json.loads((self.out / 'cpu-policy.json').read_text()), errors

    def test_policy_groups_topology_units_and_allowlist(self):
        self.put('online', '0-3\n')
        self.put('cpuidle/current_driver', 'intel_idle\n')
        self.put('cpuidle/current_governor_ro', 'menu\n')
        self.put('cpufreq/boost', '1\n')
        for index in (10, 2):
            prefix = f'cpufreq/policy{index}/'
            self.put(prefix + 'related_cpus', '0 1\n')
            self.put(prefix + 'affected_cpus', '0\n')
            self.put(prefix + 'scaling_governor', 'schedutil\n')
            self.put(prefix + 'scaling_min_freq', '500000\n')
            self.put(prefix + 'cpuinfo_transition_latency', '10000\n')
            self.put(prefix + 'scaling_setspeed', 'EXCLUDED')
            self.put(prefix + 'private', 'EXCLUDED')
        for index in (10, 2):
            self.put(f'cpu{index}/topology/core_id', '1\n')
            self.put(f'cpu{index}/topology/thread_siblings_list', '0,2\n')
            for state in (10, 2):
                prefix = f'cpu{index}/cpuidle/state{state}/'
                self.put(prefix + 'name', f'C{state}\n')
                self.put(prefix + 'latency', '10\n')
                self.put(prefix + 'residency', '20\n')
                self.put(prefix + 'time', '123456\n')
                self.put(prefix + 'usage', '0\n')
                self.put(prefix + 'disable', '0\n')
                self.put(prefix + 'private', 'EXCLUDED')
        self.put('cpu2/online', '0\n')
        self.put('cpuinvalid/online', 'EXCLUDED')
        self.put('cpufreq/policyinvalid/scaling_driver', 'EXCLUDED')
        before = {p: p.read_bytes() for p in self.sys.rglob('*') if p.is_file()}
        result, errors = self.capture()
        self.assertEqual(errors, [])
        self.assertEqual(result['schema'], 'srvros-linux-cpu-policy-v1')
        self.assertEqual(result['cpuidle']['properties']['current_driver'], 'intel_idle')
        self.assertEqual([p['name'] for p in result['cpufreq']['policies']], ['policy2', 'policy10'])
        self.assertEqual([c['name'] for c in result['cpus']], ['cpu2', 'cpu10'])
        self.assertEqual(result['cpus'][0]['properties']['online'], '0')
        self.assertNotIn('online', result['cpus'][1]['properties'])
        self.assertEqual([s['index'] for s in result['cpus'][0]['cpuidle']['states']], [2, 10])
        self.assertEqual(result['cpus'][0]['cpuidle']['states'][0]['properties']['usage'], '0')
        self.assertFalse(result['online_changed'])
        self.assertGreaterEqual(result['duration_ns'], 0)
        self.assertEqual(result['units']['frequency'], 'kHz')
        self.assertNotIn('EXCLUDED', json.dumps(result))
        self.assertEqual(before, {p: p.read_bytes() for p in self.sys.rglob('*') if p.is_file()})

    def test_absent_subsystems_are_not_zero_measurements(self):
        result, errors = self.capture()
        self.assertEqual(errors, [])
        self.assertFalse(result['available'])
        self.assertFalse(result['cpuidle']['available'])
        self.assertFalse(result['cpufreq']['available'])
        self.assertEqual(result['cpus'], [])
        self.assertIsNone(result['online_after'])
        self.assertIsNone(result['online_changed'])

    def test_failed_attribute_retains_null_and_error(self):
        target = self.put('cpufreq/policy0/cpuinfo_cur_freq', '500000')
        self.put('cpufreq/policy0/scaling_cur_freq', '0')
        original = Path.read_text
        def read(path, *args, **kwargs):
            if path == target:
                raise PermissionError('fixture denied')
            return original(path, *args, **kwargs)
        with patch.object(Path, 'read_text', read):
            result, errors = self.capture()
        props = result['cpufreq']['policies'][0]['properties']
        self.assertIsNone(props['cpuinfo_cur_freq'])
        self.assertEqual(props['scaling_cur_freq'], '0')
        self.assertNotIn('cpuinfo_avg_freq', props)
        self.assertEqual(len(errors), 1)
        self.assertIn('fixture denied', errors[0])

    def test_directory_enumeration_failure_is_unknown(self):
        self.put('cpuidle/current_driver', 'intel_idle')
        original = Path.iterdir
        def iterdir(path):
            if path == self.cpu / 'cpuidle':
                raise PermissionError('fixture denied')
            return original(path)
        with patch.object(Path, 'iterdir', iterdir):
            result, errors = self.capture()
        self.assertIsNone(result['cpuidle']['available'])
        self.assertEqual(len(errors), 1)

    def test_hotplug_disappearing_state_and_changed_online_set(self):
        online = self.put('online', '0-1')
        state = self.put('cpu1/cpuidle/state0/name', 'C1').parent
        original_iter = Path.iterdir
        original_read = Path.read_text
        reads = 0
        def iterdir(path):
            if path == state:
                raise FileNotFoundError('fixture disappeared')
            return original_iter(path)
        def read(path, *args, **kwargs):
            nonlocal reads
            if path == online:
                reads += 1
                return '0-1' if reads == 1 else '0'
            return original_read(path, *args, **kwargs)
        with patch.object(Path, 'iterdir', iterdir), patch.object(Path, 'read_text', read):
            result, errors = self.capture()
        self.assertTrue(result['online_changed'])
        self.assertEqual(result['online_after'], '0')
        self.assertFalse(result['cpus'][0]['cpuidle']['states'][0]['available'])
        self.assertEqual(len(errors), 1)
        self.assertIn('disappeared', errors[0])

    def test_disappearing_field_is_partial(self):
        target = self.put('cpu0/cpuidle/state0/time', '123')
        original = Path.read_text
        def read(path, *args, **kwargs):
            if path == target:
                raise FileNotFoundError('fixture disappeared')
            return original(path, *args, **kwargs)
        with patch.object(Path, 'read_text', read):
            result, errors = self.capture()
        self.assertIsNone(result['cpus'][0]['cpuidle']['states'][0]['properties']['time'])
        self.assertEqual(len(errors), 1)


if __name__ == '__main__':
    unittest.main()
