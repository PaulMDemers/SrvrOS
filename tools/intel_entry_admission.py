"""Same-owner disabled BCS child before existing reset admission; no new writer."""
import re
from intel_engine_census import _fields
FIELDS='v policy borrowed returned blocked bound complete retained reason checks begin end last source_begin source_end source_last epoch epoch_after parent_attempts_before parent_attempts_after clock_at clock_prev samples reads bcs_admitted bcs_disabled bcs_entry_disabled bcs_attempts bcs_writes bcs_uncertain bcs_reason bcs_begin bcs_end reset_writes managed_pages'.split()
BOOL='policy borrowed returned blocked bound complete retained bcs_admitted bcs_disabled bcs_entry_disabled'.split()

def entry_summary(line):
    s=_fields(line,'intel-compose-entry-summary:',FIELDS)
    if any(s[k]>1 for k in BOOL) or s['reason']&~63 or s['checks']>14 or s['epoch']>=1<<32 or s['epoch_after']>=1<<32 or s['samples']>4 or s['reads']!=s['samples']*8 or s['bcs_reason']>8:
        raise ValueError('entry counts/range')
    if any(s[k] for k in ('bcs_attempts','bcs_writes','bcs_uncertain','reset_writes','managed_pages')):
        raise ValueError('entry unsupported writer/authority')
    if s['returned']>s['borrowed'] or s['retained']!=s['borrowed']-s['returned'] or s['blocked']>s['borrowed'] or s['bound']>s['bcs_disabled']:
        raise ValueError('entry borrow lifecycle')
    if s['complete']:
        if not all(s[k] for k in ('policy','borrowed','returned','blocked','bound','bcs_admitted','bcs_disabled','bcs_entry_disabled')) or s['retained'] or s['reason'] or s['bcs_reason'] or s['checks']!=14 or s['samples']!=4 or s['reads']!=32:
            raise ValueError('entry false completion')
        if s['epoch']!=s['epoch_after'] or s['parent_attempts_before']!=s['parent_attempts_after'] or s['parent_attempts_before']!=2:
            raise ValueError('entry source epoch/stores')
        if s['clock_at'] or s['clock_prev'] or not 0<s['source_begin']<=s['source_last']<=s['source_end']<s['begin']<=s['bcs_begin']<s['bcs_end']<=s['end']==s['last']<=s['begin']+100000 or s['end']>s['source_end']+100000:
            raise ValueError('entry original clocks/age')
    return s

def entry_capture(text):
    rows=[];sums=[];order=[]
    for l in text.splitlines():
        if not l.startswith('intel-compose-entry-'):continue
        if l.startswith('intel-compose-entry-summary:'):
            sums.append(entry_summary(l));order.append('s')
        elif l.startswith('intel-compose-entry-row:'):
            m=re.fullmatch(r'intel-compose-entry-row: v=1 n=([0-3]) d=([0-9,]+)',l)
            if not m:raise ValueError('entry row encoding')
            from intel_joint import numbers
            words=numbers(m[2],8)
            if any(v>=1<<32 for v in words):raise ValueError('entry DWORD width')
            rows.append(dict(sample=int(m[1]),words=words));order.append('r')
        else:raise ValueError('entry unknown record')
    if not rows and not sums:return None
    if len(sums)!=1 or order!=['r']*len(rows)+['s']:raise ValueError('entry summary/order coverage')
    s=sums[0]
    if [r['sample'] for r in rows]!=list(range(s['samples'])):raise ValueError('entry row coverage')
    from intel_gt_wake import wake_result
    from intel_completion_audit import completion_result
    w=wake_result(text);c=completion_result(text)
    if not w or w['summary']['v']!=23 or not c:raise ValueError('entry image/source binding')
    if s['source_begin'] and (s['source_begin'],s['source_end'])!=(c['summary']['begin_us'],c['summary']['end_us']):raise ValueError('entry replaced census origin')
    if s['bcs_disabled']:
        if s['samples']!=4 or any(r['words']!=[0,0,0,0,1,0,512,0] for r in rows) or s['bcs_reason'] or not s['bcs_admitted'] or not s['bcs_entry_disabled'] or not 0<s['bcs_begin']<s['bcs_end']<=s['bcs_begin']+100000:
            raise ValueError('entry disabled proof')
    if s['complete']:
        cs=c['summary']
        if not cs['complete'] or any(cs[k] for k in ('enabled','pending','flip_wait','tlb_pending','reason')) or cs['parser_idle']!=3 or s['epoch']:
            raise ValueError('entry census eligibility')
        for n in range(2):
            src=[c['rows'][n*5+4]['w'+str(i)] for i in range(6)]+[c['contexts'][n]['c3'],c['contexts'][n]['c5']]
            if any(r['words']!=src for r in rows):raise ValueError('entry raw census agreement')
    from intel_takeover import takeover_summary
    takes=[takeover_summary(l) for l in text.splitlines() if l.startswith('intel-takeover-summary:')]
    if len(takes)!=1:raise ValueError('entry takeover coverage')
    t=takes[0]
    if t['borrowed'] and (not s['complete'] or s['retained'] or s['epoch_after']!=t['epoch'] or not s['end']<t['begin_us']<=s['source_end']+100000):
        raise ValueError('reset admission before entry retirement or stale census')
    return dict(complete=bool(s['complete']),native_complete=False,summary=s,rows=rows,reset_writes=0,managed_pages=0,hardware_ownership=False,physical_tearfree=False)

def entry_result(text):
    r=entry_capture(text)
    if not r:return None
    from intel_joint import joint_result
    j=joint_result(text)
    if not j:raise ValueError('entry missing joint owner')
    return dict(r,complete=bool(r['complete'] and j['complete']),joint=j)

def native_entry_result(text):
    r=entry_result(text)
    if r and r['complete']:
        from intel_joint import native_joint_result
        if not native_joint_result(text)['native_complete']:raise ValueError('entry native prerequisites')
        r['native_complete']=True
    return r

def verify_entry_checkpoint(pre,text,native=False):
    if 'intel-compose-entry-' in pre:raise ValueError('late entry checkpoint')
    from intel_joint import verify_joint_checkpoint
    verify_joint_checkpoint(pre,text,native=native)
    if any(s.count('init: deferred=joint-admit-preinit;')!=1 or 'init: started pid=' in s for s in (pre,text)):raise ValueError('entry preinit provenance')
    if not entry_capture(text):raise ValueError('entry missing final receipt')
    return True
