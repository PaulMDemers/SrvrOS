"""Strict raw cache/CPU-page-table context. No VT-d data or mapping authority."""
import base64
import re
import struct

from hardware_haswell_firmware import firmware_result

UNIT = 0xfed90000
WINDOW = 0xffffc001f3250000
LEGAL = (0, 1, 4, 5, 6)


def unpack(text, fmt):
    raw = base64.b64decode(text, validate=True)
    if len(raw) != struct.calcsize(fmt) or base64.b64encode(raw).decode() != text:
        raise ValueError('canonical context record')
    return list(struct.unpack(fmt, raw))


def cpu_record(v):
    if v[:4] != [0x756e6547, 0x49656e69, 0x6c65746e, 0x40651] or v[4] & 0x11020 != 0x11020 or v[5] != 39 or v[6] & (3 << 29):
        raise ValueError('exact Haswell CPU/features/cache mode')
    cr0, pat, cap, default, begin, end, root, cr4, hhdm, outer0, outer1 = v[6:17]
    reads, count, fixed, ptes = v[17:21]
    if count != cap & 255 or count > 16 or fixed != (11 if cap & 256 else 0) or reads != 6 + 2*count + fixed or reads > 49 or not 1 <= ptes <= 16:
        raise ValueError('read/array bounds')
    if default & ~0xcff or default & 255 not in LEGAL or (default & 255 == 1 and not cap & 1024) or (default & 0x400 and not cap & 256):
        raise ValueError('MTRR default')
    if not 0 < outer0 <= begin < end <= outer1 or outer1-outer0 > 100000 or not root or root & 4095 or root >= 1 << 39 or cr4 & (1 << 12) or not hhdm or hhdm + UNIT >= 1 << 64:
        raise ValueError('context clock/root/mode')
    types = [(pat >> (8*i)) & 255 for i in range(8)]
    if any(t not in LEGAL + (7,) for t in types):
        raise ValueError('PAT encoding')
    pairs = list(zip(v[21:53:2], v[22:53:2])); variables=[]
    addr = ((1 << 39)-1) & ~4095
    for b, m in pairs[:count]:
        if m & 0x800:
            size = (((1 << 39)-1) ^ (m & addr)) + 1
            if b & ~(addr | 255) or m & ~(addr | 0x800) or b & 255 not in LEGAL or (b & 255 == 1 and not cap & 1024) or size < 4096 or size & (size-1) or b & addr & (size-1):
                raise ValueError('MTRR variable mask/type')
            variables.append((b & addr, size, b & 255))
    if any(b or m for b, m in pairs[count:]) or any(v[53+fixed:]):
        raise ValueError('unused array contents')
    if any((word >> (8*j)) & 255 not in LEGAL or (((word >> (8*j)) & 255) == 1 and not cap & 1024) for word in v[53:53+fixed] for j in range(8)):
        raise ValueError('fixed MTRR types')
    return dict(raw=v, pat=pat, cap=cap, default=default, variables=variables,
                pm_us=[begin,end], context_us=[outer0,outer1], root=root, cr4=cr4,
                hhdm=hhdm, pte_reads=ptes, msr_reads=reads)


def effective(cpu, index):
    seen={t for b,n,t in cpu['variables'] if b <= UNIT < b+n}
    if not cpu['default'] & 0x800:
        m=0
    elif not seen:
        m=cpu['default'] & 255
    elif 0 in seen:
        m=0
    elif seen=={4,6}:
        m=4
    elif len(seen)==1:
        m=next(iter(seen))
    else:
        return dict(status='UNRESOLVED', reason='undefined variable-MTRR overlap')
    p=(cpu['pat'] >> (8*index)) & 255
    matrix={0:{0:0,1:1,4:0,5:0,6:0,7:0},
            1:{0:0,1:1,4:0,5:0,6:1,7:1},
            4:{0:0,1:1,4:4,5:5,6:4,7:0},
            5:{0:0,1:1,4:4,5:5,6:5,7:1},
            6:{0:0,1:1,4:4,5:5,6:6,7:0}}
    return dict(status='CLASSIFIED_OBSERVATION',mtrr=m,pat=p,effective=matrix[m][p] if cpu['default'] & 0x800 else 0)


def walk_record(v, va, root, rows):
    if v[0]!=va or not (va>>47==0 or va>>47==0x1ffff) or not 1<=v[10]<=4 or v[12] not in (1,2):
        raise ValueError('walk header')
    table=root;addr=((1<<39)-1)&~4095
    for i in range(v[10]):
        t,e=v[2+2*i:4+2*i]
        covered=[r for r in rows if r[0]<t+4096 and t<r[0]+r[1]]
        if t!=table or len(covered)!=1 or not covered[0][0]<=t<t+4096<=covered[0][0]+covered[0][1] or covered[0][2] not in (0,2,3,5,6):
            raise ValueError('page-table source is not admitted RAM')
        last=i==v[10]-1
        if not e&1:
            if not last or v[12]!=1 or v[1] or v[11]:raise ValueError('absent walk')
            break
        if e & 0x000ffffffffff000 & ~addr or (i==0 and e&128):raise ValueError('walk physical bits')
        if i==3 or (e&128 and i in (1,2)):
            shift=(39,30,21,12)[i];low=(1<<shift)-1
            if not last or v[12]!=2 or v[11]!=shift or (i!=3 and e&addr&low&~4096) or v[1]!=(e&addr&~low)|(va&low):raise ValueError('leaf translation')
            break
        if last:raise ValueError('incomplete present walk')
        table=e&addr
    if any(v[2+2*v[10]:10]):raise ValueError('unused walk slots')
    return dict(raw=v,state=v[12],pa=v[1],shift=v[11],levels=v[10],leaf=v[3+2*(v[10]-1)])


def cache_result(text):
    try:return _parse(text)
    except (ValueError,KeyError,IndexError,struct.error):return None


def _parse(text):
    summaries=[x for x in text.splitlines() if x.startswith('hsw-cache:')]
    if len(summaries)!=1:return None
    m=re.fullmatch(r'hsw-cache: v=1 status=([1-4]) samples=([02]) msr_cap=49 pte_cap=16 limit_us=100000 mappings=0 writes=0 ownership=0',summaries[0])
    if not m:return None
    status,count=map(int,m.groups())
    if status!=1:
        if count or 'laptop-capture: completed=yes' in text or any(x.startswith(('hsw-cache-cpu:','hsw-cache-walk:')) for x in text.splitlines()):return None
        return dict(status='REFUSED' if status==2 else 'FAILED',source_status=status,hardware_authority=False,mappings=0,ownership=False)
    if count!=2:return None
    fw=firmware_result(text)
    post_text='\n'.join(x.replace('hcache-firmware-post','hsw-firmware') for x in text.splitlines() if x.startswith(('hcache-firmware-post','laptop-capture: completed=')))+'\n'
    post=firmware_result(post_text)
    if not fw or not post or fw['status']!='OBSERVED' or post['status']!='OBSERVED':return None
    for key in ('identity','table_pa','dmar_base64','catalog'):
        if fw['samples'][0][key]!=post['samples'][0][key]:raise ValueError('changed firmware endpoint')
    cpus={};walks={}
    for line in text.splitlines():
        if line.startswith('hsw-cache-cpu:'):
            m=re.fullmatch(r'hsw-cache-cpu: v=1 phase=([01]) data=([A-Za-z0-9+/]*={0,2})',line)
            if not m or int(m[1]) in cpus:raise ValueError('CPU record')
            cpus[int(m[1])]=cpu_record(unpack(m[2],'<6I11Q4I43Q'))
        elif line.startswith('hsw-cache-walk:'):
            m=re.fullmatch(r'hsw-cache-walk: v=1 phase=([01]) edge=([01]) alias=([01]) data=([A-Za-z0-9+/]*={0,2})',line)
            if not m:raise ValueError('walk record')
            key=tuple(map(int,m.groups()[:3]))
            if key in walks:raise ValueError('duplicate walk')
            walks[key]=unpack(m[4],'<10Q3I')
    if len(cpus)!=2 or len(walks)!=8:return None
    a,b=cpus[0],cpus[1]
    # Only timings and read totals may differ; every raw CPU/control/MTRR word stays exact.
    for i in range(64):
        if i not in (10,11,15,16,20) and a['raw'][i]!=b['raw'][i]:raise ValueError('changed CPU context')
    if not fw['samples'][1]['pm_us'][1]<=a['context_us'][0]<a['context_us'][1]<=b['context_us'][0] or b['context_us'][0]-a['context_us'][1]>100000 or b['context_us'][1]>post['samples'][0]['pm_us'][0]:raise ValueError('context/firmware ordering')
    parsed={}
    for (phase,edge,alias),raw in walks.items():
        c=cpus[phase];w=walk_record(raw,WINDOW if alias else c['hhdm']+UNIT,c['root'],fw['samples'][0]['catalog']);parsed[phase,edge,alias]=w
        if (alias and w['state']!=1) or (not alias and w['state']==2 and w['pa']!=UNIT):raise ValueError('unexpected target translation')
        if raw!=walks[0,0,alias]:raise ValueError('changed raw page-table walk')
    for phase in (0,1):
        if sum(parsed[phase,e,a]['levels'] for e in (0,1) for a in (0,1))!=cpus[phase]['pte_reads']:raise ValueError('PTE read count')
    hhdm=parsed[0,0,0];index=((hhdm['leaf']>>(7 if hhdm['shift']==12 else 12))&1)*4+((hhdm['leaf']>>4)&1)*2+((hhdm['leaf']>>3)&1) if hhdm['state']==2 else None
    return dict(status='OBSERVED_CONTEXT',source_completed=True,cpu_samples=[a,b],walks=[dict(phase=p,edge=e,alias=k,**v) for (p,e,k),v in sorted(parsed.items())],
                firmware_sha256=fw['dmar_sha256'],hypothetical_types={str(i):effective(a,i) for i in (0,2,3)},
                hhdm_alias_state='present' if index is not None else 'absent',hhdm_effective=effective(a,index) if index is not None else None,
                proposed_window_absent=True,all_aliases_proven=False,native_mapping_authorized=False,mappings=0,msr_writes=0,hardware_authority=False,ownership=False)
