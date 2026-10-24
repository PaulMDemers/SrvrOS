#!/usr/bin/env python3
"""Validate a complete, counterbalanced fixed-file storage wait capture."""
import re
import statistics

ORDER=((0,4000,20000),(4000,20000,0),(20000,0,4000),
       (20000,4000,0),(4000,0,20000),(0,20000,4000))
PAYLOAD=bytes((i*37+11)&255 for i in range(32768))
FIELDS=('round position poll_us bytes elapsed_us sampled_ticks idle_ticks cbw_calls cbw_us '
        'data_calls data_us csw_calls csw_us failures ok').split()
PATTERN=r'storage-wait: sample '+ ' '.join(k+r'=(\d+)' for k in FIELDS)

def capture_result(text):
    starts=re.findall(r'storage-wait: begin version=1 path=(/work/srvros-work/Logs/session-[0-9a-f]{16}-[0-9a-f]{2}/wait-fixture.bin) bytes=32768 rounds=6 policies=3 original_us=(\d+)',text)
    ends=re.findall(r'storage-wait: complete=(\S+) samples=(\d+) restored=(\S+) elapsed_us=(\d+)',text)
    records=[dict(zip(FIELDS,map(int,m))) for m in re.findall(PATTERN,text)]
    if len(starts)!=1 or len(ends)!=1 or len(records)!=18:return None
    if ends[0][:3]!=('yes','18','yes') or starts[0][1] not in ('4000','20000'):return None
    for i,r in enumerate(records):
        if (r['round'],r['position'],r['poll_us'])!=(i//3,i%3,ORDER[i//3][i%3]):return None
        if r['bytes']!=32768 or r['ok']!=1 or r['failures'] or not r['elapsed_us']:return None
        if r['idle_ticks']>r['sampled_ticks']:return None
        if not all(r[k+'_calls'] for k in ('cbw','data','csw')):return None
        if sum(r[k+'_us'] for k in ('cbw','data','csw'))>r['elapsed_us']:return None
    groups=[]
    for policy in (0,4000,20000):
        rows=[r for r in records if r['poll_us']==policy];times=[r['elapsed_us'] for r in rows]
        ticks=sum(r['sampled_ticks'] for r in rows);idle=sum(r['idle_ticks'] for r in rows)
        groups.append(dict(poll_us=policy,count=len(rows),total_us=sum(times),median_us=statistics.median(times),min_us=min(times),max_us=max(times),sampled_ticks=ticks,idle_ticks=idle,idle_sample_percent=100*idle/ticks if ticks else None,csw_us=sum(r['csw_us'] for r in rows)))
    return dict(path=starts[0][0],original_poll_us=int(starts[0][1]),records=records,policies=groups,elapsed_us=int(ends[0][3]),
        scope='Same 32 KiB overwrite/readback/flush, six balanced orders in one boot. Includes metadata and yielded work. Timer samples are not CPU runtime; no physical input or power acceptance.')
