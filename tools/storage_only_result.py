"""Strict receipt for the explicit fixed32KiB storage-only control.

Wall/CSW observation is software evidence, not device completion time. This
profile does not replace the independent 30-minute combined workload.
"""
import re
import zlib

BYTES=32768
LIMIT=180000000
ORDER=[((i//2)^(i%2))&1 for i in range(12)]

def payload(sequence):
    return bytes((i*37+sequence*29+11)&255 for i in range(BYTES))

def rows(text,prefix):
    result=[]
    for line in text.splitlines():
        if not line.startswith(prefix+':'):continue
        if len(line)>=1024:raise ValueError('truncated/oversize row')
        values={}
        for field in line[len(prefix)+2:].split():
            key,value=field.split('=')
            if key in values:raise ValueError('duplicate field')
            values[key]=int(value)
            if not 0<=values[key]<2**64:raise ValueError('out of range')
        result.append(values)
    return result

def capture_result(text,require_audio=True):
    try:
        begin=re.findall(r'^storage-only: begin version=1 path=(/work/srvros-work/storage-compare-[0-9a-f]{16}\.bin) start_us=(\d+) bytes=32768 pairs=6 samples=12 limit_us=180000000 policy=unchanged$',text,re.M)
        end=re.findall(r'^storage-only: complete=(\S+) samples=(\d+) elapsed_us=(\d+) mounted=(\d+) clean=(\d+) final_seq=(\d+)$',text,re.M)
        assert len(begin)==len(end)==1
        path,start=begin[0];start=int(start);status,count,elapsed,mounted,clean,final=end[0]
        assert status=='yes' and count==final=='12' and mounted==clean=='1' and 0<int(elapsed)<=LIMIT
        assert start and int(path[-20:-4],16)==start
        groups=[rows(text,p) for p in ('storage-only-sample','storage-only-command','storage-only-service')]
        assert all(len(g)==12 and [r['seq'] for r in g]==list(range(1,13)) for g in groups)
        lease_lines=re.findall(r'^storage-only-lease: seq=(\d+) offset=(\d+) bytes=32768 parked=1 aliases_revoked=1 claimed=1$',text,re.M)
        leases={int(s):int(o) for s,o in lease_lines}
        assert len(lease_lines)==len(leases)==6 and set(leases)=={i+1 for i,k in enumerate(ORDER) if k}
        assert text.count('storage-only-lease:')==6 and text.count('storage-only:')==2
        previous=start
        for i,(r,q,b) in enumerate(zip(*groups)):
            assert r['version']==q['version']==b['version']==1 and r['kind']==ORDER[i] and r['bytes']==BYTES
            assert previous<=r['start_us']<r['end_us']<=start+int(elapsed)
            previous=r['end_us']
            assert r['write_us']+r['flush_us']+r['read_us']==r['end_us']-r['start_us']
            assert r['crc']==zlib.crc32(payload(i+1)) and r['ok']==r['valid']==r['flush_valid']==r['read_valid']==1 and r['failures']==0
            assert r['cbws']==r['csws']>0 and r['flush_csws']>=1 and r['read_csws']>=2
            assert r['cbw_us']+r['data_us']+r['csw_us']<=r['write_us']
            assert r['retired']==ORDER[i]
            if ORDER[i]:
                assert r['offset']==leases[i+1]>0 and r['offset']%512==0
                assert (r['cbws'],r['data_calls'],r['csws'],r['flush_csws'],r['read_csws'])==(2,8,2,1,2)
                assert q['opcode']==0x2a and q['bytes']==16384
            else:assert r['offset']==0
            assert q['valid']==q['complete']==q['ok']==1 and q['role']<7 and q['opcode'] in (0x28,0x2a,0x35)
            assert q['cbw_calls']==q['csw_calls']==1 and q['csw_us']>0
            assert r['start_us']<=q['start_us']<=q['csw_start']<q['csw_end']<=q['end_us']<=r['start_us']+r['write_us']
            assert q['csw_us']==q['csw_end']-q['csw_start']
            assert q['cbw_us']+q['data_us']+q['csw_us']+q['ck_us']<=q['end_us']-q['start_us']
            assert q['bytes']<=16384 and q['bytes']%512==0 and q['data_req']==q['data_actual']==q['bytes']
            assert q['data_calls']==(q['bytes']+4095)//4096
            assert (q['opcode']==0x35)==(q['bytes']==0)
            assert b['start_us']==q['csw_start'] and b['end_us']==q['csw_end'] and b['requested']==b['actual']==13
            assert b['ok']==b['seen']==b['valid']==1
            assert b['start_us']<=b['submitted']<=b['observed']<=b['resumed']<=b['end_us']
            assert b['checks'] and b['samples']==(b['checks']-1)//256+1 and b['events']>=1 and b['empty']<=b['checks']
            observe=b['observed']-b['submitted']
            assert b['completion_gap']<=b['gap']<=observe and observe<=(b['samples']+1)*b['gap']
            assert b['gap']<=b['end_us']-b['start_us'] and b['completion_gap']<=b['end_us']-b['start_us']
            assert (b['empty']>0)==(b['last_empty']>0)
            if b['last_empty']:assert b['start_us']<=b['last_empty']<=b['observed']
        assert len('\n'.join(l for l in text.splitlines() if l.startswith('storage-only'))) <32768
        if require_audio:
            assert len(re.findall(r'^storage-only-audio: volume=1 cues=2 stopped=yes quiet=yes clean=yes$',text,re.M))==1
            assert 'laptop-capture: completed=yes' in text
        return dict(status='PASS',path=path,bytes=BYTES,pairs=6,samples=12,elapsed_us=int(elapsed),volume=1,
                    filesystem=[r for r in groups[0] if not r['kind']],direct=[r for r in groups[0] if r['kind']],
                    commands=groups[1],service=groups[2],native_acceptance=False)
    except (AssertionError,KeyError,ValueError,TypeError):return None
