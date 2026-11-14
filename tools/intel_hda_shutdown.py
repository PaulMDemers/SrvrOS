"""Original-owner HDA/BCS shutdown; no claim, reset or memory authority."""
import re
from intel_joint import joint_capture, ENDPOINT
from intel_engine_census import _fields

IO = 'v entry_attempts entry_matches store_checks at previous phase owner fault'.split()

def paired_recipe(cmd):
    from hardware_boot import limine_config
    if not isinstance(cmd,str) or len(cmd)>2048 or any(c.isspace() and c!=' ' for c in cmd):
        raise ValueError('HDA BCS recipe encoding')
    tokens=cmd.split()
    volume=[t.split('=',1)[1] for t in tokens if re.fullmatch(r'srvros.workspace-volume=[0-9a-f]{8}',t)]
    release=[t.split('=',1)[1] for t in tokens if re.fullmatch(r'srvros.desktop-release=[0-9a-f]{16}',t)]
    if len(cmd)+len(tokens)>2048 or len(tokens)!=16 or len(set(tokens))!=16 or len(volume)!=1 or len(release)!=1:
        raise ValueError('HDA BCS exact16 recipe')
    config=limine_config(release[0],'ram-usb-v1-intel-paired-shutdown-laptop-probe-'+volume[0]+'-auto-return')
    expected=next(l.split('cmdline:',1)[1].strip().split() for l in config.splitlines() if 'cmdline:' in l)
    if set(tokens)!=set(expected):raise ValueError('HDA BCS foreign recipe')
    return dict(volume=volume[0],release=release[0])

def paired_result(text):
    lines=[l for l in text.splitlines() if l.startswith('intel-hda-bcs-')]
    if not lines:return None
    if any(l.startswith(('intel-joint-','intel-claim-','intel-gpu-hold-','intel-held-','intel-prepare-','intel-rehold-')) for l in text.splitlines()):
        raise ValueError('HDA BCS foreign operation')
    if any(not l.startswith(('intel-hda-bcs-hda:','intel-hda-bcs-summary:','intel-hda-bcs-entry:','intel-hda-bcs-io:')) for l in lines):
        raise ValueError('HDA BCS unknown record')
    transformed=text.replace('intel-hda-bcs-hda:','intel-joint-hda:').replace('intel-hda-bcs-summary:','intel-joint-summary:')
    pair=joint_capture(transformed)
    if not pair:raise ValueError('HDA BCS missing owner')
    h=pair['summary']
    if h['complete'] or h['gpu_complete'] or h['attached']:raise ValueError('HDA BCS unsupported claim completion')
    from intel_gt_wake import wake_result
    wake=wake_result(text)
    if not wake or wake['summary']['v']!=22:raise ValueError('HDA BCS image binding')
    from intel_shutdown import shutdown_result
    from intel_blitter import blitter_result, BCS_WORDS
    # Existing shutdown structural contract, without accepting legacy image
    # attribution as native evidence. Native checks use the unmodified v22 text.
    shutdown=shutdown_result(text,paired=True)
    b=blitter_result(text)
    if not shutdown or not b:raise ValueError('HDA BCS missing shutdown/blitter')
    rows=[]
    for line in lines:
        if not line.startswith('intel-hda-bcs-entry:'):continue
        m=re.fullmatch(r'intel-hda-bcs-entry: v=1 s=([01]) n=(0|8) d=([0-9,]+)',line)
        if not m:raise ValueError('HDA BCS entry encoding')
        from intel_joint import numbers
        words=numbers(m[3],8)
        if any(w>=1<<32 for w in words) or int(m[2])==0 and any(words):raise ValueError('HDA BCS entry prefix')
        rows.append(dict(sample=int(m[1]),count=int(m[2]),words=words))
    if [r['sample'] for r in rows]!=[0,1]:raise ValueError('HDA BCS entry coverage')
    kinds=['h' if l.startswith('intel-hda-bcs-hda:') else 's' if l.startswith('intel-hda-bcs-summary:') else 'e' if l.startswith('intel-hda-bcs-entry:') else 'i' for l in lines]
    if kinds!=['h']*len(pair['phases'])+['s','e','e','i']:raise ValueError('HDA BCS receipt order')
    ios=[_fields(l,'intel-hda-bcs-io:',IO) for l in lines if l.startswith('intel-hda-bcs-io:')]
    if len(ios)!=1:raise ValueError('HDA BCS store coverage')
    io=ios[0]
    if io['v']!=1 or io['entry_attempts']>2 or io['entry_matches']>3 or io['store_checks']>1 or io['fault']>1:
        raise ValueError('HDA BCS I/O progress')
    if any(r['count'] for r in rows[io['entry_attempts']:]) or any(rows[n]['count']!=8 for n in range(2) if io['entry_matches']&(1<<n)):
        raise ValueError('HDA BCS entry bookkeeping')
    if not io['store_checks'] and any(io[k] for k in ('at','previous','fault')):raise ValueError('HDA BCS unexplained store')
    if io['store_checks'] and (b['summary']['entry_disabled'] or not b['summary']['attempts'] or io['entry_matches']!=3):
        raise ValueError('HDA BCS premature store')
    if h['origin'] and h['origin']!=wake['summary']['begin_us']:raise ValueError('HDA BCS replacement origin')
    retired=bool(h['entry'] and not h['admission'] and h['state']==4 and not h['reason'] and not h['pinned'] and not h['retained'] and
        h['hda_state']==2 and not h['hda_reason'] and h['hda_epoch'] and h['hda_phases']==3 and h['hda_checks']==1 and
        not h['hda_pins'] and not h['hda_retained'] and h['hda_complete'] and h['hda_attempts']==h['hda_writes']==2 and
        h['root_maps']==h['root_retired']==1 and h['borrows']==h['returns']==4 and not h['inspection'] and h['cache']==22 and
        h['checks'] and not h['clock_cause'] and not h['hda_clock_cause'])
    complete=bool(retired and shutdown['complete'] and wake['lease_complete'])
    if complete:
        if not 0<h['origin']<=h['begin']<=h['hda_begin']<h['hda_end']<=h['end']==h['last']<=h['origin']+500000 or h['end']>h['hda_begin']+400000:
            raise ValueError('HDA BCS original deadlines')
        phases=pair['phases']
        if len(phases)!=4 or not all(p['quiet'] and p['summary']['complete'] for p in phases) or [p['endpoint']['command'] for p in phases]!=[6,2,2,6]:
            raise ValueError('HDA BCS inhibition/restoration')
        previous=h['hda_begin']
        for p in phases:
            if not previous<=p['summary']['begin']<p['summary']['end']<=h['hda_end']:raise ValueError('HDA BCS phase clock')
            previous=p['summary']['end']
            if any(p['endpoint'][k]!=phases[0]['endpoint'][k] for k in ENDPOINT if k!='command'):raise ValueError('HDA BCS resource drift')
        ss=shutdown['summary']
        if not phases[1]['summary']['end']<ss['owner_begin']<ss['owner_end']<=phases[2]['summary']['begin'] or h['end']>wake['summary']['end_us']:
            raise ValueError('HDA BCS owner ordering')
        if io['phase']!=ss['begin'] or io['owner']!=ss['owner_begin'] or io['entry_attempts']!=2 or io['entry_matches']!=3:
            raise ValueError('HDA BCS source bookkeeping')
        for n,row in enumerate(rows):
            expected=[b['rows'][n][k] for k in BCS_WORDS]
            if row['count']!=8 or row['words']!=expected:raise ValueError('HDA BCS entry/source agreement')
        e=phases[1]['endpoint']
        for audio in b['audio']:
            if (audio['id'],audio['class'],audio['command'],audio['flags'],audio['pm'],audio['state'])!=(e['id'],e['class_rev']>>8,2,e['flags'],e['pm'],e['state']):
                raise ValueError('HDA BCS reader inhibition')
        enabled=not b['summary']['entry_disabled']
        if io['store_checks']!=int(enabled) or io['fault']:raise ValueError('HDA BCS final admission')
        if enabled and not b['summary']['begin_us']<=io['previous']<=io['at']<=b['summary']['end_us']<=b['summary']['begin_us']+100000:
            raise ValueError('HDA BCS store clock')
    if wake['summary']['complete'] and not complete:raise ValueError('HDA BCS false enclosing completion')
    return dict(complete=complete,native_complete=False,pair=pair,shutdown=shutdown,blitter=b,entries=rows,io=io,
        enabled_writer_complete=bool(complete and not b['summary']['entry_disabled']),
        managed_pages=0,hardware_ownership=False,reset=False,physical_tearfree=False)

def native_paired_result(text):
    r=paired_result(text)
    if r and r['complete']:
        from intel_gt_wake import native_wake_result
        if not native_wake_result(text)['native_wake_complete']:raise ValueError('HDA BCS native prerequisites')
        r['native_complete']=True
    return r

def verify_paired_checkpoint(pre,text,native=False):
    if 'intel-hda-bcs-' in pre:raise ValueError('HDA BCS late checkpoint')
    from intel_gt_wake import verify_wake_checkpoint
    verify_wake_checkpoint(pre,text,native=native)
    boots=[re.findall(r'^laptop-capture: boot options=(.*)$',s,re.M) for s in (pre,text)]
    if any(len(v)!=1 for v in boots) or boots[0]!=boots[1]:raise ValueError('HDA BCS boot provenance')
    paired_recipe(boots[0][0])
    if any(s.count('init: deferred=hda-bcs-preinit;')!=1 or 'init: started pid=' in s for s in (pre,text)):raise ValueError('HDA BCS preinit provenance')
    if not paired_result(text):raise ValueError('HDA BCS missing result')
    return True
