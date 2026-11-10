"""Strict optional MCU companion observations; no ownership/retirement authority.

These are later CPU/RAM snapshots and copies from dispatch_transfer_event,
not hardware-arrival enumeration. Damaged descriptor contents remain raw data.
"""
import re

RING = 'wifi-mcu-ring-observation'
SUMMARY = 'wifi-mcu-event-summary'
SAMPLE = 'wifi-mcu-event-sample'
SCHEMAS = {
    RING: ('version sequence valid index producer_index producer_cycle '
           'expected_parameter expected_cycle submitted_parameter '
           'submitted_status submitted_control link_parameter link_status '
           'link_control').split(),
    SUMMARY: ('version sequence valid slot seen transfers relevant exact '
              'samples dropped overflow').split(),
    SAMPLE: 'version sequence index parameter status control'.split(),
}
HEX = frozenset(('expected_parameter', 'submitted_parameter', 'submitted_status',
                 'submitted_control', 'link_parameter', 'link_status',
                 'link_control', 'parameter', 'status', 'control'))
U64 = frozenset(('sequence', 'expected_parameter', 'submitted_parameter',
                 'link_parameter', 'parameter'))
PATTERNS = {prefix: re.compile(prefix + ': ' + ' '.join(
    key + (r'=(0x[0-9a-f]{16})' if key in HEX else r'=(0|[1-9][0-9]*)')
    for key in keys)) for prefix, keys in SCHEMAS.items()}


def require(ok, why):
    if not ok:
        raise ValueError(why)


def decode(line, prefix):
    require(len(line) <= 1536, 'oversized MCU diagnostic companion')
    match = PATTERNS[prefix].fullmatch(line)
    require(match is not None, 'wrong MCU diagnostic schema/order/numbers')
    fields = {key: int(value, 16 if key in HEX else 10)
              for key, value in zip(SCHEMAS[prefix], match.groups())}
    require(all(value < (1 << (64 if key in U64 else 32))
                for key, value in fields.items()), 'MCU diagnostic overflow')
    require(fields['version'] == 1, 'unknown MCU diagnostic version')
    for key in ('valid', 'producer_cycle', 'expected_cycle', 'overflow'):
        if key in fields:
            require(fields[key] in (0, 1), 'invalid MCU diagnostic Boolean')
    return fields


def observe_details(log, bulk_rows):
    """Return optional details, refusing partial or incorrectly bound companions.

    A valid snapshot is populated, not necessarily safe to execute. Do not reject
    corrupt Normal/Link type, cycle, status or address as an invalid diagnostic.
    """
    selected = []
    lines = log.splitlines()
    for position, line in enumerate(lines):
        for prefix in SCHEMAS:
            if line.lstrip().startswith(prefix):
                selected.append((position, prefix, decode(line, prefix)))
                break
        else:
            require(not line.lstrip().startswith(('wifi-mcu-ring-',
                                                   'wifi-mcu-event-')),
                    'unknown MCU diagnostic companion family')
    if not selected:
        return None
    require(len(bulk_rows) == 1, 'MCU companions lack one bulk observation')
    anchor = bulk_rows[0]['fields']
    require(anchor['sequence'] > 0, 'MCU companions lack recorded bulk')
    require(2 <= len(selected) <= 6, 'wrong MCU companion count')
    require([x[1] for x in selected] ==
            [RING, SUMMARY] + [SAMPLE] * (len(selected) - 2),
            'missing/duplicate/out-of-order MCU companions')
    bulk_positions = [i for i, line in enumerate(lines)
                      if line.startswith('wifi-mcu-bulk-observation:')]
    require(len(bulk_positions) == 1 and bulk_positions[0] < selected[0][0],
            'MCU companions precede bulk anchor')
    require(all(row['sequence'] == anchor['sequence'] for _, _, row in selected),
            'MCU companion sequence differs from bulk')
    ring, events = selected[0][2], selected[1][2]
    samples = [row for _, _, row in selected[2:]]
    if not ring['valid']:
        require(not any(value for key, value in ring.items()
                        if key not in ('version', 'sequence', 'valid')),
                'unrecorded MCU ring snapshot contains values')
    else:
        require(anchor['reason'] in (3, 4, 5, 6),
                'MCU ring snapshot claims unsubmitted/success failure')
        offset = anchor['trb'] - anchor['ring']
        require(anchor['ring'] > 0 and anchor['ring'] % 4096 == 0 and
                offset >= 0 and offset % 16 == 0 and
                offset < 63 * 16 and ring['index'] == offset // 16 and
                ring['producer_index'] < 63,
                'MCU ring snapshot outside captured transfer geometry')
    if not events['valid']:
        require(not any(value for key, value in events.items()
                        if key not in ('version', 'sequence', 'valid')),
                'unrecorded MCU event observation contains values')
    else:
        require(anchor['reason'] in (2, 3, 4, 5, 6) and
                1 <= events['slot'] <= 255,
                'MCU event observation lacks admitted bulk/slot')
        require(events['exact'] <= events['relevant'] <= events['transfers'] <=
                events['seen'], 'inconsistent MCU dispatch subset counts')
        require(events['samples'] <= 4 and
                events['samples'] == min(4, events['relevant']) and
                events['dropped'] <= events['relevant'],
                'invalid bounded MCU event counts')
        if events['overflow']:
            require(any(events[key] == (1 << 32) - 1 for key in
                        ('seen', 'transfers', 'relevant', 'exact', 'dropped')),
                    'MCU overflow flag lacks saturated counter')
        else:
            require(events['relevant'] == events['samples'] + events['dropped'],
                    'MCU relevant events not accounted for')
    require(len(samples) == events['samples'] and
            [s['index'] for s in samples] == list(range(len(samples))),
            'missing/duplicate/out-of-order MCU event samples')
    exact_samples = 0
    for sample in samples:
        control, pointer = sample['control'], sample['parameter']
        slot, dci = control >> 24, (control >> 16) & 31
        require(((control >> 10) & 63) == 32,
                'MCU sample is not a transfer event')
        require((slot == events['slot'] and dci in (11, 16)) or
                pointer == anchor['trb'] or
                (anchor['ring'] > 0 and
                 anchor['ring'] <= pointer < anchor['ring'] + 4096),
                'MCU sample outside diagnostic relevance rule')
        exact_samples += (slot == events['slot'] and dci == anchor['dci'] and
                          pointer == anchor['trb'])
    require(exact_samples <= events['exact'],
            'MCU exact sample count exceeds dispatch count')
    if events['valid'] and not events['dropped']:
        require(exact_samples == events['exact'],
                'MCU exact dispatch count differs from complete samples')
    return dict(ring=ring, events=events, samples=samples,
                native_accepted=False, ownership_proven=False,
                dma_retirement_proven=False, hardware_event_absence_proven=False,
                scope='Post-branch descriptor RAM and bounded copies from '
                      'dispatch_transfer_event only; valid means populated, '
                      'not admissible. No payload bytes, hardware-arrival '
                      'enumeration, retry, recovery or retirement authority.')
