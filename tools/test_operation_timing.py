import unittest
from operation_timing import operation_snapshots

def fixture(final=0):
    lines=[f'desktop-operations: version=1 at_us=500 queued=1 exiting=1 final={final}']
    for kind in ('current','slowest'):
        lines.append(f'desktop-capture-state: version=1 snapshot_us=500 kind={kind} id=2 sent=100 total=200 received=0 phase=4 final={final}')
        lines.extend(f'desktop-capture-phase: snapshot_us=500 kind={kind} id=2 phase={p} elapsed_us={p} final={final}' for p in range(1,13))
    return ('\n'.join(lines)+'\n').encode()
class Timing(unittest.TestCase):
    def test_failed_run_periodic_evidence(self):
        d=operation_snapshots([fixture()]);self.assertEqual(d['operations']['queued'],1)
        with self.assertRaises(ValueError):operation_snapshots([fixture()],True)
    def test_final_and_partial_tail(self):
        raw=fixture(1);self.assertEqual(operation_snapshots([raw,raw],True)['captures']['current']['id'],2)
        with self.assertRaises(ValueError):operation_snapshots([raw.rsplit(b'\n',2)[0]],True)
    def test_invalid_counters(self):
        with self.assertRaises(ValueError):operation_snapshots([fixture(1).replace(b'sent=100',b'sent=201')],True)
if __name__=='__main__':unittest.main()
