"""Post-reset BCS ready-request/child lifetime; no memory/completion grant."""
import re
FIELDS='v origin owner_origin begin end epoch attempts writes reads polls acks samples child_borrows child_returns child_started child_completed child_retained restored uncertain stage reason clock_at clock_prev live_checks live_reads live_failed complete obligations assurances pte_writes managed_pages hardware_ownership'.split()
def rehold_summary(line):
    if not line.startswith('intel-rehold-summary: '):raise ValueError('rehold prefix')
    s={}
    for token in line.split(': ',1)[1].split():
        k,sep,v=token.partition('=')
        if not sep or k in s or not re.fullmatch(r'0|[1-9][0-9]{0,19}',v) or int(v)>=1<<64:raise ValueError('rehold field encoding')
        s[k]=int(v)
    if set(s)!=set(FIELDS) or s['v']!=1 or s['epoch']>=1<<32:raise ValueError('rehold schema')
    if any(s[k]>1 for k in ('child_started','child_completed','child_retained','restored','uncertain','live_failed','complete')):raise ValueError('rehold boolean')
    if not 0<=s['acks']<=s['writes']<=s['attempts']<=2 or s['polls']>512 or s['samples']>4 or s['stage']>5 or s['reason']>511:raise ValueError('rehold progress')
    if not 0<=s['child_returns']<=s['child_borrows']<=1 or s['child_retained']>s['child_borrows']-s['child_returns'] or s['child_completed']>s['child_started']:raise ValueError('rehold child')
    if s['live_checks']>19 or s['live_reads']>95 or not s['live_checks'] and s['live_reads']:raise ValueError('rehold live bounds')
    if s['obligations']!=255 or any(s[k] for k in ('assurances','pte_writes','managed_pages','hardware_ownership')):raise ValueError('rehold unsupported authority')
    if s['begin'] and not 0<s['origin']<=s['begin']<=s['end']<=s['origin']+100000:raise ValueError('rehold shared deadline')
    if s['begin'] and not 0<s['owner_origin']<=s['origin']<=s['begin']<=s['end']<=s['owner_origin']+300000:raise ValueError('rehold parent deadline')
    if (s['clock_at'] or s['clock_prev']) and not s['reason']&2:raise ValueError('rehold unexplained clock')
    if s['complete'] and (s['reason'] or s['uncertain'] or s['child_retained'] or s['live_failed'] or s['stage']!=5 or not s['restored'] or not s['child_completed'] or not s['child_started'] or s['child_borrows']!=1 or s['child_returns']!=1 or s['samples']!=4 or s['attempts']!=2 or s['writes']!=2 or s['acks']!=2 or s['live_checks']!=19 or s['live_reads']!=95 or not s['begin'] or s['end']<=s['begin']):raise ValueError('rehold false completion')
    return s
def rehold_capture(text):
    rows=[];acks=[];sums=[];order=[]
    for line in text.splitlines():
        if line.startswith('intel-rehold-row:'):
            m=re.fullmatch(r'intel-rehold-row: v=1 s=([0-4]) n=([0-6]) h=(-|[0-9a-f]{8}(?:[0-9a-f]{8}){0,5})',line)
            if not m:raise ValueError('rehold raw encoding')
            words=[] if m[3]=='-' else [int(m[3][i:i+8],16) for i in range(0,len(m[3]),8)]
            if len(words)!=int(m[2]) or int(m[1])==4 and len(words)>5:raise ValueError('rehold raw count')
            rows.append(dict(sample=int(m[1]),words=words));order.append(('r',int(m[1])))
        elif line.startswith('intel-rehold-ack:'):
            m=re.fullmatch(r'intel-rehold-ack: v=1 n=([01]) d=([0-9]+),([0-9]+),([0-9]+) begin=([0-9]+) end=([0-9]+)',line)
            if not m:raise ValueError('rehold ack encoding')
            a=dict(index=int(m[1]),first=int(m[2]),final=int(m[3]),polls=int(m[4]),begin=int(m[5]),end=int(m[6]))
            if max(a['first'],a['final'])>=1<<32 or a['polls']>256 or max(a['begin'],a['end'])>=1<<64:raise ValueError('rehold ack width')
            acks.append(a);order.append(('a',a['index']))
        elif line.startswith('intel-rehold-summary:'):sums.append(rehold_summary(line));order.append(('s',))
        elif line.startswith('intel-rehold-'):raise ValueError('rehold unknown record')
    if not rows and not acks and not sums:return None
    if len(sums)!=1:raise ValueError('rehold summary coverage')
    s=sums[0]
    if order!=[('r',i) for i in range(5)]+[('a',i) for i in range(s['writes'])]+[('s',)]:raise ValueError('rehold receipt order/coverage')
    counts=[len(r['words']) for r in rows[:4]]
    if sum(n==6 for n in counts)!=s['samples'] or sum(counts)+s['writes']+s['polls']!=s['reads'] or sum(a['polls'] for a in acks)!=s['polls']:raise ValueError('rehold total reads')
    if any(counts[i]<6 and any(counts[i+1:]) for i in range(4)):raise ValueError('rehold read past failed prefix')
    last=len(rows[4]['words'])
    if s['live_reads']!=((s['live_checks']-1)*5+last if s['live_checks'] else 0) or not s['live_checks'] and last:raise ValueError('rehold live raw coverage')
    for n,row in enumerate(rows):
        for i,v in enumerate(row['words']):
            want=3 if i<4 or n in (1,2,4) else 0
            bad=v!=0 if i==5 else v==0xffffffff or bool(v&0xfffc) or v&3!=want
            if n in (1,2,3) and i<5 and counts[0]==6:
                bad|=(v&~3)!=(rows[0]['words'][i]&~3)
            if bad and (not s['live_failed'] if n==4 else not s['reason']&64):raise ValueError('rehold unexplained raw state')
    for a in acks:
        if not s['begin']<=a['begin']<=a['end']<=s['end']:raise ValueError('rehold ack clock')
        if a['end']-a['begin']>10000 and not s['reason']&128:raise ValueError('rehold phase deadline')
    if s['complete']:
        if counts!=[6]*4 or rows[0]['words']!=rows[3]['words'] or rows[1]['words']!=rows[2]['words'] or rows[4]['words']!=rows[1]['words'][:5]:raise ValueError('rehold preservation')
        for a in acks:
            want=3 if a['index']==0 else 0
            if a['final']&0xffff!=want or a['first']==0xffffffff or a['first']&0xfffc or a['first']&1!=(1-a['index']):raise ValueError('rehold ack state')
            if (a['first']&~3)!=(rows[0]['words'][4]&~3) or (a['final']&~3)!=(rows[0]['words'][4]&~3):raise ValueError('rehold ack changed mask')
            if not a['begin']<a['end']<=a['begin']+10000:raise ValueError('rehold ack completion')
        if acks[0]['end']>acks[1]['begin']:raise ValueError('rehold phase ordering')
    return dict(summary=s,rows=rows,acks=acks,complete=bool(s['complete']),native_complete=False,hardware_ownership=False,managed_pages=0,physical_tearfree=False)
def rehold_result(text):
    r=rehold_capture(text)
    if not r:return None
    from intel_held_interval import interval_result
    from intel_gt_wake import wake_result
    interval=interval_result(text);wake=wake_result(text)
    if not interval or not wake or wake['summary']['v']!=19:raise ValueError('rehold wrong image/prerequisites')
    s=r['summary'];x=interval['reset']['summary'];p=interval['reset']['prepare']['summary'];i=interval['summary']
    if s['begin']:
        if s['origin']!=x['origin'] or s['epoch']!=x['epoch_after'] or not x['settle_end']<=s['begin']<=s['end'] or s['end']>x['end'] and not x['reason']&2:raise ValueError('rehold original source binding')
    if i['begin']:
        if not s['child_started'] or len(r['acks'])<1 or r['acks'][0]['final']&3!=3 or not r['acks'][0]['end']<=i['begin']<=i['end']<=s['end']:raise ValueError('rehold child ordering')
        if len(r['acks'])>1 and i['end']>r['acks'][1]['begin']:raise ValueError('rehold cancelled before child')
    if s['child_returns'] and not i['complete']:raise ValueError('rehold released failed child')
    if s['child_borrows'] and not s['child_started']:raise ValueError('rehold child before hold')
    if not text.index('intel-held-reset-summary:')<text.index('intel-rehold-row:')<text.index('intel-rehold-summary:')<text.index('intel-held-interval-row:'):raise ValueError('rehold report order')
    complete=bool(r['complete'] and interval['complete'])
    if wake['summary']['complete'] and not complete:raise ValueError('rehold false enclosing completion')
    if s['begin'] and not r['complete'] and (not p['retained'] or p['returned'] or not p['uncertain'] or wake['summary']['released']):raise ValueError('rehold failure lost original pin')
    return dict(r,scope_complete=r['complete'],complete=complete,interval=interval)
def native_rehold_result(text):
    r=rehold_result(text)
    if not r or not r['complete']:return r
    from intel_gt_wake import native_wake_result
    w=native_wake_result(text)
    if not w or not w['native_wake_complete']:raise ValueError('rehold native prerequisite coverage')
    r['native_complete']=True
    return r
def verify_rehold_checkpoint(pre,text,native=False):
    from intel_held_interval import verify_interval_checkpoint
    if 'intel-rehold-' in pre:raise ValueError('rehold late checkpoint')
    return verify_interval_checkpoint(pre,text,native=native)
