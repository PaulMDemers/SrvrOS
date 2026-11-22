"""Bounded read-to-pop AUTH TX-status observations, never protocol acceptance.

An absent status does not prove non-transmission. Even a reported ACK does not
replace the authenticated peer response, original deadline or complete parent.
"""
import re
from wifi_power5_log import fields
from wifi_held_log import require


def decode(rows):
    pos=0;phases=[]
    def take(prefix,keys):
        nonlocal pos
        require(pos<len(rows),'truncated AUTH radio observation')
        line=rows[pos];pos+=1
        d=fields(line,prefix)
        require(set(d)==set(keys.split()) and all(0<=v<2**32 for v in d.values()),'wrong AUTH radio schema/range')
        require(all(re.fullmatch(r'(?:0|[1-9][0-9]*|0x[0-9a-f]{16})',v.split('=',1)[1]) for v in line[len(prefix)+2:].split()),'noncanonical AUTH radio values')
        return d
    for phase in (0,1):
        s=take('wifi-stream5-radio','version phase attempted complete reads empty saturated observation_only')
        require(s['version']==s['attempted']==s['complete']==s['observation_only']==1 and s['phase']==phase and
                1<=s['reads']<=17 and s['empty'] in (0,1) and s['saturated']==1-s['empty'] and
                (s['empty'] or s['reads']==17),'incomplete/unbounded AUTH radio snapshot')
        entries=[]
        for i in range(s['reads']):
            e=take('wifi-stream5-radio-entry','phase i ext fifo')
            require(e['phase']==phase and e['i']==i and bool(e['fifo']&1)==(i<s['reads']-1 or not s['empty']),
                    'reordered AUTH radio entry or read after empty FIFO')
            entries.append(e)
        phases.append(dict(summary=s,entries=entries))
    return phases,pos
