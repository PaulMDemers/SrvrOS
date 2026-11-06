"""Read-only GT requester observation; never clock/engine/firmware authority."""
import re
FIELDS={
 'summary':'v status maps retained acquired before after released guards cache_checks guard_retained mmio asls swsci complete forcewake_writes engine_reads opregion_maps pte_writes ownership'.split(),
 'sample':'v sample begin_us end_us requests ack reads'.split(),
}
HEX={'mmio','asls','swsci','requests','ack'}
def gt_result(text):
    rows={k:[] for k in FIELDS}
    for line in text.splitlines():
        if not line.startswith('intel-gt-'):continue
        m=re.fullmatch(r'intel-gt-(\w+): (.+)',line)
        if not m or m[1] not in rows:raise ValueError('unknown GT receipt')
        kind=m[1];row={}
        for token in m[2].split():
            k,sep,v=token.partition('=')
            if not sep or k in row:raise ValueError('GT duplicate/invalid field')
            if k=='ownership':row[k]=v;continue
            if not re.fullmatch(r'(?:0|0x[0-9a-fA-F]{1,16})' if k in HEX else r'[0-9]{1,20}',v):raise ValueError('GT numeric encoding')
            row[k]=int(v,16 if k in HEX else 10)
            if row[k]>=1<<64:raise ValueError('GT overflow')
        if set(row)!=set(FIELDS[kind]) or row['v']!=1:raise ValueError('GT schema')
        rows[kind].append(row)
    if not any(rows.values()):return None
    if len(rows['summary'])!=1 or [r['sample'] for r in rows['sample']]!=[0,1]:raise ValueError('GT missing/duplicate rows')
    s=rows['summary'][0];samples=rows['sample']
    if s['status']>6 or s['maps']>4 or s['guards']>5 or s['cache_checks']>4:raise ValueError('GT bounds')
    if any(s[k]>1 for k in ('retained','acquired','before','after','released','guard_retained','complete')):raise ValueError('GT boolean')
    if s['retained']!=int(s['status']==6) or s['retained'] and s['guard_retained']:raise ValueError('GT retention')
    if s['forcewake_writes'] or s['engine_reads'] or s['opregion_maps'] or s['pte_writes'] or s['ownership']!='none':raise ValueError('GT unsupported authority')
    if s['asls']>=1<<32 or s['swsci']>=1<<32:raise ValueError('GT PCI width')
    if max(s['before'],s['after'],s['released'])>s['acquired'] or ((s['retained'] or s['guard_retained']) and s['released']):raise ValueError('GT exclusion ordering')
    for r in samples:
        if r['reads']>2 or r['requests']>=1<<32 or r['ack']>=1<<32:raise ValueError('GT sample width/bounds')
    observed=s['status']==1
    if observed:
        if s['maps']!=4 or s['retained'] or s['mmio']!=0xc0000000 or s['guards']!=5 or s['cache_checks']!=4 or not s['before'] or not s['acquired']:raise ValueError('GT incomplete readout')
        if s['asls']==0xffffffff or s['swsci']==0xffffffff:raise ValueError('GT floating PCI')
        for r in samples:
            if r['reads']!=2 or not 0<r['begin_us']<r['end_us']<=r['begin_us']+100000 or 0xffffffff in (r['requests'],r['ack']):raise ValueError('GT timing/reads')
        if not samples[0]['end_us']<=samples[1]['begin_us']<=samples[0]['end_us']+100000:raise ValueError('GT unordered samples')
        if (samples[0]['requests'],samples[0]['ack'])!=(samples[1]['requests'],samples[1]['ack']):raise ValueError('GT changed samples')
    complete=observed and s['before'] and s['after'] and s['released'] and not s['guard_retained']
    if s['complete']!=int(bool(complete)):raise ValueError('GT completion mismatch')
    return dict(readout_complete=observed,qualification_complete=bool(complete),native_observation_complete=False,hardware_ownership=False,forcewake_permission=False,engine_retirement=False,managed_range=False,physical_tearfree=False,summary=s,rows=rows)

def native_gt_result(text):
    r=gt_result(text)
    if not r or not r['qualification_complete']:return r
    from intel_handoff import native_handoff_result
    from intel_ggtt_window import native_window_result
    h=native_handoff_result(text);w=native_window_result(text)
    if not h or not h['native_qualification_complete'] or not w or not w['native_observation_complete']:raise ValueError('GT native prerequisites')
    if r['summary']['mmio']!=w['summary']['mmio'] or w['rows']['sample'][1]['end_us']>r['rows']['sample'][0]['begin_us']:raise ValueError('GT stale/changed native endpoint')
    from intel_cache import native_cache_result,_snapshot,_page
    cache=native_cache_result(text)
    if not cache or not cache['native_observation_complete']:raise ValueError('GT native cache prerequisite')
    for n,s in enumerate(cache['rows']['sample']):
        variables=[v for v in cache['rows']['variable'] if v['sample']==n]
        fixed=[v for v in cache['rows']['fixed'] if v['sample']==n]
        regions=_snapshot(s,variables,fixed)
        for offset in (0xa000,0x130000):
            decoded=_page(s,regions,r['summary']['mmio']+offset,2)
            if decoded is None or decoded[0]!=0 or decoded[2]!=0:raise ValueError('GT pages require matching UC MTRR and effective type')
    r['native_observation_complete']=True
    return r
