"""Strict proof of the VM-only returned query failure, never native acceptance."""
import re
from serial_records import complete_lines


def check_query(data):
    data = complete_lines(data)
    def one(prefix, pattern):
        lines = [line for line in data.splitlines() if line.startswith(prefix)]
        assert len(lines) == 1, (prefix, lines)
        match = re.fullmatch(pattern, lines[0])
        assert match, lines[0]
        return tuple(map(int, match.groups()))
    policy, policy_code, policy_class = one(b'battery-query-policy:',
        rb'battery-query-policy: policy=(\d+) code=(\d+) class=(\d+); dispatch=refused')
    assert policy in (1,2)
    assert policy_class == (5 if policy==2 and policy_code==16 else 6 if policy==2 and policy_code==128 else 4)
    receipt = one(b'battery-query-return:',
        rb'battery-query-return: sequence=(\d+) generation=(\d+) code=(\d+) transfer=1 aml_calls=(\d+) aml_returns=(\d+) effects=0 completed_before=(\d+) completed_after=(\d+) notifications_before=(\d+) notifications_after=(\d+) idle=1; refused=yes')
    sequence, generation, code, calls, returns, before, after, notify_before, notify_after = receipt
    assert sequence > 0 and generation == 2 and code in (16, 128, 255) and code == policy_code
    assert calls == returns and after > before and notify_after >= notify_before
    assert 0 <= calls <= 128 and notify_after-notify_before <= 16*calls
    assert after-before >= 1+notify_after-notify_before
    resources = one(b'battery-recovery-resources:',
        rb'battery-recovery-resources: known=1 lease=0 ec_phase=(6) poison=0 sample=0 waiting=0 smbus=0 api=0 aml=0 mutex=0 pending=0 facs=0 blocked=0')
    retired = one(b'battery-service: retired ',
        rb'battery-service: retired epoch=(\d+) attempts=(\d+) rejected=0 queries=(\d+) notifications=(\d+) attached=(\d+) detached=(\d+) heap=0 errors=0 denied=0')
    epoch, attempts, queries, notifications, attached, detached = retired
    assert epoch == 1 and attempts == generation and queries >= calls
    assert notifications == notify_after and attached == detached > 0
    outcome = one(b'battery-query: state=',
        rb'battery-query: state=(6) error=(5) generation=(2) fresh=0 before=1 after=1 io_owned=1; retired=yes outcome=query-refused')
    assert outcome[2] == generation
    assert data.index(b'battery-query-policy:') < data.index(b'battery-query-return:')
    assert data.index(b'battery-query-return:') < data.index(b'battery-recovery-resources:') < data.index(b'battery-service: retired ') < data.index(b'battery-query: state=')
    restores = re.findall(rb'^battery: foreign-event restored enable=0x([0-9a-f]+) original=0x([0-9a-f]+) protected_writes=(\d+)\r?$', data, re.M)
    assert len(restores) == 2 and {(int(p,16),int(v,16)) for p,v,_ in restores} == {(0x62f,0x40),(0x62e,4)}
    assert sum(line.startswith(b'battery: foreign-event restored ') for line in data.splitlines()) == 2
    for forbidden in (b'battery: HALTED', b'battery: FAILED', b'battery-quarantine:', b'battery-recovery: held=', b'battery-desktop: PASS'):
        assert forbidden not in data
    return dict(status='PASS', outcome='returned-failure', policy=policy, code=code, generation=generation,
                aml_calls=calls, notifications_drained=notify_after-notify_before,
                worker_completed_before=before, worker_completed_after=after,
                runtime_retired=True, cache_fresh=False, native_acceptance='not tested')


def check_query_finish(data):
    """Live serial completion is separate from the earlier saved failure log."""
    data = complete_lines(data)
    expected = (
        b'battery-query: saved=yes unmounted=yes; runtime_retired=yes',
        b'battery-query: returning after storage drain; test outcome=returned-failure',
    )
    for prefix, line in zip((b'battery-query: saved=', b'battery-query: returning'), expected):
        assert [row for row in data.splitlines() if row.startswith(prefix)] == [line]
    assert data.index(b'battery-query: state=') < data.index(b'workspace: unmounted; USB writes disabled') < data.index(expected[0]) < data.index(expected[1])
