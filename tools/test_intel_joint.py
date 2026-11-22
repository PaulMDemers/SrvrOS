"""Actual native adapter, exact boot gate, lossless raw joins and fault prefixes."""
import base64
import os
from pathlib import Path
import re
import subprocess
import unittest
from hardware_boot import stage, limine_config
from intel_joint import joint_capture, joint_result, native_joint_result, joint_recipe
from intel_display_backing import backing_result, pte_chunk
from intel_gt_firmware import firmware_result

ROOT = Path(__file__).resolve().parents[1]
EXE = Path(os.environ.get('INTEL_JOINT_EXE', ROOT / 'build/tests/intel-joint-adapter'))


def run(mode):
    p = subprocess.run([str(EXE), '--wake-engine-' + mode], capture_output=True, text=True, timeout=15)
    if p.returncode:
        raise AssertionError((mode, p.stdout[-2500:], p.stderr))
    return p.stdout


def field(text, prefix, key, value):
    result, count = re.subn(r'(^' + re.escape(prefix) + r':.*?\b' + key + r'=)[^\s]+', lambda m: m[1] + str(value), text, count=1, flags=re.M)
    assert count == 1
    return result


class JointAdapterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = run('report')
        cls.stats = {k: int(v) for k, v in re.findall(r'(reads|endpoints|cache|attempts|stores|claims)=(\d+)', cls.text.split('joint-fixture:')[1])}

    def test_complete_actual_adapter_and_native_refusal(self):
        r = joint_result(self.text)
        self.assertTrue(r['complete'])
        self.assertFalse(r['native_complete'])
        s = r['summary']
        self.assertEqual([p['endpoint']['command'] for p in r['phases']], [6, 2, 2, 6])
        self.assertEqual((s['root_maps'], s['root_retired'], s['borrows'], s['returns'], s['cache']), (1, 1, 4, 4, 22))
        self.assertEqual((self.stats['reads'], self.stats['claims'], self.stats['stores']), (232, 12, 2))
        self.assertEqual((r['held']['summary']['live_checks'], r['held']['summary']['live_reads']), (55, 275))
        with self.assertRaises(ValueError): native_joint_result(self.text)
        # A microsecond clock can tie at adjacent lifetime boundaries.
        tied = field(self.text, 'intel-joint-summary', 'begin', s['hda_begin'])
        tied = field(tied, 'intel-wake-summary', 'end_us', s['end'])
        self.assertTrue(joint_result(tied)['complete'])

    def test_every_hda_load_and_endpoint_boundary(self):
        cases = 0
        for mode, count in (('joint-read', self.stats['reads']), ('joint-drift', self.stats['reads']), ('joint-read-time', self.stats['reads']), ('joint-alias', self.stats['reads']), ('joint-endpoint', self.stats['endpoints']), ('joint-cache', self.stats['cache'])):
            for point in range(1, count + 1):
                with self.subTest(mode=mode, point=point):
                    text = run(f'{mode}:{point}')
                    raw = joint_capture(text)
                    self.assertFalse(raw['scope_complete'])
                    try: result = joint_result(text)
                    except ValueError: continue
                    self.assertFalse(result['complete'])
                cases += 1
        print('native joint HDA fault prefixes:', cases)

    def test_separate_guard_and_return_keep_terminal_deadline(self):
        def preparation(result):
            return result['held']['interval']['reset']['prepare']['summary']

        good = preparation(joint_result(self.text))
        self.assertEqual(good['v'], 3)
        self.assertLessEqual(good['begin'], good['guard_begin'])
        self.assertLess(good['guard_begin'], good['guard_end'])
        self.assertLessEqual(good['guard_end'], good['return_begin'])
        self.assertLess(good['return_begin'], good['end'])
        for key, value in [('guard_begin', good['guard_end'] + 1),
                           ('guard_end', good['return_begin'] + 1), ('return_begin', 0)]:
            with self.subTest(key=key), self.assertRaises(ValueError):
                joint_result(field(self.text, 'intel-prepare-summary', key, value))
        late = joint_result(run('joint-return-deadline'))
        self.assertFalse(late['complete'])
        timing = preparation(late)
        self.assertEqual((timing['reason'], timing['stage'], timing['returned'], timing['retained']), (2, 6, 1, 0))
        self.assertLessEqual(timing['return_begin'] - timing['begin'], 100000)
        self.assertGreater(timing['end'] - timing['begin'], 100000)
        self.assertEqual(timing['clock_at'], timing['end'])
        self.assertEqual(timing['clock_prev'], timing['return_begin'])
        self.assertEqual(late['summary']['hda_complete'], 1)
        self.assertEqual(late['wake']['summary']['released'], 1)

    def test_native_store_last_admission_and_retirement(self):
        for mode in ('joint-none', 'joint-uncertain', 'joint-store-time', 'joint-store-zero', 'joint-store-backward'):
            for point in (1, 2):
                with self.subTest(mode=mode, point=point):
                    text = run(f'{mode}:{point}'); s = joint_capture(text)['summary']
                    self.assertFalse(s['complete']); self.assertTrue(s['retained'])
                    if mode.startswith('joint-store-'):
                        self.assertEqual(s['hda_writes'], point - 1)
                        self.assertTrue(s['clock_cause'])
        for mode in ('joint-map:1', 'joint-retire:1', 'joint-leave'):
            with self.subTest(mode=mode):
                s = joint_capture(run(mode))['summary']
                self.assertFalse(s['complete']); self.assertTrue(s['retained'])
                if mode=='joint-leave':self.assertEqual((s['hda_pins'],s['hda_attempts'],s['hda_writes']),(1,1,1))
        text=run('joint-upper:1'); s=joint_capture(text)['summary']
        self.assertEqual((s['entry'],s['admission'],s['root_maps'],s['hda_attempts']),(1,1,0,0))
        self.assertIn('state=0 reason=0 writes=0 attempts=0',next(l for l in text.splitlines() if l.startswith('intel-wake-summary:')))

    def test_both_original_entry_observations_and_lost_sources(self):
        modes = ('interval-read', 'interval-drift', 'interval-foreign', 'interval-map', 'interval-time', 'joint-drop-hda-child', 'joint-drop-hda-list', 'joint-drop-hda-count', 'joint-drop-claim')
        for mode in modes:
            for point in range(1, 13):
                with self.subTest(mode=mode, point=point):
                    text = run(f'{mode}:{point}'); r = joint_capture(text); s = r['summary']
                    self.assertFalse(s['complete']); self.assertTrue(s['retained'])
                    self.assertEqual((s['hda_attempts'], s['hda_writes'], s['hda_pins']), (1, 1, 1))
                    try: result = joint_result(text)
                    except ValueError: continue
                    self.assertFalse(result['complete'])

    def test_lossless_pte_chunks_and_image_binding(self):
        b = backing_result(self.text)
        self.assertEqual(len(b['ptes']), 1356)
        self.assertEqual({x['v'] for x in b['rows']['pte']}, {3})
        self.assertEqual({x['w'] for x in b['rows']['pte']}, {4})
        self.assertEqual({x['h'] for x in b['rows']['pte']}, {0})
        for version in (18, 19):
            with self.assertRaises(ValueError): backing_result(self.text.replace('intel-wake-summary: v=20 ', f'intel-wake-summary: v={version} '))
        p = subprocess.run([str(EXE), '--joint-codec'], capture_output=True, text=True, timeout=15)
        self.assertEqual(p.returncode, 0, p.stderr)
        words = []; widths = []
        for line in p.stdout.splitlines():
            if not line.startswith('intel-backing-pte:'): continue
            tokens = dict(t.split('=', 1) for t in line.split()[1:])
            chunk = {k: v if k == 'data' else int(v) for k, v in tokens.items()}
            self.assertEqual(chunk['first'], len(words)); words.extend(pte_chunk(chunk, 3)); widths.append(chunk['w'])
        expected = [((0x12340000 + i // 64) << 32) | (0x90000001 + i * 4096) for i in range(1356)]
        expected[0] ^= 1 << 33
        self.assertEqual(words, expected); self.assertEqual(widths, [8] + [4] * 21)
        with self.assertRaises(ValueError): backing_result(p.stdout)
        for chunk in b['rows']['pte']:
            wrong = dict(chunk, w=8)
            with self.assertRaises(ValueError): pte_chunk(wrong, 3)

    def test_joint_firmware_vectors_keep_both_samples(self):
        result = firmware_result(self.text)
        self.assertTrue(result['qualification_complete'])
        words = [int(vector[i:i + 8], 16)
                 for vector in re.findall(r'^intel-firmware-vector: .* h=([0-9a-f]{104})$', self.text, re.M)
                 for i in range(0, 104, 8)]
        self.assertEqual(len(words), 26)
        self.assertEqual([row['value'] for row in result['rows']['word']], words)
        for version in (16, 22):
            with self.subTest(version=version), self.assertRaises(ValueError):
                firmware_result(self.text.replace('intel-wake-summary: v=20 ', f'intel-wake-summary: v={version} '))

    def test_strict_raw_join_mutations(self):
        for key, value in [('complete', '01'), ('state', 2), ('reason', 4), ('pinned', 1), ('retained', 1), ('root_retired', 0), ('returns', 3), ('hda_pins', 1), ('hda_writes', 1), ('hda_epoch', 0), ('clock_at', 1), ('cache', 21), ('origin', 1), ('assurances', 255), ('managed_pages', 1)]:
            with self.subTest(key=key), self.assertRaises(ValueError): joint_result(field(self.text, 'intel-joint-summary', key, value))
        for prefix, changes in [('intel-claim-summary', [('origin', 1), ('expected', 1), ('epoch', 2), ('state', 2), ('pinned', 1)]), ('intel-claim-final-summary', [('origin', 1), ('reads', 5), ('complete', 0), ('epoch_after', 2)])]:
            for key, value in changes:
                with self.subTest(prefix=prefix, key=key), self.assertRaises(ValueError): joint_result(field(self.text, prefix, key, value))
        for prefix in ('intel-joint-hda:', 'intel-claim-final-row:', 'intel-claim-summary:', 'intel-joint-summary:'):
            line = next(l for l in self.text.splitlines() if l.startswith(prefix))
            for bad in (self.text.replace(line + '\n', ''), self.text.replace(line, line + '\n' + line), self.text.replace(line, line + ' extra=1')):
                with self.subTest(prefix=prefix), self.assertRaises(ValueError): joint_result(bad)
        rows = [l for l in self.text.splitlines() if l.startswith('intel-joint-hda:')]
        for line in rows:
            encoded = line.split(' b=')[1]; raw = bytearray(base64.b64decode(encoded)); raw[0] ^= 1
            bad = self.text.replace(line, line.split(' b=')[0] + ' b=' + base64.b64encode(raw).decode())
            with self.assertRaises(ValueError): joint_result(bad)

    def test_exact21_token_recipe_and_older_image_refusal(self):
        config = limine_config('0123456789abcdef', 'ram-usb-v1-intel-joint-laptop-probe-37d8c5d0-auto-return')
        recipe = next(l.split('cmdline: ', 1)[1] for l in config.splitlines() if 'cmdline: ' in l)
        self.assertEqual(joint_recipe(recipe), dict(volume='37d8c5d0', release='0123456789abcdef'))
        self.assertEqual(subprocess.run(['build/tests/intel-gt-boot', 'joint', recipe], capture_output=True).returncode, 0)
        for old in ('rehold', 'held-interval', 'held-reset'):
            self.assertNotEqual(subprocess.run(['build/tests/intel-gt-boot', old, recipe], capture_output=True).returncode, 0)
        tokens = recipe.split()
        for n in range(21):
            for bad in (' '.join(tokens[:n] + tokens[n + 1:]), recipe + ' ' + tokens[n]):
                with self.assertRaises(ValueError): joint_recipe(bad)
                self.assertNotEqual(subprocess.run(['build/tests/intel-gt-boot', 'joint', bad], capture_output=True).returncode, 0)
        for extra in ({'intel_rehold': True}, {'intel_held_interval': True}, {'intel_held_reset': True}, {'intel_hda_lease_trial': True}, {'repeat': 2}, {'hold_on_failure': True}, {'desktop_volume': '37d8c5d0'}, {'aml_capture': True}, {'desktop_scale': 2}):
            with self.subTest(extra=extra), self.assertRaises(ValueError): stage(laptop_probe_volume='37d8c5d0', intel_joint=True, **extra)
        with self.assertRaises(ValueError): stage(intel_joint=True)


if __name__ == '__main__':
    unittest.main()
