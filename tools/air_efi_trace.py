"""Diagnostic receipts outside the immutable release on the unit's ESP."""
import re
import secrets
import struct
import zlib

GRUB = 'AIRBOOT.ENV'
PROBE = 'AIRBOOT.LOG'
HEADER = b'SRVR-AIR-EFI-V1 '
STAGES = ('entered', 'capture', 'locate', 'allocate', 'install')


def seed(release):
    if not re.fullmatch('[0-9a-f]{16}', release):
        raise ValueError('Invalid EFI trace release')
    nonce = secrets.token_hex(8)
    header = HEADER + f'{release} {nonce}\n'.encode()
    env = f'# GRUB Environment Block\nair_release={release}\nair_nonce={nonce}\nair_state=prepared\n'
    return {GRUB: env.encode().ljust(1024, b'#'), PROBE: header.ljust(512, b'\0')}


def decode(env, probe, release, nonce=None):
    result = _decode_progress(env, probe[:512], release, nonce)
    if len(probe) > 512:
        clients = result.get('eg2_clients', {})
        if clients.get('state') != 'observed' or result.get('probe', {}).get('capture') != (2 << 32 | 6):
            raise ValueError('Missing completed capture for EFI GOP routes')
        result['gop_routes'] = _decode_routes(probe[512:], clients['children'])
    return result


def _instances(data):
    instances, nodes, pos = [], [], 0
    while pos + 4 <= len(data):
        kind, subtype, size = struct.unpack_from('<BBH', data, pos)
        if size < 4 or pos + size > len(data):
            break
        node = data[pos:pos + size]
        pos += size
        if kind != 0x7f:
            nodes.append(node.hex())
            continue
        if size != 4 or subtype not in (1, 255):
            break
        instances.append(nodes)
        nodes = []
        if subtype == 255:
            if pos == len(data):
                return instances
            break
    raise ValueError('Malformed EFI device path')


def _compare(active, other):
    comparisons = []
    for i, left in enumerate(active['instances']):
        for j, right in enumerate(other['instances']):
            common = 0
            for a, b in zip(left, right):
                if a != b:
                    break
                common += 1
            if left == right:
                relation = 'exact'
            elif common == len(right):
                relation = 'other-is-ancestor'
            elif common == len(left):
                relation = 'gop-is-ancestor'
            else:
                relation = 'diverged'
            comparisons.append(dict(gop_instance=i, other_instance=j, common_nodes=common, relation=relation))
    return comparisons


def _decode_routes(data, children):
    if (len(data) < 10 or data[:4] != b'GOP1'
            or struct.unpack_from('<I', data, 4)[0] != zlib.crc32(data[8:])):
        raise ValueError('Invalid EFI GOP route checksum or header')
    active, count = data[8:10]
    if not 1 <= active <= 16 or count != children or count > 16:
        raise ValueError('Invalid EFI GOP or child count')
    roles = ['ConOut', 'console_handle'] + [f'gop{i}' for i in range(active)] + [f'child{i}' for i in range(count)]
    pos, paths, handles = 10, {}, {}
    for role in roles:
        if pos + 4 > len(data):
            raise ValueError('Missing EFI GOP route')
        handle, state, size = struct.unpack_from('<BBH', data, pos)
        pos += 4
        if (size > 4096 or pos + size > len(data) or state > 2 or (state == 0) != bool(size)
                or (role == 'ConOut' and handle) or (role != 'ConOut' and state != 1 and not handle)
                or (role.startswith('gop') and not handle) or handle > len(roles) - 1):
            raise ValueError('Invalid EFI GOP route')
        raw = data[pos:pos + size]
        pos += size
        if handle and handles.setdefault(handle, (state, raw)) != (state, raw):
            raise ValueError('Inconsistent path on the same EFI handle')
        paths[role] = dict(handle=handle or None, state=('observed', 'absent', 'unavailable')[state],
                           instances=_instances(raw) if size else [])
    if pos != len(data):
        raise ValueError('Trailing EFI GOP route data')
    others = ['ConOut', 'console_handle'] + [f'child{i}' for i in range(count)]
    comparisons = {gop: {other: _compare(paths[gop], paths[other]) for other in others}
                   for gop in paths if gop.startswith('gop')}
    return dict(paths=paths, comparisons=comparisons)


def _decode_progress(env, probe, release, nonce):
    header = re.match(rb'SRVR-AIR-EFI-V1 ([0-9a-f]{16}) ([0-9a-f]{16})\n', probe)
    if len(probe) != 512 or not header or any(probe[header.end():64]):
        raise ValueError('Invalid EFI progress file')
    tag, token = (s.decode() for s in header.groups())
    rows = re.fullmatch(rb'# GRUB Environment Block\n((?:air_(?:release|nonce|state)=[a-z0-9-]+\n){3})#+', env)
    if not rows or len(env) != 1024:
        raise ValueError('Invalid GRUB progress file')
    fields = dict(line.split('=', 1) for line in rows[1].decode().splitlines())
    if tag != release or fields.get('air_release') != tag or fields.get('air_nonce') != token or (nonce and token != nonce):
        raise ValueError('Boot progress identity mismatch')
    state = fields.get('air_state')
    if state not in ('prepared','selected','loaded','load-failed','returned','start-failed'):
        raise ValueError('Unknown GRUB progress')
    progress = {}
    for i, stage in enumerate(STAGES, 1):
        block = probe[i*64:(i+1)*64]
        if any(block):
            row = re.fullmatch(stage.encode() + rb' ([0-9a-f]{16})\n\x00*', block)
            if not row or len(progress) != i-1:
                raise ValueError('Invalid EFI progress sequence')
            progress[stage] = int(row[1], 16)
    result = dict(release=tag, nonce=token, grub=state, probe=progress)
    if any(probe[384:448]):
        if 'capture' not in progress:
            raise ValueError('Missing EFI capture for EG2 open receipt')
        for state in ('absent', 'unavailable'):
            if probe[384:448] == f'eg2-{state}\n'.encode().ljust(64, b'\0'):
                result['eg2_clients'] = dict(state=state)
                if any(probe[448:]):
                    raise ValueError('Unexpected EFI panel receipt')
                return result
        row = re.fullmatch(rb'eg2-open (\d{1,5}) (\d{1,5}) (\d{1,5}) (\d{1,5}) (\d{1,5})\n\x00*', probe[384:448])
        if not row:
            raise ValueError('Invalid EFI EG2 open receipt')
        entries, drivers, core, children, exclusive = map(int, row.groups())
        if entries > 65535 or core > drivers or drivers + children > entries or exclusive > entries:
            raise ValueError('Invalid EFI EG2 open counts')
        result['eg2_clients'] = dict(state='observed', entries=entries, drivers=drivers, core_drivers=core,
                                     children=children, exclusive=exclusive)
    if any(probe[448:]):
        clients = result.get('eg2_clients', {})
        if clients.get('state') != 'observed' or progress.get('capture') != (2 << 32 | 6):
            raise ValueError('Missing complete EFI capture for panel receipt')
        block = probe[448:]
        if block == b'panel-unavailable\n'.ljust(64, b'\0'):
            result['eg2_panel'] = dict(state='unavailable')
            return result
        row = re.fullmatch(rb'panel ([0-9a-f]{4}) ([0-9a-f]{4}) ([0-9a-f]{16}) ([01]) ([0-9a-f]{8})\n\x00*', block)
        if not row:
            raise ValueError('Invalid EFI panel receipt')
        gops, matches, path_hash, has_adr, adr = (int(v, 16) for v in row.groups())
        unique = matches.bit_count() == 1
        if (clients['children'] > 16 or gops >> clients['children'] or matches & ~gops
                or (not unique and (path_hash or has_adr)) or (adr and not has_adr)):
            raise ValueError('Inconsistent EFI panel child masks or identity')
        result['eg2_panel'] = dict(state='observed', gop_mask=gops, active_mask=matches,
                                   path_hash=f'{path_hash:016x}' if path_hash else None,
                                   acpi_adr=adr if has_adr else None)
    return result


def read(base, release, nonce=None):
    return decode((base / GRUB).read_bytes(), (base / PROBE).read_bytes(), release, nonce)
