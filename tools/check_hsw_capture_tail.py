#!/usr/bin/env python3
"""Conservative dedicated-profile retention, explicitly not native acceptance."""
import argparse
import hashlib
import json
import re
from pathlib import Path
from collections import Counter
from check_cpu_checkpoint_budget import function,formats
from hardware_haswell_backing import backing_result
import hardware_boot as boot

SOURCES=('kernel/src/main.c','kernel/src/workspace_log.c','kernel/src/scheduler.c',
         'kernel/src/fs/exfat.c','kernel/include/srvros/exfat.h',
         'kernel/diagnostics/haswell_backing_core.c','kernel/diagnostics/haswell_backing_adapter.inc',
         'kernel/include/srvros/haswell_backing.h','tools/hardware_haswell_backing.py','tools/hardware_boot.py','tools/check_hsw_capture_tail.py')

def render(fmt,string_width=32):
    text=re.sub(r'%[udx]',str(2**64-1),fmt).replace('%s','x'*string_width)
    if '%' in text:raise ValueError('unmodeled formatter')
    return text

def postamble_bound():
    main=Path(SOURCES[0]).read_text();log=Path(SOURCES[1]).read_text();sched=Path(SOURCES[2]).read_text();fs=Path(SOURCES[3]).read_text()
    for version in (1,2):
        config=boot.limine_config('test',f'ram-usb-v{version}-hsw-laptop-probe-1234abcd-auto-return')
        assert 'srvros.power=' not in config
        assert ('srvros.hsw-cpu-control=read-only' in config)==(version==2)
    assert main.count('power_init();')==1 and 'if(cmdline_has_token(cmdline,"srvros.power=read-only")) power_init();' in main
    # Power stays disabled in this dedicated profile, so activity/worker rows
    # cannot be emitted by device_checkpoint. Other bounded reports stay intact.
    device=function(log,'device_checkpoint')
    assert 'if(power.state!=POWER_DISABLED)' in device and device.count('scheduler_print_activity();')==1
    d=formats(device);assert len(d)==6 and d[-1].startswith('workspace-devices: power_state=')
    io=formats(function(sched,'scheduler_print_io_owners'))
    assert len(io)==5 and '#define IO_OBSERVE_ROWS 8' in sched
    costs=formats(function(fs,'exfat_print_costs'));assert len(costs)==2 and '#define EXFAT_COST_ROWS 4' in fs
    enums=Path(SOURCES[4]).read_text();assert len(re.findall(r'EXFAT_IO_(?!PHASE_COUNT)[A-Z]+',re.search(r'enum exfat_io_phase \{([^}]+)',enums)[1]))==4
    # Max uint64 widths; string allowance 32 covers bounded thread names,
    # operation names, save names and the short scope label.
    total=sum(len(render(x,32)) for x in d[:-1])
    total+=sum(n*len(render(x,32)) for n,x in zip((1,8,1,9,1),io))
    total+=4*len(render(costs[0],32))+16*len(render(costs[1],32))
    for name,mult in [('report_save',1),('report_transfers',3)]:
        body=function(log,name) if name=='report_save' else function(log,name)
        fmts=formats(body);assert len(fmts)==1;total+=mult*len(render(fmts[0],32))
    total+=1024 # final snapshot marker and bounded completion/return messages.
    assert total<=32768,total
    return total

def assess(text):
    result=backing_result(text)
    if not result or result['status']!='OBSERVED':raise ValueError('complete source receipt required')
    bound=postamble_bound()
    stressed='synthetic-pre-final:'+'x'*131072+'\n'+text+'synthetic-post-final:'+'x'*32768+'\n'
    tail=stressed.encode()[-65536:].decode(errors='replace')
    records=lambda t:Counter(x for x in t.splitlines() if x.startswith(('hsw-backing:','hsw-backing-meta:','hsw-backing-ptes:','laptop-capture: completed=')))
    same=records(text)==records(tail)
    parsed=backing_result(tail)
    return dict(status='COVERAGE_PASS' if same and parsed and parsed['status']=='OBSERVED' else 'COVERAGE_FAIL',synthetic=True,physical_acceptance=False,tail_bytes=65536,pre_final_chatter=131072,post_final_allowance=32768,source_postamble_bound=bound,raw_records_identical=same,source_sha256={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in SOURCES})

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--log',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    r=assess(a.log.read_text());a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps({k:v for k,v in r.items() if k!='source_sha256'}))
    if r['status']!='COVERAGE_PASS':raise SystemExit(1)
