"""Anonymous advertised-channel diagnostic proof; full source-bound RF proof
is a mandatory separate input. Never accepts mismatched frames as scan rows."""
import re
from wifi_burst_log import record
from wifi_ui_log import one,snapshot,phase,CACHE,COUNTS,cache_valid,check_radio_isolation,check_desktop_input,SCAN

def observations(text,visits):
    rows=[record(x,'wifi-ds-observation','visit index tuned advertised identifiers_logged') for x in text.splitlines() if x.startswith('wifi-ds-observation:')]
    expected=[(i,n,v,item) for i,v in enumerate(visits) for n,item in enumerate(v['items']) if item['completed']]
    assert len(rows)==len(expected)<=24
    matched=mismatched=unresolved=0
    for row,(i,n,v,item) in zip(rows,expected):
        assert (row['visit'],row['index'],row['tuned'])==(i,n,v['channel'])
        advertised=row['advertised'];assert 0<=advertised<=11 and row['identifiers_logged']==0
        if advertised:
            assert item['view']==0 and item['beacon_status'] in [0,3] and item['frame_control']&0xf0 in [0x50,0x80]
        if item['beacon_status']==3:
            assert advertised!=v['channel']
            mismatched+=bool(advertised);unresolved+=not advertised
        elif item['beacon']:
            assert advertised==(v['channel'] if item['ds'] else 0)
            matched+=bool(advertised)
        else:assert not advertised
    summary=record(one(text,'wifi-ds-summary:'),'wifi-ds-summary','attempted complete entries identifiers_logged')
    assert summary==dict(attempted=1,complete=1,entries=len(rows),identifiers_logged=0)
    for line in text.splitlines():
        if line.startswith('wifi-ds-'):assert line.startswith(('wifi-ds-observation:','wifi-ds-summary:'))
    return dict(entries=rows,matched=matched,mismatched=mismatched,unresolved_mismatches=unresolved)

def check(radio,begin,end,release,rf):
    r=radio.decode();b=begin.decode();e=end.decode()
    assert 'srvros.wifi-channel-diagnosis=1' in r and 'srvros.wifi-desktop-empty=1' in r
    assert set(re.findall(r'srvros.desktop-release=([0-9a-f]{16})',r))=={release}
    inputs=check_desktop_input(radio);isolation=check_radio_isolation(radio)
    obs=observations(r,rf['visits'])
    assert 'laptop-capture: completed=' not in r
    assert r.index('wifi-scan-batch:')<r.index('wifi-ds-summary:')<r.index('wifi-firmware-capture: completed=yes')<r.index('wifi-ui-cache: phase=before ')
    for line in r.splitlines():
        if line.startswith('wifi-ds-observation:'):assert r.index('wifi-scan-batch:')<r.index(line)<r.index('wifi-ds-summary:')
    _,rend,rt=snapshot(radio,'wifi-scan.log',1048576,True)
    bstart,bend,bt=snapshot(begin,'wifi-ui-begin.log',65536)
    estart,eend,et=snapshot(end,'exit.log',65536)
    assert rt<=bt<=et and rend<bend<eend
    overlap=rend-bstart;assert overlap>=4096 and begin[:overlap]==radio[bstart:]
    lo=max(bstart,estart);hi=min(bend,eend);assert hi-lo>=4096 and begin[lo-bstart:hi-bstart]==end[lo-estart:hi-estart]
    cache=phase(r,'wifi-ui-cache','before',CACHE);cache_valid(cache)
    eligible=sum(not any(row[k] for k in ['hidden','ssid_conflict','security_conflict','channel_conflict']) for row in rf['rows'])
    assert cache['selected']==2**32-1 and cache['selectable']==eligible
    counts=phase(r,'wifi-ui-counts','before',COUNTS)
    assert cache['owner'] and cache['generation'] and cache['ready'] and cache['adapters']==1 and cache['rows']==len(rf['rows'])
    assert counts==dict(published=1,reads=1,selected=0,stale=0,refused=0,removed=0,identifiers_logged=0)
    run=one(r,'wifi-ui-run: ');m=re.fullmatch(r'wifi-ui-run: id=([1-9][0-9]*) stage=scan identifiers_logged=0',run);assert m
    ident=int(m[1]);assert ident<2**64 and run+'\n' in b
    assert r.count(SCAN+'\n')==1 and b.count('workspace-log: phase=wifi-desktop startup_saved=1 tail_limit=65536\n')==1
    completed=f'wifi-channel-diagnosis: completed=yes run={ident} desktop_opened=0 network_data_out=0'
    assert one(e,'wifi-channel-diagnosis:')==completed and e.count('laptop-capture: completed=yes\n')==1
    assert e.index(completed)<e.index('laptop-capture: completed=yes')
    for forbidden in ['deskd:','wifi-ui-capture:','exception:','workspace-log: SAVE FAILED','laptop-capture: completed=NO']:
        assert forbidden not in e
    return dict(observations=obs,inputs=inputs,isolation=isolation,cache=cache,radio_bytes=len(radio),desktop_opened=False,network_data_out=False,association_acceptance='NOT_TESTED')
