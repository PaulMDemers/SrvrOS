"""Strict lossless dormant two-pass proof. Never grants native admission/I/O."""
import hashlib
import re
import struct
from hardware_haswell_ecam import Reader,firmware,cpu_record,MCFG_PIN
from hardware_haswell_alias import decode_pages
from hardware_haswell_cache import effective_at
PA=0xfed90000
LEAF=0x80000000fed90039
RECEIPT_CAP=45000
RAW_CAP=70000
OFFSETS=(0x1c,0,0x0c,8,0x0c,0x14,0x10,0x14,0x24,0x20,0x24,0x1c)

def _read_gate(r, root, hhdm, begin, end):
    phases, fw_bytes, config, selectors, efer_reads, reads0, reads1 = r.take('7I')
    if (phases, fw_bytes, config, selectors, efer_reads) != (2, 3996, 20, 40, 2): raise ValueError('fresh gate accounting')
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


def _pass(data):
    r=Reader(data)
    if r.raw(8)!=b'R1PASS61':raise ValueError('window version')
    state,=r.take('I')
    owner,epoch,root,pdpt,pd,pt,hhdm,begin,end=r.take('9Q')
    words,writes,parents,unlinks,invalidations,frees=r.take('6I')
    if state!=4 or owner!=0x61697272656164 or not 0<epoch<=begin<end or end-begin>=100000 or not 0<words<=65536 or (writes,parents,unlinks,invalidations,frees)!=(1026,1,1,2,2) or r.take('3B')!=(1,1,0):raise ValueError('retired lifetime')
    if len({root,pdpt,pd,pt})!=4 or any(not p or p&4095 or p>=1<<39 or p in (PA,PA+4096) for p in (root,pdpt,pd,pt)):raise ValueError('frames')
    if not hhdm or hhdm&4095 or hhdm>>47!=0x1ffff or hhdm+((1<<39)-1)>=1<<64:raise ValueError('HHDM')
    root0,pdpt0=r.words(512),r.words(512)
    root1,pdpt1,root2,pdpt2=r.delta(root0),r.delta(pdpt0),r.delta(root0),r.delta(pdpt0)
    pd_words,pt_words=r.delta([0]*512),r.delta([0]*512)
    expected=pdpt0.copy();expected[7]=pd|0x23
    if root0!=root1 or root0!=root2 or root0[384]&(((1<<39)-1)&~4095)!=pdpt or root0[384]&7!=3 or not root0[384]&32 or pdpt0[7] or pdpt1!=expected or pdpt2!=pdpt0:raise ValueError('borrowed publication/retirement')
    if pd_words!=[pt|0x23 if i==409 else 0 for i in range(512)] or pt_words!=[LEAF if i==80 else 0 for i in range(512)]:raise ValueError('exact private mapping')
    status,pages,prep_reads,prep_pages,reads0,reads1,b0,b1,targets=r.take('9I')
    cb,ce,cf=r.take('3Q')
    if status!=1 or not 1<=pages<=64 or prep_pages!=pages or (prep_reads,reads0,reads1)!=(pages*512,)*3 or not 0<b0<=4096 or b0!=b1 or targets or not begin<=cb<ce<cf<=end or cf-cb>=100000:raise ValueError('bounded census')
    raw0,raw1=r.raw(b0),r.raw(b1)
    if raw0!=raw1:raise ValueError('raw census changed')
    graph=decode_pages(raw0,pages)
    g=_read_gate(r,root,hhdm,begin,end);catalog=g['catalog']
    def ram(page,owned=False):
        rows=[x for x in catalog if x[0]<page+4096 and page<x[0]+x[1]]
        if len(rows)!=1 or not rows[0][0]<=page<page+4096<=sum(rows[0][:2]) or rows[0][2] not in ((0,) if owned else (0,2,3,5,6)):raise ValueError('RAM provenance')
    if root not in graph or graph[root]['level'] or pdpt not in graph or graph[pdpt]['level']!=1 or graph[root]['raw']!=root0 or graph[pdpt]['raw']!=pdpt0 or pd in graph or pt in graph:raise ValueError('graph binding')
    if pd<0x2000000 or pt<0x2000000:raise ValueError('allocation minimum')
    ram(pd,True);ram(pt,True)
    reached={root};todo=[root];addr=((1<<39)-1)&~4095
    while todo:
        page=todo.pop();ram(page)
        if page in (PA,PA+4096):raise ValueError('target source')
        node=graph[page];level=node['level']
        for word in node['raw']:
            if not word&1:continue
            if word&0x000ffffffffff000&~addr or (not level and word&128):raise ValueError('reserved graph')
            if level==3 or (level in (1,2) and word&128):
                shift=(39,30,21,12)[level];low=(1<<shift)-1
                if level!=3 and word&addr&low&~4096:raise ValueError('large alignment')
                base=word&addr&~low
                if base<=PA<=base+low:raise ValueError('existing target alias')
            else:
                child=word&addr
                if child not in graph or graph[child]['level']!=level+1:raise ValueError('graph completeness')
                if child not in reached:reached.add(child);todo.append(child)
    if len(reached)!=len(graph):raise ValueError('orphan graph')
    if any(x[0]<PA+4096 and PA<x[0]+x[1] for x in catalog):raise ValueError('catalog target overlap')
    from haswell_vtd_read_audit import candidate_resource
    candidate_resource(catalog)
    for cpu in g['cpus']:
        c=effective_at(cpu,3,PA)
        if c['status']!='CLASSIFIED_OBSERVATION' or c['effective']!=0:raise ValueError('strong UC')
    if not g['cpus'][0]['pm_us'][1]<=cb<cf<=g['firmware'][1]['samples'][0]['pm_us'][0]:raise ValueError('fresh census ordering')
    return dict(pd=pd,pt=pt,pdpt=pdpt,_policy=g['policy'][0],_mcfg_pa=g['mcfg_pa'][0],_root_words=root0,root=root,epoch=epoch,begin=begin,end=end,words=words,pages=pages,raw_bytes=len(data),census_bytes=b0,retirement_verified=True,claim_released=True,target_reads=0,target_writes=0,register_read_authorized=False,hardware_authority=False,ownership=False,_graph=graph,_cpus=g["cpus"],_firmware=g["firmware"],_controls=(g["efer"][0],g["policy"][0][0],g["policy"][0][1]))

def pack(raw):
    if not 0<len(raw)<=RAW_CAP:raise ValueError('raw bound')
    out=bytearray(b'Z1AIR610'+struct.pack('<I',len(raw)))
    def zeros(at):
        end=at
        while end<len(raw) and raw[end]==0 and end-at<32767:end+=1
        return end-at
    at=0
    while at<len(raw):
        n=zeros(at)
        if n>=4:
            out+=struct.pack('<H',0x8000|n);at+=n
        else:
            start=at;at+=1
            while at<len(raw) and at-start<32767 and zeros(at)<4:at+=1
            out+=struct.pack('<H',at-start)+raw[start:at]
    if len(out)>RECEIPT_CAP:raise ValueError('wire bound')
    return bytes(out)

def unpack(blob):
    if not 12<=len(blob)<=RECEIPT_CAP:raise ValueError('wire bound')
    r=Reader(blob)
    if r.raw(8)!=b'Z1AIR610':raise ValueError('compression version')
    size,=r.take('I')
    if not 0<size<=RAW_CAP:raise ValueError('raw bound')
    out=bytearray()
    while len(out)<size:
        code,=r.take('H');n=code&32767
        if not n or n>size-len(out):raise ValueError('run bound')
        out+=bytes(n) if code&32768 else r.raw(n)
    if r.at!=len(blob) or pack(out)!=blob:raise ValueError('noncanonical compression')
    return bytes(out)

def decode(blob,session):
    if not re.fullmatch(r'session-[0-9a-f]{16}-[0-9a-f]{2}',session):raise ValueError('session grammar')
    raw=unpack(blob);r=Reader(raw)
    if r.raw(8)!=b'R1AIR610' or r.raw(27)!=session.encode():raise ValueError('series identity')
    owner,epoch,root,begin,end,gap=r.take('6Q')
    started,retired,spins=r.take('3I')
    if owner!=0x61697272656164 or not 0<epoch<=begin<end or (started,retired)!=(2,2) or spins>1000000 or not 20000<=gap<=100000:raise ValueError('series bounds')
    passes=[]
    for index in range(2):
        size,=r.take('I')
        if not 0<size<=33000:raise ValueError('pass extent')
        p=_pass(r.raw(size))
        if p['root']!=root or p['epoch']!=epoch+index:raise ValueError('pass binding')
        slots,coalescings,first,last=r.take('IIQQ')
        costs=r.take('7I');times=r.take('3Q');ram_steps,=r.take('Q')
        if not ram_steps:raise ValueError('RAM classifier accounting')
        if slots!=12 or coalescings!=1 or not p['begin']<=times[0]<=times[1]<=times[2]<=first<last<=p['end']:raise ValueError('slot/lifetime ordering')
        if costs[6]!=3*p['pages']*512 or costs[4]<costs[6] or costs[1]<p['words']+costs[6] or costs[2]<costs[1] or costs[3]<costs[1] or costs[5]!=1028 or not costs[0]:raise ValueError('cost accounting')
        if not p['_cpus'][1]['pm_us'][1]<=first:raise ValueError('fresh pre-slot gate')
        path=(p['_root_words'][384],p['pd']|0x23,p['pt']|0x23,LEAF)
        observations=[]
        for offset in OFFSETS:
            observed,=r.take('I');before=r.take('4Q');after=r.take('4Q')
            if observed!=offset or before!=path or after!=path:raise ValueError('exact per-slot path')
            observations.append((observed,before,after))
        p.update(slot_begin=first,slot_end=last,cost=costs,timing=times,ram_steps=ram_steps,slots=observations)
        passes.append(p)
    if r.at!=len(raw):raise ValueError('trailing series')
    a,b=passes
    if begin!=a['begin'] or end!=b['end'] or a['end']>b['begin'] or b['slot_begin']-a['slot_end']!=gap:raise ValueError('independent lifetime/gap')
    if a['_controls']!=b['_controls'] or a['_policy'][:10]+a['_policy'][11:]!=b['_policy'][:10]+b['_policy'][11:] or a['_mcfg_pa']!=b['_mcfg_pa']:raise ValueError('changed pass controls')
    x,y=a['_firmware'][0]['samples'][0],b['_firmware'][0]['samples'][0]
    for key in ('identity','table_pa','dmar_base64','catalog'):
        if x[key]!=y[key]:raise ValueError('changed pass source')
    for i,(x,y) in enumerate(zip(a['_cpus'][0]['raw'],b['_cpus'][0]['raw'])):
        if i not in (10,11,15,16) and x!=y:raise ValueError('changed pass cache')
    if a['_graph']!=b['_graph']:raise ValueError('changed borrowed graph')
    return dict(status='RETAINED_QUALIFICATION_OBSERVATION',session=session,root=root,epoch=epoch,begin=begin,end=end,gap_us=gap,passes=passes,raw_payload=raw,wire_bytes=len(blob),target_reads=0,target_writes=0,source_completed=False,native_accepted=False,register_read_authorized=False,hardware_authority=False,ownership=False)
