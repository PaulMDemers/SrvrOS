"""Read retained desktop operation snapshots, including failed/incomplete runs."""
import re

def operation_snapshots(logs, require_final=False):
    records={}
    for raw in logs:
        for line in raw.decode(errors='replace').splitlines():
            if not line.startswith(('desktop-operations:','desktop-capture-state:','desktop-capture-phases:')):continue
            fields=dict(re.findall(r'(\w+)=([^ ]+)',line))
            try:
                values={k:(v if k=='kind' else int(v)) for k,v in fields.items()}
                at=values.get('at_us',values.get('snapshot_us'))
                if at is None:continue
                group=records.setdefault(at,{'at_us':at,'captures':{},'phases':{}})
                if line.startswith('desktop-operations:'):group['operations']=values
                elif line.startswith('desktop-capture-state:'):group['captures'][values['kind']]=values
                else:group['phases'][values['kind']]={p:dict(id=values['id'],final=values['final'],elapsed_us=values[f'p{p}_us']) for p in range(1,13)}
            except (ValueError,KeyError):continue # Partial serial lines are not evidence.
    complete=[]
    for at,group in sorted(records.items()):
        op=group.get('operations')
        if not op or op.get('version')!=1 or (require_final and op.get('final')!=1):continue
        valid=True
        for kind in ('current','slowest'):
            c=group['captures'].get(kind)
            if not c or c.get('version')!=1 or c.get('final')!=op['final']:valid=False;break
            if c['sent']>c['total'] or c['received']>32 or c['phase'] not in range(13):raise ValueError('Invalid capture counters')
            phases=group['phases'].get(kind,{})
            if c['id'] and (set(phases)!=set(range(1,13)) or any(p['id']!=c['id'] or p['final']!=op['final'] for p in phases.values())):valid=False;break
            if any(p['elapsed_us']<0 for p in phases.values()):raise ValueError('Invalid phase duration')
        if valid:complete.append(group)
    if not complete:raise ValueError('Missing complete final operation snapshot' if require_final else 'Missing complete operation snapshot')
    return complete[-1]
