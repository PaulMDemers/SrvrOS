"""Strict combined control/file evidence. Pending jobs are not USB DMA proof."""
import re
from serial_records import complete_lines
from battery_native_smbus_records import expand_compact_records
from battery_workspace_controls_records import check_hold,check_events,check_home_returns,check_home_refusal


def rows(data,prefix,pattern):
    matches=re.findall(rb'^'+pattern+rb'$',data,re.M)
    assert len(matches)==sum(r.startswith(prefix) for r in data.splitlines()),prefix
    return matches


def check(data,case='normal',home=False,vm=True):
    assert case in ('normal','cancel','refusal','unmounted-refusal','profile-refusal','query-refusal')
    data=complete_lines(data.replace(b'\r\n',b'\n'))
    if home:data=expand_compact_records(data)
    if case=='unmounted-refusal' or (case=='refusal' and not home):
        refusal=b'explicit Home ownership' if home else b'VM-only mounted study'
        assert data.count(b'battery-workspace-controls: REFUSED '+refusal)==1
        for bad in (b'battery-workspace-hold:',b'battery-service:',b'text-battery:',b'battery-control: dispatched'):assert bad not in data
        return dict(status='PASS',expected=case,runtime_started=False)
    assert case!='query-refusal' or home
    unavailable=case=='profile-refusal' or (home and case=='refusal')
    mode=b'explicit' if home else b'disabled'
    token=check_hold(data,False if unavailable else True);assert token==1
    assert data.count(b'battery-workspace-controls:')==2
    assert data.count(b'battery-workspace-controls: begin mounted=yes native='+mode+b'\n')==1
    assert data.count(b'battery-control-files: begin pending-application-jobs=yes native='+mode)==1
    outcome=rows(data,b'battery-workspace-controls: complete=',rb'battery-workspace-controls: complete=(yes|NO) epoch=(\d+) state=(\d+); revoked=yes retired=yes mounted=yes native='+mode)
    assert len(outcome)==1
    controls=rows(data,b'battery-controls:',rb'battery-controls: (PASS|FAIL) requests=(\d+) descendants=3 epoch=(\d+); revoked=yes retired=yes')
    assert len(controls)==1
    requests=rows(data,b'text-battery: request=',rb'text-battery: request=([12]) serial=(\d+) epoch=(\d+) cycle=(\d+) pending_file=([01]) file_op=(\d+) submitted=([01])')
    requests=[tuple(map(int,r)) for r in requests]
    expected=[(1,1,0,0,0,0,0),(2,2,1,0,1,2,1),(1,3,1,1,1,2,1),(2,4,2,2,1,2,1),(1,5,2,3,1,2,1)]
    assert requests==expected[:len(requests)]
    dispatch=rows(data,b'battery-control: dispatched ',rb'battery-control: dispatched serial=(\d+) operation=([12]) epoch=(\d+) result=0')
    dispatch=[tuple(map(int,r)) for r in dispatch]
    assert dispatch==[(r[1],r[0],r[2]) for r in requests[:len(dispatch)]]
    observed=rows(data,b'text-battery: observed=',rb'text-battery: observed=([12]) serial=(\d+) epoch=(\d+) state=(\d+) generation=(\d+) pending_file=([01])')
    observed=[tuple(map(int,r)) for r in observed]
    for r,want in zip(observed,requests):
        op,serial,epoch,state,generation,pending=r
        assert (op,serial,epoch,state)==(want[0],want[1],want[2]+int(want[0]==1),2 if op==1 else 4)
        assert generation>=2
    assert len(observed)<=len(requests)
    for bad in (b'battery: HALTED',b'workspace: unmounted',b'workspace-image:',b'battery-smbus: FAILED'):assert bad not in data
    if unavailable:
        assert len(requests)==len(dispatch)==1 and not observed
        reason=(b'battery: REFUSED platform/resource ownership preflight' if case=='refusal' else b'battery-native-smbus: REFUSED captured controller profile') if home else b'battery-smbus-study: REFUSED model ownership profile'
        assert reason in data
        if home:check_home_returns(data,0)
        assert outcome==[(b'NO',b'1',b'5')] and controls==[(b'FAIL',b'1',b'1')]
        assert b'desktools: file-result' not in data and b'battery-smbus: event=' not in data
        return dict(status='PASS',expected=case,scope_released=True)
    if case=='query-refusal':
        assert requests==expected[:1] and len(dispatch)==1 and len(observed)<=1
        assert outcome==[(b'NO',b'1',b'6')] and controls==[(b'FAIL',b'1',b'1')]
        assert b'text-battery: completed=NO' in data
        assert b'desktools: file-result' not in data and b'text-battery: completed=yes' not in data
        assert data.count(b'text-files: cycles=0 pending=1 storage=workspace')==1
        child=rows(data,b'deskd: child-exit ',rb'deskd: child-exit pid=([1-9][0-9]*) status=(1|143) tick=(\d+)')
        assert len(child)==1
        assert child[0][1]==b'143' or b'desktools: exited role=text failure=1' in data
        events,alarms=check_home_refusal(data)
        return dict(status='PASS',expected='failed session after normal refused-query return',workload_complete=False,events=events,alarms=alarms,scope_released=True,native_acceptance=False)
    events,alarms=check_events(data,0,1 if case=='cancel' else 3,vm=vm)
    if home:check_home_returns(data,1 if case=='cancel' else 3,vm=vm)
    if case=='cancel':
        assert len(requests)==2 and len(dispatch) in (1,2) and len(observed)==1
        assert data.count(b'text-battery: cancel serial=2 pending_file=1 submitted=1')==1
        assert outcome==[(b'NO',b'1',b'4')] and controls==[(b'FAIL',b'2',b'1')]
        assert b'text-battery: completed=NO' in data
        # Cancellation can exhaust the compositor's bounded close grace while
        # Text releases its surface. Require an explicit child termination
        # receipt rather than assuming the final userspace log always ran.
        exits=rows(data,b'desktools: exited role=text',rb'desktools: exited role=text failure=(1)')
        child=rows(data,b'deskd: child-exit ',rb'deskd: child-exit pid=([1-9][0-9]*) status=(1|143) tick=(\d+)')
        assert len(child)==1 and len(exits)<=1
        assert child[0][1]==b'143' or exits==[b'1']
        exit_mode='terminated during bounded close' if child[0][1]==b'143' else 'reported incomplete exit'
        assert b'text-files: cycles=0 pending=1 storage=workspace' in data
        assert b'desktools: file-result' not in data
        return dict(status='PASS',expected='cancelled after submitting first write and Stop',events=events,alarms=alarms,clients_retired=3,scope_released=True,client_exit=exit_mode)
    assert requests==expected and len(dispatch)==len(observed)==5
    assert outcome==[(b'yes',b'3',b'4')] and controls==[(b'PASS',b'5',b'3')]
    assert data.count(b'text-battery: completed=yes requests=5 waiting=0 epoch=3 state=2; expected_owner=active')==1
    for bad in (b'text-battery: FAILED',b'text-battery: cancel',b'completed=NO',b'desktools: exited role=text failure=1'):assert bad not in data
    assert rows(data,b'desktools: exited role=text',rb'desktools: exited role=text failure=(0)')==[b'0']
    edits=rows(data,b'text-coexist: checkpoint ',rb'text-coexist: checkpoint serial=(\d+) length=2 caret=0 anchor=0 clipboard_busy=0 hex=4141')
    assert edits==[str(n).encode() for n in (12,24,36,48,60)]
    assert data.count(b'text-coexist: final serial=60 length=2 caret=0 anchor=0 clipboard_busy=0 hex=4141')==1
    assert data.count(b'text-coexist: file_worker=enabled checkpoints=5')==1
    assert data.count(b'text-files: cycles=5 pending=0 storage=workspace')==1
    assert data.count(b'desktop-text: generated=60 presented=60 keyboard_sent=0 keyboard_presented=0 storage=workspace')==1
    jobs=rows(data,b'desktools: file-result ',rb'desktools: file-result role=text op=(\d+) status=0 bytes=(\d+)')
    assert [tuple(map(int,r)) for r in jobs]==[(op,n) for _ in range(5) for op,n in ((2,0),(8,32),(5,0),(1,2))]
    submitted=list(re.finditer(rb'^text-battery: request=.*$',data,re.M))
    finishes=list(re.finditer(rb'^desktools: file-result.*$',data,re.M))
    for i in range(4):
        assert submitted[i+1].start()<finishes[i*4].start()
        if i:assert finishes[i*4-1].start()<submitted[i+1].start()
    desktop=rows(data,b'desktop-coexist: completed=',rb'desktop-coexist: completed=yes start_us=(\d+) end_us=(\d+) loops=(\d+) max_gap_us=(\d+) generated=60 presented=60 frames=(\d+) storage=workspace')
    assert len(desktop)==1
    start,end,loops,gap,frames=map(int,desktop[0]);assert end-start>=60000000 and loops>=60 and frames>=60 and 0<gap<end-start
    ui=rows(data,b'desktop-battery:',rb'desktop-battery: reads=(\d+) first_gen=(\d+) last_gen=(\d+) cached=yes')
    assert len(ui)==1 and int(ui[0][0])>=10 and min(map(int,ui[0][1:]))>=1
    assert data.index(b'text-battery: completed=yes')<data.index(b'battery-service: retired epoch=3')<data.index(b'battery-workspace-hold: released')
    return dict(status='PASS',requests=5,lifetimes=3,submitted_job_requests=4,worker_operations=20,file_cycles=5,edits=60,events=events,alarms=alarms,final_active_owner_retired=True,mount_token=token,physical_input='not tested',usb_dma_overlap='not claimed',native_acceptance=False)


def check_capture(data,vm=False,case="normal"):
    data=complete_lines(data.replace(b'\r\n',b'\n'))
    assert b'retired=NO' not in data
    for bad in (b'battery: HALTED',b'battery: FAILED',b'laptop-capture: saved=NO'):
        assert not any(row.startswith(bad) for row in data.splitlines())
    # Check the session before ordinary final-log storage/unmount receipts.
    end=data.index(b'battery-workspace-hold: released')
    end=data.index(b'\n',end)+1
    result=check(data[:end],case=case,home=True,vm=vm)
    required=(b'laptop-capture: battery Home mounted=yes logging-paused=yes fault-reset=disabled',
              b'battery-home-session-capture: returned=yes clean='+ (b'yes' if case=='normal' else b'NO') +b'; physical accuracy NOT_TESTED',
              b'laptop-capture: completed='+ (b'yes' if case=='normal' else b'NO'))
    for row in required:assert data.splitlines().count(row)==1
    assert data.index(required[0])<data.index(b'battery-workspace-hold: acquired')<end<=data.index(required[1])<data.index(required[2])
    assert b'laptop-capture: battery media ' not in data
    result.update(scope='explicit held Home session with controls and Text file work',physical_accuracy='not tested')
    result.pop('native_acceptance',None)
    return result
