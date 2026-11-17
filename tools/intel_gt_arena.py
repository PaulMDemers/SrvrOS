"""Original full-GT CPU command arena. No GPU address or submission grant."""
import struct
from intel_command_native import FIELDS
from intel_source_audit import _raw
from intel_gt_bootstrap import fields
from intel_gt_timing import PRACTICAL


def arena_result(text,parent,binding,memory,entry,parent_complete):
    lines=[l for l in text.splitlines() if l.startswith('intel-arena-')]
    if len(lines)!=2 or not lines[0].startswith('intel-arena-memory:') or not lines[1].startswith('intel-arena-binding:'):
        raise ValueError('Arena coverage/order')
    c=dict(zip(FIELDS,struct.unpack('<64Q',_raw(lines[0],'intel-arena-memory:',512))))
    b=fields(lines[1],'intel-arena-binding:','v owner provider lifetime original child borrows returns parent_retired'.split())
    if any(b[k]>1 for k in b if k!='v') or b['returns']>b['borrows']:
        raise ValueError('Arena original registration range')
    if c['state']>6 or c['reason']&~255 or c['clock_reason']>9:
        raise ValueError('Arena state/reason')
    if (c['virtual'],c['budget'],c['ring_page'],c['context_page'],c['completion_page'],c['arena_pages'])!=(0xffffc001f5160000,250000,0,1,3,4):
        raise ValueError('Arena layout/policy')
    if any(c[k] for k in ('pte_writes','managed_pages','submissions')):
        raise ValueError('Arena false GPU authority')
    flags='retained complete registered busy initializations checks zeros release_attempts releases hold_active hold_fault'.split()
    if any(c[k]>1 for k in flags) or c['retire_flags']>5 or not 0<=c['retired']<=c['maps']<=4 or not 0<=c['released']<=c['allocated']<=4:
        raise ValueError('Arena resource prefix')
    if c['allocated'] not in (0,4) or c['released'] not in (0,4) or any(c[k]>4 for k in ('observations','cache_checks','alias_checks')) or c['guards']>2 or c['words']>2048:
        raise ValueError('Arena callback coverage')
    if c['physical'] and (c['physical']&4095 or c['physical']+16384>0x100000000 or c['physical']<=c['root']<c['physical']+16384):
        raise ValueError('Arena physical backing')
    if not c['state']:
        constants={'virtual','budget','context_page','completion_page','arena_pages'}
        if any(v for k,v in c.items() if k not in constants) or any(b[k] for k in b if k!='v'):
            raise ValueError('Arena unstarted state')
    if c['state']==5 and not c['complete']:raise ValueError('Arena closed without completed child')
    if c['complete']:
        if c['state'] not in (4,5) or any(c[k] for k in ('reason','retained','busy','registered','clock','previous','clock_reason')):
            raise ValueError('Arena false child completion')
        if (c['maps'],c['guards'],c['initializations'],c['allocated'],c['zeros'],c['serial'],c['sequence'])!=(4,2,1,4,1,2,1):
            raise ValueError('Arena original allocation/lease')
        if any(b[k]!=1 for k in ('owner','provider','lifetime','original','borrows','returns')) or b['child']:
            raise ValueError('Arena original child return')
        if any(c[k]!=binding[k] for k in ('root','prepare','gt','hda','outer')) or c['epoch']!=parent['epoch_after']:
            raise ValueError('Arena original new epoch/clocks')
        if not 0<c['storage_begin']<c['storage_end']<=c['outer'] or c['storage_end']-c['storage_begin']>100000:
            raise ValueError('Arena preparation outside GPU scope')
        if not memory['summary']['end']<=c['begin']<c['end']<=parent['end'] or any(c['end']<c[k] or c['end']-c[k]>PRACTICAL[k] for k in ('prepare','gt','hda','outer')):
            raise ValueError('Arena original child order/deadlines')
        if (c['pages'],c['leaves'],c['index'],c['writable'],c['user'],c['nx'],c['sizes'],c['mtrr'],c['pat'],c['effective'])!=(4,4,0,1,0,1,1,6,6,6):
            raise ValueError('Arena CPU alias/cache policy')
        from intel_cache import _snapshot,_page
        cache=memory['cache'];regions=_snapshot(cache,memory['variable'],memory['fixed'])
        if any(_page(cache,regions,c['physical']+4096*i,0)!=(6,6,6) for i in range(4)):
            raise ValueError('Arena independent original CPU cache policy')
    if c['complete'] and c['state']==4:
        expected=dict(retired=0,observations=3,checks=0,released=0,cache_checks=3,alias_checks=3,words=0,memory_state=2,hold_active=1,hold_fault=0,retire_flags=0,close_begin=0,close_end=0)
        if any(c[k]!=v for k,v in expected.items()) or c['last']!=c['end'] or b['parent_retired']:
            raise ValueError('Arena returned hold prefix')
    closed=bool(c['complete'] and c['state']==5)
    if closed:
        expected=dict(retired=4,observations=4,checks=1,released=4,cache_checks=4,alias_checks=4,words=2048,memory_state=5,release_attempts=1,releases=1,hold_active=0,hold_fault=0,retire_flags=5)
        if any(c[k]!=v for k,v in expected.items()) or c['dma_before']!=c['dma_after'] or not b['parent_retired'] or not parent_complete:
            raise ValueError('Arena false retirement')
        wake=entry['wake']['summary']
        if not wake['end_us']<=c['close_begin']<c['close_end']==c['last'] or c['close_end']-c['close_begin']>100000:
            raise ValueError('Arena cleanup outside GPU scope')
    if parent_complete and not closed:raise ValueError('Arena false enclosing completion')
    return dict(summary=c,binding=b,complete=closed,hardware_ownership=False,managed_pages=0,physical_tearfree=False)
