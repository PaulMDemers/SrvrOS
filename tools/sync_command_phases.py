"""Strict same-command BOT phase companions for selected content timing.

These records reuse existing callback/checkpoint clocks and sampled ring checks.
They describe software observation, not USB wire or device completion latency.
Historical command-role records remain independently parseable without them.
"""
import re

from sync_timing import sync_timing
from sync_content import sync_content
from sync_commands import sync_commands

_COMMON = 'version snapshot_us pid id identity role csw_start_us csw_end_us'
_FIELDS = {
    'command': _COMMON + ' valid complete ok start_us end_us checkpoint_calls checkpoint_us checkpoint_first_us checkpoint_last_us final',
    'phase': _COMMON + ' phase calls requested actual us first_us last_us final',
    'service': _COMMON + ' empty_checks event_checks last_empty_sample_us final',
}
_U64 = 1 << 64


def _row(line):
    name, separator, text = line.partition(': ')
    kind = name.removeprefix('sync-bot-')
    if not separator or kind not in _FIELDS:
        raise ValueError('Malformed BOT companion prefix')
    fields = _FIELDS[kind].split()
    match = re.fullmatch(' '.join(k + r'=([0-9]+)' for k in fields), text)
    if not match:
        raise ValueError('Malformed BOT ' + kind)
    row = dict(zip(fields, map(int, match.groups())))
    if any(v >= _U64 for v in row.values()):
        raise ValueError('Overflowing BOT companion value')
    if row['version'] != 1 or row['final'] != 1 or not row['snapshot_us']:
        raise ValueError('Unsupported/nonfinal BOT companion')
    return kind, row


def sync_command_phases(blobs, commands=None, content=None, sync=None,
                        require_work=True):
    """Join the first strict maximum CSW's whole command and ring sample.

    Selection is inside the same content window. Each nonempty role already
    retains its first strict maximum; this companion must equal one of those
    samples and have the largest duration among them. Equal maxima use their
    earliest callback start, not role row order; indistinguishable clock ties
    remain resolved by the exact matching retained role.
    """
    groups = {}
    for blob in blobs:
        for line in blob.decode(errors='replace').splitlines():
            if not line.startswith('sync-bot-'):
                continue
            kind, row = _row(line)
            group = groups.setdefault(row['snapshot_us'],
                                      {'command': {}, 'phase': {}, 'service': {}})
            key = row['phase'] if kind == 'phase' else 0
            table = group[kind]
            if key in table and table[key] != row:
                raise ValueError('Conflicting BOT ' + kind)
            table[key] = row
    observed = sync_timing(blobs) if sync is None else sync
    selected = (sync_content(blobs, observed, require_work=require_work)
                if content is None else content)
    joined = (sync_commands(blobs, selected, observed, require_work=require_work)
              if commands is None else commands)
    if any(value.get('status') != 'VALID' for value in (observed, selected, joined)):
        raise ValueError('Invalid joined BOT observation')
    at = observed['snapshot_us']
    if selected.get('snapshot_us') != at or joined.get('snapshot_us') != at:
        raise ValueError('Conflicting BOT parent snapshots')
    group = groups.get(at)
    if group is None or set(group['command']) != {0}:
        raise ValueError('Missing bound BOT command')
    header = group['command'][0]
    if not joined['commands']:
        zero_keys = set(header) - {'version', 'snapshot_us', 'final'}
        if (require_work or selected['worst'] is not None or
                any(header[k] for k in zero_keys) or group['phase'] or group['service']):
            raise ValueError('Absent BOT command or unmarked details')
        return dict(status='VALID', snapshot_us=at, command=header, phases=[],
                    service=None, csw=None, span_us=0, phase_us=0,
                    checkpoint_us=0, residual_us=0,
                    scope='Explicit no-content BOT command observation')
    if (header['valid'], header['complete'], header['ok']) != (1, 1, 1):
        raise ValueError('Invalid/incomplete/failed BOT command')
    current = selected['worst']
    selection = selected['selection']
    identity = (selection['pid'], selection['id'], current['identity'])
    if ((header['pid'], header['id'], header['identity']) != identity or
            header['role'] not in range(7)):
        raise ValueError('BOT command not bound to selected content')
    worsts = [r['worst'] for r in joined['roles'] if r['worst'] is not None]
    matches = [w for w in worsts
               if (w['role'], w['start_us'], w['end_us']) ==
               (header['role'], header['csw_start_us'], header['csw_end_us'])]
    if len(matches) != 1:
        raise ValueError('BOT command lacks identical retained role CSW')
    csw = matches[0]
    csw_us = csw['end_us'] - csw['start_us']
    maximum = max(w['end_us'] - w['start_us'] for w in worsts)
    if csw_us != maximum:
        raise ValueError('BOT command is not content maximum CSW')
    earliest = min(w['start_us'] for w in worsts
                   if w['end_us'] - w['start_us'] == maximum)
    if csw['start_us'] != earliest:
        raise ValueError('BOT command is not first maximum CSW')
    if not (0 < current['start_us'] <= header['start_us'] <= header['csw_start_us'] <=
            header['csw_end_us'] <= header['end_us'] <= current['end_us']):
        raise ValueError('BOT command clocks outside selected content')
    workers = [w for w in observed['workers'] if w['pid'] == selection['pid']]
    if len(workers) != 1 or workers[0]['worst']['start_us'] != selection['id']:
        raise ValueError('Missing selected BOT parent sync')
    parent = workers[0]['phases']['files']
    if set(group['phase']) != {0, 1, 2} or set(group['service']) != {0}:
        raise ValueError('Missing/extra BOT phase or service rows')
    bind_keys = _COMMON.split()[1:]
    for row in list(group['phase'].values()) + list(group['service'].values()):
        if any(row[k] != header[k] for k in bind_keys):
            raise ValueError('Unbound BOT phase/service row')
    phases = [group['phase'][i] for i in range(3)]
    byte_count = csw['command_bytes']
    expected = [(1, 31), ((byte_count + 4095) // 4096, byte_count), (1, 13)]
    phase_us = 0
    previous_end = header['start_us']
    for phase, row in enumerate(phases):
        calls, size = expected[phase]
        if (row['calls'], row['requested'], row['actual']) != (calls, size, size):
            raise ValueError('BOT phase segmentation/bytes differ from command')
        if not calls:
            if any(row[k] for k in ('us', 'first_us', 'last_us')):
                raise ValueError('Time on empty BOT phase')
            continue
        if not (previous_end <= row['first_us'] <= row['last_us'] <= header['end_us']):
            raise ValueError('Unordered BOT phase clocks')
        phase_span = row['last_us'] - row['first_us']
        if row['us'] > phase_span or (calls == 1 and row['us'] != phase_span):
            raise ValueError('BOT phase wall inconsistent with its span')
        legacy = parent['usb'][phase]
        if row['calls'] > legacy['calls'] or row['us'] > legacy['callback_us']:
            raise ValueError('BOT phase exceeds enclosing sync callbacks')
        phase_us += row['us']
        previous_end = row['last_us']
    if phases[0]['first_us'] != header['start_us']:
        raise ValueError('BOT command does not start at its CBW')
    if ((phases[2]['first_us'], phases[2]['last_us'], phases[2]['us']) !=
            (header['csw_start_us'], header['csw_end_us'], csw_us)):
        raise ValueError('BOT status phase differs from retained CSW')
    checkpoint_calls = max(1, expected[1][0])
    if header['checkpoint_calls'] != checkpoint_calls:
        raise ValueError('BOT checkpoints differ from original command sequence')
    if not (header['start_us'] <= header['checkpoint_first_us'] <=
            header['checkpoint_last_us'] == header['end_us'] and
            header['checkpoint_last_us'] >= header['csw_end_us']):
        raise ValueError('Invalid BOT checkpoint clocks')
    if checkpoint_calls == 1 and header['checkpoint_first_us'] < header['csw_end_us']:
        raise ValueError('BOT final checkpoint precedes CSW')
    if checkpoint_calls > 1 and not (phases[1]['first_us'] <=
                                     header['checkpoint_first_us'] <= phases[1]['last_us']):
        raise ValueError('BOT inter-data checkpoint outside data span')
    checkpoint_span = header['checkpoint_last_us'] - header['checkpoint_first_us']
    if (header['checkpoint_us'] > checkpoint_span or
            (checkpoint_calls == 1 and header['checkpoint_us'] != checkpoint_span)):
        raise ValueError('BOT checkpoint wall inconsistent with its span')
    checkpoints = parent['command']
    if (header['checkpoint_calls'] > checkpoints['checkpoint_calls'] or
            header['checkpoint_us'] > checkpoints['checkpoint_us']):
        raise ValueError('BOT checkpoints exceed enclosing sync')
    span = header['end_us'] - header['start_us']
    if phase_us + header['checkpoint_us'] > span:
        raise ValueError('BOT callbacks/checkpoints exceed whole command')
    service = group['service'][0]
    empty, events = service['empty_checks'], service['event_checks']
    last_empty = service['last_empty_sample_us']
    if not events or empty + events != csw['service_checks']:
        raise ValueError('BOT ring classification differs from CSW checks')
    if (last_empty and (not empty or not csw['submitted'] <= last_empty <= csw['observed'])):
        raise ValueError('Invalid BOT sampled empty-ring clock')
    return dict(status='VALID', snapshot_us=at, command=header, phases=phases,
                service=service, csw=csw, span_us=span, phase_us=phase_us,
                checkpoint_us=header['checkpoint_us'],
                residual_us=span - phase_us - header['checkpoint_us'],
                scope='Same selected-content maximum-CSW command callback/checkpoint wall and sampled ring results; not hardware/device completion latency or CPU time')
