"""Validate sparse app lifecycle receipts from a complete automatic stress VM.

Native failed/tail-only logs may intentionally lack an end marker. Never treat
that as zero duration or a passing cycle. These are elapsed wall times, including
scheduling and I/O waits, not CPU or gate-wait attribution.
"""
import re

RECORD = re.compile(
    rb'app-timing: pid=(\d+) role=(\d+) window=(\d+) stage=([a-z-]+) '
    rb'edge=(begin|end|event) at_us=(\d+) elapsed_us=(\d+)')


def validate_app_timing(output, completed):
    """completed contains (cycle, pid) pairs from the existing closed receipts."""
    records = {}
    for match in RECORD.finditer(output):
        pid, role, window, stage, edge, at, elapsed = match.groups()
        records.setdefault(int(pid), []).append(
            (int(role), int(window), stage.decode(), edge.decode(), int(at), int(elapsed)))
    summary = []
    for cycle, pid in completed:
        rows = records.get(pid, [])
        assert rows, f'missing app timing for pid {pid}'
        role = (2, 1, 4)[cycle % 3]  # Text, Files, System
        pending, spans, events = {}, {}, {}
        previous = 0
        for observed_role, window, stage, edge, at, elapsed in rows:
            assert observed_role == role and at >= previous and at > 0, (pid, stage, 'identity/clock')
            previous = at
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
        required |= {'document-map', 'window-dispose', 'document-unmap', 'pool-retire'} if role == 2 else {'job-drain', 'window-close'}
        if role == 1:
            required.add('pool-retire')
        assert required <= spans.keys(), (pid, 'missing phases', required - spans.keys())
        order = ('main-entry', 'first-publish', 'close-received', 'main-return')
        assert all(len(events.get(stage, [])) == 1 for stage in order), (pid, 'missing/duplicate event')
        times = [events[stage][0] for stage in order]
        assert times == sorted(times), (pid, 'lifecycle order')
        summary.append(dict(cycle=cycle, pid=pid, role=role,
                            entry_to_publish_us=times[1]-times[0],
                            close_to_return_us=times[3]-times[2], phases_us=spans))
    return summary
