"""Strict original-held System Agent invalidate record; no GPU interval grant."""
import struct
from intel_source_audit import _raw, shared_view
from intel_timing_policy import prepare_us

FIELDS='''state reason native_reason flags returned epoch kind mode root prepare gt hda outer begin last end clock previous clock_reason reads guards attempts writes initial posted uncertain native_begin native_end mmio mmio_bytes virtual maps retired registered retained cache_checks alias_checks pages leaves index writable user nx sizes mtrr pat effective sequence budget mapped native_clock native_previous native_stage parents'''.split()


def ggtt_result(text, drain, parent, *, native=False):
    lines=[line for line in text.splitlines() if line.startswith('intel-ggtt-invalidate:')]
    if len(lines)!=1:
        raise ValueError('GGTT receipt coverage')
    g=dict(zip(FIELDS,struct.unpack('<54Q',_raw(lines[0],'intel-ggtt-invalidate:',432))))
    if len(FIELDS)!=54 or g['state']>6 or g['reason']&~127 or g['native_reason']&~127 or g['flags']&~63:
        raise ValueError('GGTT receipt schema')
    if g['budget']!=150000 or prepare_us()!=150000 or g['virtual']!=0xffffc001f5151000:
        raise ValueError('GGTT explicit policy/alias')
    if any(g[k]>1 for k in ('returned','uncertain','maps','retired','registered','retained','mapped','writes','attempts')) or g['reads']>2 or g['guards']>4 or g['native_stage']>2:
        raise ValueError('GGTT progress width')
    if g['writes']>g['attempts'] or g['retired']>g['maps'] or (g['returned'] and (g['registered'] or g['mapped'] or g['retained'])):
        raise ValueError('GGTT progress order')
    core_complete=bool(g['flags']&1)
    if core_complete:
        if g['flags']!=57 or g['state']!=5 or any(g[k] for k in ('reason','clock','previous','clock_reason','uncertain','initial','posted')):
            raise ValueError('GGTT false core completion')
        if (g['reads'],g['guards'],g['attempts'],g['writes'])!=(2,4,1,1):
            raise ValueError('GGTT issued/posting-read coverage')
    if g['state']:
        if any(g[k]!=drain[k] for k in ('epoch','kind','mode','root','prepare','gt','hda','outer')) or g['sequence']!=1 or g['mmio_bytes']!=0x1000000 or not g['mmio'] or g['mmio']&65535:
            raise ValueError('GGTT original binding')
        if not g['native_begin'] or g['native_begin']<drain['native_end'] or g['begin']<g['native_begin']:
            raise ValueError('GGTT original child ordering')
    elif core_complete or any(g[k] for k in ('epoch','root','prepare','gt','hda','outer','begin','last','end','clock','previous','clock_reason','reads','guards','attempts','writes','initial','posted','uncertain','mmio','mmio_bytes','sequence')):
        raise ValueError('GGTT unstarted prefix')
    if core_complete:
        if not g['begin']<g['end']==g['last'] or any(g['end']>g[k]+cap for k,cap in (('prepare',150000),('gt',300000),('hda',400000),('outer',500000))):
            raise ValueError('GGTT original deadlines')
        if (g['pages'],g['leaves'],g['index'],g['writable'],g['user'],g['nx'],g['sizes'],g['effective'])!=(1,1,2,1,0,1,1,0):
            raise ValueError('GGTT admitted UC alias')
        if native:
            from intel_cache import native_cache_result,_snapshot,_page
            from intel_display_capture import capture_result
            view=shared_view(text)
            identity=capture_result(view)['rows']['identity'][-1]
            mmio=(identity['bar1']<<32)|(identity['bar0']&~15)
            if g['mmio']!=mmio:raise ValueError('GGTT captured MMIO binding')
            rows=native_cache_result(view)['rows']
            regions=_snapshot(rows['sample'][1],[v for v in rows['variable'] if v['sample']==1],[v for v in rows['fixed'] if v['sample']==1])
            if _page(rows['sample'][1],regions,mmio+0x101000,2)!=(g['mtrr'],g['pat'],g['effective']):
                raise ValueError('GGTT independently derived UC policy')
    local=bool(core_complete and g['maps']==g['retired']==1 and not any(g[k] for k in ('native_reason','retained','mapped','registered')))
    if local:
        if g['native_end']<g['end'] or g['cache_checks']!=10 or g['alias_checks']!=12 or not g['parents'] or any(g[k] for k in ('native_clock','native_previous','native_stage')):
            raise ValueError('GGTT closing alias/clock coverage')
        e=parent['engines']['summary']
        if g['native_end']>e['child_end'] or not drain['core_complete'] or not drain['retired']:
            raise ValueError('GGTT enclosing original return')
    complete=bool(local and parent['complete'] and g['returned'])
    if parent['complete'] and not complete or g['returned'] and not complete:
        raise ValueError('GGTT false enclosing completion')
    return dict(summary=g,scope_complete=local,complete=complete,native_complete=bool(native and complete and parent['native_complete']),
                hardware_ownership=False,managed_pages=0,physical_tearfree=False,
                scope='ordered System Agent graphics TLB invalidation only')
