"""Strict opt-in final launch/fcntl gate observations; never admission authority."""
import re

if not __debug__:
    raise RuntimeError('evidence validation requires assertions')

REASONS=('entry','request','parent','mappings','path','atomic','stdio','resolve','source','elf','image_bound','handled','error')
ATOMIC=('ok','state','no_eligible_foreign_owner','same_image','caller','root','sibling','sibling_root')
COMMANDS={1:'GETFD',2:'SETFD',3:'GETFL',4:'SETFL',5:'GETLK',6:'SETLK',7:'SETLKW',8:'DUPFD',9:'DUPFD_CLOEXEC',10:'RESERVE_APPEND_RAM'}
SCALAR={2,4,8,9,10}
COMMON='version snapshot_us epoch'
GATE='pid thread owner_pid owner_thread owner_seq start_us end_us elapsed_us'
FIELDS={
 'summary':(COMMON+' entries decisions fcntl_calls invalid exec_waits fcntl_waits min_us '+' '.join(REASONS[1:])+' final').split(),
 'exec':(COMMON+' '+GATE+' reason request_mask mappings atomic detail final').split(),
 'fcntl':(COMMON+' '+GATE+' fd command scalar arg type flags final').split(),
}

def _row(line):
    prefix,sep,text=line.partition(': ');kind=prefix.removeprefix('launch-gate-')
    if not sep or kind not in FIELDS:raise ValueError('Malformed launch gate prefix')
    m=re.fullmatch(' '.join(k+r'=(\d+)' for k in FIELDS[kind]),text)
    if not m:raise ValueError('Malformed launch gate '+kind)
    r=dict(zip(FIELDS[kind],map(int,m.groups())))
    if any(n>=2**64 for n in r.values()):raise ValueError('Launch gate integer overflow')
    if r['version']!=1 or r['final']!=1 or not r['snapshot_us'] or not r['epoch']:raise ValueError('Unsupported/nonfinal launch gate')
    return kind,r

def launch_gate_trace(blobs,require_calls=True):
    groups={};legacy=[]
    for raw in blobs:
        seen=set()
        for binary in raw.splitlines(keepends=True):
            if binary.startswith(b'io-gate: '):legacy.append(binary.decode(errors='replace').strip())
            if not binary.startswith(b'launch-gate-'):continue
            if not binary.endswith(b'\n'):raise ValueError('Truncated launch gate line')
            line=binary.decode(errors='strict').rstrip('\r\n')
            if line in seen:raise ValueError('Duplicate launch gate in snapshot')
            seen.add(line);kind,r=_row(line);g=groups.setdefault((r['snapshot_us'],r['epoch']),{})
            if kind in g and g[kind]!=r:raise ValueError('Conflicting launch gate observation')
            g[kind]=r
    if not groups:raise ValueError('Missing launch gate observation')
    g=groups[max(groups)];h=g.get('summary')
    if h is None:raise ValueError('Missing launch gate summary')
    if h['invalid'] or h['min_us']!=100000 or h['decisions']>h['entries'] or sum(h[k] for k in REASONS[1:])!=h['decisions']:raise ValueError('Invalid launch gate counters')
    if require_calls and (not h['entries'] or not h['decisions'] or not h['fcntl_calls']):raise ValueError('Missing observed launch/fcntl calls')
    result={'status':'VALID','summary':h,'exec':None,'fcntl':None}
    for kind,call in [('exec',54),('fcntl',51)]:
        r=g.get(kind);n=h[kind+'_waits']
        if (r is not None)!=(n>0):raise ValueError('Missing/extra worst '+kind+' gate')
        if not r:continue
        if not h['entries' if kind=='exec' else 'fcntl_calls']:raise ValueError('Wait without observed call')
        if not all(r[k]>0 for k in ['pid','thread','owner_pid','owner_thread','owner_seq','start_us']):raise ValueError('Missing launch gate identity')
        if r['thread']==r['owner_thread'] or not r['start_us']<r['end_us']<=r['snapshot_us'] or r['end_us']-r['start_us']!=r['elapsed_us'] or r['elapsed_us']<h['min_us']:raise ValueError('Invalid launch gate clock/owner')
        if kind=='exec':
            code=r['reason']
            if code>10 or r['request_mask']>63 or r['atomic']>=len(ATOMIC):raise ValueError('Invalid exec reason')
            if code==1:
                if not r['request_mask'] or any(r[k] for k in ['mappings','atomic','detail']):raise ValueError('Invalid request refusal')
            elif r['request_mask']:raise ValueError('Request mask outside request refusal')
            if code==0 and any(r[k] for k in ['mappings','atomic','detail']):raise ValueError('Invalid raw-entry context')
            if code==3 and r['mappings']<=4096:raise ValueError('Invalid mapping refusal')
            if code>=4 and r['mappings']>4096:raise ValueError('Post-bound refusal exceeds mapping bound')
            if (code==5)!=(r['atomic']>0):raise ValueError('Invalid atomic refusal')
            if code==6 and ((r['detail']&255)>2 or ((r['detail']>>8)&255)>12 or r['detail']>>17):raise ValueError('Invalid stdio detail')
            if code==7 and r['detail'] not in (1,2):raise ValueError('Invalid resolution detail')
            if code==8 and r['detail'] not in (1,2,3):raise ValueError('Invalid source detail')
            if code in (0,1,3,4,5,9) and r['detail']:raise ValueError('Unexpected exec detail')
            if code==10 and not r['detail']:raise ValueError('Missing image bound detail')
            if code and not h[REASONS[code]]:raise ValueError('Worst exec reason absent from counters')
            result[kind]={'record':r,'reason':REASONS[code],'atomic_reason':'not_evaluated' if code<5 else ATOMIC[r['atomic']]}
        else:
            if r['scalar'] not in (0,1) or bool(r['scalar'])!=(r['command'] in SCALAR) or (not r['scalar'] and r['arg']) or r['type']>12:raise ValueError('Invalid/unredacted fcntl context')
            result[kind]={'record':r,'command':COMMANDS.get(r['command'],'UNKNOWN'),'descriptor_scope':'Pre-gate scalar snapshot only; descriptor may change while waiting'}
        # Old top-eight waits can evict this independent per-operation worst.
        # When the exact old record survives, require the complete same binding.
        for line in legacy:
            pairs=dict(re.findall(r'(\w+)=(\d+)',line))
            if any(int(pairs.get(k,-1))!=r[v] for k,v in [('pid','pid'),('thread','thread'),('start_us','start_us'),('end_us','end_us')]):continue
            if any(int(pairs.get(k,-1))!=v for k,v in [('syscall',call),('initial_owner_pid',r['owner_pid']),('initial_owner_thread',r['owner_thread']),('initial_owner_seq',r['owner_seq']),('elapsed_us',r['elapsed_us'])]):raise ValueError('Crossed legacy gate binding')
    result['reproduced_exec_wait']=result['exec'] is not None
    result['reproduced_fcntl_wait']=result['fcntl'] is not None
    result['scope']='Validated raw-entry decisions and two independent >=100ms worst waits; initial owner is a correlation anchor, not a claim of uninterrupted ownership or CPU/device time'
    return result
