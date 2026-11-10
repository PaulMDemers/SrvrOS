"""Decode deferred MCU bulk failure observations without granting authority.

A reason names the branch recorded by wifi_mcu_bulk, not the physical cause of
failure. Snapshot flags and endpoint RAM are read later; a late IRQ can change
pending/done between the boundary decision and the snapshot. In particular an
elapsed-limit reason does not prove that no completion arrived. SUCCESS can be
the last bulk result when a higher-level calibration reply/owner check fails.

Missing rows are normal in older logs. Neither absence nor a syntactically valid
row accepts calibration, ownership, transport, recovery, DMA retirement or a
native connection. ``record_valid`` only distinguishes populated branch metadata
from NONE; it does not authenticate the origin of a row. No packet, key or
credential data is decoded here.
"""
import re
from wifi_mcu_failure_details import observe_details

PREFIX = 'wifi-mcu-bulk-observation'
KEYS = ('version cal_step first_failure sequence reason dci requested flags cc '
        'residue elapsed_us polls dispatched event_start event_start_cycle '
        'event_end event_end_cycle trb ring endpoint_valid endpoint_state '
        'endpoint_dequeue').split()
HEX_KEYS = frozenset(('trb', 'ring', 'endpoint_dequeue'))
U64_KEYS = frozenset(('sequence', 'elapsed_us')) | HEX_KEYS
BOOL_KEYS = frozenset(('first_failure', 'event_start_cycle', 'event_end_cycle',
                       'endpoint_valid'))
REASONS = ('none', 'refused', 'success', 'completion_failed', 'elapsed',
           'stalled', 'clock_reversed')
FLAG_NAMES = ('pending', 'done', 'mcu_failed', 'firmware_failed',
              'ep0_quarantined', 'removed', 'retiring', 'stopping')
INTERPRETATIONS = (
    'No bulk branch was recorded; this is zero-initialized diagnostic state.',
    'Bulk admission refused before publishing a new TD; this grants no '
    'endpoint-stop, ownership or retirement proof.',
    'The last recorded bulk used its success branch; higher-level calibration '
    'still failed. No particular reply or ownership-check cause is inferred.',
    'The bulk completion-failed branch was recorded; the physical device or '
    'firmware cause is not identified by this observation.',
    'The elapsed-limit failure branch was recorded. A late completion can be '
    'present in the later snapshot; absence of a completion is not proved.',
    'The unchanged-clock loop-limit failure branch was recorded; no physical '
    'controller or firmware cause is inferred.',
    'The clock-reversed failure branch was recorded. The unsigned elapsed '
    'value can wrap and does not establish a physical wait duration.',
)
_PATTERN = re.compile(PREFIX + ': ' + ' '.join(
    key + (r'=(0x[0-9a-f]{16})' if key in HEX_KEYS else r'=(0|[1-9][0-9]*)')
    for key in KEYS))


def _require(condition, message):
    if not condition:
        raise ValueError(message)


def _decode(line):
    _require(len(line) <= 1024, 'oversized MCU bulk observation')
    match = _PATTERN.fullmatch(line)
    _require(match is not None, 'wrong MCU bulk observation schema/order/numbers')
    fields = {key: int(value, 16 if key in HEX_KEYS else 10)
              for key, value in zip(KEYS, match.groups())}
    _require(all(value < (1 << (64 if key in U64_KEYS else 32))
                 for key, value in fields.items()),
             'MCU bulk observation integer overflow')
    _require(fields['version'] == 1, 'unknown MCU bulk observation version')
    _require(all(fields[key] in (0, 1) for key in BOOL_KEYS),
             'invalid MCU bulk observation Boolean')
    _require(fields['cal_step'] <= 4 and fields['reason'] < len(REASONS) and
             fields['dci'] <= 31 and fields['flags'] <= 255 and
             fields['cc'] <= 255 and fields['residue'] <= 0xffffff and
             fields['event_start'] < 64 and fields['event_end'] < 64 and
             fields['endpoint_state'] <= 7,
             'MCU bulk observation field out of range')
    _require(fields['dispatched'] <= fields['polls'],
             'MCU bulk dispatch count exceeds caller polls')
    _require(fields['endpoint_valid'] or
             fields['endpoint_state'] == fields['endpoint_dequeue'] == 0,
             'unrecorded MCU endpoint snapshot contains values')
    reason = fields['reason']
    if reason == 0:
        _require(not any(value for key, value in fields.items()
                         if key not in ('version', 'cal_step')),
                 'unrecorded MCU bulk observation contains values')
    else:
        _require(fields['sequence'] > 0,
                 'recorded MCU bulk observation lacks sequence')
        _require(fields['first_failure'] == (reason != 2),
                 'inconsistent MCU bulk first-failure selection')
        if reason == 1:
            # REFUSED can carry a zero/invalid requested size and a stale prior
            # completion snapshot. It has no newly published TD or endpoint read.
            _require(fields['trb'] == fields['ring'] == fields['polls'] ==
                     fields['dispatched'] == fields['elapsed_us'] ==
                     fields['endpoint_valid'] == 0,
                     'refused MCU admission claims submitted work')
        else:
            _require(fields['dci'] in (11, 16) and
                     0 < fields['requested'] <=
                     (1024 if fields['dci'] == 11 else 200),
                     'invalid admitted MCU bulk direction/size')
            if reason == 2:
                _require(fields['endpoint_valid'] == 0,
                         'successful MCU bulk claims a failure endpoint snapshot')
    # Address values, endpoint state and flag combinations remain raw diagnostic
    # data. They are not physical-pointer, event-absence or ownership admission.
    return {
        'fields': fields,
        'reason_name': REASONS[reason],
        'record_valid': reason != 0,
        'snapshot_flags': {name: bool(fields['flags'] & (1 << bit))
                           for bit, name in enumerate(FLAG_NAMES)},
        'interpretation': INTERPRETATIONS[reason],
    }


def observe(log):
    """Return zero/one decoded rows; malformed/duplicate observations refuse.

    This is an observation API, deliberately not a protocol ``check`` or an
    acceptance API. Unknown rows in this diagnostic family also refuse rather
    than disappearing as an apparently absent observation.
    """
    _require(isinstance(log, str), 'MCU bulk observation log must be text')
    rows = [line for line in log.splitlines()
            if line.lstrip().startswith(PREFIX)]
    _require(len(rows) <= 1, 'duplicate MCU bulk observations')
    result = {
        'rows': [_decode(line) for line in rows],
        'native_accepted': False,
        'full_protocol_acceptance': False,
        'ownership_proven': False,
        'dma_retirement_proven': False,
        'recovery_authorized': False,
        'scope': 'Deferred branch and later CPU-owned metadata observations '
                 'only; source/session/transport and complete native proof '
                 'are independently required.',
    }
    details = observe_details(log, result['rows'])
    if details is not None:
        result['diagnostics'] = details
    return result
