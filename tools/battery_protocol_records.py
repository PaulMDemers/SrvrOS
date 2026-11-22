"""Strict first-upper-protocol failure records; observations never grant reuse."""
import re

SCHEMA={
 'context':'at_us if current expected lease io busy begun result ec_phase ec_error poison',
 'sample':'valid time_valid at_us previous_us max_gap_us started_us reason polls index active waiting current',
 'state':'generation completed_us error index polls active waiting bus_phase pending failed ack_attempted acknowledged',
 'bus':'valid at_us started_us previous_us max_gap_us reason phase pending status value_valid value',
 'bus-ec':'phase failed_phase error poison polls status started_us last_us timeout_us',
}
BOOLS={
 'context':'if lease io busy begun poison',
 'sample':'valid time_valid active waiting current',
 'state':'active waiting pending failed ack_attempted acknowledged',
 'bus':'valid pending value_valid',
 'bus-ec':'poison',
}
HEADER=re.compile(r'battery-protocol: version=1 snapshot=(retired|fault) snapshot_us=(\d+) epoch=(\d+) captured=([01]) kind=(\d+)')
def first_protocol_record(blobs, *, epoch=1, require_clean=True):
    groups={}
    for blob in blobs:
        current=None
        for line in blob.decode(errors='replace').splitlines():
            if not line.startswith('battery-protocol'):continue
            if line.startswith('battery-protocol:'):
                m=HEADER.fullmatch(line)
                if not m:raise ValueError('Malformed protocol header')
                name,at,e,c,k=m.groups()
                header=dict(snapshot=name,snapshot_us=int(at),epoch=int(e),captured=int(c),kind=int(k))
                if any(v>=2**64 for v in header.values() if isinstance(v,int)) or not header['snapshot_us']:
                    raise ValueError('Invalid protocol header clocks')
                key=(header['epoch'],header['snapshot_us'])
                current=groups.setdefault(key,dict(header=header,rows={}))
                if current['header']!=header:raise ValueError('Conflicting protocol header')
                continue
            prefix,sep,rest=line.partition(': ')
            key=prefix.removeprefix('battery-protocol-')
            if not sep or key not in SCHEMA or current is None:raise ValueError('Unbound/malformed protocol body')
            fields=SCHEMA[key].split()
            pattern=' '.join(f+r'=(-?\d+)' if f=='result' else f+r'=(\d+)' for f in fields)
            m=re.fullmatch(pattern,rest)
            if not m:raise ValueError('Malformed protocol '+key)
            row=dict(zip(fields,map(int,m.groups())))
            if any(v<0 or v>=2**64 for k,v in row.items() if k!='result'):raise ValueError('Overflowing protocol value')
            if 'result' in row and row['result'] not in (-1,0,1):raise ValueError('Invalid protocol result')
            if any(row[f] not in (0,1) for f in BOOLS[key].split()):raise ValueError('Invalid protocol flags')
            if key in current['rows'] and current['rows'][key]!=row:raise ValueError('Conflicting protocol body')
            current['rows'][key]=row
    selected=[g for (e,_),g in groups.items() if e==epoch]
    if not selected:raise ValueError('Missing bound protocol evidence')
    for group in selected:
        h=group['header'];rows=group['rows']
        if not h['captured']:
            if h['kind'] or rows:raise ValueError('Uncaptured protocol has body/kind')
        else:
            if h['kind'] not in range(1,6) or set(rows)!=set(SCHEMA):raise ValueError('Incomplete protocol failure')
            c=rows['context'];s=rows['sample'];b=rows['bus'];ec=rows['bus-ec'];state=rows['state']
            if not 0<c['at_us']<=h['snapshot_us']:raise ValueError('Unbound first failure clock')
            if c['ec_phase']>7 or c['ec_error']>5 or ec['phase']>7 or ec['failed_phase']>7 or ec['error']>5:raise ValueError('Invalid EC enum')
            if s['valid'] and s['reason'] not in range(1,7):raise ValueError('Invalid sample failure')
            if b['valid'] and b['reason'] not in range(1,10):raise ValueError('Invalid bus failure')
            if s['index']>15 or state['index']>15 or state['error']>6 or b['phase']>7 or state['bus_phase']>8:raise ValueError('Invalid operation state')
            if any(b[f]>255 for f in ('status','value')) or ec['status']>255:raise ValueError('Invalid status byte')
            if not s['valid'] and any(s.values()):raise ValueError('Unmarked sample data')
            if not b['valid'] and (any(b.values()) or any(ec.values())):raise ValueError('Unmarked bus data')
    finals=[g for g in selected if g['header']['snapshot']=='retired']
    if not finals:raise ValueError('Missing clean-retirement protocol snapshot')
    last=max(finals,key=lambda g:g['header']['snapshot_us'])
    if require_clean and any(g['header']['captured'] or g['header']['snapshot']=='fault' for g in selected):
        raise ValueError('Protocol failure observed')
    return dict(status='OBSERVED_FAILURE' if last['header']['captured'] else 'VALID_CLEAN',**last['header'],**last['rows'])
