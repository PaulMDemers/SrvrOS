#!/usr/bin/env python3
"""Collect preserved native-desktop USB diagnostics after returning to Ubuntu."""
import argparse
import hashlib
import json
import re
import subprocess
import tempfile
from pathlib import Path
import hardware_test as host

SESSION = re.compile(r"session-[0-9a-f]{16}-[0-9a-f]{2}")
FILES = ("boot.log", "session-a.log", "session-b.log", "exit.log")


def session_releases(text):
    """Use boot-option records, not commands or app text echoed into the log."""
    releases = set()
    for line in text.splitlines():
        record = next((p for p in ("cmdline: ", "desktop-trial: boot options=", "laptop-capture: boot options=") if line.startswith(p)), None)
        if record is None:
            continue
        for token in line[len(record):].split():
            prefix = "srvros.desktop-release="
            if token.startswith(prefix) and re.fullmatch(r"[0-9a-f]{16}", token[len(prefix):]):
                releases.add(token[len(prefix):])
    return sorted(releases)


def device_snapshot(line):
    if not line.startswith("workspace-devices: "):
        return None
    fields={}
    for token in line.split()[1:]:
        key,sep,value=token.partition("=")
        if not sep or not re.fullmatch(r"(?:-?[0-9]{1,20}|0x[0-9a-fA-F]{1,16})",value):
            return None
        fields[key]=int(value,16 if value.startswith("0x") else 10)
    for kind in ("audio","thermal","smc","pcm","power"):
        if kind+"_state" in fields and fields.get("monotonic_us",-1)>=0:
            return kind,fields
    return None


def device_summaries(text):
    latest={}
    for line in text.splitlines():
        parsed=device_snapshot(line)
        if parsed:
            kind,fields=parsed
            if kind not in latest or fields["monotonic_us"]>=latest[kind]["monotonic_us"]:
                latest[kind]=fields
    return latest


def battery_diagnostic(exit_text):
    """Complete runtime evidence, never automatic physical-value acceptance."""
    lines=exit_text.splitlines()
    finish="battery-capture: returned=yes clean=yes; physical accuracy NOT_TESTED"
    if lines.count(finish)!=1 or any(line.startswith(("battery: HALTED", "battery: FAILED", "battery: REFUSED")) for line in lines):return None
    complete=re.findall(r"^battery: PASS samples=5 notifications=([0-9]{1,20}) queries=([0-9]{1,20}) attached=([0-9]{1,20}) detached=([0-9]{1,20}) heap=0 errors=0 denied=0; physical accuracy NOT_TESTED$",exit_text,re.M)
    if len(complete)!=1 or complete[0][2]!=complete[0][3] or not int(complete[0][2]):return None
    held=[line for line in lines if line.startswith('battery: foreign-event held')]
    restored=[line for line in lines if line.startswith('battery: foreign-event restored')]
    handoffs=[]
    if held or restored:
        if len(held)!=len(restored) or not 1<=len(held)<=2:return None
        pending={}
        for line in held:
            h=re.fullmatch(r"battery: foreign-event held enable=(0x[0-9a-f]{1,16}) original=(0x[0-9a-f]{1,16})",line)
            if not h:return None
            port,original=map(lambda v:int(v,16),h.groups())
            if port in pending:return None
            pending[port]=(original,lines.index(line))
        for line in restored:
            r=re.fullmatch(r"battery: foreign-event restored enable=(0x[0-9a-f]{1,16}) original=(0x[0-9a-f]{1,16}) protected_writes=([0-9]{1,20})",line)
            if not r or not int(r[3]):return None
            port,original=int(r[1],16),int(r[2],16)
            if port not in pending:return None
            before,position=pending.pop(port)
            if original!=before or not position<lines.index(line)<next(i for i,row in enumerate(lines) if row.startswith('battery: PASS'))<lines.index(finish):return None
            handoffs.append({'enable_port':port,'original_enable':original,'protected_writes':int(r[3]),'restored':True})
        if pending:return None
    pattern=(r"^battery: sample=(?P<generation>[0-9]{1,20}) ac_present=(?P<ac>[01]) soc=(?P<soc>[0-9]{1,3}) "
             r"voltage_uv=(?P<voltage_uv>[0-9]{1,20}) current_ua=(?P<current_ua>-?[0-9]{1,20}) "
             r"capacity=(?P<capacity>[0-9]{1,20}) full=(?P<full>[0-9]{1,20}) unit=(?P<unit>[12]) "
             r"temp_mc=(?P<temperature_mc>-?[0-9]{1,12}) valid=(?P<valid>0x[0-9a-f]{1,16}) age_us=(?P<age_us>[0-9]{1,20}) "
             r"ac_us=(?P<ac_us>[0-9]{1,20}) start_us=(?P<start_us>[0-9]{1,20}) end_us=(?P<end_us>[0-9]{1,20})$")
    samples=[]
    for match in re.finditer(pattern,exit_text,re.M):
        sample={k:int(v,16 if k=='valid' else 10) for k,v in match.groupdict().items()}
        if sample['generation']!=len(samples)+1 or sample['soc']>100 or sample['valid']&11!=11 or \
           not 0<=sample['ac_us']<=sample['start_us']<=sample['end_us']<2**64 or \
           sample['age_us']>1000000 or (samples and sample['ac_us']<=samples[-1]['end_us']):return None
        sample['capacity_unit']='uAh' if sample['unit']==1 else 'uWh'
        samples.append(sample)
    if len(samples)!=5 or sum(line.startswith('battery: sample=') for line in lines)!=5:return None
    return {'runtime_complete':True,'physical_accuracy':'manual-pending','samples':samples,
            'queries':int(complete[0][1]),'notifications':int(complete[0][0]),'foreign_event':handoffs[0] if len(handoffs)==1 else None,'foreign_events':handoffs}


def battery_transitions(exit_text):
    """Compact end-of-run proof survives a long human wait and bounded log tail."""
    lines=exit_text.splitlines()
    finish='battery-transition-capture: returned=yes clean=yes; physical accuracy NOT_TESTED'
    if lines.count(finish)!=1 or any(row.startswith(('battery: HALTED','battery: FAILED','battery: REFUSED','battery-transition: INCOMPLETE','battery-transition-capture: returned=yes clean=NO')) for row in lines):return None
    passes=[row for row in lines if row.startswith('battery-transition: PASS')]
    if len(passes)!=1:return None
    match=re.fullmatch(r'battery-transition: PASS phases=3 samples=([0-9]{1,20}) notifications=([0-9]{1,20}) queries=([0-9]{1,20}) attached=([0-9]{1,20}) detached=([0-9]{1,20}) heap=0 errors=0 denied=0; retired=yes',passes[0])
    if not match:return None
    samples,notifications,queries,attached,detached=map(int,match.groups())
    if any(value>=2**64 for value in (samples,notifications,queries,attached,detached)):return None
    if samples<9 or not attached or attached!=detached or not lines.index(passes[0])<lines.index(finish):return None
    # v32 is the native image, with precisely these two configured leases. The
    # initial held records can age out; final exact restoration records cannot.
    restored=[row for row in lines if row.startswith('battery: foreign-event restored')]
    expected={0x189c:0x40,0x189e:4}
    if len(restored)!=2:return None
    for row in restored:
        m=re.fullmatch(r'battery: foreign-event restored enable=(0x[0-9a-f]{1,16}) original=(0x[0-9a-f]{1,16}) protected_writes=([0-9]{1,20})',row)
        if not m:return None
        port,original=int(m[1],16),int(m[2],16)
        if port not in expected or original not in (0,expected.pop(port)) or not int(m[3]) or lines.index(row)>lines.index(passes[0]):return None
    phases=[]
    keys={'accepted','stable','first_gen','last_gen','first_us','ac_us','start_us','end_us','relative_soc','voltage_uv','current_ua','capacity','full','design','unit','valid'}
    for row in (line for line in lines if line.startswith('battery-transition: accepted=')):
        tokens=row[len('battery-transition: '):].split();values={}
        for token in tokens:
            key,sep,value=token.partition('=')
            if not sep or key in values or not re.fullmatch(r'-?[0-9]{1,20}|0x[0-9a-f]{1,16}',value):return None
            values[key]=int(value,16 if value.startswith('0x') else 10)
        if set(values)!=keys or values['accepted']!=len(phases) or values['stable']!=3:return None
        if any(not 0<=v<2**64 for k,v in values.items() if k!='current_ua') or not -2**63<=values['current_ua']<2**63:return None
        if not 1<=values['first_gen'] or values['last_gen']!=values['first_gen']+2 or values['last_gen']>samples:return None
        if not 0<values['first_us']<=values['ac_us']<=values['start_us']<=values['end_us']:return None
        if values['relative_soc']>100 or values['valid']&15!=15 or values['unit'] not in (1,2) or not values['voltage_uv'] or not values['full']:return None
        if (values['accepted']==1 and values['current_ua']>=0) or (values['accepted']!=1 and values['current_ua']<0):return None
        if phases and (values['first_gen']<=phases[-1]['last_gen'] or values['first_us']<=phases[-1]['end_us']):return None
        if lines.index(row)>lines.index(passes[0]):return None
        phases.append(values)
    if len(phases)!=3:return None
    return {'runtime_complete':True,'transition_complete':True,'physical_accuracy':'manual-pending',
            'samples':samples,'queries':queries,'notifications':notifications,'phases':phases}


def battery_service_result(exit_text):
    """v33 bounded native reader/lifecycle proof, not desktop acceptance."""
    lines=exit_text.splitlines()
    finish='battery-service-capture: returned=yes clean=yes; physical accuracy NOT_TESTED'
    if lines.count(finish)!=1 or any(row.startswith(('battery: HALTED','battery: FAILED','battery: REFUSED',
        'battery-service-test: FAIL','battery-service-capture: returned=yes clean=NO')) for row in lines):return None
    passes=[row for row in lines if row.startswith('battery-service-test: PASS')]
    if len(passes)!=1:return None
    m=re.fullmatch(r'battery-service-test: PASS reads=(\d{1,20}) busy_reads=(\d{1,20}) max_read_us=(\d{1,20}) stale=0 recovered=0 stop_timeout=([01]) epochs=3; retired=yes',passes[0])
    if not m or lines.index(passes[0])>=lines.index(finish):return None
    reads,busy,maximum,timed_out=map(int,m.groups())
    if not 1000<=reads<2**64 or not 0<busy<=reads or maximum>=2**64:return None
    leases={};retired=[];retired_at={};summary_at=lines.index(passes[0])
    for i,row in enumerate(lines):
        if row.startswith('battery: foreign-event restored'):
            m=re.fullmatch(r'battery: foreign-event restored enable=(0x[0-9a-f]{1,16}) original=(0x[0-9a-f]{1,16}) protected_writes=(\d{1,20})',row)
            if not m:return None
            port,original,writes=int(m[1],16),int(m[2],16),int(m[3])
            masks={0x189c:0x40,0x189e:4}
            if i>=summary_at or port not in masks or port in leases or original not in (0,masks[port]) or not 0<writes<2**64:return None
            leases[port]=original
        if row.startswith('battery-service: retired'):
            m=re.fullmatch(r'battery-service: retired epoch=(\d{1,20}) attempts=(\d{1,20}) rejected=0 queries=(\d{1,20}) notifications=(\d{1,20}) attached=(\d{1,20}) detached=(\d{1,20}) heap=0 errors=0 denied=0',row)
            if not m or i>=summary_at or set(leases)!={0x189c,0x189e}:return None
            epoch,attempts,queries,notifications,attached,detached=map(int,m.groups())
            if any(n>=2**64 for n in (epoch,attempts,queries,notifications,attached,detached)) or not attached or attached!=detached:return None
            retired.append(dict(epoch=epoch,attempts=attempts,queries=queries,notifications=notifications))
            retired_at[epoch]=i
            leases={}
    if leases or [r['epoch'] for r in retired] not in ([2,3],[1,2,3]):return None
    samples=[row for row in lines if row.startswith('battery-service-test: sample ')]
    if len(samples)!=1 or lines.index(samples[0])>=retired_at[2]:return None
    values={}
    for token in samples[0].removeprefix('battery-service-test: sample ').split():
        key,sep,value=token.partition('=')
        if not sep or key in values or not re.fullmatch(r'-?\d{1,20}',value):return None
        values[key]=int(value)
    if set(values)!={'epoch','generation','flags','age_us','ac_us','start_us','end_us','relative_soc','voltage_uv','current_ua','capacity','full','design','unit','valid'}:return None
    if any(not 0<=v<2**64 for k,v in values.items() if k!='current_ua') or not -2**63<=values['current_ua']<2**63:return None
    if values['epoch']!=2 or not 5<=values['generation']<=retired[-2]['attempts']:return None
    if values['flags']&3!=3 or values['flags']&4 or values['flags']&~31 or values['age_us']>6000000:return None
    if not 0<values['ac_us']<=values['start_us']<=values['end_us']:return None
    if values['relative_soc']>100 or not values['voltage_uv'] or not values['full'] or values['unit'] not in (1,2) or values['valid']&15!=15:return None
    return dict(runtime_complete=True,reader_complete=True,reads=reads,busy_reads=busy,
                max_read_us=maximum,stop_wait_timed_out=bool(timed_out),lifetimes=retired,
                sample=values,physical_accuracy='manual-pending',desktop_acceptance='pending')


def battery_usb_read_result(exit_text,require_service=True):
    """v34 raw USB reads with writes revoked; never write/input acceptance."""
    if require_service and battery_service_result(exit_text) is None:return None
    lines=exit_text.splitlines()
    rows=[row for row in lines if row.startswith('battery-usb-read:')]
    if len(rows)!=1:return None
    match=re.fullmatch(r'battery-usb-read: PASS reads=(\d{1,20}) commands=(\d{1,20}) busy_reads=(\d{1,20}) progress_bursts=(\d{1,20}) bursts=(\d{1,20}) max_burst_us=(\d{1,20}) bytes_per_read=4096 writes=0; retired=yes',rows[0])
    if not match:return None
    reads,commands,busy,progress,bursts,maximum=map(int,match.groups())
    if any(n>=2**64 for n in (reads,commands,busy,progress,bursts,maximum)):return None
    if not 0<busy<=reads<=commands or not 0<progress<=bursts<=reads or maximum<250000:return None
    summaries=[row for row in lines if row.startswith('battery-service-test: PASS reads=')]
    if len(summaries)!=1 or lines.index(rows[0])>=lines.index(summaries[0]):return None
    return dict(read_complete=True,reads=reads,commands=commands,busy_reads=busy,
                progress_bursts=progress,bursts=bursts,max_burst_us=maximum,
                bytes_per_read=4096,writes=0,write_acceptance='pending',input_acceptance='pending')


def battery_usb_write_result(exit_text,require_service=True):
    """Bounded scratch data writes/flushes; no filesystem-mutation acceptance."""
    if require_service and battery_service_result(exit_text) is None:return None
    lines=exit_text.splitlines()
    prepared=[r for r in lines if r.startswith('workspace-scratch: prepared ')]
    revoked=[r for r in lines if r.startswith('workspace-scratch: revoked ')]
    rows=[r for r in lines if r.startswith('battery-usb-write:')]
    summaries=[r for r in lines if r.startswith('battery-service-test: PASS reads=')]
    if any(len(r)!=1 for r in (prepared,revoked,rows,summaries)):return None
    extent=re.fullmatch(r'workspace-scratch: prepared path=(/work/srvros-work/battery-io-[0-9a-f]{16}\.bin) raw_offset=(\d{1,20}) bytes=4096; unmounted=yes aliases=revoked',prepared[0])
    proof=re.fullmatch(r'battery-usb-write: PASS cycles=(\d{1,20}) commands=(\d{1,20}) busy_cycles=(\d{1,20}) progress_bursts=(\d{1,20}) bursts=(\d{1,20}) max_burst_us=(\d{1,20}) bytes_per_cycle=4096; retired=yes revoked=yes',rows[0])
    if not extent or not proof or revoked[0]!='workspace-scratch: revoked clean=yes':return None
    cycles,commands,busy,progress,bursts,maximum=map(int,proof.groups());offset=int(extent[2])
    if any(n>=2**64 for n in (cycles,commands,busy,progress,bursts,maximum,offset)):return None
    if not 0<busy<=cycles or commands<cycles*3 or not 0<progress<=bursts<=cycles or maximum<250000 or not offset or offset%512:return None
    if not lines.index(prepared[0])<lines.index(revoked[0])<lines.index(rows[0])<lines.index(summaries[0]):return None
    return dict(write_complete=True,cycles=cycles,commands=commands,busy_cycles=busy,
                progress_bursts=progress,bursts=bursts,max_burst_us=maximum,
                path=extent[1],raw_offset=offset,bytes_per_cycle=4096,
                filesystem_mutation_acceptance='pending',input_acceptance='pending')


def battery_filesystem_profile(lines, cycles, slowest, maximum):
    """Validate callback wall times; these include admitted service execution."""
    headers=[r for r in lines if r.startswith('battery-filesystem-profile:')]
    rows=[r for r in lines if r.startswith('battery-filesystem-phase:')]
    if len(headers)!=1 or len(rows)!=4:return None
    number=r'(\d{1,20})'
    header=re.fullmatch('battery-filesystem-profile: cycle='+number+' step='+number+
                        ' elapsed_us='+number+' io_us='+number,headers[0])
    if not header:return None
    cycle,step,elapsed,io_us=map(int,header.groups())
    if not 0<=cycle<cycles or step!=slowest or not 0<elapsed<=maximum or io_us>elapsed:return None
    phases={};total=0
    fields=('phase','reads','read_bytes','read_us','read_max_us','writes',
            'write_bytes','write_us','write_max_us','flushes','flush_us','flush_max_us')
    pattern='battery-filesystem-phase: '+' '.join(k+'='+number for k in fields)
    for row in rows:
        match=re.fullmatch(pattern,row)
        if not match:return None
        values=list(map(int,match.groups()))
        if any(v>=2**64 for v in values):return None
        sample=dict(zip(fields,values));phase=sample.pop('phase')
        if phase>=4 or phase in phases:return None
        for op,count_key in (('read','reads'),('write','writes'),('flush','flushes')):
            count=sample[count_key];duration=sample[op+'_us'];peak=sample[op+'_max_us']
            if not peak<=duration<=count*peak or (not count and peak):return None
            if op!='flush':
                size=sample[op+'_bytes']
                if (not count and size) or (count and (size<count*512 or size%512)):return None
            total+=duration
        phases[phase]=sample
    if total!=io_us or any(lines.index(r)<=lines.index(headers[0]) for r in rows):return None
    names=('metadata','allocation','data','free')
    return dict(cycle=cycle,step=step,elapsed_us=elapsed,io_us=io_us,
                other_us=elapsed-io_us,phases={names[k]:phases[k] for k in range(4)},
                scope='callback wall time includes admitted services; not CPU or input latency')


def battery_filesystem_result(exit_text, require_service=True, desktop=False):
    """Completion of the isolated image workload, not general desktop acceptance."""
    if require_service and battery_service_result(exit_text) is None:return None
    lines=exit_text.splitlines()
    prepared=[r for r in lines if r.startswith('workspace-image: prepared ')]
    revoked=[r for r in lines if r.startswith('workspace-image: revoked ')]
    rows=[r for r in lines if r.startswith('battery-filesystem:')]
    summaries=[r for r in lines if r.startswith('battery-text-load: PASS ' if desktop else 'battery-service-test: PASS reads=')]
    if any(len(r)!=1 for r in (prepared,revoked,rows,summaries)):return None
    extent=re.fullmatch(r'workspace-image: prepared path=/work/srvros-work/battery-fs.img raw_offset=(\d{1,20}) bytes=16777216; unmounted=yes aliases=revoked',prepared[0])
    proof=re.fullmatch(r'battery-filesystem: PASS cycles=(\d{1,20}) commands=(\d{1,20}) busy_cycles=(\d{1,20}) progress_bursts=(\d{1,20}) bursts=(\d{1,20}) max_burst_us=(\d{1,20}) elapsed_us=(\d{1,20})(?: steps=(\d{1,20}) max_step_us=(\d{1,20}) slowest_step=(\d{1,20}))?(?: profile=(1))?; retired=yes revoked=yes',rows[0])
    if not extent or not proof or revoked[0]!='workspace-image: revoked clean=yes':return None
    cycles,commands,busy,progress,bursts,maximum,elapsed=map(int,proof.groups()[:7]);offset=int(extent[1])
    if any(n>=2**64 for n in (cycles,commands,busy,progress,bursts,maximum,elapsed,offset)):return None
    if cycles<2 or not 0<busy<=cycles or commands<cycles*3 or not 0<progress<=bursts or not maximum or elapsed<60000000 or not offset or offset%512:return None
    stepped={}
    if proof[8] is not None:
        steps,step_us,slowest=map(int,proof.groups()[7:10])
        if steps>=2**64 or steps!=cycles*18 or bursts!=steps or not 0<step_us<=maximum or slowest>=18:return None
        stepped=dict(steps=steps,max_step_us=step_us,slowest_step=slowest)
    elif bursts>cycles or maximum<250000:return None
    if not lines.index(prepared[0])<lines.index(revoked[0])<lines.index(rows[0])<lines.index(summaries[0]):return None
    profile_lines=[r for r in lines if r.startswith(('battery-filesystem-profile:', 'battery-filesystem-phase:'))]
    if proof[11] is not None:
        if not stepped:return None
        profile=battery_filesystem_profile(lines,cycles,slowest,step_us)
        if not profile or any(not lines.index(rows[0])<lines.index(r)<lines.index(summaries[0]) for r in profile_lines):return None
        stepped['profile']=profile
    elif profile_lines:return None
    return dict(isolated_complete=True,cycles=cycles,commands=commands,busy_cycles=busy,
                progress_bursts=progress,bursts=bursts,max_burst_us=maximum,elapsed_us=elapsed,
                **stepped,raw_offset=offset,bytes=16777216,input_acceptance='pending',desktop_acceptance='pending')


def verify_battery_image(workspace, proof, saved):
    from battery_filesystem_image import verify, SIZE
    from exfat_test_image import check_clean_image
    source=workspace/'battery-fs.img'
    if source.is_symlink() or not source.is_file() or source.stat().st_size!=SIZE:
        raise ValueError('Missing or unsafe battery filesystem image')
    with source.open('rb') as stream:data=stream.read(SIZE+1)
    if len(data)!=SIZE:raise ValueError('Battery filesystem image changed while reading')
    try:verify(data,proof['cycles'])
    except (AssertionError,ValueError,KeyError,IndexError) as error:
        raise ValueError('Battery filesystem contents do not match completed cycles') from error
    target=saved/'battery-filesystem.exfat';target.write_bytes(data)
    if target.read_bytes()!=data:raise OSError('Battery filesystem image copy failed')
    (saved/'battery-filesystem-fsck.txt').write_text(check_clean_image(target))
    driver=Path(__file__).resolve().parents[1]/'build/tests/exfat-driver'
    result=subprocess.run([str(driver),str(target),'allocation-check'],capture_output=True,text=True)
    (saved/'battery-filesystem-allocation.txt').write_text(result.stdout+result.stderr)
    if result.returncode:raise ValueError('Battery filesystem allocation check failed')
    return hashlib.sha256(data).hexdigest()


def verify_battery_scratch(workspace,proof):
    """Read only the strictly named, fixed-size diagnostic file from this volume."""
    name=proof['path'].removeprefix('/work/srvros-work/')
    if not re.fullmatch(r'battery-io-[0-9a-f]{16}\.bin',name):raise ValueError('Invalid scratch filename')
    source=workspace/name
    if source.is_symlink() or not source.is_file() or source.stat().st_size!=4096:
        raise ValueError('Missing or unsafe scratch payload')
    with source.open('rb') as stream:data=stream.read(4097)
    if data!=bytes((i+proof['cycles']-1)&255 for i in range(4096)):
        raise ValueError('Scratch payload does not match completed cycles')
    return data


def idle_sample(exit_text):
    records=re.findall(r"^idle-capture: elapsed_us=([0-9]{1,20}) sampled_ticks=([0-9]{1,20}) idle_ticks=([0-9]{1,20}) switches=([0-9]{1,20}) preemptions=([0-9]{1,20}) completed=yes$",exit_text,re.M)
    if len(records)!=1:return None
    elapsed,total,idle,switches,preemptions=map(int,records[0])
    if elapsed<59000000 or total==0 or idle>total:return None
    return {"elapsed_us":elapsed,"sampled_ticks":total,"idle_ticks":idle,
            "idle_sample_percent":round(100*idle/total,2),"switches":switches,
            "preemptions":preemptions,"scope":"console with periodic device and USB logging; not power or desktop acceptance"}


def desktop_idle_sample(exit_text):
    records=re.findall(r"^desktop-idle: elapsed_us=([0-9]{1,20}) sampled_ticks=([0-9]{1,20}) idle_ticks=([0-9]{1,20}) switches=([0-9]{1,20}) preemptions=([0-9]{1,20}) frames=([0-9]{1,20}) fb_bytes=([0-9]{1,20}) measured=yes$",exit_text,re.M)
    if len(records)!=1 or exit_text.splitlines().count("desktop-idle: completed=yes screenshot=yes")!=1 or \
       "desktop-idle-capture: returned=yes clean=yes" not in exit_text.splitlines() or "deskd: FAIL" in exit_text:return None
    elapsed,total,idle,switches,preemptions,frames,fb_bytes=map(int,records[0])
    if elapsed<59000000 or not total or idle>total:return None
    return {"elapsed_us":elapsed,"sampled_ticks":total,"idle_ticks":idle,
            "idle_sample_percent":round(100*idle/total,2),"switches":switches,
            "preemptions":preemptions,"frames":frames,"framebuffer_bytes":fb_bytes,
            "scope":"CPU0 scheduler samples with Text open and periodic device/USB logs; not energy or interactive acceptance"}


def power_idle_sample(exit_text):
    """Derive only the covered interval inside a completed desktop idle window.

    Sampling continues during screenshot writes. Those later measurements must
    not masquerade as idle energy. Missing/refused/invalid telemetry yields None.
    """
    idle=desktop_idle_sample(exit_text)
    window=re.findall(r"^desktop-idle-window: start_us=([0-9]{1,20}) end_us=([0-9]{1,20})$",exit_text,re.M)
    if not idle or len(window)!=1:return None
    start,end=map(int,window[0])
    if not 0<=start<end<2**64 or end-start!=idle["elapsed_us"]:return None
    samples={}
    required=("flags","energy_shift","nominal_mhz","sample_us","interval_us","samples","rejected","energy","aperf","mperf","monotonic_us")
    for line in exit_text.splitlines():
        parsed=device_snapshot(line)
        if not parsed or parsed[0]!="power":continue
        s=parsed[1]
        if any(k not in s for k in required):return None
        at=s["sample_us"]
        if at<start or s["monotonic_us"]>end:continue
        if s["power_state"]!=2 or s["flags"]!=3 or s["energy_shift"]!=14 or s["rejected"] or \
           not 0<s["nominal_mhz"]<=6400 or s["nominal_mhz"]%100 or not 100000<=s["interval_us"]<=5000000 or \
           not 0<=s["monotonic_us"]-at<=3000000 or not 0<=s["energy"]<2**32 or \
           any(not 0<=s[k]<2**64 for k in ("aperf","mperf","samples")):return None
        if at in samples and any(samples[at].get(k)!=v for k,v in s.items() if k!="monotonic_us"):return None
        samples[at]=s
    ordered=sorted(samples.values(),key=lambda s:s["sample_us"])
    if len(ordered)<3:return None
    first,last=ordered[0],ordered[-1];elapsed=last["sample_us"]-first["sample_us"]
    if elapsed<20000000:return None
    energy=0
    for a,b in zip(ordered,ordered[1:]):
        dt=b["sample_us"]-a["sample_us"]
        if not 0<dt<=20000000 or b["nominal_mhz"]!=first["nominal_mhz"] or \
           b["samples"]<=a["samples"] or b["aperf"]<a["aperf"] or b["mperf"]<a["mperf"]:return None
        uj=(((b["energy"]-a["energy"])&0xffffffff)*1000000)>>14
        if uj*1000//dt>100000:return None
        energy+=uj
    a=last["aperf"]-first["aperf"];m=last["mperf"]-first["mperf"]
    if not m:return None
    mhz=a*first["nominal_mhz"]//m
    if not 0<mhz<=6000:return None
    return {"start_us":first["sample_us"],"end_us":last["sample_us"],"covered_us":elapsed,
            "desktop_window_us":end-start,"checkpoints":len(ordered),"package_uj":energy,
            "average_package_mw":energy*1000//elapsed,"active_cpu0_mhz":mhz,
            "scope":"covered interval inside desktop idle; package energy and CPU0 active frequency, not whole-laptop power"}


def battery_overlap_recipe(exit_text, recipe):
    # Exact boot-option tokens, not a substring of another capture profile.
    from serial_records import complete_lines
    rows=complete_lines(exit_text.encode()).decode().splitlines()
    boots=[line for line in rows if line.startswith('laptop-capture: boot options=')]
    return len(boots)==1 and ('srvros.capture='+recipe) in boots[0].split('=',1)[1].split()


def battery_overlap_result(exit_text, text=False):
    from serial_records import complete_lines
    from battery_desktop_smoke import check_capture, check_text_capture
    recipe='battery-text' if text else 'battery-desktop'
    if not battery_overlap_recipe(exit_text,recipe):return None
    rows=complete_lines(exit_text.encode()).decode().splitlines()
    marker=recipe+'-capture: returned=yes clean=yes; physical accuracy NOT_TESTED'
    returns=[line for line in rows if line.startswith(recipe+'-capture:')]
    if returns!=[marker]:return None
    try:return (check_text_capture if text else check_capture)(exit_text.encode())
    except (AssertionError,ValueError):return None


def battery_desktop_result(exit_text):
    return battery_overlap_result(exit_text)


def battery_text_result(exit_text):
    return battery_overlap_result(exit_text,text=True)


def battery_load_result(exit_text):
    from serial_records import complete_lines
    from battery_desktop_smoke import check_load_capture
    if not battery_overlap_recipe(exit_text,'battery-load'):return None
    rows=complete_lines(exit_text.encode()).decode().splitlines()
    if [r for r in rows if r.startswith('battery-load-capture:')]!=['battery-load-capture: returned=yes clean=yes; physical accuracy NOT_TESTED']:return None
    try:return check_load_capture(exit_text.encode())
    except (AssertionError,ValueError):return None


def needs_attention(line):
    if any(token in line for token in ("SAVE FAILED","deskd: FAIL","reason=","exception:")):
        return True
    if line.startswith(("laptop-capture: completed=NO", "battery: HALTED", "battery: FAILED", "battery: REFUSED", "battery-capture: returned=yes clean=NO",
                        "battery-transition: INCOMPLETE", "battery-transition-capture: returned=yes clean=NO",
                        "battery-load-capture: returned=yes clean=NO", "battery-text-load: FAIL", "battery-text-capture: returned=yes clean=NO", "battery-desktop: FAIL", "desktop-coexist: completed=NO", "battery-desktop-capture: returned=yes clean=NO",
                        "battery-service-test: FAIL", "battery-service-capture: returned=yes clean=NO",
                        "workspace-image: revoked clean=NO", "workspace-image: preparation FAILED")):return True
    if line.startswith("deskd: brightness requested=") and "result=-1" in line:
        return True
    if re.match(r"(?:audio|smc): state=4(?: |$)",line):
        return True
    parsed=device_snapshot(line)
    if not parsed:return False
    kind,fields=parsed
    if kind=="pcm":return fields["pcm_state"]==6 or fields.get("underruns",0)>0
    if kind=="power":return fields.get("rejected",0)>0 or (fields.get("flags",0)&4)!=0
    return (kind in ("audio","smc") and fields[kind+"_state"]==4) or (kind=="thermal" and fields.get("flags",0)&8)!=0


def collect(mount, volume, output):
    workspace = mount / "srvros-work"
    marker = workspace / ".workspace"
    if workspace.is_symlink() or marker.is_symlink() or not marker.is_file():
        raise ValueError("Prepared workspace missing or unsafe")
    expected = f"SRVROS-WORKSPACE-V1 {volume}\n".encode()
    if marker.stat().st_size != len(expected) or marker.read_bytes() != expected:
        raise ValueError("Workspace marker does not match the USB volume")
    logs = workspace / "Logs"
    if logs.is_symlink() or not logs.is_dir():
        raise ValueError("No desktop session logs on this volume yet")
    sessions = sorted(p for p in logs.iterdir() if SESSION.fullmatch(p.name))
    if not sessions or len(sessions) > 4096:
        raise ValueError("No sessions found, or session count exceeds collection limit")
    # Validate and read everything before creating the local collection.
    snapshots = []
    total_bytes = 0
    for folder in sessions:
        if folder.is_symlink() or not folder.is_dir():
            raise ValueError(f"Unsafe session path: {folder.name}")
        payload = {}
        for name in FILES:
            source = folder / name
            if source.is_symlink():
                raise ValueError(f"Unsafe log path: {folder.name}/{name}")
            if not source.exists():
                continue
            if not source.is_file() or source.stat().st_size > 65536:
                raise ValueError(f"Invalid log: {folder.name}/{name}")
            total_bytes += source.stat().st_size
            if total_bytes > 64*1024*1024:
                raise ValueError("Collection exceeds 64 MiB; archive older session folders first")
            payload[name] = source.read_bytes()
            if len(payload[name]) > 65536:
                raise ValueError("Log changed while reading")
        snapshots.append((folder.name, payload))
    output.mkdir(parents=True, exist_ok=True)
    destination = Path(tempfile.mkdtemp(prefix="collection-", dir=output))
    records = []
    for name, payload in snapshots:
        saved = destination / name
        saved.mkdir()
        for filename, data in payload.items():
            (saved / filename).write_bytes(data)
            if (saved / filename).read_bytes() != data:
                raise OSError("Local collection verification failed")
        text = "\n".join(data.decode("utf-8", errors="replace") for data in payload.values())
        releases = session_releases(text)
        attention = sorted(set(line for line in text.splitlines() if needs_attention(line)))
        if "exit.log" not in payload:
            attention.append("collection: missing exit snapshot; session incomplete")
        if len(releases) > 1:
            attention.append("collection: conflicting boot release identities")
        write_proof=battery_usb_write_result(payload.get("exit.log",b"").decode("utf-8",errors="replace"))
        if write_proof:
            try:
                data=verify_battery_scratch(workspace,write_proof)
                scratch_copy=saved/"battery-scratch.bin"
                scratch_copy.write_bytes(data)
                if scratch_copy.read_bytes()!=data:raise OSError("Scratch copy verification failed")
                write_proof['payload_verified']=True
                write_proof['payload_sha256']=hashlib.sha256(data).hexdigest()
            except (OSError,ValueError) as error:
                attention.append("collection: "+str(error))
                write_proof=None
        image_proof=battery_filesystem_result(payload.get("exit.log",b"").decode("utf-8",errors="replace"))
        if 'srvros.capture=battery-filesystem' in text and image_proof is None:
            attention.append('collection: isolated filesystem completion missing or invalid')
        if image_proof:
            try:
                image_proof['image_sha256']=verify_battery_image(workspace,image_proof,saved)
                image_proof['image_verified']=True
            except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as error:
                attention.append("collection: "+str(error));image_proof=None
        load_proof=battery_load_result(payload.get("exit.log",b"").decode("utf-8",errors="replace"))
        if 'srvros.capture=battery-load' in text and load_proof is None:
            attention.append('collection: Text/filesystem overlap completion missing or invalid')
        if load_proof:
            try:
                load_proof['image_sha256']=verify_battery_image(workspace,load_proof['filesystem'],saved)
                load_proof['image_verified']=True
            except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as error:
                attention.append("collection: "+str(error));load_proof=None
        desktop_proof=battery_desktop_result(payload.get("exit.log",b"").decode("utf-8",errors="replace"))
        text_proof=battery_text_result(payload.get("exit.log",b"").decode("utf-8",errors="replace"))
        if 'srvros.capture=battery-text' in text and text_proof is None:
            attention.append('collection: battery Text overlap completion missing or invalid')
        if 'srvros.capture=battery-desktop' in text and desktop_proof is None:
            attention.append('collection: battery desktop overlap completion missing or invalid')
        records.append({
            "session": name,
            "releases": releases,
            "files": {key: hashlib.sha256(data).hexdigest() for key, data in payload.items()},
            "exit_snapshot_present": "exit.log" in payload,
            "desktop_exit_seen": "deskd: exited" in text,
            "attention": attention,
            "devices": device_summaries(text),
            "desktop_idle_sample": desktop_idle_sample(payload.get("exit.log",b"").decode("utf-8",errors="replace")),
            "power_idle_sample": power_idle_sample(payload.get("exit.log",b"").decode("utf-8",errors="replace")),
            "battery_transitions": battery_transitions(payload.get("exit.log",b"").decode("utf-8",errors="replace")),
            "battery_load": load_proof,
            "battery_text": text_proof,
            "battery_desktop": desktop_proof,
            "battery_filesystem": image_proof,
            "battery_usb_write": write_proof,
            "battery_usb_read": battery_usb_read_result(payload.get("exit.log",b"").decode("utf-8",errors="replace")),
            "battery_service": battery_service_result(payload.get("exit.log",b"").decode("utf-8",errors="replace")),
            "battery_diagnostic": battery_diagnostic(payload.get("exit.log",b"").decode("utf-8",errors="replace")),
            "idle_sample": idle_sample(payload.get("exit.log",b"").decode("utf-8",errors="replace")),
            "probe": sorted(set(line for line in text.splitlines() if line.startswith("laptop-probe:"))),
            "brightness": sorted(set(line for line in text.splitlines() if line.startswith(("backlight:", "deskd: brightness", "desktools: brightness")))),
        })
    report = {"format": "srvros-desktop-collection-v1", "volume": volume,
              "hardware_acceptance": "manual-pending", "sessions": records}
    (destination / "collection.json").write_text(json.dumps(report, indent=2) + "\n")
    return destination, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uuid", default="37d8c5d0")
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "build/desktop-collections")
    args = parser.parse_args()
    try:
        volume = host.volume_id(args.uuid)
        destination, report = collect(host.volume_mount(volume), volume, args.output)
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f"desktop-collect: {error}\n")
    print(f"desktop-collect: {len(report['sessions'])} sessions copied and verified: {destination}")
    for item in report["sessions"]:
        release = ",".join(item["releases"]) or "unknown"
        print(f"  {item['session']}: release={release}, exit log={'yes' if item['exit_snapshot_present'] else 'missing'}, attention={len(item['attention'])}")
    print("Logs are diagnostic evidence, not an automatic hardware pass.")


if __name__ == "__main__":
    main()
