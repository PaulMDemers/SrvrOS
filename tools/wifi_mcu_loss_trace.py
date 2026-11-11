"""Decode calibration-only MCU loss observations; no hardware acceptance.

Buffer fields describe two CPU reads after the original bulk returned and after
an optional observation window. They do not timestamp a wire/DMA arrival or
prove transfer completion. A late reply never promotes the original failure.
"""
import re

SUMMARY = 'wifi-mcu-loss-summary'
BUFFER = 'wifi-mcu-loss-buffer'
FIELDS = {
    SUMMARY: ('version sequence step published original_reason original_flags consumed '
              'consumed_exact dispatched dispatched_exact claimed overflow late_attempted '
              'late_completed late_reason elapsed_us final_flags original_result').split(),
    BUFFER: ('version sequence phase valid nonzero stable reply_match reply_seq reply_event '
             'reply_length consumer cycle available next_parameter next_status next_control').split(),
}
HEX = {'next_parameter', 'next_status', 'next_control'}
U64 = {'sequence', 'elapsed_us', 'next_parameter'}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def decode(line, prefix):
    require(len(line) < 1536, 'oversized MCU loss row')
    fields = FIELDS[prefix]
    pattern = prefix + ': ' + ' '.join(k + ('=(0x[0-9a-f]{16})' if k in HEX else
                                           '=(-1|0|[1-9][0-9]*)' if k == 'original_result'
                                           else '=(0|[1-9][0-9]*)') for k in fields)
    match = re.fullmatch(pattern, line)
    require(match is not None, 'invalid MCU loss schema/order')
    r = {k: int(v, 16 if k in HEX else 10) for k,v in zip(fields, match.groups())}
    require(r['version'] == 1, 'unsupported MCU loss version')
    require(all(-1 <= v < (1 << (64 if k in U64 else 32)) for k,v in r.items()), 'MCU loss overflow')
    booleans = ('published','overflow','late_attempted','late_completed') if prefix == SUMMARY else ('valid','stable','reply_match','cycle','available')
    require(all(r[k] in (0,1) for k in booleans), 'invalid MCU loss Boolean')
    if prefix == SUMMARY:
        require(r['step'] < 5 and r['original_reason'] <= 6 and r['late_reason'] <= 5,
                'invalid MCU loss reason/step')
        require(-1 <= r['original_result'] <= 1024, 'invalid MCU bulk result')
        require(r['consumed_exact'] <= r['consumed'] and
                r['claimed'] <= r['dispatched_exact'] <= r['dispatched'], 'invalid MCU stage counts')
        if r['late_attempted']:
            require(r['published'] and r['original_reason'] == 4 and r['original_flags'] == 29
                    and r['original_result'] == -1 and r['late_reason'] != 0,
                    'late observation lacks original quarantined timeout')
            require((r['late_reason'] == 1) == bool(r['late_completed']), 'late result contradiction')
        else:
            require(not (r['late_completed'] or r['late_reason'] or r['elapsed_us']), 'unattempted late observation has data')
    else:
        require(r['phase'] in (0,1) and r['nonzero'] <= 16 and r['reply_seq'] < 16 and
                r['reply_event'] < 16 and r['reply_length'] < 16384 and r['consumer'] < 64,
                'invalid MCU buffer fields')
        require(not r['reply_match'] or (r['valid'] and r['stable'] and r['nonzero']),
                'reply classification lacks stable snapshot')
        if not r['valid']:
            require(not any(v for k,v in r.items() if k not in ('version','sequence','phase')), 'invalid snapshot contains data')
    return r


def observe(log, required=False):
    rows = []
    for line in log.splitlines():
        if not line.lstrip().startswith('wifi-mcu-loss-'):
            continue
        prefix = line.split(':',1)[0]
        require(prefix in FIELDS, 'unknown MCU loss row')
        rows.append((prefix,decode(line,prefix)))
    require(not required or rows, 'missing MCU loss trace')
    require(len(rows) % 3 == 0, 'incomplete MCU loss group')
    groups = []
    for i in range(0,len(rows),3):
        selected=rows[i:i+3]
        require([p for p,_ in selected] == [SUMMARY,BUFFER,BUFFER], 'MCU loss group order')
        a,b,c=[r for _,r in selected]
        require(a['sequence']==b['sequence']==c['sequence'] and b['phase']==0 and c['phase']==1,
                'MCU loss group binding')
        if a['late_attempted']:
            require(b['valid'], 'late observation lacks initial pins')
        groups.append(dict(summary=a,original=b,final=c))
    return dict(groups=groups,native_accepted=False,hardware_event_absence_proven=False,
                dma_retirement_proven=False,recovery_authorized=False)
