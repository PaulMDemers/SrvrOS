"""Strict CPU-owned private command-memory lifetime; no GPU mapping authority."""
import struct
from intel_source_audit import _raw,shared_view

FIELDS='''state reason clock_reason retained complete registered busy root virtual physical serial storage_begin storage_end
begin end last close_begin close_end clock previous epoch sequence prepare gt hda outer maps retired observations guards
initializations checks pages leaves index writable user nx sizes mtrr pat effective allocated released dma_before dma_after
cache_checks alias_checks zeros words memory_state release_attempts releases hold_active hold_fault retire_flags budget
ring_page context_page completion_page arena_pages pte_writes managed_pages submissions'''.split()


def command_result(text,drain,parent,ggtt,*,native=False):
    lines=[l for l in text.splitlines() if l.startswith('intel-command-memory:')]
    if len(lines)!=1:raise ValueError('Command-memory receipt coverage')
    c=dict(zip(FIELDS,struct.unpack('<64Q',_raw(lines[0],'intel-command-memory:',512))))
    if len(FIELDS)!=64 or c['state']>6 or c['reason']&~255 or c['clock_reason']>9:
        raise ValueError('Command-memory schema')
    if (c['virtual'],c['budget'],c['ring_page'],c['context_page'],c['completion_page'],c['arena_pages'])!=(0xffffc001f5160000,150000,0,1,3,4):
        raise ValueError('Command-memory layout/policy')
    if any(c[k] for k in ('pte_writes','managed_pages','submissions')):
        raise ValueError('Command-memory false GPU authority')
    for k in ('retained','complete','registered','busy','initializations','checks','zeros','release_attempts','releases','hold_active','hold_fault'):
        if c[k]>1:raise ValueError('Command-memory flag width')
    if c['retire_flags']>5:raise ValueError('Command-memory retirement flag width')
    if c['maps']>4 or c['retired']>c['maps'] or c['released']>c['allocated'] or c['allocated'] not in (0,4) or c['released'] not in (0,4):
        raise ValueError('Command-memory resource prefix')
    if any(c[k]>4 for k in ('observations','cache_checks','alias_checks')) or c['guards']>2 or c['words']>2048:
        raise ValueError('Command-memory callback coverage')
    if not c['state']:
        constants={'virtual','budget','context_page','completion_page','arena_pages'}
        if any(v for k,v in c.items() if k not in constants):raise ValueError('Command-memory unstarted state')
    if c['physical'] and (c['physical']&4095 or c['physical']+16384>0x100000000 or c['physical']<=c['root']<c['physical']+16384):
        raise ValueError('Command-memory physical backing')
    if c['state']==5 and not c['complete']:raise ValueError('Command-memory closed without completed lease')
    if c['complete']:
        if c['reason'] or c['retained'] or c['busy'] or c['registered'] or c['clock'] or c['previous'] or c['clock_reason'] or c['state'] not in (4,5):
            raise ValueError('Command-memory false child completion')
        if (c['maps'],c['guards'],c['initializations'],c['allocated'],c['zeros'],c['serial'],c['sequence'])!=(4,2,1,4,1,2,1):
            raise ValueError('Command-memory original allocation/lease')
        if any(c[k]!=drain[k] for k in ('root','epoch','prepare','gt','hda','outer')):
            raise ValueError('Command-memory original parent binding')
        if not 0<c['storage_begin']<c['storage_end']<=c['prepare'] or c['storage_end']-c['storage_begin']>100000:
            raise ValueError('Command-memory preparation order')
        e=parent['engines']['summary']
        if not e['child_begin']<=ggtt['summary']['native_end']<=c['begin']<c['end']<=e['child_end']:
            raise ValueError('Command-memory original child order')
        if any(c['end']>c[k]+cap for k,cap in (('prepare',150000),('gt',300000),('hda',400000),('outer',500000))):
            raise ValueError('Command-memory original deadlines')
        if (c['pages'],c['leaves'],c['index'],c['writable'],c['user'],c['nx'],c['sizes'],c['mtrr'],c['pat'],c['effective'])!=(4,4,0,1,0,1,1,6,6,6):
            raise ValueError('Command-memory actual CPU mapping/cache policy')
        buffers=parent['engines']['requester']['transfer']['buffers']
        for b in buffers['buffers']:
            if c['physical']<b['physical']+b['pages']*4096 and b['physical']<c['physical']+16384:
                raise ValueError('Command-memory overlaps original display backing')
        if native:
            from intel_cache import native_cache_result,_snapshot,_page
            rows=native_cache_result(shared_view(text))['rows']
            regions=_snapshot(rows['sample'][1],[v for v in rows['variable'] if v['sample']==1],[v for v in rows['fixed'] if v['sample']==1])
            if any(_page(rows['sample'][1],regions,c['physical']+4096*i,0)!=(6,6,6) for i in range(4)):
                raise ValueError('Command-memory independent cache policy')
    if c['complete'] and c['state']==4:
        if (c['retired'],c['observations'],c['checks'],c['released'],c['cache_checks'],c['alias_checks'],c['words'],c['memory_state'],c['hold_active'],c['hold_fault'],c['retire_flags'],c['close_begin'],c['close_end'])!=(0,3,0,0,3,3,0,2,1,0,0,0,0) or c['last']!=c['end']:
            raise ValueError('Command-memory returned hold prefix')
    closed=bool(c['complete'] and c['state']==5)
    if closed:
        if (c['retired'],c['observations'],c['checks'],c['released'],c['cache_checks'],c['alias_checks'],c['words'],c['memory_state'],c['release_attempts'],c['releases'],c['hold_active'],c['hold_fault'],c['retire_flags'])!=(4,4,1,4,4,4,2048,5,1,1,0,0,5):
            raise ValueError('Command-memory complete retirement')
        if c['dma_before']!=c['dma_after'] or not parent['complete'] or not ggtt['complete']:
            raise ValueError('Command-memory enclosing retirement')
        wake=buffers['source']['reset']['wake']['summary']
        if not wake['end_us']<=c['close_begin']<c['close_end']==c['last'] or c['close_end']-c['close_begin']>100000:
            raise ValueError('Command-memory cleanup order')
    return dict(summary=c,complete=closed,scope_complete=bool(c['complete']),native_complete=bool(native and closed and parent['native_complete']),
                hardware_ownership=False,managed_pages=0,physical_tearfree=False,
                scope='private CPU-owned ring/context-storage/completion arena only; no GPU mapping or submission')
