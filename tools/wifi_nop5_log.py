"""Strict v134 controller-only NoOp/fence proof;no radio/key/IP authority."""
import re
from wifi_tx5_log import check as parent
from wifi_power5_log import fields
from wifi_held_log import require

# Exact v134 automatic recipe,with only unit UUID/release supplied by the run.
BOOT_FIXED = frozenset("""srvros.storage=ram
srvros.capture=wifi-firmware
srvros.usb-wifi=probe
srvros.wifi-mac-startup=1
srvros.wifi-mcu-probe=1
srvros.wifi-radio-reset=1
srvros.wifi-write-probe=1
srvros.wifi-pair-probe=1
srvros.wifi-timing-probe=1
srvros.wifi-mac-basics=1
srvros.wifi-mac-format=1
srvros.wifi-bbp-ready=1
srvros.wifi-bbp-core=1
srvros.wifi-bbp-front=1
srvros.wifi-bbp-agc=1
srvros.wifi-bbp-batch=1
srvros.wifi-compact=1
srvros.wifi-agc-more=1
srvros.wifi-agc-next=1
srvros.wifi-agc-tail=1
srvros.wifi-rxc-start=1
srvros.wifi-rxc-tail=1
srvros.wifi-txbe-start=1
srvros.wifi-bbp-table=1
srvros.wifi-mac-route=1
srvros.wifi-mac-policy=1
srvros.wifi-mac-policy-validate=1
srvros.wifi-mac-protect=1
srvros.wifi-mac-contention=1
srvros.wifi-mac-txsw=1
srvros.wifi-mac-beacon=1
srvros.wifi-mac-vht=1
srvros.wifi-mac-buffer=1
srvros.wifi-mac-fce-observe=1
srvros.wifi-mac-fce-validate=1
srvros.wifi-mac-ampdu=1
srvros.wifi-bbp-after-mac=1
srvros.wifi-bbp-switch=1
srvros.wifi-dcoc=1
srvros.wifi-skey=1
srvros.wifi-wcid=1
srvros.wifi-mac-tail=1
srvros.wifi-mac-fixed=1
srvros.wifi-efuse-map=1
srvros.wifi-mac-address=1
srvros.wifi-mac-address-validate=1
srvros.wifi-mac-board=1
srvros.wifi-ant-snapshot=1
srvros.wifi-ant-init=1
srvros.wifi-rf-probe=1
srvros.wifi-rf-table=1
srvros.wifi-rf-bank5=1
srvros.wifi-rf-bank6=1
srvros.wifi-rf-vga=1
srvros.wifi-5ghz-prereq=1
srvros.wifi-5ghz-band=1
srvros.wifi-5ghz-channel=1
srvros.wifi-a20-cycle=1
srvros.wifi-frontend=1
srvros.wifi-pa5-cycle=1
srvros.wifi-bbp5-cycle=1
srvros.wifi-held-channel=1
srvros.wifi-trim5-channel=1
srvros.wifi-dac5=1
srvros.wifi-vco5=1
srvros.wifi-channel-vco5=1
srvros.wifi-cal5=1
srvros.wifi-discovery5=1
srvros.wifi-power5=1
srvros.wifi-handoff5=1
srvros.wifi-peer5=1
srvros.wifi-tx5-owner=1
srvros.wifi-nop5=1
srvros.capture-failure=reboot""".split())


def boot_identity(log, release=None):
    records = {}
    for line in log.splitlines():
        for prefix in ('cmdline: ', 'laptop-capture: boot options='):
            if line.startswith(prefix):
                require(prefix not in records, 'duplicate boot identity record')
                tokens = line[len(prefix):].split()
                require(len(tokens) == len(set(tokens)), 'duplicate boot token')
                records[prefix] = set(tokens)
    require(set(records) == {'cmdline: ', 'laptop-capture: boot options='}, 'missing complete boot identity')
    tokens = records['cmdline: ']
    require(tokens == records['laptop-capture: boot options='], 'inconsistent boot identities')
    release_tokens = [x for x in tokens if x.startswith('srvros.desktop-release=')]
    volume_tokens = [x for x in tokens if x.startswith('srvros.workspace-volume=')]
    require(len(release_tokens) == len(volume_tokens) == 1 and
            re.fullmatch(r'srvros.desktop-release=[0-9a-f]{16}', release_tokens[0]) and
            re.fullmatch(r'srvros.workspace-volume=[0-9a-f]{8}', volume_tokens[0]), 'invalid run identity')
    require(tokens - set(release_tokens + volume_tokens) == BOOT_FIXED, 'wrong v134 controller-only recipe')
    actual = release_tokens[0].split('=', 1)[1]
    require(release is None or actual == release, 'wrong requested release')
    return actual

LOG_PARKED='wifi-nop5-log: parked=1 periodic=0'
PROGRESS='wifi-nop5-progress: controller-only63+1 bufferless descriptors;MAC disabled;exact events;asynchronous stop/drain/rebase;no keys/payload/RF'
SUMMARY=dict(complete=1,parent_complete=1,child_complete=1,posted=64,completed=64,
             boundary=63,after_wrap=1,wraps=1,typed_noop=64,data_prepared=0,
             data_published=0,data_completed=0,fences=1,fence_commands=2,
             sync_commands=3,stop_events=2,configured=1,stopped=1,disabled=1,
             input_restored=1,retired=1,events_empty=1,retained=1,
             backend_retired=0,network_out=0,keys=0,data_dma=0)


def summary(line):
    require(fields(line,'wifi-nop5') == SUMMARY, 'incomplete/unknown controller-only completion')


def check(log, release=None):
    actual_release=boot_identity(log,release)
    require(not any(line.startswith(('wifi-join5','wifi-eapol5','wifi-wpa5','wifi-data5','wifi-provider5','wifi-reuse5')) for line in log.splitlines()), 'unexpected live/security/data receipt')
    result=parent(log,nop=True)
    lines=log.splitlines()
    require(lines.count(LOG_PARKED)==1, 'missing/duplicate periodic logger quiesce')
    rows=[line for line in lines if line.startswith('wifi-nop5') and line!=LOG_PARKED]
    require(len(rows)==79 and rows[0]==PROGRESS, 'missing/duplicate/extra NoOp lifecycle')
    require(lines.index(LOG_PARKED)<lines.index(PROGRESS), 'NoOp started before logger parked')
    summary(rows[-1])
    pos=1
    def take(prefix,keys):
        nonlocal pos
        require(pos<len(rows)-1,'truncated NoOp proof')
        value=fields(rows[pos],prefix);pos+=1
        require(set(value)==set(keys.split()) and all(0<=x<2**64 for x in value.values()),'wrong NoOp schema/range')
        return value
    plan=take('wifi-nop5-plan','ring epoch slot dci positions first total tx_pages rx_pages')
    fixed=dict(dci=8,positions=63,first=0,total=64,tx_pages=0,rx_pages=0)
    require(all(plan[k]==v for k,v in fixed.items()) and plan['epoch']==1 and 0<plan['slot']<256,'wrong typed NoOp plan')
    ring=plan['ring'];slot=plan['slot'];epoch=plan['epoch']
    old=fields(next(x for x in lines if x.startswith('wifi-handoff5:')),'wifi-handoff5')
    tx=fields(next(x for x in lines if x.startswith('wifi-tx5:')),'wifi-tx5')
    parent_slot=fields(next(x for x in lines if x.startswith('wifi-tx5-command:')),'wifi-tx5-command')['slot']
    require(slot==parent_slot and 0<ring<=2**64-4096 and ring%4096==0 and ring not in (old['old_ring'],old['new_ring'],tx['ring']),'aliased/mismatched NoOp ring/slot')
    def transfer(control):
        return control&~1 == (slot<<24)|(8<<16)|(32<<10)
    for index in range(64):
        d=take('wifi-nop5-td','index pointer cycle cc residue event_status event_control bytes page')
        pointer=ring+16*index if index<63 else ring
        require(d['index']==index and d['pointer']==pointer and d['cycle']==int(index<63) and
                d['cc']==1 and d['residue']==d['bytes']==d['page']==0 and d['event_status']==1<<24 and transfer(d['event_control']),
                'wrong raw NoOp completion/boundary')
    contexts=[]
    for phase in range(5):
        d=take('wifi-nop5-context','phase w0 w1 w2 w3 w4 w5 w6 w7')
        require(d.pop('phase')==phase and all(v<=0xffffffff for v in d.values()),'wrong endpoint phase/range')
        contexts.append([d['w'+str(i)] for i in range(8)])
    configured=[1,(3<<1)|(2<<3)|(512<<16),(ring|1)&0xffffffff,ring>>32,124,0,0,0]
    require(contexts[0]==configured,'wrong configured NoOp endpoint')
    fences=[]
    for index in range(2):
        d=take('wifi-nop5-fence-command','index pointer epoch token slot dci type dequeue control status event_pointer event_status event_control stop_pointer stop_status stop_control stop_dequeue consumed')
        require(d['index']==index and d['epoch']==epoch and d['token']==index+1 and d['slot']==slot and d['dci']==8 and d['type']==15+index and d['dequeue']==(ring if index else 0) and d['consumed']==1 and d['status']==0,'wrong fenced command identity')
        require(d['pointer']>0 and d['pointer']%16==0 and d['pointer']==d['event_pointer'] and d['event_status']==1<<24 and
                d['event_control']&~1 == (slot<<24)|(33<<10) and
                d['control']&~1 == (slot<<24)|(8<<16)|((15+index)<<10),'wrong raw command completion')
        if index==0:
            require((d['stop_pointer'],d['stop_dequeue']) in ((ring,ring),(ring+63*16,(ring+63*16)|1)) and
                    d['stop_status']==27<<24 and transfer(d['stop_control']),'wrong stopped-event frontier')
        else:
            require(d['stop_pointer']==d['stop_status']==d['stop_control']==d['stop_dequeue']==0,'extra rebase stop event')
        fences.append(d)
    stopped=configured.copy();stopped[0]=3;dequeue=fences[0]['stop_dequeue'];stopped[2]=dequeue&0xffffffff;stopped[3]=dequeue>>32
    require(contexts[1]==stopped,'Stop context differs from forced event')
    rebased=configured.copy();rebased[0]=3;rebased[2]=ring&0xffffffff;rebased[3]=ring>>32
    require(contexts[2]==rebased,'wrong stopped rebased context')
    terminal=rebased.copy();terminal[2]=(ring+16)&0xffffffff;terminal[3]=(ring+16)>>32
    require(contexts[3]==terminal and contexts[4][0]&7==0,'terminal stop/disable not established')
    sync=[]
    for index in range(3):
        d=take('wifi-nop5-sync-command','index pointer event_pointer cc slot delta parameter status control')
        require(d['index']==index and d['pointer']>0 and d['pointer']%16==0 and d['pointer']==d['event_pointer'] and d['cc']==1 and d['slot']==slot and d['delta']==1 and d['status']==0,'wrong synchronous command completion')
        kind=15 if index==1 else 12
        require(d['control']&~1 == (slot<<24)|(kind<<10)|((8<<16) if index==1 else 0),'wrong synchronous command bits')
        require(d['parameter']==0 if index==1 else d['parameter']>0 and d['parameter']%4096==0 and d['parameter'] not in (ring,old['old_ring'],old['new_ring'],tx['ring']),'invalid command input')
        sync.append(d)
    require(sync[0]['parameter']==sync[2]['parameter'],'configuration/drop input mismatch')
    ordered=[sync[0],fences[0],fences[1],sync[1],sync[2]]
    first=ordered[0]['pointer'];cycle=ordered[0]['control']&1
    require(first%4096<=58*16 and all(d['pointer']==first+16*i and d['pointer']//4096==first//4096 and d['control']&1==cycle for i,d in enumerate(ordered)) and first//4096 not in {v//4096 for v in (ring,old['old_ring'],old['new_ring'],tx['ring'],sync[0]['parameter'])},'command-address reuse/wrap/alias')
    final=take('wifi-nop5-final-stop','pointer status control')
    require(final['pointer']==ring+16 and final['status']==27<<24 and transfer(final['control']),'wrong terminal stopped event')
    frontier=take('wifi-nop5-frontier','sequence engine_sequence pending round failed empty drain_events')
    require(frontier['sequence']==frontier['engine_sequence'] and 2<=frontier['sequence']<=64 and
            frontier['pending']==frontier['round']==frontier['failed']==0 and frontier['empty']==1 and frontier['drain_events']<32,'unretired/undrained event frontier')
    require(pos==len(rows)-1,'extra NoOp rows')
    rf=[x for x in lines if x.startswith('wifi-channel-vco5:')]
    txline=next(x for x in lines if x.startswith('wifi-tx5:'))
    require(len(rf)==1 and lines.index(txline)<lines.index(PROGRESS)<lines.index(rf[0])<lines.index(rows[1]),'NoOp receipts outside full-parent lifetime')
    firmware='wifi-firmware-capture: completed=yes'
    laptop='laptop-capture: completed=yes'
    require(log.count(firmware)==log.count(laptop)==1 and log.index(rows[-1])<log.index(firmware)<log.index(laptop),'missing complete automatic capture')
    result.update(nop5_complete=True,nop5_posted=64,nop5_completed=64,nop5_wraps=1,nop5_descriptor_boundary=63,
                  nop5_ledger_retired=True,nop5_backend_retired=False,nop5_backend_tombstones_retained=True,
                  nop5_input_restored=True,nop5_disabled=True,nop5_events_empty=True,nop5_fence_commands=2,
                  nop5_network_out=0,nop5_keys=0,nop5_data_dma=0,release=actual_release,
                  scope='controller-only typed bufferless NoOps across one real descriptor fence under unchanged fixed161 parent;no radio/key/IP/encrypted persistence acceptance')
    return result
