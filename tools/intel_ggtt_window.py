"""Bounded raw GGTT observations are never managed slots or write authority."""
import base64
import re
import struct
SCHEMAS={
 'summary':'v status window_maps retained mmio gmadr msac ggtt_bytes aperture_base aperture_bytes fence_begin fence_end first count cache_rechecks backing_preserved guard_retained pte_writes aperture_mappings ownership',
 'sample':'v sample begin_us end_us reads entries hash zero bit0 data',
}
HEX={'mmio','gmadr','aperture_base','hash'}
def _hash(values,started=True):
    if not started:return 0
    h=14695981039346656037
    for b in struct.pack('<'+'Q'*len(values),*values):h=((h^b)*1099511628211)&((1<<64)-1)
    return h

def window_result(text):
    rows={k:[] for k in SCHEMAS}
    for line in text.splitlines():
        if not line.startswith('intel-window-'):continue
        match=re.fullmatch(r'intel-window-(\w+): (.+)',line)
        if not match or match[1] not in rows:raise ValueError('unknown GGTT window receipt')
        kind=match[1];f={}
        for token in match[2].split():
            k,sep,v=token.partition('=')
            if not sep or k in f:raise ValueError('duplicate window field')
            if k in ('ownership','data'):f[k]=v;continue
            if not re.fullmatch(r'(?:0|0x[0-9a-fA-F]{1,16})' if k in HEX else r'[0-9]{1,20}',v):raise ValueError('window numeric encoding')
            f[k]=int(v,16 if k in HEX else 10)
            if f[k]>=1<<64:raise ValueError('window integer overflow')
        if set(f)!=set(SCHEMAS[kind].split()) or f['v']!=1:raise ValueError('window schema mismatch')
        rows[kind].append(f)
    if not any(rows.values()):return None
    if len(rows['summary'])!=1 or [r['sample'] for r in rows['sample']]!=[0,1]:raise ValueError('window missing/duplicate rows')
    s=rows['summary'][0]
    if s['status']>6 or s['window_maps']>2 or s['retained']>1 or s['cache_rechecks']>5 or s['backing_preserved']>1 or s['guard_retained']>1 or s['pte_writes'] or s['aperture_mappings'] or s['ownership']!='none' or (s['first'],s['count'])!=(8704,16):raise ValueError('window outcome/authority claim')
    if s['retained']!=(s['status']==6):raise ValueError('window retention mismatch')
    complete=s['status']==1
    vectors=[]
    for r in rows['sample']:
        try:data=base64.b64decode(r['data'],validate=True)
        except Exception as error:raise ValueError('window vector encoding') from error
        if len(data)!=128 or base64.b64encode(data).decode()!=r['data']:raise ValueError('window vector length/canonical form')
        values=list(struct.unpack('<16Q',data));n=r['entries'];vectors.append(values)
        if n>16 or r['reads']>48 or not 3*n<=r['reads']<=3*n+3 or any(values[n:]):raise ValueError('window vector progress')
        hash_ok=r['hash']==_hash(values[:n],bool(r['begin_us'])) or (not complete and not n and not r['reads'] and r['hash'] in (0,14695981039346656037))
        if not hash_ok or r['zero']!=sum(v==0 for v in values[:n]) or r['bit0']!=sum(v&1 for v in values[:n]):raise ValueError('window vector hash/census')
    complete=s['status']==1
    if complete:
        # Reference admits only the exact observed256MiB shape. Other legal
        # MSAC encodings are tested by the C decoder, not silently accepted here.
        b=s['gmadr']&~15
        if s['msac']>255 or s['msac']&31!=1 or s['gmadr']&0xfffffff!=12 or s['gmadr']>=1<<39 or not b or b>(1<<39)-0x10000000 or s['aperture_base']!=b or s['aperture_bytes']!=0x10000000 or s['mmio']!=0xc0000000 or s['ggtt_bytes']!=0x800000 or s['fence_begin'] or s['fence_end']!=0x2200000:raise ValueError('window aperture/fence geometry')
        if s['guard_retained'] or s['window_maps']!=2 or s['cache_rechecks']!=5 or not s['backing_preserved'] or s['retained'] or vectors[0]!=vectors[1]:raise ValueError('window incomplete success')
        for r in rows['sample']:
            if r['entries']!=16 or r['reads']!=48 or not 0<r['begin_us']<r['end_us']<=r['begin_us']+100000 or ((1<<64)-1) in vectors[r['sample']]:raise ValueError('window timing/read refusal')
        if not rows['sample'][0]['end_us']<=rows['sample'][1]['begin_us']<=rows['sample'][0]['end_us']+100000:raise ValueError('window sample ordering')
    return dict(readout_complete=complete,native_observation_complete=False,profile_bound=False,hardware_ownership=False,managed_range=False,physical_tearfree=False,summary=s,rows=rows,vectors=vectors)

def native_window_result(text):
    result=window_result(text)
    if not result or not result['readout_complete']:return result
    from intel_cache import native_cache_result
    from intel_display_capture import capture_result
    from intel_display_backing import backing_result
    cache=native_cache_result(text);display=capture_result(text);backing=backing_result(text)
    if not cache or not cache['native_observation_complete'] or not display or not display['observation_complete'] or not backing or not backing.get('backing_relationship_observed'):raise ValueError('window native prerequisites')
    if cache['rows']['sample'][1]['end_us']>result['rows']['sample'][0]['begin_us']:raise ValueError('window stale or unordered cache prerequisite')
    s=result['summary'];id=display['rows']['identity'][-1]
    if s['mmio']!=(id['bar0']&~15) or s['gmadr']!=(id['bar3']<<32|id['bar2']) or s['ggtt_bytes']!=backing['summary']['ggtt_bytes']:raise ValueError('window changed endpoint identity')
    result['profile_bound']=True;result['native_observation_complete']=True
    return result
