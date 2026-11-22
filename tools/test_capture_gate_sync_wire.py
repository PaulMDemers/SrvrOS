"""Actual live C observer -> production workspace formatter -> strict parser."""
import argparse
from pathlib import Path
import subprocess
import re
from capture_write_timing import capture_write_timing, capture_gate_timing


def parse(raw):
    writes=capture_write_timing([raw],1,71000)
    return capture_gate_timing([raw],writes)['workers'][0]['sync_context']


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,required=True)
    args=parser.parse_args()
    raw=subprocess.check_output([str(args.binary.resolve())])
    context=parse(raw)
    assert context['state']==3 and context['content_bound'] and context['parent_bound']
    q=context['observation']
    assert (q['owner_pid'],q['sync_id'],q['identity'],q['size'],q['content_start_us'])==(7,100,444,65536,110)
    assert context['io_us']==20 and context['io'][2]['write_bytes']==1024 and context['io'][3]['read_bytes']==4096
    assert max(map(len,raw.splitlines()))<1024
    # Explicit unavailable and parent-only observations preserve their meaning;
    # zero rows cannot masquerade as a completed/empty content operation.
    for state in (0,1,2):
        rows=[]
        for line in raw.decode().splitlines():
            if line.startswith('capture-write-gate-sync:'):
                line=line.replace('state=3',f'state={state}')
                fields='slot identity inode size content_start_us'.split()
                if state<2:fields+=['owner_pid','sync_id','phase']
                for field in fields:line=re.sub(r'\b'+field+r'=\d+',field+'=0',line)
            elif line.startswith('capture-write-gate-sync-io:'):
                for field in 'reads read_bytes read_us writes write_bytes write_us flushes flush_us failures'.split():
                    line=re.sub(r'\b'+field+r'=\d+',field+'=0',line)
            rows.append(line)
        value=parse(('\n'.join(rows)+'\n').encode())
        assert value['state']==state and not value['content_bound'] and value['parent_bound']==(state==2)
    changes=[
        (b'state=3 invalid=0',b'state=3 invalid=1'),
        (b'owner_pid=7 sync_id=100',b'owner_pid=8 sync_id=100'),
        (b'sync_id=100',b'sync_id=300'),
        (b'sync_id=100',b'sync_id=119'),
        (b'phase=0 slot=2',b'phase=1 slot=2'),
        (b'identity=444',b'identity=0'),
        (b'content_start_us=110',b'content_start_us=121'),
        (b'size=65536 content_start',b'size=16777217 content_start'),
        (b'write_us=12',b'write_us=51'),
        (b'writes=1 write_bytes=1024',b'writes=0 write_bytes=1024'),
        (b'failures=0 final=1',b'failures=1 final=1'),
        (b'state=3 invalid=0',b'state=2 invalid=0'),
        (b'state=3 invalid=0',b'state=1 invalid=0'),
        (b'state=3 invalid=0',b'state=0 invalid=0'),
        (b'role=5 reads=',b'role=6 reads='),
        (b'identity=444',b'identity=18446744073709551616'),
        (b'content_start_us=110',b'unknown=110'),
        (b'capture-write-gate-owner: version=2',b'capture-write-gate-owner: version=1'),
    ]
    bad=[raw.replace(a,b) for a,b in changes]
    header=next(line for line in raw.splitlines() if line.startswith(b'capture-write-gate-sync:'))
    role=next(line for line in raw.splitlines() if line.startswith(b'capture-write-gate-sync-io:'))
    bad.extend([raw.replace(header+b'\n',b''),raw.replace(role+b'\n',b''),
                raw+b'\n'+header.replace(b'identity=444',b'identity=445')+b'\n',
                raw+b'\n'+header.replace(b'pid=42',b'pid=43')+b'\n'])
    for value in bad:
        try:parse(value)
        except ValueError:pass
        else:raise AssertionError('Unbound/missing/corrupt exact owner observation accepted')
    print('capture-gate-sync-wire: PASS actual live C observer/production formatter, exact parent/content counters, legacy separation and missing/conflicting/future/failed/overflow attribution refusal')


if __name__=='__main__':main()
