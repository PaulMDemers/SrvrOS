"""Strict shadow scanout lifecycle receipt; never native flip acceptance."""
import re
from intel_display_capture import capture_result
from intel_display_backing import backing_result
SCHEMAS={
 'summary':'v kind admitted ok cases frames_per_buffer allocated released retained copies requests completed restores polls max_wait_us elapsed_us preserved dma_before dma_after hardware_writes hardware_completion ownership',
 'case':'v case ok epoch state front phase released fault injected',
}
def lifetime_result(text):
    rows={k:[] for k in SCHEMAS}
    for line in text.splitlines():
        if not line.startswith(('intel-lifetime-summary:','intel-lifetime-case:')):continue
        match=re.fullmatch(r'intel-lifetime-(summary|case): (v=.*)',line)
        if not match:raise ValueError('malformed scanout lifetime row')
        kind=match[1];fields={}
        for item in match[2].split():
            key,sep,value=item.partition('=')
            if not sep or key in fields:raise ValueError('duplicate/malformed lifetime field')
            if key in ('kind','ownership'):fields[key]=value
            else:
                if not re.fullmatch('[0-9]{1,20}',value):raise ValueError('invalid lifetime number')
                fields[key]=int(value)
                if fields[key]>=1<<64:raise ValueError('lifetime overflow')
        if set(fields)!=set(SCHEMAS[kind].split()) or fields['v']!=1:raise ValueError('unsupported lifetime schema')
        rows[kind].append(fields)
    if not any(rows.values()):return None
    if len(rows['summary'])!=1:raise ValueError('missing/duplicate lifetime summary')
    s=rows['summary'][0]
    if s['kind'] not in ('native','fixture') or s['ownership']!='shadow' or s['hardware_writes'] or s['hardware_completion'] or s['ok']>1 or s['admitted']>1 or s['preserved']>1 or s['cases']>8:raise ValueError('invalid lifetime outcome or hardware claim')
    result=dict(kind=s['kind'],complete=bool(s['ok']),hardware_ownership=False,physical_tearfree=False,summary=s,rows=rows)
    if not s['ok']:return result
    if not s['admitted'] or not s['preserved'] or s['cases']!=8 or len(rows['case'])!=8:raise ValueError('incomplete shadow lifecycle')
    if [c['case'] for c in rows['case']]!=list(range(8)):raise ValueError('missing/duplicate/reordered lifetime case')
    epochs=[]
    for n,c in enumerate(rows['case']):
        epochs.append(c['epoch'])
        if not c['ok'] or not c['epoch'] or c['front']!=0 or c['injected']!=int(n>=5):raise ValueError('invalid case outcome')
        expected=(3,0,3,0) if n<5 else (4,2 if n==6 else 1,0,{5:8,6:1,7:4}[n])
        if tuple(c[k] for k in ('state','phase','released','fault'))!=expected:raise ValueError('invalid retirement/quarantine')
    if any(b<=a for a,b in zip(epochs,epochs[1:])):raise ValueError('reused lifetime epoch')
    f=s['frames_per_buffer']
    if not 0<f<=4096 or s['allocated']!=16*f or s['released']!=10*f or s['retained']!=6*f or s['allocated']!=s['released']+s['retained'] or s['copies']!=12 or s['requests']!=13 or s['completed']!=10 or s['restores']!=3:raise ValueError('incomplete resource/publication accounting')
    if s['dma_after']<s['dma_before']+s['retained'] or s['polls']<20 or not 0<s['max_wait_us']<100000 or not 0<s['elapsed_us']<=30000000:raise ValueError('invalid timing or retained RAM accounting')
    if s['kind']=='native':
        capture=capture_result(text);backing=backing_result(text)
        if not capture or not capture['observation_complete'] or not backing or not backing['backing_relationship_observed']:raise ValueError('missing native backing prerequisites')
        fb=capture['rows']['framebuffer'][0]
        if f!=(fb['pitch']*fb['height']+4095)//4096:raise ValueError('mismatched native RAM extent')
    return result
