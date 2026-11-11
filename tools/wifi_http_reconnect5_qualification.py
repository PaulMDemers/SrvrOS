"""Synthetic full-RF composition qualification; NEVER physical acceptance.

Runs the actual two-generation HOST fixture using the captured parent's exact
restored endpoint words and slot. Replaces only boot identities/old child rows,
inserts actual new child rows, and retotals the saved prefix. Every RF/outer
parent row remains byte-for-byte unchanged. Its output must never be collected
or archived as a physical run. No credentials, mounts, native USB or power work.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

import wifi_http_reconnect5_log as checker
from wifi_held_log import require
from wifi_power5_log import fields

HOST_LABEL='http-reconnect5-host: modeled_usb=1 modeled_scheduler=1 modeled_child=1 acceptance=0'
MARK='workspace-log: snapshot=exit.log '
OLD_CHILD=('wifi-stream5','wifi-stack5','wifi-http5','wifi-reconnect5','wifi-http-reconnect5')
IDENTITY=('cmdline: ','laptop-capture: boot options=')
TAG=checker.QUALIFICATION_TAG


def sha(data):return hashlib.sha256(data).hexdigest()


def retotal(log):
    matches=list(re.finditer(r'^workspace-log: snapshot=exit.log monotonic_us=(0|[1-9][0-9]*) total_before=(0|[1-9][0-9]*) tail_limit=4194304\n',log,re.M))
    require(len(matches)==1 and matches[0].end()==len(log),'incomplete qualification saved prefix')
    mark=matches[0]
    return log[:mark.start()]+re.sub(r'total_before=\d+',f'total_before={len(log[:mark.start()].encode())}',mark[0])


def parent_words(parent):
    lines=parent.splitlines();contexts={}
    plan=[fields(s,'wifi-join5-plan') for s in lines if s.startswith('wifi-join5-plan:')]
    require(len(plan)==1 and 0<plan[0]['slot']<256,'missing captured native parent slot')
    for dci in (9,8):
        found=[fields(s,'wifi-join5-context') for s in lines if s.startswith(f'wifi-join5-context: phase=8 dci={dci} ')]
        require(len(found)==1 and all(0<=found[0]['w'+str(w)]<2**32 for w in range(8)),
                'missing captured restored parent words')
        contexts[dci]=[found[0]['w'+str(w)] for w in range(8)]
    return contexts[9]+contexts[8]+[plan[0]['slot']]


def _preserved(lines):
    return [s for s in lines if not s.startswith(OLD_CHILD+IDENTITY+(MARK,)) and s!=TAG]


def compose(parent,trace,release,volume,*,startup_journal=False):
    require(TAG not in parent and HOST_LABEL not in parent,'synthetic log cannot supply accepted RF parent')
    require(re.fullmatch('[0-9a-f]{16}',release) and re.fullmatch('[0-9a-f]{8}',volume),
            'invalid synthetic qualification identity')
    require(trace.splitlines().count(HOST_LABEL)==1,'fixture lacks explicit modeled label')
    actual=[s for s in trace.splitlines() if s.startswith(OLD_CHILD) and s!=HOST_LABEL]
    require(actual and actual[0]=='wifi-http-reconnect5-prepare: version=1 generation=1 prepared=1 retained=1' and
            actual[-1].startswith('wifi-http-reconnect5-final:'),'fixture lacks complete bracketed native groups')
    original=parent.splitlines();lines=[];seen=set()
    require(type(startup_journal) is bool,'invalid startup journal fixture mode')
    metadata={'srvros.startup-journal=1'} if startup_journal else set()
    command=' '.join(sorted(checker.BOOT_FIXED|metadata|{'srvros.desktop-release='+release,'srvros.workspace-volume='+volume}))
    for line in original:
        identity=next((p for p in IDENTITY if line.startswith(p)),None)
        if identity:
            require(identity not in seen,'duplicate qualification boot identity');seen.add(identity)
            lines.append(identity+command)
        elif line in (checker.stack.BOOT,checker.reconnect.BOOT):lines.append(checker.BOOT)
        elif line==checker.stack.LOG_PARKED:lines.append(line)
        elif not line.startswith(OLD_CHILD):lines.append(line)
    require(seen==set(IDENTITY) and lines.count(checker.BOOT)==1 and
            lines.count(checker.stack.LOG_PARKED)==1,'missing captured boot/logger boundary')
    joins=[i for i,s in enumerate(lines) if s.startswith('wifi-join5:')]
    require(len(joins)==1,'missing unique captured join parent')
    at=joins[0]+1
    while at<len(lines) and lines[at].startswith(('wifi-join5-response:','wifi-join5-cleanup:')):at+=1
    lines[at:at]=actual[1:-1]
    first_firmware=next(i for i,s in enumerate(lines) if s.startswith('wifi-firmware-control:'))
    lines.insert(first_firmware,actual[0])
    firmware=[i for i,s in enumerate(lines) if s.startswith('wifi-firmware-capture:')]
    require(len(firmware)==1,'missing unique captured outer completion')
    lines[firmware[0]:firmware[0]]=[actual[-1]]
    lines.insert(0,TAG)
    require(_preserved(original)==_preserved(lines),'RF/outer parent bytes were changed during composition')
    text=retotal('\n'.join(lines)+'\n')
    return text,dict(synthetic=True,native_acceptance=False,unchanged_outer_rows=len(_preserved(lines)),
                     unchanged_outer_sha256=sha(('\n'.join(_preserved(lines))+'\n').encode()),
                     parent_sha256=sha(parent.encode()),trace_sha256=sha(trace.encode()),
                     composition_sha256=sha(text.encode()))


def generate(parent_path,binary_path,destination,*,release='0123456789abcdef',volume='12345678',startup_journal=False):
    parent_path=Path(parent_path).resolve();binary_path=Path(binary_path).resolve();destination=Path(destination)
    oldmask=os.umask(0o077)
    try:destination.mkdir(mode=0o700,parents=True,exist_ok=False)
    finally:os.umask(oldmask)
    source=parent_path.read_bytes();parent=source.decode('utf8');binary_sha=sha(binary_path.read_bytes())
    argv=[str(binary_path),'--trace']+[str(v) for v in parent_words(parent)]
    run=subprocess.run(argv,capture_output=True,text=True,timeout=120)
    (destination/'host-trace.log').write_text(run.stdout);(destination/'host-stderr.log').write_text(run.stderr)
    require(run.returncode==0,'source-bound native HOST fixture refused captured parent context')
    composed,proof=compose(parent,run.stdout,release,volume,startup_journal=startup_journal)
    if startup_journal:proof["startup_journal_profile"]=True
    (destination/'synthetic-composition.log').write_text(composed)
    sessions=re.findall(r'^workspace-log: directory=/work/srvros-work/Logs/(session-[0-9a-f]{16}-[0-9a-f]{2})(?: |$)',parent,re.M)
    require(len(sessions)==1,'captured parent lacks unique saved session')
    result=checker.check_qualification(composed,release,volume=volume,session=sessions[0])
    require(source==parent_path.read_bytes() and binary_sha==sha(binary_path.read_bytes()),
            'qualification inputs changed during fixture')
    proof.update(passed=True,parent_path=str(parent_path),binary_path=str(binary_path),binary_sha256=binary_sha,
                 command=argv,fixture_exit=run.returncode,result=result,native_acceptance=False,
                 scope='Synthetic full-RF composition and actual modeled-native two-generation checker seam only; not a physical run')
    (destination/'proof.json').write_text(json.dumps(proof,indent=2)+'\n')
    return composed,proof


if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--parent',required=True);p.add_argument('--binary',required=True);p.add_argument('--output',required=True)
    p.add_argument('--startup-journal',action='store_true')
    args=p.parse_args();_,proof=generate(args.parent,args.binary,args.output,startup_journal=args.startup_journal)
    print(json.dumps({k:proof[k] for k in ('passed','synthetic','native_acceptance','parent_sha256','trace_sha256','composition_sha256')},indent=2))
