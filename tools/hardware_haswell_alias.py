"""Strict raw single-managed-root census; no device mapping or global ownership."""
import base64
import re
import struct
from hardware_haswell_cache import cache_result, effective, UNIT
from hardware_haswell_firmware import firmware_result

SUMMARY = (r'hsw-alias: v=1 status=([1-5]) step=([0-6]) pages=(\d+) reads=(\d+),(\d+) bytes=(\d+),(\d+) '
           r'target_leaves=(\d+) root=0x([0-9a-f]{16}) nx=0x([0-9a-f]{16}),0x([0-9a-f]{16}) max=0x([0-9a-f]{16}),0x([0-9a-f]{16}) '
           r'efer=0x([0-9a-f]{16}),0x([0-9a-f]{16}) irq=0x([0-9a-f]{16}),0x([0-9a-f]{16}) begin=(\d+) end=(\d+) '
           r'cursor=0x([0-9a-f]{16}),(\d+),0x([0-9a-f]{16}),0x([0-9a-f]{16}) table_cap=64 entry_cap=32768 data_cap=4096 limit_us=100000 scope=sole-managed-root writes=0 maps=0 ownership=0')


def decode_pages(data, count):
    pos = 0; pages = {}
    for _ in range(count):
        if pos+9 > len(data): raise ValueError('table header')
        pa, level = struct.unpack_from('<QB', data, pos); pos += 9
        if not pa or pa & 4095 or pa >= 1 << 39 or level > 3 or pa in pages:
            raise ValueError('table identity')
        words = []
        while len(words) < 512:
            if pos+18 > len(data): raise ValueError('table run')
            n, first, step = struct.unpack_from('<HQQ', data, pos); pos += 18
            if not 1 <= n <= 512-len(words) or (n == 1 and step): raise ValueError('run length')
            words.extend((first+step*i) & ((1 << 64)-1) for i in range(n))
        pages[pa] = dict(level=level, raw=words)
    if pos != len(data): raise ValueError('trailing table bytes')
    return pages


def alias_result(text):
    try: return _parse(text)
    except (ValueError, KeyError, IndexError, struct.error): return None


def _parse(text):
    lines = [x for x in text.splitlines() if x.startswith('hsw-alias:')]
    if len(lines) != 1: return None
    m = re.fullmatch(SUMMARY, lines[0])
    if not m: return None
    vals = list(m.groups()); step=int(vals.pop(1)); status, count, r0, r1, b0, b1, targets = map(int, vals[:7])
    root, nx0, nx1, max0, max1, ef0, ef1, irq0, irq1 = (int(x, 16) for x in vals[7:16])
    begin, end = map(int, vals[16:18])
    cursor_pa, cursor_index, expected, observed = int(vals[18],16), int(vals[19]), int(vals[20],16), int(vals[21],16)
    chunks = {}
    for line in text.splitlines():
        if not line.startswith('hsw-alias-data:'): continue
        match = re.fullmatch(r'hsw-alias-data: v=1 phase=([01]) chunk=([0-7]) data=([A-Za-z0-9+/]*={0,2})', line)
        if not match: raise ValueError('chunk record')
        key = (int(match[1]), int(match[2])); data = base64.b64decode(match[3], validate=True)
        if key in chunks or not 1 <= len(data) <= 512 or base64.b64encode(data).decode() != match[3]: raise ValueError('chunk encoding')
        chunks[key] = data
    cache = cache_result(text)
    if status != 1:
        if chunks or 'laptop-capture: completed=yes' in text: return None
        return dict(status='REFUSED' if cache and cache['status']=='REFUSED' else 'FAILED', source_status=status, step=step, cursor=dict(physical=cursor_pa,index=cursor_index,expected=expected,observed=observed),
                    hardware_authority=False, ownership=False, native_mapping_authorized=False)
    if step != 6 or not cache or cache['status'] != 'OBSERVED_CONTEXT' or not 1 <= count <= 64 or r0 != r1 or r0 != 512*count or not 1 <= b0 <= 4096 or b0 != b1:
        return None
    a, b = cache['cpu_samples']
    if root != a['root'] or nx0 != nx1 or not nx0 & (1 << 20) or max0 != max1 or max0 < 0x80000001 or ef0 != ef1 or ef0 & 0xd00 != 0xd00 or ef0 & ~0xd01 or (irq0 ^ irq1) & 512 or not irq0 & 2 or not irq1 & 2:
        raise ValueError('control/exclusion state')
    if not a['context_us'][1] <= begin < end <= b['context_us'][0] or end-begin >= 100000:
        raise ValueError('census clock/order')
    blobs = []
    for phase, size in ((0, b0), (1, b1)):
        n = (size+511)//512
        if any((phase, i) not in chunks for i in range(n)): raise ValueError('missing chunk')
        if any(len(chunks[phase, i]) != min(512, size-512*i) for i in range(n)): raise ValueError('chunk extent')
        blobs.append(b''.join(chunks[phase, i] for i in range(n)))
    if len(chunks) != 2*((b0+511)//512) or blobs[0] != blobs[1]: raise ValueError('changed/extra raw census')
    pages = decode_pages(blobs[0], count)
    if cursor_index != 511 or cursor_pa != next(reversed(pages)) or expected != observed or observed != pages[cursor_pa]['raw'][511]:
        raise ValueError('final cursor')
    if root not in pages or pages[root]['level'] != 0: raise ValueError('root coverage')
    catalog = firmware_result(text)['samples'][0]['catalog']; addr = ((1 << 39)-1) & ~4095
    for pa in pages:
        rows = [r for r in catalog if r[0] < pa+4096 and pa < r[0]+r[1]]
        if len(rows) != 1 or not rows[0][0] <= pa < pa+4096 <= rows[0][0]+rows[0][1] or rows[0][2] not in (0, 2, 3, 5, 6) or pa in (UNIT, UNIT+4096):
            raise ValueError('RAM-only table source')
    for walk in cache['walks']:
        raw = walk['raw']; va = raw[0]
        for level in range(walk['levels']):
            pa, entry = raw[2+2*level:4+2*level]
            if pa not in pages or pages[pa]['level'] != level or pages[pa]['raw'][(va >> (39-9*level)) & 511] != entry:
                raise ValueError('census and point walks disagree')
    reached = {root}; todo = [root]; leaves = []
    while todo:
        pa = todo.pop(); page = pages[pa]; level = page['level']
        for index, entry in enumerate(page['raw']):
            if not entry & 1: continue
            if entry & 0x000ffffffffff000 & ~addr or (level == 0 and entry & 128): raise ValueError('reserved physical/PS bits')
            if level == 3 or (level in (1, 2) and entry & 128):
                shift = (39, 30, 21, 12)[level]; low = (1 << shift)-1
                if level == 1 and not nx0 & (1 << 26): raise ValueError('unsupported 1GiB')
                if level != 3 and entry & addr & low & ~4096: raise ValueError('large-leaf alignment')
                base = entry & addr & ~low
                if base <= UNIT <= base+low:
                    pat = ((entry >> (7 if level == 3 else 12)) & 1)*4 + ((entry >> 4) & 1)*2 + ((entry >> 3) & 1)
                    leaves.append(dict(table=pa, index=index, raw=entry, shift=shift, classification=effective(a, pat)))
            else:
                child = entry & addr
                if child not in pages or pages[child]['level'] != level+1: raise ValueError('incomplete/cyclic graph')
                if child not in reached: reached.add(child); todo.append(child)
    if len(reached) != len(pages) or len(leaves) != targets: raise ValueError('coverage/count')
    return dict(status='OBSERVED_MANAGED_ROOT_CONTEXT', source_completed=True, root=root, pages=count,
                reads_per_pass=r0, raw_encoded_bytes_per_pass=b0, begin_us=begin, end_us=end,
                nx_edx=nx0, cpuid_max=max0, efer=ef0, irq_before=irq0, irq_after=irq1,
                raw_pages=[dict(physical=p, **v) for p, v in pages.items()], target_leaves=leaves,
                sole_managed_root_coverage=True, unchanged_two_pass_raw=True, nx_enablement_observed=True,
                all_hardware_aliases_proven=False, resource_lease_proven=False, native_translation_observed=False,
                native_mapping_authorized=False, vt_d_reads=0, mappings=0, msr_writes=0,
                hardware_authority=False, ownership=False)
