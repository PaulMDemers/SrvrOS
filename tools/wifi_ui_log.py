"""Strict v103 phase/anonymous UI proof. Caller must ALSO verify the complete
radio file with the source-bound RF/USB/ownership checker; this module alone
never accepts radio initialization, physical UI behavior or connectivity.
"""
import re
from wifi_burst_log import record
RADIO_CAP=1048576
TAIL_CAP=65536
SCAN='wifi-ui-scan: completed=yes; capture cleanup/cache verified; no network data OUT'
CACHE='status owner generation adapters ready rows selected selectable identifiers_logged'
COUNTS='published reads selected stale refused removed identifiers_logged'

def one(text,prefix):
    rows=[x for x in text.splitlines() if x.startswith(prefix)]
    assert len(rows)==1,(prefix,len(rows))
    return rows[0]

def phase(text,family,which,keys):
    prefix=f'{family}: phase={which} '
    line=one(text,prefix)
    return record(line.replace(f' phase={which}','',1),family,keys)

def snapshot(raw,name,limit,full=False,*,rolling=False):
    assert isinstance(raw,bytes) and 0<len(raw)<=limit
    pattern=rb'^workspace-log: snapshot='+re.escape(name.encode())+rb' monotonic_us=([1-9][0-9]*) total_before=(0|[1-9][0-9]*) tail_limit='+str(limit).encode()+rb'\n'
    matches=list(re.finditer(pattern,raw,re.M));assert matches
    if rolling:assert name in ['session-a.log','session-b.log'] and limit==TAIL_CAP and not full
    else:assert len(matches)==1
    m=matches[-1];assert m.end()==len(raw)
    stamp,total=map(int,m.groups());assert stamp<2**64 and total<2**64
    end=total+len(m.group());assert len(raw)==min(end,limit)
    if full:assert end==len(raw)
    start=end-len(raw);previous=0
    for item in matches:
        item_stamp,item_total=map(int,item.groups())
        assert previous<item_stamp<2**64 and item_total==start+item.start()
        previous=item_stamp
    return start,end,stamp

def cache_valid(c):
    assert c['status']==0 and c['identifiers_logged']==0 and c['ready'] in (0,1)
    assert 0<=c['adapters']<=4 and 0<=c['rows']<=24 and 0<=c['selectable']<=c['rows']
    assert c['selected']==2**32-1 or c['selected']<c['rows']
    if not c['owner']:assert c['generation']==c['adapters']==c['ready']==c['rows']==c['selectable']==0 and c['selected']==2**32-1
    else:assert c['generation'] and c['adapters'] and c['ready']

def check_desktop_input(radio):
    r=radio.decode('utf-8')
    assert 'srvros.trackpad=gestures' in r
    rows=re.findall(r'^xhci: trackpad slot=([1-9][0-9]*) dci=([1-9][0-9]*) ready=yes request=([1-9][0-9]*) pointer=yes$',r,re.M)
    assert len(rows)==1
    slot,dci,size=map(int,rows[0]);assert slot<=255 and dci<=31 and size<=4096
    return dict(trackpad_ready=True,pointer_enabled=True,slot=slot,dci=dci,request=size)

def check_radio_isolation(radio):
    """v104 only: complete proof must contain one owned, I/O-free RF phase."""
    r=radio.decode('utf-8')
    start='workspace-log: phase=wifi-radio periodic=paused tail_limit=1048576\n'
    parked='wifi-ui-log: isolation_required=1 parked=1\n'
    assert 'srvros.wifi-log-isolation=1' in r
    assert r.count(start)==r.count(parked)==r.count(SCAN+'\n')==1
    assert r.count('wifi-firmware-control:')==1
    a=r.index(start);b=r.index(SCAN+'\n')
    assert a<r.index(parked)<r.index('wifi-firmware-control:')<b
    phase=r[a:b]
    for forbidden in ['workspace-log: snapshot=', 'workspace: flushed',
                      'workspace-cost:', 'usb-timing:']:
        assert forbidden not in phase,forbidden
    assert 'wifi-ui-log: isolation_required=1 parked=0' not in r
    return dict(periodic_paused=True,isolated_radio_phase=True)

def check_ui(radio,begin,exit_log,release):
    assert re.fullmatch('[0-9a-f]{16}',release)
    r=radio.decode('utf-8',errors='replace');b=begin.decode('utf-8',errors='replace');e=exit_log.decode('utf-8',errors='replace')
    assert set(re.findall(r'srvros.desktop-release=([0-9a-f]{16})',r))=={release}
    assert 'srvros.wifi-desktop-trial=1' in r and r.count(SCAN+'\n')==1
    assert 'laptop-capture: completed=' not in r
    _,radio_end,radio_stamp=snapshot(radio,'wifi-scan.log',RADIO_CAP,True)
    begin_start,begin_end,begin_stamp=snapshot(begin,'wifi-ui-begin.log',TAIL_CAP)
    exit_start,exit_end,exit_stamp=snapshot(exit_log,'exit.log',TAIL_CAP)
    assert radio_stamp<=begin_stamp<=exit_stamp and radio_end<begin_end<exit_end
    overlap=radio_end-begin_start
    assert 4096<=overlap<=len(begin) and begin[:overlap]==radio[begin_start:]
    before=phase(r,'wifi-ui-cache','before',CACHE);cache_valid(before)
    assert before['owner'] and before['ready'] and before['selected']==2**32-1
    empty_profile='srvros.wifi-desktop-empty=1' in r
    if empty_profile:
        check_desktop_input(radio);check_radio_isolation(radio)
    else:assert before['selectable']
    first=phase(r,'wifi-ui-counts','before',COUNTS)
    assert not first['identifiers_logged'] and first['published']>=before['adapters'] and first['reads']
    assert first['selected']==first['stale']==first['refused']==first['removed']==0
    run=one(r,'wifi-ui-run: ')
    m=re.fullmatch(r'wifi-ui-run: id=([1-9][0-9]*) stage=scan identifiers_logged=0',run);assert m
    ident=int(m[1]);assert ident<2**64
    assert run+'\n' in b and b.count('workspace-log: phase=wifi-desktop startup_saved=1 tail_limit=65536\n')==1
    assert r.index('wifi-ui-cache: phase=before ')<r.index('wifi-ui-counts: phase=before ')<r.index(run)<r.index(SCAN)<r.rindex('workspace-log: snapshot=wifi-scan.log ')
    assert b.index(run)<b.index('workspace-log: phase=wifi-desktop ')<b.rindex('workspace-log: snapshot=wifi-ui-begin.log ')
    after=phase(e,'wifi-ui-cache','after',CACHE);cache_valid(after)
    last=phase(e,'wifi-ui-counts','after',COUNTS)
    assert last['identifiers_logged']==0 and all(last[k]>=first[k] for k in COUNTS.split())
    assert last['published']==first['published'] and last['reads']>first['reads']
    if not before['selectable']:assert last['selected']==first['selected']
    assert last['removed']<=last['published']
    if before['adapters']==1:
        assert after['owner'] in [0,before['owner']]
        assert after['adapters']==int(bool(after['owner']))
        assert last['removed']-first['removed']==int(not after['owner'])
        if after['owner']:
            assert (after['generation'],after['ready'],after['rows'],after['selectable'])==(before['generation'],1,before['rows'],before['selectable'])
            assert (after['selected']!=2**32-1)==(last['selected']>first['selected'])
    completed=f'wifi-ui-capture: desktop_returned=yes run={ident} manual_acceptance=PENDING'
    assert e.count(completed+'\n')==1 and e.count('laptop-capture: completed=yes\n')==1
    assert 'deskd: exited' in e
    assert e.index('deskd: exited')<e.index('wifi-ui-cache: phase=after ')<e.index('wifi-ui-counts: phase=after ')<e.index(completed)<e.index('laptop-capture: completed=yes')<e.rindex('workspace-log: snapshot=exit.log ')
    assert not any(x in e for x in ['exception:','workspace-log: SAVE FAILED','laptop-capture: completed=NO','desktop_returned=NO'])
    if 'wifi-ui-capture: admitted=' in e:
        assert one(e,'wifi-ui-capture: admitted=')==f'wifi-ui-capture: admitted=1 run={ident} checklist=wifi-trial timed_exit=0 credentials=0 identifiers_logged=0'
    choices=[]
    for line in e.splitlines():
        if not line.startswith('wifi-ui-choice:'):continue
        c=record(line,'wifi-ui-choice','result owner generation index identifiers_logged')
        assert 0<=c['result']<=5 and c['owner']==before['owner'] and c['generation']==before['generation'] and c['index']<before['rows'] and c['identifiers_logged']==0
        choices.append(c)
    delta={k:last[k]-first[k] for k in ['selected','stale','refused','removed']}
    assert sum(c['result']==0 for c in choices)<=delta['selected']
    assert sum(c['result']==3 for c in choices)<=delta['stale']
    assert sum(c['result'] not in (0,3) for c in choices)<=delta['refused']
    if exit_start<=begin_end:
        assert sum(delta[k] for k in ['selected','stale','refused'])==len(choices)
    return dict(run=ident,before=before,after=after,first_counts=first,last_counts=last,delta=delta,visible_choices=choices,
        radio_bytes=len(radio),begin_bytes=len(begin),exit_bytes=len(exit_log),overlap_bytes=overlap,
        discovery_observed=before['rows']>0,selectable_observed=before['selectable']>0,
        selection_observed=delta['selected']>0,removal_observed=delta['removed']>0,
        manual_acceptance='PENDING',association_acceptance='NOT_TESTED',connectivity_acceptance='NOT_TESTED')
