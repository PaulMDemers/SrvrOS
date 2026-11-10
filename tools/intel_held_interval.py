"""One native GGTT original under the live post-reset borrow; no range grant."""
import re
FIELDS='v origin begin end epoch_before epoch_after first pages attempts reads samples n0 n1 stable bound reason clock_at clock_prev complete obligations assurances pte_writes managed_pages hardware_ownership'.split()
def interval_summary(line):
    prefix='intel-held-interval-summary: '
    if not line.startswith(prefix):raise ValueError('interval summary prefix')
    s={}
    for token in line[len(prefix):].split():
        k,sep,v=token.partition('=')
        if not sep or k in s or not re.fullmatch(r'0|[1-9][0-9]{0,19}',v) or int(v)>=1<<64:raise ValueError('interval field encoding')
        s[k]=int(v)
    if set(s)!=set(FIELDS) or s['v']!=1:raise ValueError('interval schema')
    if any(s[k]>1 for k in ('stable','bound','complete')) or s['reason']>63 or max(s['epoch_before'],s['epoch_after'])>=1<<32:raise ValueError('interval outcome range')
    if s['obligations']!=255 or any(s[k] for k in ('assurances','pte_writes','managed_pages','hardware_ownership')):raise ValueError('interval unsupported authority')
    if (s['first'],s['pages']) not in ((0,0),(8704,1)) or max(s['n0'],s['n1'])>3 or s['samples']>2 or not 0<=s['attempts']-s['reads']<=1 or s['attempts']>6 or s['reads']!=s['n0']+s['n1']:raise ValueError('interval counts/extent')
    if s['n1'] and (s['n0']!=3 or not s['samples']) or s['samples']>int(s['n0']==3)+int(s['n1']==3):raise ValueError('interval sample progress')
    if s['begin'] and (not s['pages'] or not 0<s['origin']<=s['begin']<=s['end']<=s['origin']+100000):raise ValueError('interval shared deadline')
    if s['reads'] and not s['begin']:raise ValueError('interval reads before scope')
    if (s['clock_at'] or s['clock_prev']) and not s['reason']&2:raise ValueError('interval unexplained clock')
    if s['bound'] and not s['complete']:raise ValueError('interval bound before completion')
    if s['complete'] and (s['reason'] or not s['stable'] or not s['bound'] or s['samples']!=2 or s['attempts']!=6 or s['reads']!=6 or s['n0']!=3 or s['n1']!=3 or s['epoch_before']==0xffffffff or s['epoch_after']!=s['epoch_before']+1 or s['end']<=s['begin']):raise ValueError('interval incomplete declaration')
    return s
def interval_capture(text):
    rows=[];summaries=[];order=[]
    for line in text.splitlines():
        if line.startswith('intel-held-interval-row:'):
            m=re.fullmatch(r'intel-held-interval-row: v=1 s=([01]) n=([0-3]) begin=(0|[1-9][0-9]{0,19}) end=(0|[1-9][0-9]{0,19}) h=(-|[0-9a-f]{8}(?:[0-9a-f]{8}){0,2})',line)
            if not m:raise ValueError('interval row encoding')
            words=[] if m[5]=='-' else [int(m[5][i:i+8],16) for i in range(0,len(m[5]),8)]
            row=dict(sample=int(m[1]),count=int(m[2]),begin=int(m[3]),end=int(m[4]),words=words)
            if len(words)!=row['count'] or max(row['begin'],row['end'])>=1<<64:raise ValueError('interval row width')
            rows.append(row);order.append(('r',row['sample']))
        elif line.startswith('intel-held-interval-summary:'):summaries.append(interval_summary(line));order.append(('s',))
        elif line.startswith('intel-held-interval-'):raise ValueError('interval unknown record')
    if not rows and not summaries:return None
    if len(summaries)!=1 or order!=[('r',0),('r',1),('s',)]:raise ValueError('interval receipt coverage/order')
    s=summaries[0]
    if [r['count'] for r in rows]!=[s['n0'],s['n1']]:raise ValueError('interval raw counts')
    for row in rows:
        if row['count'] and not row['begin'] or row['end'] and not row['begin']:raise ValueError('interval row missing start')
        if row['begin'] and not s['begin']<=row['begin']<=s['end']:raise ValueError('interval row start')
        if row['end'] and not row['begin']<=row['end']<=s['end']:raise ValueError('interval row end')
    if rows[1]['begin'] and (not rows[0]['end'] or rows[1]['begin']<rows[0]['end']):raise ValueError('interval sample ordering')
    if s['complete']:
        if any(r['count']!=3 or not r['begin'] or not r['end'] for r in rows) or rows[0]['words']!=rows[1]['words'] or rows[0]['words'][0]!=rows[0]['words'][2]:raise ValueError('interval torn/changed complete entry')
    return dict(summary=s,rows=rows,complete=bool(s['complete']),native_complete=False,hardware_ownership=False,managed_range=False,managed_pages=0,physical_tearfree=False)
def interval_result(text):
    r=interval_capture(text)
    if not r:return None
    from intel_engine_held_reset import held_reset_result
    from intel_gt_wake import wake_result
    from intel_ggtt_window import window_result
    reset=held_reset_result(text);wake=wake_result(text);window=window_result(text)
    if not reset or not wake or wake['summary']['v']!=18:raise ValueError('interval wrong image/reset coverage')
    s=r['summary'];x=reset['summary'];p=reset['prepare']['summary']
    if s['pages'] and (s['origin']!=x['origin'] or s['epoch_before']!=x['epoch_before'] or s['epoch_after']!=x['epoch_after']):raise ValueError('interval source binding')
    if s['begin']:
        if x['writes']!=2 or x['acks']!=2 or not x['settle_end'] or x['n1']!=9 or not x['provider_enter'] or not x['settle_end']<=s['begin']<=s['end'] or s['end']>x['end'] and not x['reason']&2:raise ValueError('interval outside original post-reset scope')
    if s['reads']:
        if not window or not window['readout_complete']:raise ValueError('interval original window missing')
        original=window['vectors'][1][0];expected=[original>>32,original&0xffffffff,original>>32]*2
        words=[v for row in r['rows'] for v in row['words']]
        drift=[i for i,v in enumerate(words) if v!=expected[i]]
        if drift and (drift!=[len(words)-1] or not s['reason']&32):raise ValueError('interval original mismatch/read past drift')
    complete=bool(s['complete'] and reset['complete'] and x['provider_exit'] and p['returned'] and not p['retained'] and wake['lease_complete'])
    if wake['summary']['complete'] and not complete:raise ValueError('interval enclosing false completion')
    if s['begin'] and not s['complete'] and (not p['retained'] or p['returned'] or not p['uncertain'] or wake['summary']['released']):raise ValueError('interval failure lost pin')
    if not text.index('intel-held-reset-summary:')<text.index('intel-held-interval-row:')<text.index('intel-held-interval-summary:')<text.index('intel-prepare-summary:'):raise ValueError('interval report ordering')
    return dict(r,observation_complete=r['complete'],complete=complete,reset=reset,window=window)
def native_held_interval_result(text):
    r=interval_result(text)
    if not r or not r['complete']:return r
    from intel_gt_wake import native_wake_result
    wake=native_wake_result(text)
    if not wake or not wake['native_wake_complete']:raise ValueError('interval native prerequisite coverage')
    r['native_complete']=True
    return r
def verify_interval_checkpoint(pre,text,native=False):
    from intel_gt_wake import verify_wake_checkpoint
    verify_wake_checkpoint(pre,text,native)
    if 'intel-held-interval-' in pre:raise ValueError('interval late checkpoint')
    if native:
        from intel_ggtt_window import native_window_result
        a=native_window_result(pre);b=native_window_result(text)
        if not a or not b or not a['native_observation_complete'] or a!=b:raise ValueError('interval original checkpoint mismatch')
    return True
