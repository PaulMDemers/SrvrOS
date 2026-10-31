"""Validate sparse app lifecycle receipts from a complete automatic stress VM.

Native failed/tail-only logs may intentionally lack an end marker. Never treat
that as zero duration or a passing cycle. These are elapsed wall times, including
scheduling and I/O waits, not CPU or gate-wait attribution.
"""
import re

RECORD = re.compile(
    rb'(?m)^app-timing: pid=(\d+) role=(\d+) window=(\d+) stage=([a-z-]+) '
    rb'edge=(begin|end|event) at_us=(\d+) elapsed_us=(\d+)(?: lane=[01])?(?=\r?\n|$)')


def validate_app_timing(output, completed, *, files_window_runtime=False, async_file_service=False):
    """Validate the caller's known lifecycle, without inferring it from missing phases.

    completed contains (cycle, pid) pairs from the existing closed receipts.
    Historical Files captures use job-drain/window-close; the shared window
    runtime uses mapped models, acknowledged close and explicit disposal instead.
    """
    records = {}
    for match in RECORD.finditer(output):
        pid, role, window, stage, edge, at, elapsed = match.groups()
        records.setdefault(int(pid), []).append(
            (int(role), int(window), stage.decode(), edge.decode(), int(at), int(elapsed),match.group(0).endswith(b' lane=1')))
    summary = []
    for cycle, pid in completed:
        rows = records.get(pid, [])
        assert rows, f'missing app timing for pid {pid}'
        role = (2, 1, 4)[cycle % 3]  # Text, Files, System
        pending, spans, events = {}, {}, {}
        previous = {}
        for observed_role, window, stage, edge, at, elapsed,lane in rows:
            assert observed_role == role and at >= previous.get(lane,0) and at > 0, (pid, stage, 'identity/clock')
            previous[lane] = at
            key = (window, stage)
            if edge == 'begin':
                assert key not in pending and elapsed == 0, (pid, stage, 'duplicate begin')
                pending[key] = at
            elif edge == 'end':
                assert key in pending, (pid, stage, 'missing begin')
                assert elapsed == at - pending.pop(key), (pid, stage, 'duration')
                spans.setdefault(stage, []).append(elapsed)
            else:
                assert elapsed == 0, (pid, stage, 'event duration')
                events.setdefault(stage, []).append(at)
        assert not pending, (pid, 'unfinished phase', pending)
        required = {'runtime-init', 'pool-init', 'window-open', 'role-init', 'history-free'}
        mapped = role == 2 or (role == 1 and files_window_runtime)
        required |= {'document-map', 'window-dispose', 'document-unmap', 'pool-retire'} if mapped else {'job-drain', 'window-close'}
        if mapped and async_file_service:
            required |= {'file-service-init','file-service-stop','canvas-unmap'}
        if role == 1:
            required.add('pool-retire')
        assert required <= spans.keys(), (pid, 'missing phases', required - spans.keys())
        order = ('main-entry', 'first-publish', 'close-received', 'main-return')
        assert all(len(events.get(stage, [])) == 1 for stage in order), (pid, 'missing/duplicate event')
        times = [events[stage][0] for stage in order]
        assert times == sorted(times), (pid, 'lifecycle order')
        if role == 1 and files_window_runtime:
            assert len(events.get('close-ack', [])) == 1, (pid, 'missing/duplicate close acknowledgement')
            assert times[2] <= events['close-ack'][0] <= times[3], (pid, 'close acknowledgement order')
        summary.append(dict(cycle=cycle, pid=pid, role=role,
                            entry_to_publish_us=times[1]-times[0],
                            close_to_return_us=times[3]-times[2], phases_us=spans))
    return summary


def saved_app_timing(snapshots):
    """Merge overlapping whole-record bootlog snapshots, never repair UART text.

    Each snapshot must preserve per-process order and contain no duplicate record.
    Snapshot overlap is deduplicated by exact record bytes; conflicting or missing
    lifecycle records remain subject to validate_app_timing's full checks.
    """
    records = {}
    for snapshot in sorted(snapshots, key=lambda raw: max(
            (int(m[6]) for m in RECORD.finditer(raw)), default=0)):
        seen, previous = set(), {}
        for match in RECORD.finditer(snapshot):
            record = match.group(0)
            pid, *_, at, elapsed = match.groups()
            lane=match.group(0).endswith(b' lane=1');identity=pid,lane
            assert record not in seen, 'duplicate record within saved snapshot'
            assert int(at) >= previous.get(identity, 0), 'saved snapshot clock/order'
            previous[identity] = int(at)
            seen.add(record)
        for match in RECORD.finditer(snapshot):
            records.setdefault(match.group(0), None)
    return b'\n'.join(sorted(records, key=lambda row: (
        int(RECORD.fullmatch(row)[1]), int(RECORD.fullmatch(row)[6]))))


def lifecycle_receipts(snapshots, count):
    """Recover whole ready/closed records; require every unique cycle and PID."""
    pattern = re.compile(rb'desktop-life: (ready|closed) cycle=(\d+) pid=(\d+) elapsed_us=(\d+)( errors=0)?')
    records = {b'ready': {}, b'closed': {}}
    for raw in snapshots:
        seen = set()
        for line in raw.splitlines():
            match = pattern.fullmatch(line)
            if not match:
                continue
            edge, cycle, pid, elapsed, errors = match.groups()
            assert bool(errors) == (edge == b'closed'), 'malformed lifecycle receipt'
            cycle, pid, elapsed = int(cycle), int(pid), int(elapsed)
            assert 0 <= cycle < count and pid > 0 and elapsed > 0
            key = edge, cycle
            assert key not in seen, 'duplicate lifecycle receipt within snapshot'
            seen.add(key)
            value = (cycle, pid, elapsed)
            assert cycle not in records[edge] or records[edge][cycle] == value, 'conflicting lifecycle receipt'
            records[edge][cycle] = value
    ready, closed = [sorted(records[edge].values()) for edge in (b'ready', b'closed')]
    assert len(ready) == len(closed) == count, 'missing lifecycle receipt'
    assert [(a, b) for a, b, _ in ready] == [(a, b) for a, b, _ in closed], 'lifecycle identity mismatch'
    assert len({pid for _, pid, _ in ready}) == count, 'reused lifecycle PID'
    return ready, closed
