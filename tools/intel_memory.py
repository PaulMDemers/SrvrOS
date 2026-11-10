"""Private backing ownership qualification; no native GPU range or writer grant."""
import re
SUMMARY='v kind admitted complete cases pages visible_pages bytes allocated released retained cpu_borrows cpu_ends close_blocked range_refused words callbacks preserved dma_before dma_after elapsed_us terminal_retained gpu_writes managed_pages hardware_ownership obligations'.split()
CASE='v id epoch pages state error release_attempts releases injected ok'.split()
def fields(line,prefix,names):
    if not line.startswith(prefix+': '):raise ValueError('memory record prefix')
    out={}
    for word in line[len(prefix)+2:].split():
        k,sep,v=word.partition('=')
        if not sep or k in out:raise ValueError('memory duplicate/invalid field')
        if k=='kind':out[k]=v
        else:
            if not re.fullmatch(r'0|[1-9][0-9]{0,19}',v) or int(v)>=1<<64:raise ValueError('memory numeric range/canonical form')
            out[k]=int(v)
    if set(out)!=set(names) or out['v']!=1:raise ValueError('memory schema')
    return out
def summary(line):
    s=fields(line,'intel-memory-summary',SUMMARY)
    if s['kind'] not in ('native','fixture') or any(s[k]>1 for k in ('admitted','complete','preserved','terminal_retained')) or s['cases']>4 or s['pages']>1536:raise ValueError('memory outcome range')
    if s['gpu_writes'] or s['managed_pages'] or s['hardware_ownership'] or s['callbacks'] or s['obligations']!=255:raise ValueError('memory unsupported GPU authority/access')
    return s
def case(line):
    c=fields(line,'intel-memory-case',CASE)
    if c['id']>3 or not c['epoch'] or c['pages'] not in (1,1356) or c['state']>6 or c['error']>5 or any(c[k]>1 for k in ('injected','ok','release_attempts','releases')):raise ValueError('memory case range')
    return c
def case_complete(c):
    n=c['id'];return bool(c['ok'] and c['pages']==(1356 if n in (1,2) else 1) and c['state']==(6 if n==3 else 5) and c['error']==(5 if n==3 else 0) and c['release_attempts']==1 and c['releases']==int(n!=3) and c['injected']==int(n==3))
def memory_result(text):
    summaries=[];cases=[]
    for line in text.splitlines():
        if not line.startswith('intel-memory-'):continue
        if line.startswith('intel-memory-summary:'):summaries.append(summary(line))
        elif line.startswith('intel-memory-case:'):cases.append(case(line))
        else:raise ValueError('memory unknown record')
    if not summaries and not cases:return None
    if len(summaries)!=1:raise ValueError('memory missing/duplicate summary')
    s=summaries[0]
    if len(cases)!=s['cases'] or [c['id'] for c in cases]!=list(range(len(cases))):raise ValueError('memory case coverage/order')
    result=dict(kind=s['kind'],complete=bool(s['complete']),summary=s,cases=cases,hardware_ownership=False,managed_range=False,physical_tearfree=False)
    if not s['complete']:return result
    if not s['admitted'] or not s['preserved'] or s['terminal_retained'] or s['cases']!=4 or not all(case_complete(c) for c in cases):raise ValueError('memory incomplete outcome')
    if any(b['epoch']<=a['epoch'] for a,b in zip(cases,cases[1:])):raise ValueError('memory reused epoch')
    expected=dict(pages=1356,visible_pages=1350,bytes=5554176,allocated=2714,released=2713,retained=1,cpu_borrows=3,cpu_ends=3,close_blocked=1,range_refused=2,words=2778112)
    if any(s[k]!=v for k,v in expected.items()) or s['allocated']!=s['released']+s['retained'] or s['dma_after']!=s['dma_before']+1 or not 0<s['elapsed_us']<=30000000:raise ValueError('memory resource/timing accounting')
    if s['kind']=='native':
        from intel_display_capture import capture_result
        from intel_display_backing import backing_result
        display=capture_result(text);backing=backing_result(text)
        if not display or not display['observation_complete'] or not backing or not backing['backing_relationship_observed'] or backing['summary']['count']!=1356:raise ValueError('memory native backing prerequisites')
        fb=display['rows']['framebuffer'][0];bs=backing['summary']
        if not bs['tiled'] or bs['bytes']!=s['bytes'] or fb['pitch']*((fb['height']+7)&~7)!=s['bytes'] or (fb['pitch']*fb['height']+4095)//4096!=s['visible_pages']:raise ValueError('memory native storage extent binding')
    return result
