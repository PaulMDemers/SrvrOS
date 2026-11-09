"""Read-only cold-takeover prerequisites; never a reset/range authority."""
import re
from intel_engine_census import _fields
FIELDS = 'v attempts samples reads begin_us end_us epoch batches blocked borrowed returned retained stable busy reserved geometry quiescent reason complete obligations reset_writes pte_writes managed_pages hardware_ownership'.split()
BOOL = 'blocked borrowed returned retained stable quiescent complete'.split()
def takeover_summary(line):
    s=_fields(line,'intel-takeover-summary:',FIELDS)
    if any(v>=1<<64 for v in s.values()) or s['v']!=1 or any(s[k]>1 for k in BOOL) or s['attempts']>2 or s['samples']>s['attempts'] or s['reads']>12 or s['batches']>2 or s['epoch']>=1<<32 or s['busy']>31 or s['reserved']>63 or s['geometry']>31 or s['reason']>511:
        raise ValueError('takeover schema/range')
    if s['obligations']!=255 or any(s[k] for k in ('reset_writes','pte_writes','managed_pages','hardware_ownership')):
        raise ValueError('takeover unsupported authority')
    if s['returned']>s['borrowed'] or s['retained']!=s['borrowed']-s['returned'] or s['blocked']>s['borrowed'] or s['attempts'] and not s['blocked']:
        raise ValueError('takeover borrow lifecycle')
    if s['retained'] and not s['reason']&128:raise ValueError('takeover unexplained retention')
    if s['quiescent']>s['complete'] or s['borrowed'] and s['batches']!=2:
        raise ValueError('takeover admission coverage')
    return s

def takeover_result(text):
    sums=[];rows=[];order=[]
    for line in text.splitlines():
        if line.startswith('intel-takeover-summary:'):
            sums.append(takeover_summary(line));order.append('s')
        elif line.startswith('intel-takeover-row:'):
            m=re.fullmatch(r'intel-takeover-row: v=1 s=([01]) n=([0-6]) d=(-|[0-9]{1,10}(?:,[0-9]{1,10}){0,5})',line)
            if not m:raise ValueError('takeover row encoding')
            sample,n=int(m[1]),int(m[2]);words=[] if m[3]=='-' else list(map(int,m[3].split(',')))
            if len(words)!=n or any(x>0xffffffff for x in words):raise ValueError('takeover row width/progress')
            rows.append(dict(sample=sample,words=words));order.append('r')
        elif line.startswith('intel-takeover-'):
            raise ValueError('unknown takeover receipt')
    if not sums:
        if rows:raise ValueError('takeover missing summary')
        return None
    if len(sums)!=1:raise ValueError('takeover duplicate summary')
    s=sums[0]
    if order!=['r']*s['attempts']+['s'] or [r['sample'] for r in rows]!=list(range(s['attempts'])) or sum(len(r['words']) for r in rows)!=s['reads'] or sum(len(r['words'])==6 for r in rows)!=s['samples']:
        raise ValueError('takeover raw coverage/order')
    if any(x==0xffffffff for r in rows for x in r['words']) and not s['reason']&32:
        raise ValueError('takeover unreported floating word')
    if s['stable']:
        if s['samples']!=2 or rows[0]['words']!=rows[1]['words'] or any(x==0xffffffff for x in rows[0]['words']):raise ValueError('takeover stability')
    elif any(s[k] for k in ('busy','reserved','geometry')):raise ValueError('takeover masks before stability')
    from intel_gt_wake import wake_result
    wake=wake_result(text)
    if not wake or wake['summary']['v'] not in (13,14):raise ValueError('takeover wrong wake image')
    from intel_completion_audit import completion_result
    pre=completion_result(text)
    if not pre:raise ValueError('takeover missing census')
    extra=0
    if wake['summary']['v']==14:
        from intel_engine_prepare import prepare_summary
        from intel_completion_audit import summary as census_summary
        ps=[prepare_summary(l) for l in text.splitlines() if l.startswith('intel-prepare-summary:')]
        post=[census_summary(l.replace('intel-prepare-post-summary:','intel-completion-summary:',1)) for l in text.splitlines() if l.startswith('intel-prepare-post-summary:')]
        if len(ps)!=1 or len(post)!=1:raise ValueError('takeover preparation coverage')
        extra=ps[0]['reads']+post[0]['reads']
    if wake['summary']['engine_reads']!=pre['summary']['reads']+s['reads']+extra:raise ValueError('takeover total read binding')
    if s['borrowed'] and (not pre['summary']['complete'] or s['epoch'] or not pre['summary']['end_us']<s['begin_us']<=pre['summary']['end_us']+100000):
        raise ValueError('takeover census epoch/time binding')
    if s['stable']:
        w=rows[0]['words'];busy=sum(1<<e for e in range(5) if w[e]&3);reserved=sum(1<<e for e in range(5) if w[e]&0xfffc)|(32 if w[5]&~0xdf else 0)
        if not pre['summary']['complete']:raise ValueError('takeover incomplete prerequisite')
        geometry=sum(1<<e for e in range(5) if [pre['rows'][e][k] for k in ('w0','w1','w2','w3','w4','w5')]==[0,0,0,0,1,0])
        if [busy,reserved,geometry]!=[s[k] for k in ('busy','reserved','geometry')]:raise ValueError('takeover derived masks')
    complete=bool(not s['reason'] and s['stable'] and s['attempts']==s['samples']==2 and s['reads']==12 and s['borrowed'] and s['blocked'] and s['returned'] and not s['retained'] and 0<s['begin_us']<s['end_us']<=s['begin_us']+100000)
    if s['complete']!=int(complete):raise ValueError('takeover completion')
    quiescent=complete and not s['busy'] and not s['reserved'] and not rows[0]['words'][5] and s['geometry']==31 and all(not pre['summary'][k] for k in ('enabled','pending','flip_wait','tlb_pending')) and pre['summary']['parser_idle']==3
    if s['quiescent']!=int(quiescent):raise ValueError('takeover quiescence')
    if s['complete'] and not wake['summary']['begin_us']<pre['summary']['begin_us']<pre['summary']['end_us']<s['begin_us']<s['end_us']<wake['summary']['end_us']:
        raise ValueError('takeover owner ordering')
    if not text.index('intel-completion-summary:')<text.index('intel-takeover-summary:')<text.index('intel-clock:')<text.index('intel-wake-summary:'):
        raise ValueError('takeover receipt ordering')
    return dict(summary=s,rows=rows,observation_complete=bool(s['complete'] and wake['lease_complete']),native_observation_complete=False,quiescent_observed=bool(s['quiescent']),reset_admitted=False,managed_range=False,hardware_ownership=False,physical_tearfree=False)

def native_takeover_result(text):
    r=takeover_result(text)
    if not r or not r['observation_complete']:return r
    from intel_gt_wake import native_wake_result
    wake=native_wake_result(text)
    if not wake or not wake['native_wake_complete']:raise ValueError('takeover native wake coverage')
    r['native_observation_complete']=True
    return r
