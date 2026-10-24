"""Software event timing, never controller completion or pure device latency."""
import re

FIELDS=('id phase calls callback_us max_us over20ms over100ms over1s observed_calls '
        'observe_us resume_us observe_max_us resume_max_us valid').split()
PATTERN=re.compile(r'usb-timing: version=1 scope=(logger|comparison) '+
                   ' '.join(k+r'=([0-9]{1,20})' for k in FIELDS))

def transfer_records(text,scope):
    records={}
    for line in text.splitlines():
        if not line.startswith('usb-timing:'):continue
        match=PATTERN.fullmatch(line)
        if not match:return None
        if match[1]!=scope:continue
        row=dict(zip(FIELDS,map(int,match.groups()[1:])))
        if any(v>=2**64 for v in row.values()) or row['phase']>2 or row['valid']!=1:return None
        calls=row['calls'];total=row['callback_us'];peak=row['max_us']
        if not peak<=total<=calls*peak or (not calls and peak):return None
        if row['observed_calls']!=calls:return None
        if not 0<=row['over1s']<=row['over100ms']<=row['over20ms']<=calls:return None
        for count,threshold in (('over20ms',20000),('over100ms',100000),('over1s',1000000)):
            n=row[count]
            if bool(n)!=(peak>threshold) or n*(threshold+1)>total:return None
        if row['observe_us']+row['resume_us']>total:return None
        for name in ('observe','resume'):
            duration=row[name+'_us'];maximum=row[name+'_max_us']
            if not maximum<=duration<=calls*maximum or maximum>peak:return None
        key=(row['id'],row['phase'])
        if key in records and (scope!='logger' or records[key]!=row):return None
        records[key]=row
    return records

def sample_transfers(records,identifier,sample):
    if records is None:return None
    rows=[]
    for phase,name in enumerate(('cbw','data','csw')):
        row=records.get((identifier,phase))
        if row is None or row['calls']!=sample[name+'_calls'] or row['callback_us']!=sample[name+'_us']:return None
        rows.append(row)
    return rows
