"""Full selected161 parent/calibration ledger for the isolated early-RX trial."""
import re
from wifi_cal5_log import check as calibration


def check(log):
    return calibration(log, early=True)


def transport(raw, release, *, volume, session, comparison=None):
    """Bind complete logger prefixes, including recurring periodic snapshots.

    This validates transport identity only. It does not accept calibration,
    radio state, storage preservation or connectivity.
    """
    from hardware_boot import limine_config
    from hardware_desktop import session_releases

    def require(ok, why):
        if not ok:
            raise ValueError(why)

    require(isinstance(raw, dict) and {'boot.log', 'exit.log'} <= set(raw) <=
            {'boot.log', 'exit.log', 'session-a.log', 'session-b.log'},
            'missing core or unknown early5 snapshots')
    require(isinstance(release, str) and re.fullmatch(r'[0-9a-f]{16}', release) and
            isinstance(volume, str) and re.fullmatch(r'[0-9a-f]{8}', volume) and
            isinstance(session, str) and re.fullmatch(r'session-[0-9a-f]{16}-[0-9a-f]{2}', session),
            'invalid early5 transport identity')
    require(comparison in (None, 'early', 'late'), 'invalid comparison arm')
    version = {None: 144, 'early': 145, 'late': 146}[comparison]
    config = limine_config(release, f'ram-usb-wifi-fw-v{version}-laptop-probe-{volume}-auto-return')
    expected = next(l.split(':', 1)[1].split() for l in config.splitlines()
                    if l.strip().startswith('cmdline:'))
    markers = {}
    forbidden = ('wifi-discovery5', 'wifi-power5', 'wifi-handoff5', 'wifi-peer5',
                 'wifi-tx5', 'wifi-join5', 'wifi-wpa5', 'wifi-data5', 'wifi-provider5',
                 'wifi-reuse5', 'wifi-nop5', 'wifi-stream5', 'wifi-stack5',
                 'wifi-http5', 'wifi-reconnect5')
    for name, data in raw.items():
        require(isinstance(data, bytes) and 0 < len(data) <= 4194304,
                'early5 snapshot exceeds source logger capacity')
        try:
            log = data.decode('utf8', errors='strict')
        except UnicodeError as error:
            raise ValueError('invalid early5 snapshot text') from error
        lines = log.splitlines()
        require(not any(l.startswith(forbidden) for l in lines),
                'foreign network owner in calibration-only capture')
        commands = [l[len('cmdline: '):].split() for l in lines if l.startswith('cmdline: ')]
        require(commands == [expected] and session_releases(log) == [release],
                'wrong early5 recipe/release')
        options = [l[len('laptop-capture: boot options='):].split() for l in lines
                   if l.startswith('laptop-capture: boot options=')]
        require(not options or options == [expected], 'conflicting early5 boot options')
        directory = [l for l in lines if l.startswith('workspace-log: directory=')]
        require(directory == ['workspace-log: directory=/work/srvros-work/Logs/' +
                              session + ' periodic_seconds=10'], 'wrong logger session')
        prefix = 'workspace-log: snapshot=' + name + ' '
        rows = list(re.finditer('^' + re.escape(prefix) + r'.+$', log, re.M))
        require(rows and (name not in ('boot.log', 'exit.log') or len(rows) == 1),
                'missing snapshot or repeated core snapshot')
        previous = 0
        for row in rows:
            m = re.fullmatch(re.escape(prefix) +
                r'monotonic_us=([0-9]+) total_before=([0-9]+) tail_limit=4194304', row.group())
            require(m is not None, 'invalid logger snapshot schema/capacity')
            require(previous < int(m[1]) < 600000000, 'unordered logger snapshot time')
            require(len(log[:row.start()].encode()) == int(m[2]), 'incomplete logger prefix')
            previous = int(m[1])
        require(log.endswith(rows[-1].group() + '\n'), 'wrong final logger suffix')
        markers[name] = len(rows)
    for name, data in raw.items():
        require(data.startswith(raw['boot.log']) and raw['exit.log'].startswith(data),
                'snapshots do not share one complete logger prefix')
    return dict(session=session, release=release, complete_logger_prefixes=True,
                logger_capacity=4194304, snapshot_marker_counts=markers,
                native_accepted=False, connectivity_tested=False)
