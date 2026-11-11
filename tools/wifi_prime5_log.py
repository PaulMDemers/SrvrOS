"""Strict two-stage calibration prime and stopped-ring transition.

This proves the bounded ledger, not a connection or physical causality. Network
checks must still validate the unchanged discovery/authentication/HTTP parents.
"""
import re
from wifi_cal5_log import check as calibration, fields
from wifi_early5_observation import observe
from wifi_held_log import require

STAGE0='wifi-prime5-stage: version=1 stage=0 pages=32 calibration_once=1 retained=1'
STAGE1='wifi-prime5-stage: version=1 stage=1 pages=8 calibration_once=1 retained=1'


def check(log):
    require('srvros.wifi-cal5-prime=1' in log, 'missing priming opt-in')
    result=calibration(log,prime=True)
    lines=log.splitlines()
    rows=[s for s in lines if s.lstrip().startswith('wifi-prime5')]
    require(len(rows)==6 and rows[0]==STAGE0 and rows[4]==STAGE1,
            'missing/duplicate/unknown priming stage records')
    contexts=[]
    for phase,row in enumerate(rows[1:3]):
        m=re.fullmatch(r'wifi-prime5-context: phase='+str(phase)+
                      ''.join(' w'+str(i)+r'=(0x[0-9a-f]{16})' for i in range(8)),row)
        require(m is not None,'malformed priming endpoint context')
        words=[int(x,16) for x in m.groups()]
        require(all(w<2**32 for w in words),'invalid priming context word')
        contexts.append(words)
    h=fields(rows[3],'wifi-prime5-handoff')
    require(set(h)==set('version ok old_ring old_dequeue new_ring slot state dcs protected new_pages retained reused context_preserved'.split()),
            'wrong priming handoff keys')
    expected=dict(version=1,ok=1,old_ring=h['old_ring'],old_dequeue=h['old_dequeue'],
                  new_ring=h['new_ring'],slot=h['slot'],state=3,dcs=1,protected=33,
                  new_pages=17,retained=1,reused=0,context_preserved=1)
    require(h==expected and 0<h['slot']<256 and
            all(0<r<2**64-4095 and r%4096==0 for r in (h['old_ring'],h['new_ring'])) and
            h['old_ring']!=h['new_ring'],'unproven fresh priming handoff')
    before,after=contexts
    require(before[0]&7==after[0]&7==3 and
            before[2]|before[3]<<32==h['old_dequeue'] and
            after[2]|after[3]<<32==h['new_ring']|1 and
            all(before[i]==after[i] for i in (0,1,4,5,6,7)),
            'priming handoff changed non-dequeue context or DCS')
    observed=observe(log)
    publish=next(s for s in lines if s.startswith('wifi-early5-publish:'))
    binding=fields(publish,'wifi-early5-publish')
    require(binding['ring']==h['old_ring'] and binding['slot']==h['slot'] and
            h['old_dequeue']==h['old_ring']+16*observed['summary']['completed']+1,
            'priming handoff disagrees with initial exact Stop')
    final=fields(rows[5],'wifi-prime5')
    require(final==dict(version=1,complete=1,first_operations=result['prime5_first_operations'],
                       operations=result['cal5_operations'],commands=5,resumed=1,held=1,
                       retained=1,reused=0,inspected=0),'incomplete priming retention/resume')
    cal_progress=next(s for s in lines if s.startswith('wifi-cal5-progress:'))
    summary=next(s for s in lines if s.startswith('wifi-early5:'))
    posts=[s for s in lines if s.startswith('wifi-cal5-post:')]
    stop=next(s for s in lines if s.startswith('wifi-cal5-stop:'))
    require(lines.index(STAGE0)<lines.index(cal_progress)<lines.index(publish)<lines.index(summary)<
            lines.index(rows[1])<lines.index(rows[2])<lines.index(rows[3])<lines.index(STAGE1)<
            lines.index(posts[0])<lines.index(stop)<lines.index(rows[5]),
            'priming transition outside stopped owner boundary')
    require(fields(stop,'wifi-cal5-stop')['ring']==h['new_ring'], 'new window used the priming ring')
    result.update(prime5_complete=True,prime5_protected_pages=33,
                  native_accepted=False,connectivity_tested=False)
    return result
