import unittest
from operation_timing import operation_snapshots

def fixture(final=0):
    lines=[f'desktop-operations: version=1 at_us=500 queued=1 exiting=1 final={final}']
    for kind in ('current','slowest'):
        lines.append(f'desktop-capture-state: version=1 snapshot_us=500 kind={kind} id=2 sent=100 total=200 received=0 phase=4 final={final}')
        lines.append(f'desktop-capture-phases: snapshot_us=500 kind={kind} id=2 '+ ' '.join(f'p{p}_us={p}' for p in range(1,13))+f' final={final}')
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
    def test_cleanup(self):
        raw=fixture(1)+b'desktop-capture-cleanup: snapshot_us=500 kind=current id=2 pending=0 total_us=10 close_us=7 unmap_us=2 reap_us=1 final=1\n'
        self.assertEqual(operation_snapshots([raw],True)['cleanup']['current']['close_us'],7)
        for bad in (raw.replace(b'close_us=7',b'close_us=-1'),raw.replace(b'pending=0',b'pending=2'),raw.replace(b'kind=current id=2 pending',b'kind=current id=3 pending')):
            with self.assertRaises(ValueError):operation_snapshots([bad],True)
if __name__=='__main__':unittest.main()
