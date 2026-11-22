"""Strict command-role joins for the selected final sync/content observation.

Counts and CSW callback timestamps are software observations. They do not
identify device completion latency or the process that wrote the content.
"""
import re
from sync_timing import sync_timing
from sync_content import sync_content

_FIELDS = {
    'snapshot': 'version snapshot_us pid id identity valid active invalid final',
    'role': 'version snapshot_us pid id identity role reads read_bytes writes write_bytes flushes other failed csws csw_us over100ms write512_commands write_csw_us write512_csw_us write_over100ms write512_over100ms final',
    'worst': 'version snapshot_us pid id identity role start_us end_us opcode command_bytes request_bytes actual ok submitted observed resumed seen service_valid service_checks service_samples service_gap_max_us completion_gap_us final',
}
_U64 = 1 << 64
_ROLES = 7
_COMMAND_BYTES = 16384


def _row(line):
    name, separator, text = line.partition(': ')
    kind = name.removeprefix('sync-command-')
    if not separator or kind not in _FIELDS:
        raise ValueError('Malformed sync command prefix')
    fields = _FIELDS[kind].split()
    match = re.fullmatch(' '.join(k + r'=([0-9]+)' for k in fields), text)
    if not match:
        raise ValueError('Malformed sync command ' + kind)
    row = dict(zip(fields, map(int, match.groups())))
    if any(v >= _U64 for v in row.values()):
        raise ValueError('Overflowing sync command value')
    if row['version'] != 1 or row['final'] != 1 or not row['snapshot_us']:
        raise ValueError('Unsupported/nonfinal sync command observation')
    return kind, row


def sync_commands(blobs, content=None, sync=None, require_work=True):
    """Validate fixed role aggregates and CSW maxima from the same content.

    Role6 covers unclassified/pre-mutation callbacks inside the selected content
    window. Its commands must not be compared with the six cost-active logical
    block-callback roles as if the two counted the same operations or bytes.
    """
    groups = {}
    for blob in blobs:
        for line in blob.decode(errors='replace').splitlines():
            if not line.startswith('sync-command-'):
                continue
            kind, row = _row(line)
            g = groups.setdefault(row['snapshot_us'], {'snapshot': {}, 'role': {}, 'worst': {}})
            key = 0 if kind == 'snapshot' else row['role']
            table = g[kind]
            if key in table and table[key] != row:
                raise ValueError('Conflicting sync command ' + kind)
            table[key] = row
    observed = sync_timing(blobs) if sync is None else sync
    selected = sync_content(blobs, observed, require_work=require_work) if content is None else content
    if observed.get('status') != 'VALID' or selected.get('status') != 'VALID':
        raise ValueError('Invalid joined sync/content observation')
    at = observed['snapshot_us']
    if selected.get('snapshot_us') != at:
        raise ValueError('Conflicting sync/content snapshots')
    g = groups.get(at)
    if g is None or set(g['snapshot']) != {0}:
        raise ValueError('Missing bound command snapshot')
    header = g['snapshot'][0]
    if (header['valid'], header['active'], header['invalid']) != (1, 0, 0):
        raise ValueError('Incomplete/invalid command window')
    selection = selected['selection']
    current = selected['worst']
    if current is None:
        if (require_work or (selection['pid'], selection['id']) != (0, 0) or
                (header['pid'], header['id'], header['identity']) != (0, 0, 0) or g['role'] or g['worst']):
            raise ValueError('Absent content or unmarked command details')
        return dict(status='VALID', snapshot_us=at, header=header, roles=[],
                    commands=0, csw_us=0, scope='Explicit no-content command observation')
    identity = (selection['pid'], selection['id'], current['identity'])
    if not all(identity) or (header['pid'], header['id'], header['identity']) != identity:
        raise ValueError('Command snapshot not bound to selected content')
    workers = [w for w in observed['workers'] if w['pid'] == selection['pid']]
    if len(workers) != 1 or workers[0]['worst']['start_us'] != selection['id']:
        raise ValueError('Missing/conflicting selected sync')
    parent = workers[0]
    if not (parent['worst']['start_us'] <= current['start_us'] <= current['end_us'] <= parent['worst']['files_end_us'] <= at):
        raise ValueError('Invalid joined content clock')
    if set(g['role']) != set(range(_ROLES)):
        raise ValueError('Missing/extra command roles')
    elapsed = current['end_us'] - current['start_us']
    legacy = parent['phases']['files']['usb'][2]
    total_commands = total_us = read_bytes = write_bytes = 0
    roles = []
    expected_worst = set()
    for role in range(_ROLES):
        r = g['role'][role]
        if (r['pid'], r['id'], r['identity']) != identity or r['failed']:
            raise ValueError('Unbound/failed command role')
        commands = r['reads'] + r['writes'] + r['flushes'] + r['other']
        if commands != r['csws'] or commands >= _U64 or r['over100ms'] > r['csws']:
            raise ValueError('Command/CSW count mismatch')
        for count, size in [('reads', 'read_bytes'), ('writes', 'write_bytes')]:
            if not r[count]:
                if r[size]:
                    raise ValueError('Bytes without commands')
            elif not r[count] * 512 <= r[size] <= r[count] * _COMMAND_BYTES or r[size] % 512:
                raise ValueError('Invalid read/write command byte total')
        small = r['write512_commands']; large = r['writes'] - small
        if small > r['writes'] or not 0 <= r['write512_csw_us'] <= r['write_csw_us'] <= r['csw_us']:
            raise ValueError('Invalid write command/time subsets')
        if not small * 512 + large * 1024 <= r['write_bytes'] <= small * 512 + large * _COMMAND_BYTES:
            raise ValueError('Invalid 512-byte/larger write byte totals')
        if not 0 <= r['write512_over100ms'] <= r['write_over100ms'] <= r['over100ms']:
            raise ValueError('Invalid slow-write subsets')
        classes = [(small, r['write512_csw_us'], r['write512_over100ms']),
                   (large, r['write_csw_us'] - r['write512_csw_us'], r['write_over100ms'] - r['write512_over100ms']),
                   (r['csws'] - r['writes'], r['csw_us'] - r['write_csw_us'], r['over100ms'] - r['write_over100ms'])]
        for count, clock, slow in classes:
            if slow > count or (not count and clock) or slow * 100001 > clock:
                raise ValueError('Invalid command-class time/slow count')
        total_commands += commands
        total_us += r['csw_us']
        read_bytes += r['read_bytes']; write_bytes += r['write_bytes']
        if not r['csws']:
            if r['csw_us'] or r['over100ms']:
                raise ValueError('CSW time without CSWs')
            roles.append(dict(summary=r, worst=None))
            continue
        expected_worst.add(role)
        w = g['worst'].get(role)
        if w is None or (w['pid'], w['id'], w['identity']) != identity:
            raise ValueError('Missing/unbound worst role CSW')
        if not (0 < current['start_us'] <= w['start_us'] <= w['submitted'] <= w['observed'] <=
                w['resumed'] <= w['end_us'] <= current['end_us']):
            raise ValueError('CSW outside selected content')
        maximum = w['end_us'] - w['start_us']
        if not maximum <= r['csw_us'] <= r['csws'] * maximum or maximum > legacy['max_us']:
            raise ValueError('Worst CSW inconsistent with role/legacy aggregate')
        if (bool(r['over100ms']) != (maximum > 100000) or
                r['over100ms'] * 100001 > r['csw_us']):
            raise ValueError('Invalid slow-CSW count')
        if (w['opcode'] > 255 or w['command_bytes'] > _COMMAND_BYTES or
                (w['request_bytes'], w['actual'], w['ok'], w['seen'], w['service_valid']) != (13, 13, 1, 1, 1)):
            raise ValueError('Invalid worst command/sample context')
        if w['opcode'] in (40, 42):
            count, size = ('reads', 'read_bytes') if w['opcode'] == 40 else ('writes', 'write_bytes')
            if not r[count] or not 512 <= w['command_bytes'] <= _COMMAND_BYTES or w['command_bytes'] % 512:
                raise ValueError('Unbound read/write CSW class/size')
            if w['opcode'] == 40:
                low = w['command_bytes'] + (r[count] - 1) * 512
                high = w['command_bytes'] + (r[count] - 1) * _COMMAND_BYTES
            else:
                is_small = w['command_bytes'] == 512
                if (is_small and not small) or (not is_small and not large):
                    raise ValueError('Worst write outside its size class')
                low = w['command_bytes'] + (small - int(is_small)) * 512 + (large - int(not is_small)) * 1024
                high = w['command_bytes'] + (small - int(is_small)) * 512 + (large - int(not is_small)) * _COMMAND_BYTES
                class_us = r['write512_csw_us'] if is_small else r['write_csw_us'] - r['write512_csw_us']
                if maximum > class_us:
                    raise ValueError('Worst write exceeds its class time')
            if not low <= r[size] <= high:
                raise ValueError('Worst read/write size inconsistent with aggregate')
        elif w['opcode'] == 53:
            if not r['flushes'] or w['command_bytes']:
                raise ValueError('Unbound flush CSW class/size')
        elif not r['other']:
            raise ValueError('Unbound other CSW class')
        checks = w['service_checks']; samples = w['service_samples']
        gap = w['service_gap_max_us']; observe = w['observed'] - w['submitted']; resume = w['resumed'] - w['observed']
        if (not checks or samples != (checks - 1) // 256 + 1 or
                not w['completion_gap_us'] <= gap <= observe or observe > (samples + 1) * gap):
            raise ValueError('Invalid CSW service coverage')
        if (observe > legacy['observe_max_us'] or resume > legacy['resume_max_us'] or
                checks > legacy['service_checks'] or samples > legacy['service_samples'] or
                gap > legacy['service_gap_max_us'] or w['completion_gap_us'] > legacy['completion_gap_max_us']):
            raise ValueError('Role CSW sample exceeds enclosing sync aggregates')
        roles.append(dict(summary=r, worst=w))
    if set(g['worst']) != expected_worst:
        raise ValueError('Extra/unmarked worst command rows')
    if (total_commands > legacy['calls'] or total_us > min(elapsed, legacy['callback_us']) or
            read_bytes >= _U64 or write_bytes >= _U64 or (require_work and not total_commands)):
        raise ValueError('Command totals exceed selected content/sync or contain no work')
    # Both observers retain the first strict maximum from the same callback.
    # If the legacy maximum is inside this content, one role must retain that
    # identical observation. Zero-duration boundaries can be ambiguous.
    old = parent['phases']['files']['command']
    if (old['start_us'] < old['end_us'] and
            current['start_us'] <= old['start_us'] <= old['end_us'] <= current['end_us']):
        matching = [r['worst'] for r in roles if r['worst'] and
                    (r['worst']['start_us'], r['worst']['end_us']) == (old['start_us'], old['end_us'])]
        fields = _FIELDS['worst'].split()[6:-1]
        if len(matching) != 1 or any(matching[0][key] != old[key] for key in fields):
            raise ValueError('Legacy maximum inside content lacks an identical role sample')
    return dict(status='VALID', snapshot_us=at, header=header, roles=roles,
                commands=total_commands, read_bytes=read_bytes, write_bytes=write_bytes, csw_us=total_us,
                scope='Selected content command classes and role-bound CSW callback wall; not hardware latency, CPU time or content-writer identity')
