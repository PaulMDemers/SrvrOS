#!/usr/bin/env python3
"""Explicitly synthetic PR261 checkpoint budget, never a native trace or pass.

Keep check_cpu_capture_tail's source-match guard intact. Combine historical
native proofs with source-derived maximum *additional* checkpoint output. This
measures the new diagnostic budget; only a real new native log accepts it.
"""
import argparse
import ast
import hashlib
import json
from pathlib import Path
import re
import subprocess
from check_cpu_capture_tail import assess

SOURCES=('kernel/src/scheduler.c','kernel/src/workspace_log.c','kernel/src/fs/exfat.c','kernel/include/srvros/exfat.h')

def function(source,name):
    start=source.index('void '+name+'(') if name!='device_checkpoint' else source.index('static void device_checkpoint(')
    at=source.index('{',start);depth=1;end=at+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]

def formats(body):
    return [ast.literal_eval(x) for x in re.findall(r'console_printf\(("(?:[^"\\]|\\.)*")',body)]

def render(fmt,string_width=16):
    out=re.sub(r'%u',str(2**64-1),fmt).replace('%s','x'*string_width)
    if '%' in out:raise ValueError('unsupported formatter')
    return out

def checkpoint_addition(current,old):
    # Refuse silently modeling an unrelated logger or future output contract.
    before=function(old['kernel/src/workspace_log.c'],'device_checkpoint')
    after=function(current['kernel/src/workspace_log.c'],'device_checkpoint')
    if after.replace('    exfat_print_costs();\n','')!=before or after.count('exfat_print_costs();')!=1:
        raise ValueError('unmodeled device checkpoint change')
    old_fmt=formats(function(old['kernel/src/scheduler.c'],'scheduler_print_io_owners'))
    new_fmt=formats(function(current['kernel/src/scheduler.c'],'scheduler_print_io_owners'))
    added=[x for x in new_fmt if x not in old_fmt]
    if len(added)!=2 or not added[0].startswith('io-gate-snapshot:') or not added[1].startswith('io-gate:'):
        raise ValueError('unmodeled scheduler output change')
    if [x for x in new_fmt if x not in added]!=old_fmt:raise ValueError('old owner formatter changed')
    n=int(re.search(r'#define IO_OBSERVE_ROWS (\d+)',current[SOURCES[0]])[1])
    costs=int(re.search(r'#define EXFAT_COST_ROWS (\d+)',current[SOURCES[2]])[1])
    enum=re.search(r'enum exfat_io_phase \{([^}]+)',current[SOURCES[3]])[1]
    phases=len(re.findall(r'EXFAT_IO_(?!PHASE_COUNT)[A-Z]+',enum))
    if (n,costs,phases)!=(8,4,4):raise ValueError('unreviewed capacity change')
    cost_fmt=formats(function(current[SOURCES[2]],'exfat_print_costs'))
    if len(cost_fmt)!=2 or not cost_fmt[0].startswith('exfat-cost:') or not cost_fmt[1].startswith('exfat-cost-phase:'):
        raise ValueError('unmodeled cost output change')
    operations=re.findall(r'mutation_begin(?:_result)?\([^;]+?,\s*"([^"]+)"\)',current[SOURCES[2]])
    operations+=re.findall(r'cost\.operation="([^"]+)"',current[SOURCES[2]])
    if not operations:raise ValueError('missing bounded operation labels')
    label_width=max(len(x) for x in operations)
    # Four costs x four phases. Owner gate rows include the separately retained
    # exit attribution in addition to eight longest waits. All fields are u64
    # width, conservative for small phase/count/boolean fields too.
    extra=render(added[0])+render(added[1])*(n+1)
    extra+=(render(cost_fmt[0],label_width)+render(cost_fmt[1])*phases)*costs
    return extra,dict(gate_rows=n+1,cost_rows=costs,cost_phase_rows=costs*phases)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--log',type=Path,required=True)
    ap.add_argument('--trace-source-ref',required=True)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args()
    current={x:Path(x).read_text() for x in SOURCES}
    old={x:subprocess.check_output(['git','show',args.trace_source_ref+':'+x],text=True) for x in SOURCES}
    extra,capacity=checkpoint_addition(current,old)
    text=args.log.read_text()
    if 'io-gate-snapshot:' in text or 'exfat-cost:' in text:raise ValueError('input already contains new reports')
    if text.count('laptop-capture: completed=yes\n')!=1:raise ValueError('need unique completed trace')
    modeled=text+extra
    result=assess(modeled)
    result.update(status='SYNTHETIC_BUDGET_PASS' if result['status']=='COVERAGE_PASS' else 'SYNTHETIC_BUDGET_FAIL',
                  scope='Historical proofs plus reviewed current-source maximum added final checkpoint output; not source-matched native replay',
                  trace_source_ref=args.trace_source_ref,
                  trace_sha256=hashlib.sha256(args.log.read_bytes()).hexdigest(),
                  current_source_sha256={x:hashlib.sha256(v.encode()).hexdigest() for x,v in current.items()},
                  old_source_sha256={x:hashlib.sha256(v.encode()).hexdigest() for x,v in old.items()},
                  additional_checkpoint_bytes=len(extra.encode()),capacities=capacity)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result))
    if result['status']!='SYNTHETIC_BUDGET_PASS':raise SystemExit(1)

if __name__=='__main__':main()
