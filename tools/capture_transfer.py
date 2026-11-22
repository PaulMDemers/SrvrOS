"""Strict immutable capture-transfer companions joined to final capture history.

Clocks describe software syscall/poll observations. Worker receive includes
allocation and pipe reception; these records split those spans without claiming
CPU execution, pipe-publication times, or a causal performance improvement.
"""
import re

from capture_history import capture_history

_COMMON = 'version snapshot_us index id pid started_us finished_us retired_us'
_FIELDS = {
    'transfer': _COMMON + ' enabled valid invalid total polls budget_polls begin_us end_us poll_us gap_us gap_max_us final',
    'write': _COMMON + ' write_calls write_ok write_bytes short_writes write_again write_intr write_failed write_first_us write_last_us write_us write_gap_us write_gap_max_us request_done_us pixels_done_us final',
    'read': _COMMON + ' read_calls read_bytes read_again read_intr read_failed progress_frames worker_receive_us worker_allocated_us worker_read_done_us worker_final_us final',
}
_PREFIXES = {
    'desktop-capture-transfer': 'transfer',
    'desktop-capture-transfer-write': 'write',
    'desktop-capture-transfer-read': 'read',
}
_U64 = 1 << 64
_REQUEST_BYTES = 176
_FRAME_BYTES = 32
_CHUNK_BYTES = 4096
_MAX_PIXELS = ((8 * 1024 * 1024 - 54) // 4) * 4


def _row(line):
    prefix, separator, text = line.partition(': ')
    kind = _PREFIXES.get(prefix)
    if not separator or kind is None:
        raise ValueError('Malformed capture transfer prefix')
    fields = _FIELDS[kind].split()
    match = re.fullmatch(' '.join(key + r'=([0-9]+)' for key in fields), text)
    if not match:
        raise ValueError('Malformed capture transfer ' + kind)
    row = dict(zip(fields, map(int, match.groups())))
    if any(value >= _U64 for value in row.values()):
        raise ValueError('Overflowing capture transfer value')
    if row['version'] != 1 or not row['snapshot_us'] or row['final'] not in (0, 1):
        raise ValueError('Unsupported capture transfer observation')
    return kind, row


def capture_transfer(blobs, expected=3, expected_pixels=None, history=None):
    """Require one complete scalar companion for each retained final capture.

    Writer completion clocks are syscall-return observations: a worker may
    receive pixels before the writer resumes. Do not falsely order worker
    read completion after the writer's pixels_done_us observation.
    """
    if type(expected) is not int or not 0 <= expected <= 3:
        raise ValueError('Capture transfer expectation exceeds retained capacity')
    if expected_pixels is not None and (type(expected_pixels) is not int or
            not 0 < expected_pixels <= _MAX_PIXELS or expected_pixels % 4):
        raise ValueError('Invalid expected capture pixel size')
    groups = {}
    for blob in blobs:
        for line in blob.decode(errors='replace').splitlines():
            if not line.startswith('desktop-capture-transfer'):
                continue
            kind, row = _row(line)
            group = groups.setdefault(row['snapshot_us'],
                                      {'transfer': {}, 'write': {}, 'read': {}})
            table = group[kind]
            if row['index'] in table and table[row['index']] != row:
                raise ValueError('Conflicting capture transfer ' + kind)
            table[row['index']] = row
    joined = capture_history(blobs, expected=expected) if history is None else history
    if (joined.get('status') != 'VALID' or joined.get('completed') != expected or
            len(joined.get('captures', [])) != expected or not joined.get('snapshot_us')):
        raise ValueError('Invalid joined capture history')
    at = joined['snapshot_us']
    group = groups.get(at, {'transfer': {}, 'write': {}, 'read': {}})
    if any(set(group[kind]) != set(range(expected)) for kind in _FIELDS):
        raise ValueError('Missing/extra bound capture transfer companions')
    captures = []
    for index, completed in enumerate(joined['captures']):
        header, writes, reads = (group[kind][index] for kind in ('transfer', 'write', 'read'))
        for row in (header, writes, reads):
            if row['final'] != 1 or any(row[key] != completed[key]
                    for key in _COMMON.split()[1:]):
                raise ValueError('Capture transfer not bound to final completion')
        if (header['enabled'], header['valid'], header['invalid']) != (1, 1, 0):
            raise ValueError('Disabled/invalid capture transfer observation')
        pixels = header['total'] - _REQUEST_BYTES
        if (not 0 < pixels <= _MAX_PIXELS or pixels % 4 or
                (expected_pixels is not None and pixels != expected_pixels)):
            raise ValueError('Capture transfer input size differs from request')
        polls = header['polls']
        if not polls or not (completed['started_us'] <= header['begin_us'] <=
                header['end_us'] <= completed['finished_us']):
            raise ValueError('Capture transfer poll span outside completion')
        span = header['end_us'] - header['begin_us']
        if (header['poll_us'] + header['gap_us'] != span or
                header['gap_max_us'] > header['gap_us'] or
                header['gap_us'] > (polls - 1) * header['gap_max_us']):
            raise ValueError('Capture poll wall/gaps do not partition its span')
        calls, success = writes['write_calls'], writes['write_ok']
        outcomes = success + writes['write_again'] + writes['write_intr'] + writes['write_failed']
        minimum = 1 + (pixels + _CHUNK_BYTES - 1) // _CHUNK_BYTES
        if (writes['write_failed'] or calls != outcomes or not minimum <= success <= header['total'] or
                calls > 4 * polls or writes['write_bytes'] != header['total'] or
                writes['short_writes'] > success or
                (not writes['short_writes'] and success != minimum)):
            raise ValueError('Capture write counts/bytes violate bounded transfer')
        if (header['budget_polls'] >= polls or header['budget_polls'] * 4 > success):
            raise ValueError('Invalid four-write budget exhaustion count')
        # Every unfinished send poll either consumes four successful writes or
        # breaks on AGAIN/INTR after at most three successes. The final send
        # poll contributes at most four more; response-only polls add none.
        if success > 4 * (header['budget_polls'] + 1) + 3 * (writes['write_again'] + writes['write_intr']):
            raise ValueError('Capture successes lack bounded send-poll accounting')
        if not (header['begin_us'] <= writes['write_first_us'] <= writes['request_done_us'] <=
                writes['pixels_done_us'] == writes['write_last_us'] <= header['end_us']):
            raise ValueError('Invalid capture write completion observations')
        write_span = writes['write_last_us'] - writes['write_first_us']
        if (writes['write_us'] + writes['write_gap_us'] != write_span or
                writes['write_us'] > header['poll_us'] or
                writes['write_gap_max_us'] > writes['write_gap_us'] or
                writes['write_gap_us'] > (calls - 1) * writes['write_gap_max_us']):
            raise ValueError('Capture write wall/gaps do not partition its span')
        if (reads['read_failed'] or reads['read_calls'] != polls or
                reads['read_again'] + reads['read_intr'] > reads['read_calls'] or
                not 2 <= reads['progress_frames'] <= 0xffffffff):
            raise ValueError('Invalid capture response call/frame counters')
        positive_reads = reads['read_calls'] - reads['read_again'] - reads['read_intr']
        frame_count = reads['progress_frames'] + 2  # One READY, one final response.
        if (reads['read_bytes'] != frame_count * _FRAME_BYTES or
                not positive_reads <= reads['read_bytes'] <= positive_reads * _FRAME_BYTES):
            raise ValueError('Capture response bytes differ from complete frame stream')
        if not (completed['started_us'] <= reads['worker_receive_us'] <=
                reads['worker_allocated_us'] <= reads['worker_read_done_us'] <=
                reads['worker_final_us'] <= completed['finished_us']):
            raise ValueError('Invalid worker allocation/read/final clocks')
        if (reads['worker_receive_us'] < writes['write_first_us'] or
                reads['worker_read_done_us'] - reads['worker_receive_us'] != completed['p1_us']):
            raise ValueError('Worker receive span differs from retained phase')
        captures.append(dict(index=index, id=completed['id'], pid=completed['pid'],
                transfer=header, write=writes, read=reads,
                poll_span_us=span, write_span_us=write_span,
                allocation_us=reads['worker_allocated_us'] - reads['worker_receive_us'],
                receive_us=reads['worker_read_done_us'] - reads['worker_allocated_us'],
                worker_receive_phase_us=reads['worker_read_done_us'] - reads['worker_receive_us'],
                final_observation_us=completed['finished_us'] - reads['worker_final_us']))
    return dict(status='VALID', snapshot_us=at, completed=expected, captures=captures,
                scope='Immutable per-capture syscall/poll observations and worker allocation/receive spans; not CPU, exact pipe-publication, hardware time or a causal performance improvement')
