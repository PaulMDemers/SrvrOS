"""Strict isolated R2 sample history, never a current access/ownership grant."""
import re,struct
import hardware_air61_read_qualification as q

def sample_result(text,ecam_text,exit_text,session):
    try:return _sample_result(text,ecam_text,exit_text,session)
    except (ValueError,KeyError,IndexError,TypeError,struct.error):return None

def _sample_result(text,ecam_text,exit_text,session):
    import base64
    if any(line.startswith(('hsw-read:','hsw-read-data:','hsw-window:')) for line in (text+'\n'+exit_text).splitlines()):raise ValueError('mixed receipt profile')
    from hardware_haswell_ecam import one,ecam_result
    state,complete,saved_session,epoch,root=one(text,'hsw-sample:',r'hsw-sample: v=1 state=([0-4]) complete=([01]) session=(session-[0-9a-f]{16}-[0-9a-f]{2}) epoch=(\d+) root=0x([0-9a-f]{16})')
    if session!=saved_session:raise ValueError('session mismatch')
    parts=[]
    for line in text.splitlines():
        if not line.startswith('hsw-sample-data:'):continue
        m=re.fullmatch(r'hsw-sample-data: v=1 chunk=(\d+) data=([A-Za-z0-9+/]*={0,2})',line)
        if not m or int(m[1])!=len(parts) or len(parts)>=88:raise ValueError('chunk ordering')
        b=base64.b64decode(m[2],validate=True)
        if not 1<=len(b)<=512 or base64.b64encode(b).decode()!=m[2]:raise ValueError('chunk encoding')
        parts.append(b)
    size,chunks,report,ok,reads=map(int,one(text,'hsw-sample-end:',r'hsw-sample-end: v=1 bytes=(\d+) chunks=(\d+) report=(\d+) ok=([01]) target_reads=(\d+) target_writes=0'))
    if chunks!=len(parts) or size!=sum(map(len,parts)) or size>45000 or not 0<report<1<<64 or any(len(p)!=min(512,size-i*512) for i,p in enumerate(parts)):raise ValueError('extent/report')
    ref=one(exit_text,'hsw-sample-ref:',r'hsw-sample-ref: v=1 session=(session-[0-9a-f]{16}-[0-9a-f]{2}) epoch=(\d+) root=0x([0-9a-f]{16}) state=([0-4]) saved=([01]) target_reads=(\d+) target_writes=0')
    if ref!=(session,epoch,root,state,'1',str(reads)):raise ValueError('saved reference')
    reason,=one(text,'hsw-sample-failure:',r'hsw-sample-failure: reason=([A-Za-z0-9/_-]+)')
    progress=[]
    for line in text.splitlines():
        if not line.startswith('hsw-sample-pass:'):continue
        m=re.fullmatch(r'hsw-sample-pass: pass=([01]) state=([0-5]) begin=(\d+) end=(\d+) words=(\d+) slots=(\d+) first=(\d+) last=(\d+) claimed=([01])',line)
        if not m or int(m[1])!=len(progress):raise ValueError('pass progress ordering')
        row=tuple(map(int,m.groups()[1:]))
        if row[4]>12 or row[3]>65536 or any(v>=1<<64 for v in row):raise ValueError('progress bounds')
        progress.append(row)
    if len(progress)!=2:raise ValueError('missing pass progress')
    options=[line.split('boot options=',1)[1].split() for line in exit_text.splitlines() if line.startswith('laptop-capture: boot options=')]
    required={'srvros.capture=haswell-firmware','srvros.hsw-cache=alias-nx-preinit','srvros.ecam=air61-2m-test','srvros.vtd-window=air61-two-pass-read-only'}
    if not options or any(not required<=set(row) or row.count('srvros.vtd-window=air61-two-pass-read-only')!=1 or 'srvros.vtd-window=air61-no-read' in row or 'srvros.vtd-window=air61-two-pass-no-read' in row for row in options):raise ValueError('explicit boot profile')
    attempts,count=map(int,one(text,'hsw-sample-counts:',r'hsw-sample-counts: attempts=(\d+) reads=(\d+)'))
    if count!=reads or not 0<=reads<=attempts<=24:raise ValueError('actual read accounting')
    if complete=='0':
        if attempts or reads or parts or ok or 'laptop-capture: completed=yes' in exit_text:raise ValueError('incomplete source')
        return dict(status='REFUSED' if state in ('0','2') else 'FAILED',source_completed=False,reason=reason,progress=progress,target_reads=0,target_writes=0,register_read_authorized=False,hardware_authority=False,ownership=False)
    if state!='3' or not ok or reason!='none' or (attempts,reads)!=(24,24):raise ValueError('series completion')
    p=q._decode(b''.join(parts),session,True)
    if p['root']!=int(root,16) or p['epoch']!=int(epoch) or p['end']>=report:raise ValueError('header/report binding')
    for v,row in zip(p['passes'],progress):
        if row!=(4,v['begin'],v['end'],v['words'],12,v['slot_begin'],v['slot_end'],0):raise ValueError('progress binding')
    result=q._bind_context(p,ecam_text,exit_text,session)
    result.update(status='OBSERVED_RETIRED_READ_ONLY_SAMPLE',target_reads=24,target_attempts=24,
        raw_samples=[v['raw_sample'] for v in p['passes']],
        full_interval_stability_proven=False,root_register_is_loaded_table_proof=False,
        register_writes_authorized=False,root_table_walk_authorized=False)
    return result
