#!/usr/bin/env python3
"""Validate a complete, counterbalanced fixed-file storage wait capture."""
import re
import statistics
from usb_timing import transfer_records, sample_transfers

ORDER=((0,4000,20000),(4000,20000,0),(20000,0,4000),
       (20000,4000,0),(4000,0,20000),(0,20000,4000))
PAYLOAD=bytes((i*37+11)&255 for i in range(32768))
FIELDS=('round position poll_us bytes elapsed_us sampled_ticks idle_ticks cbw_calls cbw_us '
        'data_calls data_us csw_calls csw_us failures ok').split()

def capture_result(text):
    starts=re.findall(r'^storage-wait: begin version=([12]) path=(/work/srvros-work/Logs/session-[0-9a-f]{16}-[0-9a-f]{2}/wait-fixture.bin) bytes=32768 rounds=6 policies=3 original_us=([0-9]{1,20})$',text,re.M)
    ends=re.findall(r'^storage-wait: complete=(\S+) samples=([0-9]{1,20}) restored=(\S+) elapsed_us=([0-9]{1,20})$',text,re.M)
    if len(starts)!=1:return None
    version=int(starts[0][0]);fields=FIELDS.copy()
    if version==2:fields.insert(3,'start_us')
    pattern=r'storage-wait: sample '+' '.join(k+r'=([0-9]{1,20})' for k in fields)
    rows=[line for line in text.splitlines() if line.startswith('storage-wait: sample')]
    matches=[re.fullmatch(pattern,line) for line in rows]
    if any(m is None for m in matches):return None
    records=[dict(zip(fields,map(int,m.groups()))) for m in matches]
    transfers=transfer_records(text,'comparison')
    if transfers is None or (version==2 and len(transfers)!=54):return None
    if len(starts)!=1 or len(ends)!=1 or len(records)!=18:return None
    if ends[0][:3]!=('yes','18','yes') or starts[0][2] not in ('4000','20000'):return None
    for i,r in enumerate(records):
        if any(v>=2**64 for v in r.values()):return None
        if version==2:
            detail=sample_transfers(transfers,r['start_us'],r)
            if detail is None:return None
            if i and records[i-1]['start_us']+records[i-1]['elapsed_us']>r['start_us']:return None
        if (r['round'],r['position'],r['poll_us'])!=(i//3,i%3,ORDER[i//3][i%3]):return None
        if r['bytes']!=32768 or r['ok']!=1 or r['failures'] or not r['elapsed_us']:return None
        if r['idle_ticks']>r['sampled_ticks']:return None
        if not all(r[k+'_calls'] for k in ('cbw','data','csw')):return None
        if sum(r[k+'_us'] for k in ('cbw','data','csw'))>r['elapsed_us']:return None
    if version==2:
        for r in records:r['transfers']=sample_transfers(transfers,r['start_us'],r)
    if not sum(r['elapsed_us'] for r in records)<=int(ends[0][3])<2**64:return None
    groups=[]
    for policy in (0,4000,20000):
        rows=[r for r in records if r['poll_us']==policy];times=[r['elapsed_us'] for r in rows]
        ticks=sum(r['sampled_ticks'] for r in rows);idle=sum(r['idle_ticks'] for r in rows)
        groups.append(dict(poll_us=policy,count=len(rows),total_us=sum(times),median_us=statistics.median(times),min_us=min(times),max_us=max(times),sampled_ticks=ticks,idle_ticks=idle,idle_sample_percent=100*idle/ticks if ticks else None,csw_us=sum(r['csw_us'] for r in rows)))
    return dict(version=version,path=starts[0][1],original_poll_us=int(starts[0][2]),records=records,policies=groups,elapsed_us=int(ends[0][3]),
        scope='Same 32 KiB overwrite/readback/flush, six balanced orders in one boot. Includes metadata and yielded work. Timer samples are not CPU runtime; no physical input or power acceptance.')
