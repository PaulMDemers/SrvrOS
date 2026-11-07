"""Strict retained window-only proof; never authorizes a register load."""
import base64
import re
import struct
from hardware_haswell_ecam import Reader, one, decode_gate, ecam_result
from hardware_haswell_alias import decode_pages
from hardware_haswell_cache import effective_at
PA=0xfed90000
VA=0xffffc001f3250000
LEAF=0x80000000fed90019

def decode(data):
    r=Reader(data)
    if r.raw(8)!=b'W1AIR610':raise ValueError('window version')
    state,=r.take('I')
    owner,epoch,root,pdpt,pd,pt,hhdm,begin,end=r.take('9Q')
    words,writes,parents,unlinks,invalidations,frees=r.take('6I')
    if state!=4 or owner!=0x616972363177696e or not 0<epoch<=begin<end or end-begin>=100000 or not 0<words<=65536 or (writes,parents,unlinks,invalidations,frees)!=(1026,1,1,2,2) or r.take('3B')!=(1,1,0):raise ValueError('retired lifetime')
    if len({root,pdpt,pd,pt})!=4 or any(not p or p&4095 or p>=1<<39 or p in (PA,PA+4096) for p in (root,pdpt,pd,pt)):raise ValueError('frames')
    if not hhdm or hhdm&4095 or hhdm>>47!=0x1ffff or hhdm+((1<<39)-1)>=1<<64:raise ValueError('HHDM')
    root0,pdpt0=r.words(512),r.words(512)
    root1,pdpt1,root2,pdpt2=r.delta(root0),r.delta(pdpt0),r.delta(root0),r.delta(pdpt0)
    pd_words,pt_words=r.delta([0]*512),r.delta([0]*512)
    expected=pdpt0.copy();expected[7]=pd|3
    if root0!=root1 or root0!=root2 or root0[384]&(((1<<39)-1)&~4095)!=pdpt or root0[384]&7!=3 or pdpt0[7] or pdpt1!=expected or pdpt2!=pdpt0:raise ValueError('borrowed publication/retirement')
    if pd_words!=[pt|3 if i==409 else 0 for i in range(512)] or pt_words!=[LEAF if i==80 else 0 for i in range(512)]:raise ValueError('exact private mapping')
    status,pages,prep_reads,prep_pages,reads0,reads1,b0,b1,targets=r.take('9I')
    cb,ce,cf=r.take('3Q')
    if status!=1 or not 1<=pages<=64 or prep_pages!=pages or (prep_reads,reads0,reads1)!=(pages*512,)*3 or not 0<b0<=4096 or b0!=b1 or targets or not begin<=cb<ce<cf<=end or cf-cb>=100000:raise ValueError('bounded census')
    raw0,raw1=r.raw(b0),r.raw(b1)
    if raw0!=raw1:raise ValueError('raw census changed')
    graph=decode_pages(raw0,pages)
    g=decode_gate(r,root,hhdm,begin,end);catalog=g['catalog']
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
    return dict(root=root,epoch=epoch,begin=begin,end=end,words=words,pages=pages,raw_bytes=len(data),census_bytes=b0,retirement_verified=True,claim_released=True,target_reads=0,target_writes=0,register_read_authorized=False,hardware_authority=False,ownership=False,_graph=graph,_cpus=g["cpus"],_firmware=g["firmware"],_controls=(g["efer"][0],g["policy"][0][0],g["policy"][0][1]))

def window_result(window_text,ecam_text,exit_text,session):
    try:return _result(window_text,ecam_text,exit_text,session)
    except (ValueError,KeyError,IndexError,TypeError,struct.error):return None

def _result(text,ecam_text,exit_text,session):
    state,complete,saved_session,epoch,root=one(text,'hsw-window:',r'hsw-window: v=1 state=([0-5]) complete=([01]) session=(session-[0-9a-f]{16}-[0-9a-f]{2}) epoch=(\d+) root=0x([0-9a-f]{16})')
    parts=[]
    for line in text.splitlines():
        if not line.startswith('hsw-window-data:'):continue
        m=re.fullmatch(r'hsw-window-data: v=1 chunk=(\d+) data=([A-Za-z0-9+/]*={0,2})',line)
        if not m or int(m[1])!=len(parts) or len(parts)>=65:raise ValueError('chunk order')
        raw=base64.b64decode(m[2],validate=True)
        if not 1<=len(raw)<=512 or base64.b64encode(raw).decode()!=m[2]:raise ValueError('chunk encoding')
        parts.append(raw)
    size,chunks,report,ok=map(int,one(text,'hsw-window-end:',r'hsw-window-end: v=1 bytes=(\d+) chunks=(\d+) report=(\d+) ok=([01]) target_reads=0 target_writes=0'))
    if session!=saved_session or chunks!=len(parts) or size!=sum(map(len,parts)) or size>33000 or not report or any(len(p)!=min(512,size-i*512) for i,p in enumerate(parts)):raise ValueError('envelope')
    ref=one(exit_text,'hsw-window-ref:',r'hsw-window-ref: v=1 session=(session-[0-9a-f]{16}-[0-9a-f]{2}) epoch=(\d+) root=0x([0-9a-f]{16}) state=([0-5]) saved=([01]) target_reads=0 target_writes=0')
    if ref!=(session,epoch,root,state,'1'):raise ValueError('saved reference')
    progress=tuple(map(int,one(text,'hsw-window-progress:',r'hsw-window-progress: begin=(\d+) end=(\d+) census=(\d+) pages=(\d+) prepare=(\d+) reads=(\d+),(\d+) bytes=(\d+),(\d+) gate_phases=(\d+) firmware_bytes=(\d+) parent=(\d+) unlink=(\d+) inv=(\d+) frees=(\d+)')))
    reason,gate,failed_words=one(text,'hsw-window-failure:',r'hsw-window-failure: reason=([A-Za-z0-9/_-]+) gate=([A-Za-z0-9/_-]+) words=(\d+)')
    if progress[2]>5 or progress[3]>64 or any(x>32768 for x in progress[4:7]) or any(x>4096 for x in progress[7:9]) or progress[9]>2 or progress[10]>4096 or any(x>1 for x in progress[11:13]) or any(x>2 for x in progress[13:15]) or int(failed_words)>65536:raise ValueError('failure progress bounds')
    cost=None
    if any(x.startswith('hsw-window-cost:') for x in text.splitlines()):
        cost_parts=one(text,'hsw-window-cost:',r'hsw-window-cost: v=1 clocks=(\d+) loads=(\d+) guards=(\d+) ram=(\d+) reads=(\d+) stores=(\d+) census_reads=(\d+) gate_end=(\d+) census_begin=(\d+) census_end=(\d+)(?: ram_steps=(\d+))?')
        values=tuple(int(x) for x in cost_parts if x is not None)
        if any(x>=1<<32 for x in values[:7]) or any(x>=1<<64 for x in values[7:]):raise ValueError('cost bounds')
        cost=dict(zip(('clocks','loads','guards','ram','reads','stores','census_reads','gate_end','census_begin','census_end','ram_steps'),values))
        if cost['census_reads']!=sum(progress[4:7]) or cost['reads']<cost['census_reads'] or cost['loads']<int(failed_words)+cost['census_reads']:raise ValueError('cost read accounting')
        if not 0<=cost['gate_end']<=cost['census_begin']<=cost['census_end']<=progress[1]:raise ValueError('cost timing')
    if complete=='0':
        if parts or ok or 'laptop-capture: completed=yes' in exit_text:raise ValueError('incomplete acceptance')
        return dict(status='REFUSED' if int(state) in (0,3) else 'FAILED',source_completed=False,reason=reason,gate=gate,progress=progress,mapper_words=int(failed_words),cost=cost,target_reads=0,target_writes=0,hardware_authority=False,ownership=False)
    joint=ecam_result(ecam_text,exit_text,session)
    if not ok or not joint or not joint['source_completed']:raise ValueError('whole source')
    p=decode(b''.join(parts))
    if progress!=(p['begin'],p['end'],1,p['pages'],p['pages']*512,p['pages']*512,p['pages']*512,p['census_bytes'],p['census_bytes'],2,1840,1,1,2,2) or reason!='none' or gate!='none' or int(failed_words)!=p['words']:raise ValueError('success progress')
    if p['root']!=int(root,16) or p['root']!=joint['root'] or p['epoch']!=int(epoch) or not p['end']<report:raise ValueError('identity/order')
    from hardware_haswell_alias import alias_preinit_result
    alias=alias_preinit_result(exit_text)
    if not alias or alias['end_us']>p['begin']:raise ValueError('pre-user ordering')
    if p.pop('_controls')!=(alias['efer'],alias['cpuid_max'],alias['nx_edx']):raise ValueError('changed NX/EFER controls')
    before={v['physical']:v for v in alias['raw_pages']}
    after=p.pop('_graph')
    if set(before)!=set(after):raise ValueError('changed root graph')
    for pa,row in before.items():
        if row['level']!=after[pa]['level']:raise ValueError('changed graph level')
        for a,b in zip(row['raw'],after[pa]['raw']):
            leaf=row['level']==3 or (row['level'] in (1,2) and a&128)
            allowed=(32|(64 if leaf and a&2 else 0)) if a&1 else 0
            if (a^b)&~allowed or (a&allowed)&~b:raise ValueError('cross-epoch graph mutation')
    from hardware_haswell_cache import cache_result
    from hardware_haswell_firmware import firmware_result
    old_cache=cache_result(exit_text)
    for cpu in p.pop('_cpus'):
        base=old_cache['cpu_samples'][0]
        if any(cpu[k]!=base[k] for k in ('root','cr4','hhdm','pat','cap','default','variables')) or cpu['raw'][:10]!=base['raw'][:10] or cpu['raw'][21:]!=base['raw'][21:]:raise ValueError('changed CPU/cache controls')
    old_fw=firmware_result(exit_text)['samples'][0]
    for fw in p.pop('_firmware'):
        for sample in fw['samples']:
            for key in ('identity','table_pa','dmar_base64','catalog'):
                if sample[key]!=old_fw[key]:raise ValueError('changed firmware epoch')
    return dict(status='OBSERVED_RETIRED_WINDOW_ONLY',source_completed=True,session=session,cost=cost,**p)
