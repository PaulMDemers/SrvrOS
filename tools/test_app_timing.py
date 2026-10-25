import unittest
from app_timing import validate_app_timing


def fixture():
    lines = []
    for cycle, role in enumerate((2, 1, 4)):
        now = 100
        def emit(stage, edge='event', elapsed=0):
            nonlocal now
            now += 10
            lines.append(f'app-timing: pid={cycle+8} role={role} window=1 stage={stage} edge={edge} at_us={now} elapsed_us={elapsed}\n')
        emit('main-entry')
        phases = ['runtime-init', 'pool-init', 'window-open', 'role-init']
        if role == 2:
            phases.insert(0, 'document-map')
        for phase in phases:
            emit(phase, 'begin');emit(phase, 'end', 10)
        emit('first-publish');emit('close-received')
        phases = ['history-free'] + (['window-dispose', 'document-unmap', 'pool-retire'] if role == 2 else ['job-drain', 'window-close'])
        if role == 1:
            phases.append('pool-retire')
        for phase in phases:
            emit(phase, 'begin');emit(phase, 'end', 10)
        emit('main-return')
    return ''.join(lines).encode()


class TimingTests(unittest.TestCase):
    def test_complete_roles(self):
        result = validate_app_timing(fixture(), [(0, 8), (1, 9), (2, 10)])
        self.assertEqual([r['role'] for r in result], [2, 1, 4])
        self.assertEqual(result[0]['phases_us']['document-unmap'], [10])

    def test_missing_mismatched_and_incomplete_receipts_refused(self):
        raw = fixture()
        variants = [b'', raw.replace(b'edge=end', b'edge=event', 1),
                    raw.replace(b'elapsed_us=10', b'elapsed_us=11', 1),
                    raw.replace(b'stage=main-return', b'stage=missing', 1),
                    raw.replace(b'role=2', b'role=1', 1),
                    raw.replace(b'stage=first-publish', b'stage=main-entry', 1)]
        for bad in variants:
            with self.subTest(bad=bad[:100]), self.assertRaises(AssertionError):
                validate_app_timing(bad, [(0, 8), (1, 9), (2, 10)])


if __name__ == '__main__':
    unittest.main()
