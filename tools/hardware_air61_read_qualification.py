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
    return dict(policy=policy,efer=efer,cpus=cpus,firmware=fw,catalog=catalog,mcfg_pa=mcfg_pa,cache_raw=cache_raw)


def _pass(data, sampled=False):
    r=Reader(data)
    if r.raw(8)!=(b'R2PASS61' if sampled else b'R1PASS61'):raise ValueError('window version')
    state,=r.take('I')
    owner,epoch,root,pdpt,pd,pt,hhdm,begin,end=r.take('9Q')
    words,writes,parents,unlinks,invalidations,frees=r.take('6I')
    if state!=4 or owner!=(0x61697273616d70 if sampled else 0x61697272656164) or not 0<epoch<=begin<end or end-begin>=100000 or not 0<words<=65536 or (writes,parents,unlinks,invalidations,frees)!=(1026,1,1,2,2) or r.take('3B')!=(1,1,0):raise ValueError('retired lifetime')
    if len({root,pdpt,pd,pt})!=4 or any(not p or p&4095 or p>=1<<39 or p in (PA,PA+4096) for p in (root,pdpt,pd,pt)):raise ValueError('frames')
    if not hhdm or hhdm&4095 or hhdm>>47!=0x1ffff or hhdm+((1<<39)-1)>=1<<64:raise ValueError('HHDM')
    root0,pdpt0=r.words(512),r.words(512)
    allowed_root=(((1<<39)-1)&~4095)|0x800000000000003b
    if root0[384]&~allowed_root:raise ValueError('forbidden borrowed root flags')
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
    return dict(hhdm=hhdm,_cache_raw=g['cache_raw'],pd=pd,pt=pt,pdpt=pdpt,_policy=g['policy'][0],_mcfg_pa=g['mcfg_pa'][0],_root_words=root0,root=root,epoch=epoch,begin=begin,end=end,words=words,pages=pages,raw_bytes=len(data),census_bytes=b0,retirement_verified=True,claim_released=True,target_reads=0,target_writes=0,register_read_authorized=False,hardware_authority=False,ownership=False,_graph=graph,_cpus=g["cpus"],_firmware=g["firmware"],_controls=(g["efer"][0],g["policy"][0][0],g["policy"][0][1]))

def pack(raw, sampled=False):
    if not 0<len(raw)<=RAW_CAP:raise ValueError('raw bound')
    out=bytearray((b'Z2AIR610' if sampled else b'Z1AIR610')+struct.pack('<I',len(raw)))
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

def unpack(blob, sampled=False):
    if not 12<=len(blob)<=RECEIPT_CAP:raise ValueError('wire bound')
    r=Reader(blob)
    if r.raw(8)!=(b'Z2AIR610' if sampled else b'Z1AIR610'):raise ValueError('compression version')
    size,=r.take('I')
    if not 0<size<=RAW_CAP:raise ValueError('raw bound')
    out=bytearray()
    while len(out)<size:
        code,=r.take('H');n=code&32767
        if not n or n>size-len(out):raise ValueError('run bound')
        out+=bytes(n) if code&32768 else r.raw(n)
    if r.at!=len(blob) or pack(out,sampled)!=blob:raise ValueError('noncanonical compression')
    return bytes(out)

def decode(blob,session):
    return _decode(blob,session,False)

def _decode(blob,session,sampled):
    if not re.fullmatch(r'session-[0-9a-f]{16}-[0-9a-f]{2}',session):raise ValueError('session grammar')
    raw=unpack(blob,sampled);r=Reader(raw)
    if r.raw(8)!=(b'R2AIR610' if sampled else b'R1AIR610') or r.raw(27)!=session.encode():raise ValueError('series identity')
    owner,epoch,root,begin,end,gap=r.take('6Q')
    started,retired,spins=r.take('3I')
    if owner!=(0x61697273616d70 if sampled else 0x61697272656164) or not 0<epoch<=begin<end or (started,retired)!=(2,2) or spins>1000000 or not 20000<=gap<=100000:raise ValueError('series bounds')
    passes=[]
    for index in range(2):
        size,=r.take('I')
        if not 0<size<=33000:raise ValueError('pass extent')
        p=_pass(r.raw(size),sampled)
        if p['root']!=root or p['epoch']!=epoch+index:raise ValueError('pass binding')
        slots,coalescings,first,last=r.take('IIQQ')
        costs=r.take('7I');times=r.take('3Q');ram_steps,=r.take('Q')
        if not ram_steps:raise ValueError('RAM classifier accounting')
        if slots!=12 or coalescings!=1 or not p['begin']<=times[0]<=times[1]<=times[2]<=first<last<=p['end']:raise ValueError('slot/lifetime ordering')
        if costs[6]!=3*p['pages']*512 or costs[4]<costs[6] or costs[1]<p['words']+costs[6] or costs[2]<costs[1] or costs[3]<costs[1] or costs[5]!=1028 or not costs[0]:raise ValueError('cost accounting')
        if not p['_cpus'][1]['pm_us'][1]<=first:raise ValueError('fresh pre-slot gate')
        path=(p['_root_words'][384],p['pd']|0x23,p['pt']|0x23,LEAF)
        observations=[];values=[];load_times=[]
        admissions=[]
        if sampled:
            if r.take('2I')!=(12,12) or r.take('I')!=(13,):raise ValueError('read attempt/count')
            for index in range(13):
                attempted,=r.take('B');msrs,efer_reads=r.take('2I');efer,=r.take('Q');c=r.take('6I6Q4I43Q')
                base=p['_cache_raw'][1]
                if attempted!=1 or not 0<msrs<=49 or msrs!=c[13] or efer_reads!=1 or efer!=p['_controls'][0] or any(c[j]!=base[j] for j in range(len(c)) if j not in (10,11)):
                    raise ValueError('fresh slot cache/NX snapshot')
                if not p['begin']<=c[10]<=c[11]<=p['end']:raise ValueError('cache sample lifetime')
                admissions.append(dict(msr_reads=msrs,efer_reads=efer_reads,efer=efer,raw=c,begin=c[10],end=c[11]))
        for offset in OFFSETS:
            observed,=r.take('I');before=r.take('4Q');after=r.take('4Q')
            if observed!=offset or before!=path or after!=path:raise ValueError('exact per-slot path')
            observations.append((observed,before,after))
            if sampled:
                value,=r.take('I');lb,le=r.take('2Q')
                if not first<=lb<=le<=last or (load_times and load_times[-1][1]>lb):raise ValueError('read timing')
                values.append((observed,value));load_times.append((lb,le))
        if sampled:
            from haswell_vtd_read_lifetime_audit import sample_words
            sample_words(values)
            if load_times[0][0]!=first or load_times[-1][1]!=last:raise ValueError('read endpoints')
            for i,(lb,le) in enumerate(load_times):
                if admissions[i]['end']>lb or (i and admissions[i]['begin']<load_times[i-1][1]):raise ValueError('cache/load ordering')
            if admissions[12]['begin']<last:raise ValueError('post-batch cache ordering')
            p.update(raw_sample=values,load_times=load_times,target_reads=12,target_attempts=12,cache_admissions=admissions)
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
    if sampled and a['raw_sample']!=b['raw_sample']:raise ValueError('changed raw sample')
    return dict(status='RETAINED_QUALIFICATION_OBSERVATION',session=session,root=root,epoch=epoch,begin=begin,end=end,gap_us=gap,passes=passes,raw_payload=raw,wire_bytes=len(blob),target_reads=24 if sampled else 0,target_writes=0,source_completed=False,native_accepted=False,register_read_authorized=False,hardware_authority=False,ownership=False)


def read_result(text,ecam_text,exit_text,session):
    """Strict saved-session qualification only; never grants a target read."""
    try:return _read_result(text,ecam_text,exit_text,session)
    except (ValueError,KeyError,IndexError,TypeError,struct.error):return None


def _read_result(text,ecam_text,exit_text,session):
    if any(line.startswith('hsw-sample') for line in (text+'\n'+exit_text).splitlines()):raise ValueError('mixed sampled receipt')
    import base64
    from hardware_haswell_ecam import one,ecam_result
    state,complete,saved_session,epoch,root=one(text,'hsw-read:',r'hsw-read: v=1 state=([0-4]) complete=([01]) session=(session-[0-9a-f]{16}-[0-9a-f]{2}) epoch=(\d+) root=0x([0-9a-f]{16})')
    if session!=saved_session:raise ValueError('session mismatch')
    parts=[]
    for line in text.splitlines():
        if not line.startswith('hsw-read-data:'):continue
        m=re.fullmatch(r'hsw-read-data: v=1 chunk=(\d+) data=([A-Za-z0-9+/]*={0,2})',line)
        if not m or int(m[1])!=len(parts) or len(parts)>=88:raise ValueError('chunk ordering')
        b=base64.b64decode(m[2],validate=True)
        if not 1<=len(b)<=512 or base64.b64encode(b).decode()!=m[2]:raise ValueError('chunk encoding')
        parts.append(b)
    size,chunks,report,ok=map(int,one(text,'hsw-read-end:',r'hsw-read-end: v=1 bytes=(\d+) chunks=(\d+) report=(\d+) ok=([01]) target_reads=0 target_writes=0'))
    if chunks!=len(parts) or size!=sum(map(len,parts)) or size>45000 or not 0<report<1<<64 or any(len(p)!=min(512,size-i*512) for i,p in enumerate(parts)):raise ValueError('extent/report')
    ref=one(exit_text,'hsw-read-ref:',r'hsw-read-ref: v=1 session=(session-[0-9a-f]{16}-[0-9a-f]{2}) epoch=(\d+) root=0x([0-9a-f]{16}) state=([0-4]) saved=([01]) target_reads=0 target_writes=0')
    if ref!=(session,epoch,root,state,'1'):raise ValueError('saved reference')
    reason,=one(text,'hsw-read-failure:',r'hsw-read-failure: reason=([A-Za-z0-9/_-]+)')
    progress=[]
    for line in text.splitlines():
        if not line.startswith('hsw-read-pass:'):continue
        m=re.fullmatch(r'hsw-read-pass: pass=([01]) state=([0-5]) begin=(\d+) end=(\d+) words=(\d+) slots=(\d+) first=(\d+) last=(\d+) claimed=([01])',line)
        if not m or int(m[1])!=len(progress):raise ValueError('pass progress ordering')
        row=tuple(map(int,m.groups()[1:]))
        if row[4]>12 or row[3]>65536 or any(v>=1<<64 for v in row):raise ValueError('progress bounds')
        progress.append(row)
    if len(progress)!=2:raise ValueError('missing pass progress')
    options=[line.split('boot options=',1)[1].split() for line in exit_text.splitlines() if line.startswith('laptop-capture: boot options=')]
    required={'srvros.capture=haswell-firmware','srvros.hsw-cache=alias-nx-preinit','srvros.ecam=air61-2m-test','srvros.vtd-window=air61-two-pass-no-read'}
    if not options or any(not required<=set(row) or row.count('srvros.vtd-window=air61-two-pass-no-read')!=1 or 'srvros.vtd-window=air61-no-read' in row for row in options):raise ValueError('explicit boot profile')
    if complete=='0':
        if parts or ok or 'laptop-capture: completed=yes' in exit_text:raise ValueError('incomplete source')
        return dict(status='REFUSED' if state in ('0','2') else 'FAILED',source_completed=False,reason=reason,progress=progress,target_reads=0,target_writes=0,register_read_authorized=False,hardware_authority=False,ownership=False)
    if state!='3' or not ok or reason!='none':raise ValueError('series completion')
    p=decode(b''.join(parts),session)
    if p['root']!=int(root,16) or p['epoch']!=int(epoch) or p['end']>=report:raise ValueError('header/report binding')
    for v,row in zip(p['passes'],progress):
        if row!=(4,v['begin'],v['end'],v['words'],12,v['slot_begin'],v['slot_end'],0):raise ValueError('progress binding')
    return _bind_context(p,ecam_text,exit_text,session)


def _bind_context(p,ecam_text,exit_text,session):
    """Shared raw CPU/firmware binding only; never a live permission."""
    from hardware_haswell_ecam import ecam_result
    early=ecam_result(ecam_text,exit_text,session)
    if not early or not early['source_completed'] or early['root']!=p['root']:raise ValueError('early ECAM proof')
    from hardware_haswell_alias import alias_preinit_result
    from hardware_haswell_cache import cache_result
    from hardware_haswell_firmware import firmware_result
    alias=alias_preinit_result(exit_text)
    if not alias or alias['status']!='OBSERVED_MANAGED_ROOT_CONTEXT' or alias['end_us']>p['begin']:raise ValueError('pre-user census ordering')
    a=p['passes'][0]
    if a['_controls']!=(alias['efer'],alias['cpuid_max'],alias['nx_edx']):raise ValueError('changed NX controls')
    before={v['physical']:v for v in alias['raw_pages']};after=a['_graph']
    if set(before)!=set(after):raise ValueError('changed graph pages')
    changes=[]
    # Same existing source-to-claim relation as W1: only one-way hardware A/D
    # promotions between distinct epochs. Retain every raw delta. Inside each
    # lifetime and between the two pass snapshots all comparisons stay exact.
    for pa,row in before.items():
        if row['level']!=after[pa]['level']:raise ValueError('changed graph level')
        for index,(x,y) in enumerate(zip(row['raw'],after[pa]['raw'])):
            leaf=row['level']==3 or row['level'] in (1,2) and x&128
            allowed=(32|(64 if leaf and x&2 else 0)) if x&1 else 0
            if (x^y)&~allowed or (x&allowed)&~y:raise ValueError('source-to-claim graph mutation')
            if x!=y:changes.append(dict(page=pa,index=index,before=x,after=y))
    source_cache=cache_result(exit_text)
    source_fw=firmware_result(exit_text)
    if not source_cache or not source_fw:raise ValueError('missing independent source')
    base=source_cache['cpu_samples'][0]
    fw=source_fw['samples'][0]
    for v in p['passes']:
        for cpu in v['_cpus']:
            if any(cpu[k]!=base[k] for k in ('root','cr4','hhdm','pat','cap','default','variables')) or cpu['raw'][:10]!=base['raw'][:10] or cpu['raw'][21:]!=base['raw'][21:]:raise ValueError('source CPU/cache')
        for pair in v['_firmware']:
            for sample in pair['samples']:
                if any(sample[key]!=fw[key] for key in ('identity','table_pa','dmar_base64','catalog')):raise ValueError('source firmware')
    return dict(status='OBSERVED_RETIRED_TWO_PASS_ZERO_LOAD',source_completed=True,session=session,root=p['root'],epoch=p['epoch'],begin=p['begin'],end=p['end'],gap_us=p['gap_us'],passes=p['passes'],source_to_claim_ad_changes=changes,retirement_verified=True,claim_released=True,target_reads=0,target_writes=0,register_read_authorized=False,hardware_authority=False,ownership=False)
