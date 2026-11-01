import unittest
from app_timing import validate_app_timing, saved_app_timing, lifecycle_receipts, RECORD, PTHREAD_INIT_STAGES


def fixture(files_window_runtime=False,async_service=False,helper_initialization=False,initial_reservation=False,first_surface=False):
    lines = []
    for cycle, role in enumerate((2, 1, 4)):
        now = 100
        def emit(stage, edge='event', elapsed=0):
            nonlocal now
            now += 10
            lines.append(f'app-timing: pid={cycle+8} role={role} window=1 stage={stage} edge={edge} at_us={now} elapsed_us={elapsed}\n')
        emit('main-entry')
        phases = ['runtime-init', 'pool-init', 'window-open', 'role-init']
        mapped = role == 2 or (role == 1 and files_window_runtime)
        if mapped:
            phases.insert(0, 'document-map')
            if async_service:phases.insert(0,'file-service-init')
            if helper_initialization:phases[0:0]=sorted(PTHREAD_INIT_STAGES)
            if initial_reservation:phases.remove('document-map');phases.insert(0,'document-map')
        if first_surface and mapped:
            phases.insert(phases.index('document-map')+1,'canvas-reserve')
            phases.insert(phases.index('window-open')+1,'canvas-adopt')
            phases+=['canvas-pixels','canvas-surface','first-controls','first-draw','first-present']
            if role==2:phases.insert(phases.index('first-controls'),'first-service')
        for phase in phases:
            emit(phase, 'begin');emit(phase, 'end', 10)
        emit('first-publish');emit('close-received')
        if role == 1 and files_window_runtime:
            emit('close-ack')
        phases = ['history-free'] + (['window-dispose', 'document-unmap', 'pool-retire'] if mapped else ['job-drain', 'window-close'])
        if mapped and async_service:phases[1:1]=['canvas-unmap','file-service-stop']
        if role == 1 and not mapped:
            phases.append('pool-retire')
        for phase in phases:
            emit(phase, 'begin');emit(phase, 'end', 10)
        emit('main-return')
    return ''.join(lines).encode()


class TimingTests(unittest.TestCase):
    def test_first_surface_phases_and_order(self):
        raw=fixture(True,True,True,True,True)
        kwargs=dict(files_window_runtime=True,async_file_service=True,helper_initialization=True,initial_model_reservation=True,first_surface=True)
        validate_app_timing(raw,[(0,8),(1,9),(2,10)],**kwargs)
        for stage in ['canvas-reserve','canvas-adopt','canvas-pixels','canvas-surface','first-controls','first-draw','first-present','first-service']:
            with self.subTest(stage=stage),self.assertRaises(AssertionError):
                validate_app_timing(raw.replace(('stage='+stage+' ').encode(),b'stage=missing '),[(0,8),(1,9),(2,10)],**kwargs)
        with self.assertRaises(AssertionError):
            validate_app_timing(raw.replace(b'stage=canvas-reserve ',b'stage=canvas-adopt ').replace(b'stage=window-open ',b'stage=canvas-reserve '),[(0,8),(1,9),(2,10)],**kwargs)

    def test_reserved_text_history_order_and_completeness(self):
        raw=fixture(True,True,True,True)
        lines=raw.splitlines();out=[]
        for row in lines:
            if b'pid=8 ' in row and b'stage=window-open edge=begin' in row:
                now=int(RECORD.fullmatch(row)[6])
                for edge,stamp,elapsed in [('begin',now-8,0),('end',now-6,2)]:
                    out.append(f'app-timing: pid=8 role=2 window=0 stage=text-history-reserve edge={edge} at_us={stamp} elapsed_us={elapsed}'.encode())
            out.append(row)
            if b'pid=8 ' in row and b'stage=role-init edge=begin' in row:
                now=int(RECORD.fullmatch(row)[6])
                for edge,stamp,elapsed in [('begin',now+2,0),('end',now+4,2)]:
                    out.append(f'app-timing: pid=8 role=2 window=1 stage=text-history-init edge={edge} at_us={stamp} elapsed_us={elapsed}'.encode())
        good=b'\n'.join(out)
        kwargs=dict(files_window_runtime=True,async_file_service=True,helper_initialization=True,initial_model_reservation=True,reserved_text_history=True)
        validate_app_timing(good,[(0,8),(1,9),(2,10)],**kwargs)
        with self.assertRaises(AssertionError):validate_app_timing(raw,[(0,8),(1,9),(2,10)],**kwargs)
        with self.assertRaises(AssertionError):validate_app_timing(good.replace(b'text-history-reserve',b'missing'),[(0,8),(1,9),(2,10)],**kwargs)

    def test_initial_model_reserved_before_helper(self):
        kwargs=dict(files_window_runtime=True,async_file_service=True,helper_initialization=True,initial_model_reservation=True)
        validate_app_timing(fixture(True,True,True,True),[(0,8),(1,9),(2,10)],**kwargs)
        with self.assertRaisesRegex(AssertionError,'initial model must precede helper startup'):
            validate_app_timing(fixture(True,True,True),[(0,8),(1,9),(2,10)],**kwargs)

    def test_helper_initialization_complete_once(self):
        raw=fixture(True,True,True)
        kwargs=dict(files_window_runtime=True,async_file_service=True,helper_initialization=True)
        validate_app_timing(raw,[(0,8),(1,9),(2,10)],**kwargs)
        for phase in PTHREAD_INIT_STAGES:
            with self.subTest(phase=phase),self.assertRaises(AssertionError):
                validate_app_timing(raw.replace(('stage='+phase+' ').encode(),b'stage=missing '),[(0,8),(1,9),(2,10)],**kwargs)
        with self.assertRaises(AssertionError):
            validate_app_timing(raw.replace(b'stage=pthread-publish ',b'stage=pthread-start-alloc '),[(0,8),(1,9),(2,10)],**kwargs)

    def test_shared_files_requires_complete_new_lifecycle(self):
        raw = fixture(files_window_runtime=True)
        result = validate_app_timing(raw, [(0, 8), (1, 9), (2, 10)], files_window_runtime=True)
        self.assertEqual(result[1]['phases_us']['document-unmap'], [10])
        with self.assertRaises(AssertionError):
            validate_app_timing(raw, [(0, 8), (1, 9), (2, 10)])
        for phase in ('document-map', 'window-dispose', 'document-unmap', 'pool-retire', 'close-ack'):
            bad = b'\n'.join(line.replace(('stage='+phase).encode(), b'stage=missing')
                             if b'pid=9 ' in line else line for line in raw.splitlines())
            with self.subTest(phase=phase), self.assertRaises(AssertionError):
                validate_app_timing(bad, [(0, 8), (1, 9), (2, 10)], files_window_runtime=True)
        with self.assertRaises(AssertionError):
            validate_app_timing(fixture(), [(0, 8), (1, 9), (2, 10)], files_window_runtime=True)

    def test_complete_roles(self):
        result = validate_app_timing(fixture(), [(0, 8), (1, 9), (2, 10)])
        self.assertEqual([r['role'] for r in result], [2, 1, 4])
        self.assertEqual(result[0]['phases_us']['document-unmap'], [10])

    def test_saved_overlap_and_interleaved_uart(self):
        raw = fixture(); lines = raw.splitlines()
        snapshots = [b'\n'.join(lines[:40]), b'\n'.join(lines[20:])]
        recovered = saved_app_timing(snapshots)
        self.assertEqual(validate_app_timing(recovered, [(0, 8), (1, 9), (2, 10)]),
                         validate_app_timing(raw, [(0, 8), (1, 9), (2, 10)]))
        # An interleaved serial line is not silently accepted or reconstructed.
        serial = raw.replace(b'stage=first-publish', b'stage=peer log\nfirst-publish', 1)
        with self.assertRaises(AssertionError):
            validate_app_timing(serial, [(0, 8), (1, 9), (2, 10)])
        for bad in [raw + lines[0], b'\n'.join(reversed(lines))]:
            with self.assertRaises(AssertionError): saved_app_timing([bad])
        with self.assertRaises(AssertionError):
            validate_app_timing(saved_app_timing([serial]), [(0, 8), (1, 9), (2, 10)])

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


class LifecycleReceipts(unittest.TestCase):
    ready = b'desktop-life: ready cycle=0 pid=8 elapsed_us=20\n'
    closed = b'desktop-life: closed cycle=0 pid=8 elapsed_us=30 errors=0\n'
    def test_whole_snapshot_recovers_interleaved_console(self):
        self.assertEqual(lifecycle_receipts([b'desktop-life: reaOTHER\ndy cycle=0 pid=8 elapsed_us=20\n'+self.closed,self.ready+self.closed],1), ([(0,8,20)],[(0,8,30)]))
    def test_missing_conflicting_duplicate_and_identity_refused(self):
        for logs in ([self.ready], [self.ready+self.closed,self.ready.replace(b'20',b'21')], [self.ready+self.ready+self.closed], [self.ready+self.closed.replace(b'pid=8',b'pid=9')]):
            with self.subTest(logs=logs),self.assertRaises(AssertionError):lifecycle_receipts(logs,1)


class AsyncServiceTiming(unittest.TestCase):
    def test_lanes_and_strict_whole_records(self):
        main=b'app-timing: pid=42 role=1 window=1 stage=main-entry edge=event at_us=200 elapsed_us=0 lane=0\n'
        worker=b'app-timing: pid=42 role=1 window=1 stage=canvas-unmap edge=begin at_us=100 elapsed_us=0 lane=1\n'
        self.assertEqual(len(list(RECORD.finditer(saved_app_timing([main+worker])))),2)
        with self.assertRaises(AssertionError):saved_app_timing([main+worker.replace(b'lane=1',b'lane=0')])
        self.assertFalse(RECORD.fullmatch(main.rstrip().replace(b'lane=0',b'lane=2')))
        self.assertFalse(RECORD.fullmatch(b'corrupt'+main.rstrip()))
    def test_requires_real_canvas_and_join_phases(self):
        raw=fixture(True,True)
        self.assertEqual(len(validate_app_timing(raw,[(0,8),(1,9),(2,10)],files_window_runtime=True,async_file_service=True)),3)
        for stage in (b'canvas-unmap',b'file-service-init',b'file-service-stop'):
            missing=b'\n'.join(line for line in raw.splitlines() if b'stage='+stage not in line)
            with self.assertRaises(AssertionError):validate_app_timing(missing,[(0,8),(1,9),(2,10)],files_window_runtime=True,async_file_service=True)


if __name__ == '__main__':
    unittest.main()
