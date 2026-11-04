"""Exact Air7,2 read-only VT-d receipts. Never DMA or display authority."""
import base64
import re
import struct
from intel_display_capture import capture_result
from intel_display_backing import backing_result
DMAR_HASH=0x722775ad66626863
SCHEMAS={
 'summary':'v status maps retained hash unit other reserved_base reserved_end translation backing_preserved guard_retained writes dma_assured ownership',
 'resource':'v base length type overlaps',
 'sample':'v sample begin_us end_us version cap ecap root before after reads',
 'table':'v length data',
}
HEX={'hash','unit','other','reserved_base','reserved_end','base','version','cap','ecap','root','before','after'}
def dma_result(text):
    rows={k:[] for k in SCHEMAS}
    for line in text.splitlines():
        if not line.startswith('intel-dma-'):continue
        m=re.fullmatch(r'intel-dma-(summary|resource|sample|table): (.+)',line)
        if not m:raise ValueError('malformed DMA row')
        f={}
        for item in m[2].split():
            key,sep,value=item.partition('=')
            if not sep or key in f:raise ValueError('duplicate DMA field')
            if key in ('ownership','data'):f[key]=value
            else:
                pattern=r'(?:0|0x[0-9a-fA-F]{1,16})' if key in HEX else r'[0-9]{1,20}'
                if not re.fullmatch(pattern,value):raise ValueError('invalid DMA numeric field')
                f[key]=int(value,16 if key in HEX else 10)
                if f[key]>=1<<64:raise ValueError('DMA integer overflow')
        if set(f)!=set(SCHEMAS[m[1]].split()) or f['v']!=1:raise ValueError('unsupported DMA schema')
        rows[m[1]].append(f)
    if not any(rows.values()):return None
    if len(rows['summary'])!=1 or len(rows['resource'])!=1 or len(rows['sample'])!=2 or len(rows['table'])>1:raise ValueError('missing/duplicate DMA rows')
    s=rows['summary'][0]
    if not 0<=s['status']<=8 or s['maps']>2 or s['retained']>1 or s['guard_retained']>1 or s['backing_preserved']>1 or s['translation']>2 or s['writes'] or s['dma_assured'] or s['ownership']!='none':raise ValueError('invalid DMA outcome/authority claim')
    if [r['sample'] for r in rows['sample']]!=[0,1]:raise ValueError('invalid DMA sample identities')
    out=dict(observation_complete=s['status']==8,dma_authority=False,hardware_ownership=False,physical_tearfree=False,summary=s,rows=rows)
    if not out['observation_complete']:return out
    if s['maps']!=2 or s['retained'] or s['guard_retained'] or not s['backing_preserved'] or len(rows['table'])!=1:raise ValueError('incomplete DMA retirement/preservation')
    table=rows['table'][0]
    try:b=base64.b64decode(table['data'],validate=True)
    except Exception as e:raise ValueError('invalid DMAR encoding') from e
    if len(b)!=136 or table['length']!=136 or base64.b64encode(b).decode()!=table['data'] or b[:4]!=b'DMAR' or struct.unpack_from('<I',b,4)[0]!=136 or b[8]!=1 or sum(b)%256 or b[36:48]!=bytes([38,3])+bytes(10):raise ValueError('invalid DMAR header/checksum')
    a=struct.pack('<HHBBHQ',0,24,0,0,0,0xfed90000)+bytes([1,8,0,0,0,0,2,0])
    c=struct.pack('<HHHHQQ',1,32,0,0,0x8b800000,0x8fffffff)+bytes([1,8,0,0,0,0,2,0])
    middle=struct.pack('<HHBBHQ',0,32,1,0,0,0xfed91000)+bytes([3,8,0,0,2,240,31,0,4,8,0,0,0,240,15,0])
    if b[48:]!=a+middle+c:raise ValueError('unsupported DMAR scopes/resources')
    h=14695981039346656037
    for x in b:h=((h^x)*1099511628211)&((1<<64)-1)
    if h!=DMAR_HASH or s['hash']!=h or (s['unit'],s['other'],s['reserved_base'],s['reserved_end'])!=(0xfed90000,0xfed91000,0x8b800000,0x90000000):raise ValueError('unbound DMAR identity')
    resource=rows['resource'][0]
    if resource['type']!=1 or resource['overlaps']!=1 or resource['base']>0xfed90000 or resource['length']>((1<<64)-1)-resource['base'] or resource['base']+resource['length']<0xfed91000:raise ValueError('unreserved DMA register resource')
    first,last=rows['sample']
    for sample in (first,last):
        if sample['reads']!=12 or not 0<sample['end_us']-sample['begin_us']<=100000 or not sample['version'] or not sample['cap'] or not sample['ecap'] or sample['before']!=sample['after']:raise ValueError('invalid DMA snapshot')
        if any(sample[k]>=1<<32 for k in ('version','before','after')):raise ValueError('DMA word overflow')
    if not 20000<=last['begin_us']-first['end_us']<=100000 or any(first[k]!=last[k] for k in ('version','cap','ecap','root','before','after')) or s['translation']!=last['after']>>31:raise ValueError('changed DMA state or clock')
    display=capture_result(text);backing=backing_result(text)
    if not display or not display['observation_complete'] or not backing or not backing['backing_relationship_observed']:raise ValueError('missing native backing prerequisites')
    return out
