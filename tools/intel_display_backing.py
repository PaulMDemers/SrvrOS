"""Validate bounded raw Air7,2 fence/GGTT observations; never grant ownership."""
import base64
import re
from intel_display_capture import capture_result

SCHEMAS={
 'summary':'v status maps retained count tiled fence_matches offset bytes ggtt_bytes stolen_base stolen_end writes ownership',
 'sample':'v sample begin_us end_us reads ptes present contiguous stolen hash first last',
 'endpoint':'v sample host ggc bdsm bgsm tolud gpu_ggc gpu_bdsm safe fb_pages entry_base entry_length entry_type stolen_entry_base stolen_entry_length stolen_entry_type',
 'fence':'v index raw',
 'pte':'v first count data',
}

def backing_result(text):
    rows={key:[] for key in SCHEMAS}
    for line in text.splitlines():
        if not line.startswith('intel-backing-'):continue
        match=re.fullmatch(r'intel-backing-([a-z]+): (v=.*)',line)
        if not match or match[1] not in rows:raise ValueError('invalid backing row')
        kind=match[1];fields={}
        for token in match[2].split():
            key,sep,value=token.partition('=')
            if not sep or key in fields:raise ValueError('duplicate/malformed backing field')
            if key in ('ownership','data'):fields[key]=value
            else:
                if not re.fullmatch(r'(?:0x[0-9a-fA-F]{1,16}|[0-9]{1,20})',value):raise ValueError('invalid backing number')
                fields[key]=int(value,16 if value.startswith('0x') else 10)
                if fields[key]>=1<<64:raise ValueError('backing overflow')
        if set(fields)!=set(SCHEMAS[kind].split()) or fields['v'] not in ((1,2) if kind=='pte' else (1,)):raise ValueError('invalid backing schema/version')
        rows[kind].append(fields)
    if not any(rows.values()):return None
    if len(rows['summary'])!=1 or len(rows['sample'])!=2 or len(rows['endpoint'])!=2:raise ValueError('incomplete backing report')
    summary=rows['summary'][0]
    if not 1<=summary['status']<=7 or summary['writes'] or summary['ownership']!='none' or summary['retained']>1 or summary['maps']>32 or summary['count']>1536 or summary['tiled']>1 or summary['fence_matches']>32:raise ValueError('invalid backing outcome')
    for kind in ('sample','endpoint'):
        if sorted(row['sample'] for row in rows[kind])!=[0,1]:raise ValueError('duplicate backing sample')
        rows[kind].sort(key=lambda row:row['sample'])
    complete=summary['status']==1
    result=dict(status=summary['status'],observation_complete=complete,scanout_ownership=False,physical_tearfree=False,summary=summary,rows=rows,backing_relationship_observed=False)
    if not complete:
        if rows['pte'] or rows['fence']:raise ValueError('failed backing report claims complete raw vector')
        return result
    capture=capture_result(text)
    if not capture or not capture['observation_complete'] or capture['rows']['relation'][0]['active_planes']!=1:raise ValueError('missing matched inherited capture')
    if summary['retained'] or not summary['maps'] or not summary['count']:raise ValueError('incomplete mapping retirement')
    fb=capture['rows']['framebuffer'][0];plane=capture['rows']['pipe'][3]
    if plane['pipe']!=0 or (plane['control']>>26)&15!=6 or plane['control']&((1<<15)|(1<<9)) or plane['offset'] or plane['surface']!=plane['live'] or plane['surface']&4095 or plane['stride']!=fb['pitch'] or plane['source']!=((fb['width']-1)<<16 | (fb['height']-1)):raise ValueError('unsupported inherited layout')
    tiled=int(bool(plane['control']&(1<<10)));height=(fb['height']+7)&~7 if tiled else fb['height'];span=height*fb['pitch']
    if fb['pitch']%(512 if tiled else 64) or summary['tiled']!=tiled or summary['offset']!=plane['surface'] or summary['bytes']!=span or summary['count']!=(span+4095)//4096 or summary['offset']+span>1<<28:raise ValueError('incorrect backing extent')
    ident=capture['rows']['identity'][0]
    if ident['bar3'] or ident['bar2']&0xfffffff!=12 or fb['physical']!=(ident['bar2']&~15)+summary['offset']:raise ValueError('unsupported aperture relationship')
    endpoints=rows['endpoint']
    if {k:v for k,v in endpoints[0].items() if k!='sample'}!={k:v for k,v in endpoints[1].items() if k!='sample'}:raise ValueError('changed backing endpoints')
    e=endpoints[0];gc=e['ggc']&65535;ggms=(gc>>6)&3;gms=gc>>8
    if any(v>=1<<32 for k,v in e.items() if k in 'host ggc bdsm bgsm tolud gpu_ggc gpu_bdsm'.split()) or e['host']!=0x16048086 or e['safe']!=1 or not ggms or not 1<=gms<=16 or e['gpu_ggc']&65535!=gc or e['gpu_bdsm']&0xfff00000!=e['bdsm']&0xfff00000:raise ValueError('invalid backing config')
    base=e['bdsm']&0xfff00000;end=base+(gms<<25);table=e['bgsm']&0xfff00000;size=(1<<ggms)<<20;tolud=e['tolud']&0xfff00000
    if not base or not table or max(end,table+size)>tolud or (base<table+size and table<end) or summary['ggtt_bytes']!=size or summary['stolen_base']!=base or summary['stolen_end']!=end or (summary['offset']//4096+summary['count'])*8>size:raise ValueError('invalid stolen/table bounds')
    if e['fb_pages']!=(fb['pitch']*fb['height']+4095)//4096 or e['entry_type'] not in (1,7) or e['entry_base']>fb['physical'] or e['entry_base']+e['entry_length']>1<<64 or fb['physical']+fb['pitch']*fb['height']>e['entry_base']+e['entry_length']:raise ValueError('invalid framebuffer translation/map coverage')
    versions={chunk['v'] for chunk in rows['pte']}
    if len(versions)!=1:raise ValueError('mixed backing PTE versions')
    wire=next(iter(versions));chunk_size=64 if wire==2 else 16
    if wire==2:
        wake=re.findall(r'^intel-wake-summary: v=([0-9]+) ',text,re.M)
        boot=re.findall(r'^laptop-capture: boot options=(.*)$',text,re.M)
        if not (wake==['19'] or not wake and len(boot)==1 and boot[0].split().count('srvros.intel-rehold=request-cancel')==1):raise ValueError('packed PTE image binding')
    ptes=[]
    for chunk in rows['pte']:
        if chunk['first']!=len(ptes) or chunk['count']!=min(chunk_size,summary['count']-len(ptes)) or chunk['count']<=0:raise ValueError('incomplete/duplicate PTE chunk')
        try:data=base64.b64decode(chunk['data'],validate=True)
        except (ValueError,base64.binascii.Error) as exc:raise ValueError('invalid PTE encoding') from exc
        if len(data)!=8*chunk['count'] or base64.b64encode(data).decode()!=chunk['data']:raise ValueError('noncanonical PTE chunk')
        ptes.extend(int.from_bytes(data[i:i+8],'little') for i in range(0,len(data),8))
    if len(ptes)!=summary['count']:raise ValueError('incomplete raw PTE vector')
    digest=14695981039346656037
    for pte in ptes:
        for byte in pte.to_bytes(8,'little'):digest=((digest^byte)*1099511628211)&((1<<64)-1)
    physical=[pte&0x7ffffff000 for pte in ptes]
    present=sum(bool(pte&1) for pte in ptes);stolen=sum(bool(pte&1) and base<=pa<end for pte,pa in zip(ptes,physical))
    contiguous=int(all(pa==physical[0]+4096*i for i,pa in enumerate(physical)))
    for n,s in enumerate(rows['sample']):
        if not 0<s['end_us']-s['begin_us']<=100000 or s['reads']!=3*len(ptes)+212 or s['ptes']!=len(ptes) or s['hash']!=digest or s['first']!=ptes[0] or s['last']!=ptes[-1] or s['present']!=present or s['stolen']!=stolen or s['contiguous']!=contiguous:raise ValueError('invalid PTE sample/timing/hash')
        if n and not 0<=s['begin_us']-rows['sample'][0]['end_us']<=100000:raise ValueError('invalid backing sample ordering')
    if sorted(row['index'] for row in rows['fence'])!=list(range(32)):raise ValueError('incomplete/duplicate raw fences')
    matches=[];overlaps=[]
    for fence in rows['fence']:
        raw=fence['raw'];start=raw&0xfffff000;finish=((raw>>32)&0xfffff000)+4096;stride=(((raw>>32)&0x7ff)+1)*128
        if raw&1 and start<summary['offset']+span and summary['offset']<finish:overlaps.append(fence['index'])
        if raw&1 and not raw&2 and start==summary['offset'] and finish>=summary['offset']+span and stride==fb['pitch']:matches.append(fence['index'])
    if summary['fence_matches']!=len(matches):raise ValueError('invalid fence relationship')
    supported=all(pte<1<<39 for pte in ptes)
    stolen_reserved=(e['stolen_entry_type']==1 and e['stolen_entry_length']>0 and e['stolen_entry_base']<=base and
                     end<=e['stolen_entry_base']+e['stolen_entry_length']<=1<<64)
    relationship=(len(matches)==1 and overlaps==matches) if tiled else not overlaps
    result.update(ptes=ptes,pte_address_bits_supported=supported,matching_fences=matches,overlapping_fences=overlaps,
                  layout='x-tiled' if tiled else 'linear',stolen_firmware_reserved=stolen_reserved,backing_relationship_observed=bool(stolen_reserved and supported and relationship and present==len(ptes) and stolen==len(ptes)))
    return result
