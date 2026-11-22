"""Strict v5 dual-file boot ECAM/census proof. No VT-d or firmware ownership."""
import base64
import hashlib
import re
import struct
from hardware_haswell_firmware import firmware_result
from hardware_haswell_cache import cpu_record, effective_at
from hardware_haswell_alias import alias_preinit_result

PA, VA, SIZE = 0xe0000000, 0xffffc00300000000, 156 << 20
MCFG_PIN = '0e097004696cfc23012f877d7aeecf5652fe60b5f131a6775e24be8c83fe7abb'
SESSION = r'session-[0-9a-f]{16}-[0-9a-f]{2}'
HEADER = rf'hsw-ecam: v=5 state=([0-5]) complete=([01]) session=({SESSION}) epoch=(\d+) root=0x([0-9a-f]{{16}}) frame=0x([0-9a-f]{{16}})'
REF = rf'hsw-ecam-ref: v=5 session=({SESSION}) epoch=(\d+) root=0x([0-9a-f]{{16}}) frame=0x([0-9a-f]{{16}}) state=([0-5]) adopted=([01]) saved=([01])'


class Reader:
    def __init__(self, data): self.data, self.at = data, 0
    def raw(self, n):
        if n < 0 or self.at+n > len(self.data): raise ValueError('short raw receipt')
        v = self.data[self.at:self.at+n]; self.at += n; return v
    def take(self, fmt): return struct.unpack('<'+fmt, self.raw(struct.calcsize('<'+fmt)))
    def words(self, n): return list(self.take(f'{n}Q'))
    def delta(self, before):
        after = before.copy(); count, = self.take('H'); last = -1
        if count > 512: raise ValueError('delta bound')
        for _ in range(count):
            i, value = self.take('HQ')
            if not last < i < 512 or value == before[i]: raise ValueError('noncanonical delta')
            after[i] = value; last = i
        return after


def one(text, prefix, pattern):
    rows = [x for x in text.splitlines() if x.startswith(prefix)]
    if len(rows) != 1: raise ValueError('missing/duplicate envelope')
    m = re.fullmatch(pattern, rows[0])
    if not m: raise ValueError('envelope grammar')
    return m.groups()


def blob(text):
    parts = []
    for line in text.splitlines():
        if not line.startswith('hsw-ecam-data:'): continue
        m = re.fullmatch(r'hsw-ecam-data: v=5 chunk=(\d+) data=([A-Za-z0-9+/]*={0,2})', line)
        if not m or int(m[1]) != len(parts) or len(parts) >= 65: raise ValueError('chunk ordering')
        data = base64.b64decode(m[2], validate=True)
        if not 1 <= len(data) <= 512 or base64.b64encode(data).decode() != m[2]: raise ValueError('chunk bytes')
        parts.append(data)
    n, chunks, report, ok = map(int, one(text, 'hsw-ecam-end:', r'hsw-ecam-end: v=5 bytes=(\d+) chunks=(\d+) report=(\d+) ok=([01])'))
    if chunks != len(parts) or n != sum(map(len, parts)) or n > 33000 or not 0 < report < 1 << 64: raise ValueError('raw extent')
    if any(len(v) != min(512, n-i*512) for i, v in enumerate(parts)): raise ValueError('chunk extent')
    return b''.join(parts), report, ok


def firmware(reader):
    records = []
    for _ in range(2):
        meta = reader.take('6I3Q'); count = meta[5]
        if not 1 <= count <= 128: raise ValueError('catalog bound')
        dmar = reader.raw(136); rows = reader.raw(count*24)
        records.append((meta, dmar, rows))
    # Normalize a completed raw sub-operation into the existing strict grammar;
    # whole-trial completion is checked separately against the actual exit.log.
    text = f'hsw-firmware: v=1 status=1 count={records[0][0][5]} cap=128 dmar_bytes=136 limit_us=100000 mmio=0 writes=0 ownership=0\n'
    for phase, (meta, dmar, rows) in enumerate(records):
        for kind, raw in (('meta', struct.pack('<6I3Q', *meta)), ('dmar', dmar), ('map', rows)):
            text += f'hsw-firmware-{kind}: v=1 phase={phase} data='+base64.b64encode(raw).decode()+'\n'
    proof = firmware_result(text+'laptop-capture: completed=yes\n')
    if not proof or proof['status'] != 'OBSERVED': raise ValueError('firmware sub-proof')
    return proof


def decode_gate(r, root, hhdm, begin, end):
    phases, fw_bytes, config, selectors, efer_reads, reads0, reads1 = r.take('7I')
    if (phases, fw_bytes, config, selectors, efer_reads) != (2, 1840, 20, 40, 2): raise ValueError('fresh gate accounting')
    efer = r.take('2Q'); mcfg_pa = r.take('2Q'); mcfg = [r.raw(60), r.raw(60)]
    if efer[0] != efer[1] or efer[0] & ~0xd01 or efer[0] & 0xd00 != 0xd00: raise ValueError('NX enabled')
    if mcfg[0] != mcfg[1] or hashlib.sha256(mcfg[0]).hexdigest() != MCFG_PIN or mcfg_pa[0] != mcfg_pa[1]: raise ValueError('MCFG pin/endpoints')
    policy = [r.take('9I6Q') for _ in range(2)]
    for p in policy:
        if (p[0] < 0x80000008 or p[1] & 0x4100000 != 0x4100000 or p[2] or sorted(p[3:7]) != [0, 1, 2, 3] or
                p[7] != 15 or p[8] > 15 or any(p[8] & (1 << i) for i in range(4) if p[3+i]) or
                p[9] != 0x620 or p[10] & 512 or not p[10] & 2 or p[11:] != (0, 0, 4, 0)):
            raise ValueError('CPU0/parked topology/control')
    if policy[0][:10]+policy[0][11:] != policy[1][:10]+policy[1][11:]: raise ValueError('changed admission controls')
    if r.take('2I') != (1, 1): raise ValueError('incomplete firmware phases')
    cache_raw = [r.take('6I6Q4I43Q') for _ in range(2)]
    cpus = []
    for phase, c in enumerate(cache_raw):
        if c[12] != 1 or c[4] & 0x11028 != 0x11028 or c[6] & 0x80010001 != 0x80010001:
            raise ValueError('MSR snapshot status/features/protected paging')
        expanded = list(c[:12])+[root, policy[phase][9], hhdm, begin, end]+[c[13], c[14], c[15], 0]+list(c[16:])
        cpus.append(cpu_record(expanded, msr_only=True))
    if any(cache_raw[0][i] != cache_raw[1][i] for i in range(len(cache_raw[0])) if i not in (10, 11)) or (reads0, reads1) != tuple(c['msr_reads'] for c in cpus): raise ValueError('changed cache or read counts')
    fw = [firmware(r), firmware(r)]
    if r.at != len(r.data): raise ValueError('trailing wire data')
    a, b = fw[0]['samples'][0], fw[1]['samples'][0]
    for key in ('identity', 'table_pa', 'dmar_base64', 'catalog'):
        if a[key] != b[key]: raise ValueError('changed early firmware')
    if not begin <= a['pm_us'][0] <= fw[0]['samples'][1]['pm_us'][1] <= cpus[0]['pm_us'][0] < cpus[0]['pm_us'][1] <= b['pm_us'][0] <= fw[1]['samples'][1]['pm_us'][1] <= cpus[1]['pm_us'][0] < cpus[1]['pm_us'][1] <= end:
        raise ValueError('fresh operation ordering')
    catalog = a['catalog']
    for pa, size in ((a['table_pa'],136), (mcfg_pa[0],60)):
        if not 0 < pa < pa+size <= 1 << 39: raise ValueError('firmware source width')
        for page in range(pa & ~4095, (pa+size+4095) & ~4095, 4096):
            rows = [x for x in catalog if x[0] < page+4096 and page < x[0]+x[1]]
            if len(rows) != 1 or not rows[0][0] <= page < page+4096 <= sum(rows[0][:2]) or rows[0][2] not in (2,3):
                raise ValueError('complete firmware source page')
    return dict(policy=policy,efer=efer,cpus=cpus,firmware=fw,catalog=catalog,mcfg_pa=mcfg_pa)


def decode(data):
    r = Reader(data)
    if r.raw(8) != b'E5ECAM01': raise ValueError('wire version')
    state, root, pdpt, frame, hhdm, begin, end, adopted_us, words, writes, parents, inv, metadata, frees, adoptions = r.take('I7Q7I')
    if (state != 3 or not 0 < begin < end == adopted_us < 1 << 64 or end-begin >= 100000 or
            not 25281 <= words <= 65536 or (writes, parents, inv, metadata, frees, adoptions) != (512, 1, 39936, 39936, 0, 1)):
        raise ValueError('lifetime completion/bounds')
    if len({root, pdpt, frame}) != 3 or any(not p or p & 4095 or p >= 1 << 39 for p in (root, pdpt, frame)): raise ValueError('frame identity')
    if not hhdm or hhdm & 4095 or hhdm >> 47 != 0x1ffff or hhdm+((1<<39)-1) >= 1<<64: raise ValueError('HHDM geometry')
    before_count, after_count = r.take('2Q')
    if before_count > 128 or after_count != 128: raise ValueError('registry bounds')
    pre_root, pre_pdpt = r.words(512), r.words(512)
    post_root, post_pdpt = r.delta(pre_root), r.delta(pre_pdpt)
    pd = r.words(512)
    old_meta = [r.take('3Q') for _ in range(128)]; new_meta = [r.take('3Q') for _ in range(128)]
    allowed = (((1 << 39)-1) & ~4095) | 0x800000000000003b
    if pre_root != post_root or pre_root[384] & ~allowed or pre_root[384] & 3 != 3 or pre_root[384] & (((1 << 39)-1) & ~4095) != pdpt:
        raise ValueError('borrowed root')
    wanted = pre_pdpt.copy(); wanted[12] = frame | 3
    if pre_pdpt[12] or post_pdpt != wanted: raise ValueError('zero-slot publication')
    expected_pd = [(PA+i*(1 << 21)) | 0x8000000000000093 if i < 78 else 0 for i in range(512)]
    if pd != expected_pd: raise ValueError('large-leaf coverage/attributes')
    if any(VA <= a < VA+SIZE for a, _, _ in old_meta): raise ValueError('stale registry overlap')
    expected_meta = old_meta[:before_count] + [(VA+i*4096, PA+i*4096, 0x8000000000000012) for i in range(128-before_count)]
    if new_meta != expected_meta: raise ValueError('registry preservation')
    g=decode_gate(r,root,hhdm,begin,end)
    policy,efer,cpus,fw,catalog,mcfg_pa=(g[k] for k in ('policy','efer','cpus','firmware','catalog','mcfg_pa'))
    for pa in (root, pdpt, frame):
        rows = [x for x in catalog if x[0] < pa+4096 and pa < x[0]+x[1]]
        if len(rows) != 1 or not rows[0][0] <= pa < pa+4096 <= sum(rows[0][:2]) or rows[0][2] not in (0, 2, 3, 5, 6) or PA <= pa < PA+SIZE or pa in (0xfed90000, 0xfed91000): raise ValueError('table RAM source')
        if pa == frame and rows[0][2] != 0: raise ValueError('owned frame source')
    rows = [x for x in catalog if x[0] < mcfg_pa[0]+60 and mcfg_pa[0] < x[0]+x[1]]
    if len(rows) != 1 or not rows[0][0] <= mcfg_pa[0] < mcfg_pa[0]+60 <= sum(rows[0][:2]) or rows[0][2] not in (2, 3): raise ValueError('MCFG RAM source')
    for leaf in range(78):
        previous = None
        for i in range(512):
            values = [effective_at(c, 2, PA+leaf*(1 << 21)+i*4096) for c in cpus]
            if values[0] != values[1] or values[0]['status'] != 'CLASSIFIED_OBSERVATION' or values[0]['effective'] != 0 or (previous and previous != values[0]): raise ValueError('large-leaf memory type')
            previous = values[0]
    return dict(root=root, pdpt=pdpt, frame=frame, hhdm=hhdm, begin=begin, end=end,
                words=words, policy=policy, efer=efer, cpus=cpus, firmware=fw,
                root_before=pre_root, pdpt_before=pre_pdpt, pd=pd,
                metadata_before=old_meta, metadata_after=new_meta)


def ecam_result(ecam_text, exit_text, session):
    try: return _result(ecam_text, exit_text, session)
    except (ValueError, KeyError, IndexError, struct.error, TypeError): return None


def _result(ecam_text, exit_text, session):
    state, complete, name, epoch, root, frame = one(ecam_text, 'hsw-ecam:', HEADER)
    state, complete, epoch, root, frame = int(state), int(complete), int(epoch), int(root, 16), int(frame, 16)
    ref = one(exit_text, 'hsw-ecam-ref:', REF)
    if not re.fullmatch(SESSION, session) or name != session or ref != (name, str(epoch), f'{root:016x}', f'{frame:016x}', str(state), '1' if complete else '0', '1'):
        raise ValueError('session/epoch/reference mismatch')
    data, report, ok = blob(ecam_text)
    alias = alias_preinit_result(exit_text)
    if not complete:
        if data or ok or 'laptop-capture: completed=yes' in exit_text or exit_text.count('laptop-capture: completed=NO\n') != 1: raise ValueError('false completion')
        return dict(status='REFUSED' if state == 1 else 'FAILED', source_completed=False,
                    hardware_authority=False, ownership=False, vt_d_reads=0, native_mapping_authorized=False)
    if not ok or not alias or alias['status'] != 'OBSERVED_MANAGED_ROOT_CONTEXT' or exit_text.count('laptop-capture: completed=yes\n') != 1: raise ValueError('whole capture incomplete')
    proof = decode(data)
    if (proof['root'], proof['frame'], proof['begin']) != (root, frame, epoch) or not proof['end'] <= alias['preparation_us'][0] < alias['preparation_us'][1] <= report < alias['report_us']: raise ValueError('cross-file timing')
    if root != alias['root'] or proof['efer'][0] != alias['efer'] or proof['policy'][0][0] != alias['cpuid_max'] or proof['policy'][0][1] != alias['nx_edx']: raise ValueError('cross-file controls')
    from hardware_haswell_cache import cache_result
    later = cache_result(exit_text)
    if not later or later['status'] != 'OBSERVED_CONTEXT': raise ValueError('late cache proof')
    for early, late in zip(proof['cpus'], later['cpu_samples']):
        for key in ('pat', 'cap', 'default', 'variables', 'root', 'cr4', 'hhdm'):
            if early[key] != late[key]: raise ValueError('cross-file cache state')
        if early['raw'][:10] != late['raw'][:10] or early['raw'][21:] != late['raw'][21:]: raise ValueError('cross-file raw CPU state')
    from hardware_haswell_firmware import firmware_result
    late_fw = firmware_result(exit_text)
    for key in ('identity', 'table_pa', 'dmar_base64', 'catalog'):
        if proof['firmware'][0]['samples'][0][key] != late_fw['samples'][0][key]: raise ValueError('cross-file firmware')
    pages = {x['physical']:x for x in alias['raw_pages']}
    if pages[root]['level'] != 0 or (pages[root]['raw'][384] & ~32) != (proof['root_before'][384] & ~32) or pages[proof['pdpt']]['level'] != 1 or (pages[proof['pdpt']]['raw'][12] & ~32) != (frame | 3) or pages[frame]['level'] != 2: raise ValueError('census window linkage')
    for actual, expected in zip(pages[frame]['raw'], proof['pd']):
        # Explicitly observed PCI A/D transitions; the census's two passes still
        # require exact raw equality with no bit masking inside either pass.
        if (actual & ~96) != expected or (not expected and actual): raise ValueError('census ECAM leaf changes')
    return dict(status='OBSERVED_BOOT_ECAM_AND_MANAGED_ROOT', source_completed=True,
                session=session, epoch=epoch, root=root, frame=frame,
                ecam_cpu_mapping_observed=True, raw_bytes=len(data), mapper_words=proof['words'],
                alias_pages=alias['pages'], alias_preparation=alias.get('preparation'), unchanged_two_pass_raw=True,
                live_post_init_admission=False, all_hardware_aliases_proven=False,
                hardware_authority=False, ownership=False, vt_d_reads=0,
                native_mapping_authorized=False)
