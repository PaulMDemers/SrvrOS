"""Source-bound structural packet/timing witnesses, never admission or RF proof."""
from wifi_http5_log import canonical_decimal
from wifi_encrypted5_log import observe as encrypted_observe

SUMMARY='version epoch records observation_only'
FRAME='index ticket token direction ethernet role message sequence started_us finished_us radio_delivery_proved'

def observe(text):
    lines=text.splitlines();owned=[(i,x) for i,x in enumerate(lines) if x.startswith('wifi-packet5')]
    if not owned:return dict(present=False,native_accepted=False)
    if any(not x.startswith(('wifi-packet5: ', 'wifi-packet5-frame: ')) for _,x in owned):raise ValueError('unknown packet witness')
    starts=[(i,canonical_decimal(x,'wifi-packet5',SUMMARY)) for i,x in owned if x.startswith('wifi-packet5: ')]
    if not 1<=len(starts)<=2 or owned[0][0]!=starts[0][0]:raise ValueError('orphan packet witness')
    encrypted=encrypted_observe(text)
    if not encrypted['present'] or len(encrypted['epochs'])!=len(starts):raise ValueError('missing encrypted binding')
    epochs=[]
    for j,(at,s) in enumerate(starts):
        end=starts[j+1][0] if j+1<len(starts) else len(lines)
        if s['version']!=1 or s['epoch']!=j+1 or s['observation_only']!=1 or s['records']>332:raise ValueError('packet summary bounds')
        rows=[canonical_decimal(x,'wifi-packet5-frame',FRAME) for i,x in owned if at<i<end]
        e=encrypted['epochs'][j]
        bindings={r['index']:(direction,r) for direction,records in ((0,e['tx']),(1,[r for r in e['rx'] if r['ethernet']])) for r in records}
        if len(rows)!=s['records'] or len(rows)!=len(bindings) or [r['index'] for r in rows]!=sorted(bindings):raise ValueError('missing/duplicate packet binding')
        for r in rows:
            direction,b=bindings[r['index']]
            if r['direction']!=direction or any(r[k]!=b[k] for k in ('ticket','token')) or r['radio_delivery_proved']!=0 or r['sequence']>4095 or r['role']>5 or r['message']>8 or r['started_us']>r['finished_us']:raise ValueError('invalid packet witness')
            if not 14<=r['ethernet']<=1514 or (r['message'] and r['role'] not in (4,5)):raise ValueError('invalid packet shape')
            if direction:
                if r['ethernet']!=b['ethernet']:raise ValueError('RX bytes differ')
            elif any(r[k]!=b[k] for k in ('started_us','finished_us')):raise ValueError('TX clocks differ')
        epochs.append(dict(summary=s,frames=rows))
    return dict(present=True,epochs=epochs,native_accepted=False,radio_or_AP_delivery_proved=False,
                scope='TX publication intent and USB completion; RX USB completion and authenticated Ethernet structural role; no DHCP/XID/peer/protocol acceptance')
