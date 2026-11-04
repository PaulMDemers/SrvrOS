"""Read-only CPU0 cache observations; hypothetical leaves grant no GPU access."""
import re
SCHEMAS={
 'sample':'v sample status begin_us end_us reads count fixed signature features bits cr0 pat cap def vendor0 vendor1 vendor2',
 'variable':'v sample index base mask', 'fixed':'v sample index raw',
 'page':'v sample page physical pat_index ok mtrr pat effective',
 'summary':'v cpu_complete native_pages preserved equal queries writes mappings authority',
}
HEX=set('signature features cr0 pat cap def vendor0 vendor1 vendor2 base mask raw physical'.split())
TYPES={0,1,4,5,6}
# Independent table12-7 transcription indexed by MTRR row, PAT column.
TABLE={0:{0:0,1:1,4:0,5:0,6:0,7:0},1:{0:0,1:1,4:0,5:0,6:1,7:1},
       4:{0:0,1:1,4:4,5:5,6:4,7:0},5:{0:0,1:1,4:4,5:5,6:5,7:1},
       6:{0:0,1:1,4:4,5:5,6:6,7:0}}
def _snapshot(s,variables,fixed):
    if (s['vendor0'],s['vendor1'],s['vendor2'])!=(0x756e6547,0x49656e69,0x6c65746e):raise ValueError('non-Intel cache snapshot')
    if s['features']&0x11000!=0x11000 or not 36<=s['bits']<=52 or s['cr0']&(3<<29):raise ValueError('unsupported cache CPU')
    if s['count']!=(s['cap']&255) or s['count']>16 or s['fixed']!=(11 if s['cap']&256 else 0):raise ValueError('cache capacity mismatch')
    if s['reads']!=6+2*s['count']+s['fixed'] or not 0<s['begin_us']<s['end_us']<=s['begin_us']+100000:raise ValueError('cache read/timing mismatch')
    if s['def']&~0xcff or s['def']&255 not in TYPES or (s['def']&0x400 and not s['cap']&256):raise ValueError('cache default invalid')
    if any((s['pat']>>(8*i))&255 not in TYPES|{7} for i in range(8)):raise ValueError('invalid PAT')
    if [r['index'] for r in variables]!=list(range(s['count'])) or [r['index'] for r in fixed]!=list(range(s['fixed'])):raise ValueError('missing cache register')
    regions=[];full=(1<<s['bits'])-1;addr=full&~4095
    for r in variables:
        b,m=r['base'],r['mask']
        if not m&0x800:continue
        size=(full^(m&addr))+1;base=b&addr;t=b&255
        if b&~(addr|255) or m&~(addr|0x800) or t not in TYPES or size<4096 or size&(size-1) or base&(size-1):raise ValueError('invalid MTRR shape')
        if t==1 and not s['cap']&1024:raise ValueError('unsupported MTRR WC')
        regions.append((base,base+size,t))
    if s['def']&255==1 and not s['cap']&1024:raise ValueError('unsupported default WC')
    for r in fixed:
        for i in range(8):
            t=(r['raw']>>(8*i))&255
            if t not in TYPES or (t==1 and not s['cap']&1024):raise ValueError('invalid fixed MTRR')
    return regions

def _page(s,regions,p,index):
    if not 0<=index<8 or p<0x100000 or p&4095 or p>(1<<s['bits'])-4096:raise ValueError('invalid cache page')
    pat=(s['pat']>>(8*index))&255
    if not s['def']&0x800:return 0,pat,0
    types={t for lo,hi,t in regions if lo<=p<hi}
    if not types:m=s['def']&255
    elif 0 in types:m=0
    elif types=={4,6}:m=4
    elif len(types)==1:m=next(iter(types))
    else:return None
    return m,pat,TABLE[m][pat]

def cache_result(text):
    rows={k:[] for k in SCHEMAS}
    for line in text.splitlines():
        if not line.startswith('intel-cache-'):continue
        m=re.fullmatch(r'intel-cache-(\w+): (.+)',line)
        if not m or m[1] not in rows:raise ValueError('unknown cache receipt')
        kind=m[1];fields={}
        for token in m[2].split():
            key,sep,value=token.partition('=')
            if not sep or key in fields:raise ValueError('duplicate cache field')
            if key=='authority':fields[key]=value;continue
            is_hex=key in HEX and not (kind=='page' and key=='pat')
            if not re.fullmatch(r'0x[0-9a-fA-F]{1,16}' if is_hex else r'[0-9]{1,20}',value):raise ValueError('cache numeric encoding')
            fields[key]=int(value,16 if is_hex else 10)
            if fields[key]>=1<<64:raise ValueError('cache integer overflow')
        if set(fields)!=set(SCHEMAS[kind].split()) or fields['v']!=1:raise ValueError('cache schema mismatch')
        rows[kind].append(fields)
    if not any(rows.values()):return None
    if len(rows['summary'])!=1 or [s['sample'] for s in rows['sample']]!=[0,1]:raise ValueError('missing/duplicate cache outcome')
    summary=rows['summary'][0]
    if summary['writes'] or summary['mappings'] or summary['authority']!='none' or summary['native_pages'] not in (0,3) or any(summary[k]>1 for k in ('cpu_complete','preserved','equal','queries')):raise ValueError('cache authority/outcome')
    samples=rows['sample'];regions=[]
    for n,s in enumerate(samples):
        if s['status']>5 or s['count']>255 or s['fixed'] not in (0,11) or s['reads']>49:raise ValueError('cache sample bounds')
        variables=[r for r in rows['variable'] if r['sample']==n];fixed=[r for r in rows['fixed'] if r['sample']==n]
        if [r['index'] for r in variables]!=list(range(min(16,s['count']))) or [r['index'] for r in fixed]!=list(range(s['fixed'])):raise ValueError('cache register rows incomplete')
        regions.append(_snapshot(s,variables,fixed) if s['status']==1 else None)
    if any(r['sample'] not in (0,1) for k in ('variable','fixed','page') for r in rows[k]):raise ValueError('cache sample identity')
    same=all(s['status']==1 for s in samples)
    same=same and all(samples[0][k]==samples[1][k] for k in SCHEMAS['sample'].split() if k not in ('sample','begin_us','end_us'))
    same=same and all([{k:v for k,v in r.items() if k!='sample'} for r in rows[kind] if r['sample']==0]==[{k:v for k,v in r.items() if k!='sample'} for r in rows[kind] if r['sample']==1] for kind in ('variable','fixed'))
    complete=same and samples[1]['begin_us']>=samples[0]['end_us']
    if summary['equal']!=int(same) or summary['cpu_complete']!=int(complete):raise ValueError('cache equality/timing claim')
    expected=[(n,p,i) for n in range(2) for p in range(summary['native_pages']) for i in (0,2,3)]
    if [(r['sample'],r['page'],r['pat_index']) for r in rows['page']]!=expected:raise ValueError('cache page rows incomplete')
    queries=bool(summary['native_pages'])
    physical={}
    for r in rows['page']:
        n=r['sample'];key=r['page']
        if key in physical and physical[key]!=r['physical']:raise ValueError('changed proposed cache page')
        physical[key]=r['physical']
        decoded=_page(samples[n],regions[n],r['physical'],r['pat_index']) if regions[n] is not None else None
        if decoded is None:
            if r['ok'] or (r['mtrr'],r['pat'],r['effective'])!=(255,255,255):raise ValueError('invalid cache query success')
            queries=False
        elif r['ok']!=1 or decoded!=(r['mtrr'],r['pat'],r['effective']):raise ValueError('cache type disagreement')
    if summary['queries']!=int(queries) or (summary['preserved'] and not summary['native_pages']):raise ValueError('cache preservation/query claim')
    return dict(cpu_observation_complete=complete,query_observation_complete=bool(complete and queries and summary['preserved']),native_observation_complete=False,profile_bound=False,
                hardware_ownership=False,physical_tearfree=False,summary=summary,rows=rows)


def native_cache_result(text):
    result=cache_result(text)
    if not result or not result['summary']['native_pages']:return result
    from intel_display_capture import capture_result
    from intel_display_backing import backing_result
    display=capture_result(text);backing=backing_result(text)
    if not display or not display['observation_complete'] or not backing or not backing.get('backing_relationship_observed'):
        raise ValueError('cache native prerequisites missing')
    id=display['rows']['identity'][-1]
    fences=backing['rows']['fence']
    if len(fences)!=32 or fences[0]['raw']!=0x021ff02f00000001 or any(f['raw'] for f in fences[1:]):raise ValueError('cache full fence mismatch')
    expected=[(id['bar0']&~15)+0x101000,(id['bar0']&~15)+0x811000,(id['bar2']&~15)+0x2200000]
    if any(p['physical']!=expected[p['page']] for p in result['rows']['page']):raise ValueError('cache proposed page identity')
    if any(s['signature']!=0x306d4 or s['bits']!=39 for s in result['rows']['sample']):raise ValueError('cache native CPU identity')
    result['profile_bound']=True;result['native_observation_complete']=result['query_observation_complete']
    return result
