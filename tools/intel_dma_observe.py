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
 'admission':'v checked passed failed guards',
 'memory':'v checked valid entries conflicts shown omitted before after',
 'region':'v index relation base length type',
 'policy':'v source declared base bytes',
}
HEX={'hash','unit','other','reserved_base','reserved_end','base','version','cap','ecap','root','before','after'}
def dma_result(text):
    rows={k:[] for k in SCHEMAS}
    for line in text.splitlines():
        if not line.startswith('intel-dma-'):continue
        m=re.fullmatch(r'intel-dma-(summary|resource|sample|table|admission|memory|region|policy): (.+)',line)
        if not m:raise ValueError('malformed DMA row')
        f={}
        for item in m[2].split():
            key,sep,value=item.partition('=')
            if not sep or key in f:raise ValueError('duplicate DMA field')
            if key in ('ownership','data'):f[key]=value
            else:
                hex_field=key in HEX and not (m[1]=='memory' and key in ('before','after'))
                pattern=r'(?:0|0x[0-9a-fA-F]{1,16})' if hex_field else r'[0-9]{1,20}'
                if not re.fullmatch(pattern,value):raise ValueError('invalid DMA numeric field')
                f[key]=int(value,16 if hex_field else 10)
                if f[key]>=1<<64:raise ValueError('DMA integer overflow')
        if set(f)!=set(SCHEMAS[m[1]].split()) or f['v']!=1:raise ValueError('unsupported DMA schema')
        rows[m[1]].append(f)
    if not any(rows.values()):return None
    if len(rows['summary'])!=1 or len(rows['resource'])!=1 or len(rows['sample'])!=2 or len(rows['table'])>1:raise ValueError('missing/duplicate DMA rows')
    s=rows['summary'][0]
    if not 0<=s['status']<=8 or s['maps']>2 or s['retained']>1 or s['guard_retained']>1 or s['backing_preserved']>1 or s['translation']>2 or s['writes'] or s['dma_assured'] or s['ownership']!='none':raise ValueError('invalid DMA outcome/authority claim')
    if [r['sample'] for r in rows['sample']]!=[0,1]:raise ValueError('invalid DMA sample identities')
    out=dict(observation_complete=s['status']==8,dma_authority=False,hardware_ownership=False,physical_tearfree=False,summary=s,rows=rows)
    if len(rows['policy'])>1:raise ValueError('duplicate DMA resource policy')
    policy=rows['policy'][0] if rows['policy'] else None
    if policy and (policy['source'] not in (0,1,2) or policy['declared']>1 or policy['base']!=0xfed90000 or policy['bytes']!=4096 or policy['declared']!=int(policy['source']==2)):
        raise ValueError('invalid DMA resource policy')
    if policy:out['resource_policy']=policy
    if policy and (not rows['admission'] or not rows['memory']):raise ValueError('missing DMA policy diagnostics')
    # New diagnostics are paired and optional only for historical version1 logs.
    if rows['admission'] or rows['memory'] or rows['region']:
        if len(rows['admission'])!=1 or len(rows['memory'])!=1:raise ValueError('missing/duplicate DMA diagnostics')
        admission=rows['admission'][0];memory=rows['memory'][0];regions=rows['region']
        checked,passed,failed=(admission[k] for k in ('checked','passed','failed'))
        order=[4,8,16,1,2,32,64] if admission['guards'] else [1,2]
        prefixes=[sum(order[:i]) for i in range(1,len(order)+1)]
        if checked not in prefixes or failed not in [0]+order or passed!=(checked^failed) or (failed and (not checked&failed or failed!=order[prefixes.index(checked)])):
            raise ValueError('invalid DMA admission order/masks')
        if not failed and checked!=sum(order):raise ValueError('partial DMA admission')
        if s['status']==8 and (checked!=127 or failed or not admission['guards']):raise ValueError('unadmitted DMA observation')
        for k in ('checked','valid','before','after'):
            if memory[k]>1:raise ValueError('invalid DMA memory flags')
        if memory['valid'] and (not memory['checked'] or not 0<memory['entries']<=4096):raise ValueError('invalid DMA memory catalog')
        if memory['checked']!=int(bool(checked&32)):raise ValueError('unreached DMA memory check')
        if not memory['checked'] and any(memory[k] for k in memory if k not in ('v','checked')):raise ValueError('stale DMA memory diagnostics')
        if memory['shown']>4 or len(regions)!=memory['shown']+memory['before']+memory['after'] or memory['conflicts']>rows['resource'][0]['overlaps']:
            raise ValueError('unbounded DMA memory diagnostics')
        if memory['valid'] and (memory['shown']+memory['omitted']!=rows['resource'][0]['overlaps'] or rows['resource'][0]['overlaps']>memory['entries']):raise ValueError('inconsistent DMA memory count')
        if memory['shown'] and not memory['omitted']:
            last_region=regions[memory['shown']-1];resource=rows['resource'][0]
            if any(last_region[k]!=resource[k] for k in ('base','length','type')):raise ValueError('unbound DMA resource region')
        relations=[0]*memory['shown']+[1]*memory['before']+[2]*memory['after']
        for i,(region,relation) in enumerate(zip(regions,relations)):
            base,length=region['base'],region['length'];end=base+length
            if region['index']!=i or region['relation']!=relation or not length or end>=(1<<64):raise ValueError('invalid DMA memory region')
            if (relation==0 and not (base<0xfed91000 and 0xfed90000<end)) or (relation==1 and end>0xfed90000) or (relation==2 and base<0xfed91000):raise ValueError('invalid DMA memory relation')
        if policy and policy['source']:
            if not passed&32 or not memory['valid']:raise ValueError('unadmitted DMA resource policy')
            if policy['source']==1:
                resource=rows['resource'][0]
                if memory['conflicts'] or memory['shown']!=1 or memory['omitted'] or resource['type']!=1 or resource['overlaps']!=1 or resource['base']>0xfed90000 or resource['length']>((1<<64)-1)-resource['base'] or resource['base']+resource['length']<0xfed91000:
                    raise ValueError('unbound reserved DMA policy')
        if policy and policy['source']==2:
            expected=[(1,0xfed1c000,0x4000,1),(2,0xffd70000,0x30000,1)]
            if not memory['checked'] or not memory['valid'] or memory['conflicts'] or memory['shown'] or memory['omitted'] or memory['before']!=1 or memory['after']!=1 or rows['resource'][0]['overlaps'] or any(rows['resource'][0][k] for k in ('base','length','type')) or [(r['relation'],r['base'],r['length'],r['type']) for r in regions]!=expected:
                raise ValueError('unbound declared DMA gap')
        if s['status']==8:
            if policy and not policy['source']:raise ValueError('refused DMA resource policy')
            if not policy or policy['source']==1:
                if not memory['valid'] or memory['conflicts'] or memory['shown']!=1 or memory['omitted'] or regions[0]['type']!=1:raise ValueError('unclassified DMA observation')
        names={1:'profile',2:'DMAR table',4:'window mapped',8:'mapping retirement',16:'kernel CR3',32:'firmware resource',64:'reference clock'}
        out['admission_diagnostic']=dict(first_failed=names.get(failed),checked=checked,passed=passed,guards=admission['guards'],memory=memory,regions=regions)
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
    if not policy or policy['source']!=2:
        if resource['type']!=1 or resource['overlaps']!=1 or resource['base']>0xfed90000 or resource['length']>((1<<64)-1)-resource['base'] or resource['base']+resource['length']<0xfed91000:raise ValueError('unreserved DMA register resource')
    first,last=rows['sample']
    for sample in (first,last):
        if sample['reads']!=12 or not 0<sample['end_us']-sample['begin_us']<=100000 or not sample['version'] or not sample['cap'] or not sample['ecap'] or sample['before']!=sample['after']:raise ValueError('invalid DMA snapshot')
        if any(sample[k]>=1<<32 for k in ('version','before','after')):raise ValueError('DMA word overflow')
    if not 20000<=last['begin_us']-first['end_us']<=100000 or any(first[k]!=last[k] for k in ('version','cap','ecap','root','before','after')) or s['translation']!=last['after']>>31:raise ValueError('changed DMA state or clock')
    display=capture_result(text);backing=backing_result(text)
    if not display or not display['observation_complete'] or not backing or not backing['backing_relationship_observed']:raise ValueError('missing native backing prerequisites')
    return out
