"""Decode exact, bounded compact control companions of the same worst loop.

Each newline-terminated row is independently validated. Positional vectors
retain every scheduler field; no truncated UART reconstruction is permitted.
"""
if not __debug__:raise RuntimeError('evidence validation requires Python assertions')
from loop_timing import validate_loop_timing,SCHED_FIELDS
from render_timing import _scheduler

FIELDS='seq version valid base_us edges flags state snapshot submit wait notice child waited wait_flags result final'.split()
VECTOR_COUNTS=dict(edges=4,snapshot=2,submit=2,wait=2,notice=2,child=2,data=len(SCHED_FIELDS)-2)
SCHED_VECTOR_FIELDS=[f for f in SCHED_FIELDS if f not in ('seq','final')]
FLAG_FIELDS='due state_checked snapshot_called submit_called accepted enabled finished_before finished_after wait_called notice_called io_enabled'.split()


def _row(line,prefix,fields):
    assert line.endswith(b'\n'),'Truncated control record'
    words=line[len(prefix):].split();assert len(words)==len(fields),'Control field count'
    row={}
    for field,word in zip(fields,words):
        key,sep,value=word.partition(b'=');assert sep and key==field.encode(),'Control field order'
        values=value.split(b',') if field in VECTOR_COUNTS else [value]
        assert len(values)==VECTOR_COUNTS.get(field,1),'Control vector length'
        numbers=[]
        for v in values:
            signed=field=='result'
            assert v.isdigit() or (signed and v.startswith(b'-') and v[1:].isdigit()),'Control field type'
            n=int(v);assert (-(1<<63)<=n<(1<<63)) if signed else (0<=n<(1<<64)),'Control overflow'
            numbers.append(n)
        row[field]=numbers if field in VECTOR_COUNTS else numbers[0]
    assert row['version']==1 and row['final'] in (0,1),'Control schema/final'
    return row


def _selected(blobs,prefix,fields,worst,region=None):
    rows={}
    for blob in blobs:
        for line in blob.splitlines(keepends=True):
            if not line.startswith(prefix):continue
            row=_row(line,prefix,fields)
            if 'region' in row:
                assert row['region']<3,'Control region'
                if row['region']!=region:continue
            key=row['seq'],row['final'];assert key not in rows or rows[key]==row,'Conflicting control record'
            rows[key]=row
    matches=[r for r in rows.values() if r['seq']==worst['seq']]
    assert matches,'Missing worst control companion'
    assert all(all(r[f]==matches[0][f] for f in fields if f!='final') for r in matches),'Changed retained control companion'
    key=worst['seq'],worst['final'];assert key in rows,'Missing corresponding final control companion'
    return rows[key]


def _call(row,prefix,begin,end):
    called=row[prefix+'_called'];assert called in (0,1),'Control call flag'
    start,stop=row[prefix+'_begin_us'],row[prefix+'_end_us']
    if called:assert 0<begin<=start<=stop<=end,'Control call clock'
    else:assert start==stop==0,'Uncalled control operation has clocks'
    return stop-start if called else None


def validate_control_timing(blobs,maximum=None,*,require_final=True):
    blobs=tuple(blobs);worst=validate_loop_timing(blobs,maximum,require_final=require_final)
    raw=_selected(blobs,b'desktop-loop-control: ',FIELDS,worst)
    assert raw['valid']==1 and raw['flags']<1<<len(FLAG_FIELDS),'Invalid control flags'
    flags={f:(raw['flags']>>i)&1 for i,f in enumerate(FLAG_FIELDS)}
    base=raw['base_us'];at=[base,*[base+x for x in raw['edges']]]
    assert 0<base and at==sorted(at) and at[-1]<1<<64,'Control boundary clock'
    assert base==worst['start_us'] and at[-1]==base+worst['control_us'],'Control/coarse clock binding'
    sched={}
    for i,name in enumerate(('report','audio','io')):
        wire=_selected(blobs,b'desktop-loop-control-sched: ','seq version region data final'.split(),worst,region=i)
        q=dict(zip(SCHED_VECTOR_FIELDS,wire['data']));q.update(seq=wire['seq'],final=wire['final'])
        _scheduler(q,at[i+1],at[i+2]);sched[name]=q
    r={k:flags[k] for k in ('due','state_checked','snapshot_called','submit_called','accepted')};r['state']=raw['state']
    a={k:flags[k] for k in ('enabled','finished_before','finished_after','wait_called','notice_called')}
    a.update(child_before=raw['child'][0],child_after=raw['child'][1],wait_pid=raw['waited'],wait_flags=raw['wait_flags'],wait_result=raw['result'])
    for prefix,row in [('snapshot',r),('submit',r),('wait',a),('notice',a)]:
        offsets=raw[prefix]
        if not row[prefix+'_called']:assert offsets==[0,0],'Uncalled control has offsets'
        start,end=[base+x for x in offsets] if row[prefix+'_called'] else (0,0)
        assert max(start,end)<1<<64,'Control offset overflow'
        row[prefix+'_begin_us']=start;row[prefix+'_end_us']=end
    assert r['state_checked']==r['due'] and 0<=r['state']<=4 and (r['state_checked'] or r['state']==0),'Control report gate'
    eligible=r['due']==1 and r['state']==0
    assert r['snapshot_called']==r['submit_called']==int(eligible),'Changed report execution'
    snapshot=_call(r,'snapshot',at[1],at[2]);submit=_call(r,'submit',at[1],at[2])
    if eligible:assert r['snapshot_end_us']<=r['submit_begin_us'],'Overlapping report operations'
    else:assert r['accepted']==0,'Uncalled report accepted'
    assert a['enabled']==1 and a['finished_before']<=a['finished_after'],'Audio control state'
    assert a['child_before']>0 or a['finished_before'],'Unstarted audio control'
    should_wait=not a['finished_before'] and a['child_before']>0
    assert a['wait_called']==int(should_wait),'Changed audio wait execution'
    wait=_call(a,'wait',at[2],at[3]);notice=_call(a,'notice',at[2],at[3])
    if a['wait_called']:
        assert a['wait_pid']==a['child_before'] and a['wait_flags']==1,'Control wait identity/NOHANG'
        if a['wait_result']==a['wait_pid']:assert a['finished_after']==1 and a['child_after']==0,'Wait completion state'
        else:assert a['child_after']==a['child_before'] and not a['finished_after'],'Wait pending state'
    else:
        assert a['wait_pid']==a['wait_flags']==a['wait_result']==0,'Uncalled wait has result'
        assert a['child_before']==a['child_after']==0,'Completed audio still has child'
    if a['notice_called']:
        assert not a['finished_after'],'Notice after completion'
        if a['wait_called']:assert a['wait_end_us']<=a['notice_begin_us'],'Notice/wait order'
    phases=dict(zip(('bookkeeping_us','report_us','audio_us','io_trial_us'),(at[i+1]-at[i] for i in range(4))))
    assert sum(phases.values())==worst['control_us'],'Control conservation'
    return dict(sequence=worst['seq'],control=raw,report=r,audio=a,scheduler=sched,phases=phases,
                inner_us=dict(snapshot=snapshot,submit=submit,wait=wait,notice=notice),
                scope='Wall time includes instrumentation,interrupts and scheduling;inner clocks are subsets,not additive phases or device time')
