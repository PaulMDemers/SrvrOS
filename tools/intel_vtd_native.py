"""v36 original-lifetime disabled-state VT-d drain. No GPU ownership grant."""
import re
import struct

from intel_source_audit import (audit_recipe, audit_result, native_audit_result,
                               verify_audit_checkpoint, shared_view, _raw)

TOKEN = 'srvros.intel-ring=vtd-drain-v1'
PREVIOUS = 'srvros.intel-ring=source-audit-v1'
FIELDS = '''state reason native_reason epoch kind mode root prepare gt hda outer begin last end
clock previous clock_reason reads guards attempts writes polls uncertain initial final maps retired
registered retained core_flags returned native_begin native_end virtual cache_checks alias_checks
resource hash unit sequence pages leaves index writable user nx sizes mtrr pat effective'''.split()
CAP = 0x01c0000c40660462
ECAP = 0x7e1ff0505e
DMAR_HASH = 0x722775ad66626863
COST_FIELDS = 'calls parents failed previous stage first last max_gap max_phase'.split()


def drain_recipe(cmd):
    if not isinstance(cmd, str) or cmd.split().count(TOKEN) != 1 or PREVIOUS in cmd.split():
        raise ValueError('VT-d drain exact recipe')
    return audit_recipe(cmd.replace(TOKEN, PREVIOUS))


def drain_view(text):
    boots = re.findall(r'^laptop-capture: boot options=(.*)$', text, re.M)
    if len(boots) != 1:
        raise ValueError('VT-d drain boot coverage')
    drain_recipe(boots[0])
    marker = 'init: deferred=vtd-drain-preinit;'
    if text.count(marker) != 1 or 'init: deferred=source-audit-preinit;' in text or 'init: started pid=' in text:
        raise ValueError('VT-d drain preinit provenance')
    wakes = [l for l in text.splitlines() if l.startswith('intel-wake-summary:')]
    if wakes and (len(wakes) != 1 or not wakes[0].startswith('intel-wake-summary: v=36 ')):
        raise ValueError('VT-d drain image')
    return text.replace(TOKEN, PREVIOUS).replace(marker, 'init: deferred=source-audit-preinit;', 1).replace(
        'intel-wake-summary: v=36 ', 'intel-wake-summary: v=35 ', 1)


def _table(raw):
    if len(raw) != 136 or raw[:4] != b'DMAR' or int.from_bytes(raw[4:8], 'little') != 136 or sum(raw) & 255:
        raise ValueError('VT-d drain DMAR structure')
    h = 14695981039346656037
    for value in raw:
        h = ((h ^ value) * 1099511628211) & ((1 << 64) - 1)
    if h != DMAR_HASH:
        raise ValueError('VT-d drain exact topology')


def drain_result(text, *, native=False):
    view = drain_view(text)
    lines = [l for l in text.splitlines() if l.startswith(('intel-vtd-drain:', 'intel-vtd-table:'))]
    if len(lines) != 2 or not lines[0].startswith('intel-vtd-drain:') or not lines[1].startswith('intel-vtd-table:'):
        raise ValueError('VT-d drain receipt coverage/order')
    if any(l.startswith('intel-vtd-') and not l.startswith(('intel-vtd-drain:', 'intel-vtd-table:')) for l in text.splitlines()):
        raise ValueError('VT-d drain unknown receipt')
    version = 2 if lines[0].startswith('intel-vtd-drain: v=2 ') else 1
    raw = _raw(lines[0], 'intel-vtd-drain:', 1680 if version == 2 else 1552, version)
    timing = None
    if version == 2:
        values = struct.unpack_from('<16Q', raw, 1552)
        timing = dict(zip(COST_FIELDS, values[:9]), phases=list(values[9:]))
        if timing['max_phase'] > 6 or timing['stage'] > 2 or timing['parents'] > 100000 or timing['calls'] > 100000:
            raise ValueError('VT-d native timing schema')
        if not timing['calls']:
            if any(values[:1]+values[2:]): raise ValueError('VT-d native empty timing')
        else:
            end = timing['previous'] if timing['stage'] and timing['last'] < timing['previous'] else timing['last']
            if timing['first']:
                if timing['first'] > end or sum(timing['phases']) != end-timing['first'] or timing['max_gap'] > sum(timing['phases']):
                    raise ValueError('VT-d native timing envelope')
            elif end or any(timing['phases']) or not timing['stage']:
                raise ValueError('VT-d native missing first sample')
        if timing['stage'] and timing['failed'] != timing['last']:
            raise ValueError('VT-d native failed clock')
    d = dict(zip(FIELDS, struct.unpack_from('<50Q', raw)))
    contexts = [list(struct.unpack_from('<8Q', raw, 400 + n * 64)) for n in range(2)]
    polls = list(struct.unpack_from('<256I', raw, 528))
    table = _raw(lines[1], 'intel-vtd-table:', 136)
    if d['core_flags'] & ~63:
        raise ValueError('VT-d drain core flags')
    d.update(core_complete=d['core_flags'] & 1, core_busy=(d['core_flags'] >> 1) & 1, core_retained=(d['core_flags'] >> 2) & 1)
    if len(FIELDS) != 50 or d['state'] > 6 or d['reason'] & ~511 or d['native_reason'] & ~127:
        raise ValueError('VT-d drain schema/state')
    if any(d[k] > 1 for k in ('uncertain', 'registered', 'retained', 'core_complete', 'returned')):
        raise ValueError('VT-d drain boolean')
    if d['polls'] > 256 or d['attempts'] > 1 or d['writes'] > d['attempts'] or d['maps'] > 1 or d['retired'] > d['maps']:
        raise ValueError('VT-d drain counts')
    held_child = bool(not d['returned'] and (d['core_complete'] or (d['retained'] and d['native_reason'])))
    parent = (native_audit_result if native else audit_result)(view, held_child=held_child)
    if d['core_complete']:
        _table(table)
        if d['core_flags'] != 57 or d['state'] != 5 or any(d[k] for k in ('reason', 'clock', 'previous', 'clock_reason', 'uncertain')):
            raise ValueError('VT-d drain false core completion')
        if d['attempts'] != 1 or d['writes'] != 1 or not d['polls'] or d['reads'] != 43 + 7 * d['polls'] or d['guards'] != 45 + 8 * d['polls']:
            raise ValueError('VT-d drain complete accounting')
        if d['initial'] not in (0x0200000000000000, 0x1203000000000000) or d['final'] != 0x1203000000000000:
            raise ValueError('VT-d drain command/response')
        for n, value in enumerate(polls):
            if n >= d['polls']:
                if value:
                    raise ValueError('VT-d drain poll suffix')
            elif n + 1 == d['polls']:
                if value != 0x12030000:
                    raise ValueError('VT-d drain completion granularity')
            elif value & ~0x06000000 != 0x90030000:
                raise ValueError('VT-d drain pending prefix')
        expected = [0, 0x10, CAP, ECAP, 0, 0x0800000000000000, 0, 0]
        if contexts != [expected, expected] or (d['hash'], d['unit'], d['sequence']) != (DMAR_HASH, 0xfed90000, 1):
            raise ValueError('VT-d drain policy/capabilities')
        a = parent['summary']
        if (d['epoch'], d['kind'], d['mode'], d['prepare'], d['gt'], d['root']) != (
                a['epoch'], a['kind'], a['mode'], a['origin'], a['parent'], parent['root']):
            raise ValueError('VT-d drain original source')
        if not 0 < d['outer'] <= min(d['hda'], d['gt']) <= max(d['hda'], d['gt']) <= d['prepare'] <= d['native_begin'] <= d['begin'] < d['end'] == d['last']:
            raise ValueError('VT-d drain original clocks/order')
        if any(d['end'] > d[name] + budget for name, budget in (('prepare', 100000), ('gt', 300000), ('hda', 400000), ('outer', 500000))):
            raise ValueError('VT-d drain original deadline')
        # Independent already strict full original joint receipt, not values
        # merely repeated by the new writer record.
        joint = parent['engines']['requester']['transfer']['buffers']['source']['reset']['entry']['owner']['summary']
        if (d['hda'], d['outer']) != (joint['hda_begin'], joint['origin']):
            raise ValueError('VT-d drain original HDA/outer binding')
    if native and d['core_complete']:
        from intel_cache import native_cache_result, _snapshot, _page
        c = native_cache_result(shared_view(view))['rows']
        regions = _snapshot(c['sample'][1], [r for r in c['variable'] if r['sample'] == 1], [r for r in c['fixed'] if r['sample'] == 1])
        if _page(c['sample'][1], regions, d['unit'], 2) != (d['mtrr'], d['pat'], d['effective']):
            raise ValueError('VT-d drain independently decoded cache policy')
    complete = bool(parent['complete'] and d['returned'])
    if parent['complete'] and not complete:
        raise ValueError('VT-d drain false parent restoration')
    if timing and d['returned'] and (timing['stage'] or timing['failed'] or timing['previous'] or not timing['parents'] or not timing['calls']):
        raise ValueError('VT-d native completed timing failure')
    if d['returned']:
        e = parent['engines']['summary']
        if d['native_reason'] or d['retained'] or d['native_end'] < d['end']:
            raise ValueError('VT-d drain false native completion')
        if not d['core_complete'] or d['registered'] or d['maps'] != 1 or d['retired'] != 1 or d['resource'] not in (1, 2):
            raise ValueError('VT-d drain alias retirement')
        if d['virtual'] != 0xffffc001f5150000 or d['cache_checks'] != d['guards'] + d['reads'] + 4 or d['alias_checks'] != d['guards'] + 2*d['reads'] + 4:
            raise ValueError('VT-d drain exact alias and admission counts')
        if (d['pages'], d['leaves'], d['index'], d['writable'], d['user'], d['nx'], d['sizes'], d['effective']) != (1, 1, 2, 1, 0, 1, 1, 0) or not d['cache_checks'] or not d['alias_checks']:
            raise ValueError('VT-d drain UC alias policy')
        if not e['child_begin'] <= parent['summary']['begin'] < parent['summary']['end'] <= d['native_begin'] < d['native_end'] <= e['child_end']:
            raise ValueError('VT-d drain enclosing child order')
    return dict(version=version, timing=timing, summary=d, contexts=contexts, polls=polls, audit=parent, complete=complete,
                native_complete=bool(native and complete and parent['native_complete']),
                hardware_ownership=False, managed_pages=0, physical_tearfree=False,
                scope='AT00 memory already processed by graphics remapper only')


def verify_drain_checkpoint(pre, text, *, native=False):
    if any(l.startswith(('intel-vtd-drain:', 'intel-vtd-table:')) for l in pre.splitlines()):
        raise ValueError('VT-d drain late prerequisite')
    result = drain_result(text, native=native)
    d = result['summary']
    held_child = bool(not d['returned'] and (d['core_complete'] or (d['retained'] and d['native_reason'])))
    verify_audit_checkpoint(drain_view(pre), drain_view(text), native=native, held_child=held_child)
    return True


def drain_shared_view(text):
    return shared_view(drain_view(text))
