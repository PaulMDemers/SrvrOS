"""Matched parked-logger RX publication comparison; no causal/network authority."""
import re

from wifi_cal5_log import check as calibration
from wifi_early5_observation import observe as queue_observe
from wifi_early5_log import transport as queue_transport


def require(ok, why):
    if not ok:
        raise ValueError(why)


def observe(log, *, arm):
    require(arm in ('early', 'late'), 'invalid RX comparison arm')
    before = int(arm == 'early')
    lines = log.splitlines()
    commands = [l[len('cmdline: '):].split() for l in lines if l.startswith('cmdline: ')]
    require(len(commands) == 1, 'missing unique comparison command line')
    modes = [t for t in commands[0] if t.startswith('srvros.wifi-early5-timing=')]
    require(modes == ['srvros.wifi-early5-timing=' + arm], 'wrong comparison selection')
    logging = f'wifi-rx-compare5-log: version=1 parked=1 periodic=0 before_mac={before}'
    rows = [l for l in lines if l.lstrip().startswith('wifi-rx-compare5')]
    require(rows and rows[0] == logging and rows.count(logging) == 1,
            'missing successful matched logger quiesce')
    require(not any(l.startswith('workspace-log: snapshot=session-')
                    for l in lines[lines.index(logging) + 1:]),
            'periodic snapshot after matched logger quiesce')
    result = queue_observe(log, before_mac=bool(before))
    if result.get('owner_observation_absent'):
        require(rows == [logging], 'absent radio entered comparison owner')
        return dict(result, arm=arm, logger_parked=True)
    require(result['present'] and len(rows) == 2, 'partial/unknown comparison owner')
    owner = re.fullmatch(r'wifi-rx-compare5-owner: version=1 before_mac=([01]) capacity=32 configured=([01])', rows[1])
    require(owner is not None and int(owner[1]) == before, 'wrong comparison owner/mode')
    require(lines.index(logging) < lines.index(rows[1]), 'logger parked after owner startup')
    if result['summary']['complete']:
        require(owner[2] == '1', 'complete comparison without configuration')
    publishes = [l for l in lines if l.startswith('wifi-early5-publish:')]
    if publishes:
        require(lines.index(rows[1]) < lines.index(publishes[0]), 'publication before comparison owner')
        calibrated = [l for l in lines if l.startswith('wifi-cal5-calibration:')]
        require(len(calibrated) == 1, 'published comparison lacks calibration outcome')
        require((lines.index(publishes[0]) < lines.index(calibrated[0])) == bool(before),
                'wrong calibration/publication ordering')
    if not before:
        require(result['summary']['during_mcu'] == 0, 'late control consumed MCU-span data')
        if not result['summary']['calibration']:
            require(not result['summary']['posted'] and not result['summary']['stopped'],
                    'late control published/stopped after failed calibration')
    return dict(result, arm=arm, logger_parked=True, configured=bool(int(owner[2])))


def check(log, *, arm):
    observed = observe(log, arm=arm)
    require(observed['present'] and observed['summary']['complete'], 'comparison not complete')
    result = calibration(log, early=True, early_before_mac=arm == 'early')
    result.update(comparison_observation=observed, comparison_arm=arm,
                  logger_parked=True, native_accepted=False, root_cause_proven=False,
                  connectivity_tested=False)
    return result


def transport(raw, release, *, volume, session, arm):
    require(arm in ('early', 'late'), 'invalid RX comparison arm')
    return queue_transport(raw, release, volume=volume, session=session, comparison=arm)
