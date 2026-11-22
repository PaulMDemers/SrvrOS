import unittest
from wifi_scan_log import scan_checkpoint_trace

PREFIX = 'boot prefix\n'
BEFORE = 'wifi-tune-guard: fixture=before\n'
AFTER = 'wifi-tune-guard: fixture=after\n'


def checkpoint(prefix, name='session-b.log', sampled=100, stamp=101):
    lines = [
        f'workspace-scheduler: sampled_ticks=20 idle_ticks=1 switches=30 preemptions=10 monotonic_us={sampled}\n',
        f'workspace-devices: smc_state=0 flags=0x0000000000000000 fan_millirpm=0 sample_us=0 samples=0 failures=0 error=0 monotonic_us={sampled}\n',
        f'workspace-devices: pcm_state=0 owner=0 handle=0 submitted=0 position=0 queued=0 underruns=0 monotonic_us={sampled}\n',
        f'workspace-devices: audio_state=0 error=0 output=0 volume=25 pending=0 started=0 completed=0 headphones=0 caps=0x0000000000000000 monotonic_us={sampled}\n',
        f'workspace-devices: thermal_state=0 flags=0x0000000000000000 core_mc=0 package_mc=0 sample_us=0 samples=0 monotonic_us={sampled}\n',
    ]
    total = len((prefix + ''.join(lines)).encode())
    lines += [f'workspace-log: snapshot={name} monotonic_us={stamp} total_before={total} tail_limit=1048576\n', 'workspace: flushed\n']
    return ''.join(lines)


class CheckpointProjection(unittest.TestCase):
    def project(self, block, prefix=PREFIX):
        leg = BEFORE + block + AFTER
        return scan_checkpoint_trace(leg, prefix + leg, len(prefix))

    def test_complete_checkpoints_and_original_offsets(self):
        for prefix in [PREFIX, 'UTF8 prefix é\n', '']:
            for name in ['session-a.log', 'session-b.log']:
                block = checkpoint(prefix + BEFORE, name)
                projected, rows = self.project(block, prefix)
                self.assertEqual(projected, BEFORE + AFTER)
                self.assertEqual(len(rows), 1)
                self.assertEqual(rows[0]['name'], name)
                self.assertEqual(rows[0]['snapshot_bytes'], len((prefix + BEFORE + block.removesuffix('workspace: flushed\n')).encode()))
        projected, rows = self.project('')
        self.assertEqual((projected, rows), (BEFORE + AFTER, []))

    def test_dropped_duplicated_reordered_and_extra_fields(self):
        lines = checkpoint(PREFIX + BEFORE).splitlines(keepends=True)
        for i in range(7):
            variants = [lines[:i] + lines[i+1:], lines[:i] + [lines[i]] + lines[i:],
                        lines[:i] + [lines[i].rstrip('\n') + ' extra=1\n'] + lines[i+1:]]
            if i < 6:
                swapped = lines[:]; swapped[i], swapped[i+1] = swapped[i+1], swapped[i]
                variants.append(swapped)
            for variant in variants:
                with self.subTest(line=i, variant=variant):
                    with self.assertRaises(AssertionError):
                        clean, _ = self.project(''.join(variant))
                        # Unknown residual records are refused by the phase validator.
                        self.assertEqual(clean, BEFORE + AFTER)

    def test_profile_fields_and_timestamps(self):
        block = checkpoint(PREFIX + BEFORE)
        for old, new in [('sampled_ticks=20', 'sampled_ticks=0'), ('idle_ticks=1 ', 'idle_ticks=21 '),
                         ('switches=30', 'switches=9'), ('preemptions=10', 'preemptions=31'),
                         ('sampled_ticks=20', 'sampled_ticks=020'), ('sampled_ticks=20', f'sampled_ticks={1 << 64}'),
                         ('monotonic_us=100', 'monotonic_us=99'), ('monotonic_us=101', 'monotonic_us=99'),
                         ('smc_state=0', 'smc_state=1'), ('flags=0x0000000000000000', 'flags=0x0000000000000001'),
                         ('owner=0', 'owner=1'), ('volume=25', 'volume=26'), ('package_mc=0', 'package_mc=-1'),
                         ('session-b.log', 'exit.log'), ('tail_limit=1048576', 'tail_limit=65536'),
                         ('workspace: flushed', 'workspace: flush failed')]:
            with self.subTest(old=old, new=new):
                with self.assertRaises(AssertionError):
                    self.project(block.replace(old, new, 1))

    def test_prefix_integrity_and_input_bounds(self):
        block = checkpoint(PREFIX + BEFORE)
        with self.assertRaises(AssertionError):
            self.project(block, PREFIX + 'different length')
        leg = BEFORE + block + AFTER
        for start in [-1, len(PREFIX + leg) + 1, 0]:
            with self.assertRaises(AssertionError):
                scan_checkpoint_trace(leg, PREFIX + leg, start)
        with self.assertRaises(AssertionError):
            scan_checkpoint_trace(leg, PREFIX + leg.replace('volume=25', 'volume=26'), len(PREFIX))

    def test_two_ordered_checkpoints_and_bound(self):
        a = checkpoint(PREFIX + BEFORE, 'session-a.log')
        b = checkpoint(PREFIX + BEFORE + a, 'session-b.log', 200, 201)
        clean, rows = self.project(a + b)
        self.assertEqual(clean, BEFORE + AFTER)
        self.assertEqual(len(rows), 2)
        c = checkpoint(PREFIX + BEFORE + a + b, 'session-a.log', 300, 301)
        with self.assertRaises(AssertionError):
            self.project(a + b + c)
        b = checkpoint(PREFIX + BEFORE + a, 'session-b.log', 100, 101)
        with self.assertRaises(AssertionError):
            self.project(a + b)

    def test_unknown_lines_are_preserved_for_phase_refusal(self):
        for line in ['workspace-devices: unknown=1\n', 'wifi-tune: failed=1\n', 'workspace: flushed\n']:
            clean, rows = self.project(line)
            self.assertEqual(clean, BEFORE + line + AFTER)
            self.assertEqual(rows, [])


if __name__ == '__main__':
    unittest.main()
