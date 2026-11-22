"""Read-only full-vector layout receipts. A fitting range is never free memory."""
import re

SCHEMAS={
    'summary':'v complete phase reason acquired released retained maps retired cache_checks alias_checks meta_reads classified checked clock previous writes ownership',
    'facts':'v sample begin end host gpu subsystem ggc bdsm bgsm tolud reserved width height stride aperture ggtt fence_begin fence_end fbc psr_ctl psr_status',
    'run':'v sample range gpu bytes pages count reads first last hash bad_index bad_value',
    'reader':'v sample pipe count '+ ' '.join('w'+str(i) for i in range(13)),
    'policy':'v sample range index physical pages queried ok mtrr pat effective checks',
    'range':'v epoch slot pages checked fences closing_readers hash verified missing assurances obligations writes',
    'hardware-gate':'v begin end firmware_transfer first_unmet writes',
    'hardware-config':'v sample count '+' '.join(f'w{i}' for i in range(11)),
    'hardware-readers':'v count '+' '.join(f'w{i}' for i in range(42)),
    'guard':'v attempted begin end pinned acquired checked closed unpinned reason writes authority',
    'guard-walk':'v phase tables entries leaves',
    'cpu-span':'v range physical pages checked reserved framebuffer other uncovered tagged',
    'cpu':'v attempted observed admitted begin end ram_ranges bits pmm_checks unmapped reason level root_phase root_reason root_detail tables entries leaves span virtual physical bytes entry writes authority',
}
WIRE2='fail offset cache_index mtrr pat effective'
FAILURES=('none','recipe','timer-reference','profile','source-snapshot','root','PCI-devices',
          'console-sync','display-exclusion','identity','cache-context','source-revalidation',
          'PCI-bounds','MMIO-resource','cache-type','mapping','actual-alias','MMIO-read',
          'clock','layout-plan','torn-PTE','layout-classification','unmap','exclusion-release',
          'changed-facts','source-window','allowed-range','progress-save','invalid-reader-word','CPU-backing-guard','CPU-mapping-exclusion','hardware-observation','scanout-range')
HEX=set('host gpu subsystem ggc bdsm bgsm tolud reserved fbc psr_ctl psr_status first last hash bad_value'.split())
HEX.update(['virtual','entry','physical']+['w'+str(i) for i in range(42)])
FIXED=('srvros.storage=ram','srvros.capture=intel-display','srvros.intel-backing=read-only',
       'srvros.intel-cache=read-only','srvros.intel-window=read-only','srvros.intel-stolen=read-only','srvros.capture-failure=reboot')

PREINIT_LINE='init: deferred=stolen-preinit; terminal automatic capture, no user services'
def verify_stolen_preinit(text):
    """Exact isolated boot witness, not root/alias or GPU ownership authority."""
    lines=text.splitlines()
    if lines.count(PREINIT_LINE)!=1:raise ValueError('missing/duplicate stolen preinit witness')
    if any(re.match(r'init: (?:started\b|readiness=|startup complete\b|start failed\b)',line) for line in lines):
        raise ValueError('stolen census started user init')
    edge=lines.index(PREINIT_LINE)
    if any(line.startswith(('intel-stolen:','intel-stolen-summary:')) for line in lines[:edge]):
        raise ValueError('late stolen preinit witness')
    return True

def stolen_recipe(command):
    tokens=command.split()
    for token in ('srvros.intel-scanout=range-observation','srvros.intel-scanout=private-range'):
        if token in tokens:
            tokens.remove(token)
            break
    if len(tokens)!=9 or len(set(tokens))!=9 or not set(FIXED)<=set(tokens):
        raise ValueError('exact stolen census recipe required')
    fields={}
    for name,size in (('workspace-volume',8),('desktop-release',16)):
        matches=[t for t in tokens if re.fullmatch(r'srvros\.'+name+r'=[0-9a-f]{'+str(size)+'}',t)]
        if len(matches)!=1:raise ValueError('stolen recipe '+name)
        fields[name]=matches[0].split('=',1)[1]
    return dict(volume=fields['workspace-volume'],release=fields['desktop-release'])

def _hash(first,pages):
    h=14695981039346656037
    for i in range(pages):
        word=first+4096*i
        for b in word.to_bytes(8,'little'):h=((h^b)*1099511628211)&((1<<64)-1)
    return h

def verify_owner_progress(rows,summary):
    if [(r['sample'],r['pipe']) for r in rows['reader']]!=[(n,p) for n in range(2) for p in range(3)]:
        raise ValueError('missing/duplicate/unordered reader preflight')
    if [(r['sample'],r['range'],r['index']) for r in rows['policy']]!=[(n,r,i) for n in range(2) for r in (1,2) for i in range(8)]:
        raise ValueError('missing/duplicate/unordered candidate policy')
    reads=0;partial=False
    for r in rows['reader']:
        if r['count']>13 or (partial and r['count']):raise ValueError('reader prefix')
        partial |= r['count']<13;reads+=r['count']
        for w in range(13):
            if r['w'+str(w)]>=0xffffffff or (w>=r['count'] and r['w'+str(w)]):raise ValueError('reader word')
    # A refused all-ones word was loaded but is not admitted into the prefix.
    if summary['reader_reads']>78 or summary['reader_reads']!=reads+(summary['fail']==28):raise ValueError('reader load count')
    if summary['complete'] and reads!=78:raise ValueError('incomplete reader preflight')
    partial=False
    for q in rows['policy']:
        if q['queried'] not in (0,1) or q['ok'] not in (0,1) or (partial and q['queried']):raise ValueError('policy prefix')
        partial |= not q['queried']
        if not q['queried']:
            if any(q[k] for k in ('physical','pages','ok','mtrr','pat','effective','checks')):raise ValueError('unqueried policy')
        elif not q['ok']:
            if (q['mtrr'],q['pat'],q['effective'],q['checks'])!=(255,255,255,0):raise ValueError('unknown policy')
        elif any(q[k] not in (0,1,4,5,6,7,8,9) for k in ('mtrr','pat','effective')) or not 1<=q['checks']<=33:
            raise ValueError('policy type')
        if q['queried']:
            vectors=rows['run'][q['sample']*3:q['sample']*3+3];v=vectors[q['range']]
            if any(not v['pages'] or v['count']!=v['pages'] for v in vectors) or any(r['count']!=13 for r in rows['reader'][q['sample']*3:q['sample']*3+3]):raise ValueError('policy before complete census')
            if (q['physical'],q['pages'])!=(v['first']&~4095,v['pages']):raise ValueError('policy progress geometry')
    if summary['complete'] and partial:raise ValueError('incomplete candidate policy')

def owner_preflight(rows,physical):
    for q in rows['policy']:
        lo,hi=physical[q['range']]
        if q['physical']!=lo or q['pages']!=(hi-lo)//4096:raise ValueError('candidate policy geometry')
    stable=all(all(rows['reader'][p][k]==rows['reader'][3+p][k] for k in ['count']+['w'+str(w) for w in range(13)]) for p in range(3))
    active=[];mismatch=[];base_hits=[]
    # Only starting addresses are classified. No inferred length/tile footprint.
    gpu_spans=[(rows['run'][r]['gpu'],rows['run'][r]['gpu']+rows['run'][r]['bytes']) for r in (1,2)]
    for r in rows['reader']:
        active.append(dict(sample=r['sample'],pipe=r['pipe'],primary=bool(r['w0']&0x80000000),cursor=bool(r['w5']&63),sprite=bool(r['w8']&0x80000000)))
        for a,b in ((1,2),(3,4),(6,7),(9,10),(11,12)):
            if (r['w'+str(a)]^r['w'+str(b)])&0xfffff000:mismatch.append([r['sample'],r['pipe'],a,b])
        for w in (1,2,3,4,6,7,9,10,11,12):
            base=r['w'+str(w)]&0xfffff000
            for i,(lo,hi) in enumerate(gpu_spans,1):
                if lo<=base<hi:base_hits.append([r['sample'],r['pipe'],w,i])
    return dict(stable=stable,active=active,address_mismatches=mismatch,candidate_base_hits=base_hits,
                footprint_complete=False,maintained_retirement=False,hardware_ownership=False)

def verify_candidate_policies(rows,sample,regions):
    from intel_cache import _page
    for q in rows['policy']:
        if q['sample']!=sample['sample']:continue
        values=[_page(sample,regions,q['physical']+4096*i,q['index']) for i in range(q['pages'])]
        uniform=bool(values and values[0] is not None and all(v==values[0] for v in values))
        if q['ok']!=uniform:raise ValueError('candidate cache uniformity')
        if uniform:
            checks=1+sum(q['physical']<b<q['physical']+4096*q['pages'] for lo,hi,_ in regions for b in (lo,hi)) if sample['def']&0x800 else 1
            if (q['mtrr'],q['pat'],q['effective'])!=values[0] or q['checks']!=checks:raise ValueError('candidate cache replay')

def verify_cpu_progress(rows,summary):
    if len(rows['cpu'])!=1 or [r['range'] for r in rows['cpu-span']]!=[1,2]:raise ValueError('missing/duplicate/unordered CPU backing rows')
    c=rows['cpu'][0];spans=rows['cpu-span']
    if any(c[k] not in (0,1) for k in ('attempted','observed','admitted','unmapped')) or c['writes'] or c['authority']!='none':raise ValueError('CPU backing authority')
    if c['reason']>12 or c['level']>4 or c['span']>1 or c['ram_ranges']>256 or c['tables']>256 or c['entries']>512*c['tables'] or c['leaves']>c['entries']:
        raise ValueError('CPU root bounds')
    if c['root_phase'] not in (0,1,3) or c['root_reason']>8 or (not c['root_phase'] and (c['root_reason'] or c['root_detail'])):raise ValueError('CPU root refusal')
    if c['root_phase']:
        reason,detail=c['root_reason'],c['root_detail']
        if c['reason']!=2 or not reason or ((reason==1 and detail!=512) or (reason in (2,3,5,6) and detail) or
            (reason==4 and detail&0x000ffffffffff000) or (reason==7 and not 1<=detail<=64) or (reason==8 and not detail)):
            raise ValueError('CPU original root detail')
    if c['attempted'] and (summary['checked']!=23552 or summary['reader_reads']!=78 or summary['meta_reads']!=8 or
            any(not v['pages'] or v['count']!=v['pages'] for v in rows['run'])):raise ValueError('CPU backing before layout census')
    checked=0;partial=False
    for q in spans:
        if q['pages'] not in (0,1536) or q['checked']>q['pages'] or q['tagged']>q['checked'] or q['physical']&4095:raise ValueError('CPU backing progress')
        if sum(q[k] for k in ('reserved','framebuffer','other','uncovered'))!=q['checked']:raise ValueError('CPU coverage count')
        if partial and q['checked']:raise ValueError('CPU backing prefix')
        partial |= not q['pages'] or q['checked']<q['pages'];checked+=q['checked']
        if not q['pages'] and q['physical']:raise ValueError('unobserved CPU physical span')
        if q['pages']:
            v=rows['run'][q['range']]
            if (q['physical'],q['pages'])!=(v['first']&~4095,v['pages']):raise ValueError('CPU backing original span')
    if c['pmm_checks']!=checked:raise ValueError('CPU PMM query count')
    if not c['attempted']:
        if any(c[k] for k in c if k not in ('v','authority')) or any(any(q[k] for k in q if k not in ('v','range')) for q in spans):raise ValueError('unattempted CPU backing')
    if c['attempted'] and not 36<=c['bits']<=52:raise ValueError('CPU backing physical width')
    if c['admitted'] and not c['observed']:raise ValueError('CPU backing before observation')
    if summary['complete'] and not c['observed']:raise ValueError('missing CPU backing completion')
    if not c['observed']:return
    if not c['attempted'] or partial or checked!=3072 or not c['ram_ranges'] or not c['tables']:raise ValueError('CPU backing incomplete observation')
    final=rows['facts'][1]
    if not final['begin']<=c['begin']<c['end']<=final['end']:raise ValueError('CPU backing clock')
    if c['root_phase'] or c['root_reason'] or c['root_detail']:raise ValueError('CPU backing root context')
    if c['unmapped']:
        if c['reason'] or c['level'] or any(c[k] for k in ('span','virtual','physical','bytes','entry')) or c['entries']!=512*c['tables']:raise ValueError('CPU unmapped result')
    else:
        if c['reason']!=12 or c['level'] not in (2,3,4) or c['leaves']<1:raise ValueError('CPU present-leaf refusal')
        size={2:1<<30,3:1<<21,4:4096}[c['level']]
        if c['bytes']!=size or c['physical']&(size-1) or c['virtual']&(size-1) or c['virtual']>>47 not in (0,0x1ffff) or c['virtual']>((1<<64)-size):raise ValueError('CPU leaf geometry')
        raw=c['entry'];mask=((1<<c['bits'])-1)&~4095;physical=(raw&mask)&~(size-1)
        if raw&~(mask|4095|(1<<63)) or (c['level']<4 and raw&((size-1)&~0x1fff)):raise ValueError('CPU leaf reserved bits')
        if not raw&1 or physical!=c['physical'] or (c['level']<4 and not raw&128):raise ValueError('CPU leaf encoding')
        q=spans[c['span']]
        if not c['physical']<q['physical']+q['pages']*4096 or not q['physical']<c['physical']+size:raise ValueError('CPU leaf does not intersect candidate')
    reserved=all(q['reserved']==q['pages'] and q['tagged']==q['pages'] for q in spans)
    if c['admitted']!=bool(reserved and c['unmapped']):raise ValueError('CPU backing admission derivation')

def cpu_admission(rows,physical):
    c=rows['cpu'][0]
    for q in rows['cpu-span']:
        lo,hi=physical[q['range']]
        if (q['physical'],q['pages'])!=(lo,(hi-lo)//4096):raise ValueError('CPU backing complete geometry')
    return dict(observed=bool(c['observed']),os_backing_admitted=bool(c['admitted']),cpu_unmapped=bool(c['unmapped']),
                conflict=None if c['unmapped'] else {k:c[k] for k in ('span','virtual','physical','bytes','entry','level')},
                hardware_ownership=False,physical_tearfree=False)

def verify_guard_progress(rows,s):
    if len(rows['guard'])!=1 or [r['phase'] for r in rows['guard-walk']]!=[0,1,2]:raise ValueError('CPU guard rows')
    g=rows['guard'][0]
    flags=('attempted','pinned','acquired','checked','closed','unpinned')
    if any(g[k] not in (0,1) for k in flags) or g['reason']>15 or g['writes'] or g['authority']!='none':raise ValueError('CPU guard authority')
    if len(rows['cpu'])!=1:raise ValueError('CPU guard missing CPU prerequisite')
    c=rows['cpu'][0]
    if not g['attempted']:
        if any(g[k] for k in g if k not in ('v','authority')) or any(any(r[k] for k in ('tables','entries','leaves')) for r in rows['guard-walk']):raise ValueError('unattempted CPU guard')
        if s['complete'] and c['admitted']:raise ValueError('missing CPU guard')
        return
    if not c['observed'] or not c['admitted']:raise ValueError('CPU guard before admission')
    if g['pinned'] and (not g['begin'] or g['begin']<c['end']):raise ValueError('CPU guard pin clock')
    for child,parent in (('pinned','attempted'),('acquired','pinned'),('checked','acquired'),('closed','checked'),('unpinned','pinned')):
        if g[child] and not g[parent]:raise ValueError('CPU guard causal prefix')
    if g['unpinned'] and g['acquired'] and not g['closed']:raise ValueError('CPU guard early unpin')
    if (g['acquired'] and not g['closed']) or (g['pinned'] and not g['unpinned']):
        if not s['retained'] or s['released']:raise ValueError('CPU guard lost retained exclusion')
    for r,done in zip(rows['guard-walk'],('acquired','checked','closed')):
        if r['tables']>256 or r['entries']!=512*r['tables'] or r['leaves']>r['entries']:raise ValueError('CPU guard walk bounds')
        if bool(r['tables'])!=bool(g[done]):raise ValueError('CPU guard walk prefix')
    if g['end'] and (not all(g[k] for k in flags) or not c['end']<=g['begin']<g['end']<=rows['facts'][1]['end']):raise ValueError('CPU guard final clock')
    if s['complete'] and (not all(g[k] for k in flags) or not g['end'] or g['reason']):raise ValueError('CPU guard incomplete')
    if g['reason'] and (s['complete'] or not s['fail']):raise ValueError('CPU guard missing refusal')

def verify_hardware_gate(rows, summary):
    from intel_held_requester import _prefix as requester_prefix
    if (len(rows['hardware-gate'])!=1 or len(rows['hardware-readers'])!=1 or
        [item['sample'] for item in rows['hardware-config']]!=[0,1]):
        raise ValueError('held hardware row order')
    gate=rows['hardware-gate'][0]
    guard=rows['guard'][0]
    c=rows['cpu'][0]
    readers=rows['hardware-readers'][0]
    counts=[sample['count'] for sample in rows['hardware-config']]
    def words(row, size):
        full=[row[f'w{i}'] for i in range(size)]
        count=row['count']
        if count>size or any(word>=0xffffffff for word in full) or any(full[count:]):
            raise ValueError('held hardware word prefix')
        return full[:count]
    start, finish=(words(sample,11) for sample in rows['hardware-config'])
    display=words(readers,42)
    if ((readers['count'] and counts[0]!=11) or (counts[1] and readers['count']!=42)
        or (any(counts) and not gate['begin'])):
        raise ValueError('held hardware causal prefix')
    for sample in (start,finish):
        for n in range(1,min(9,len(sample))+1):
            if not requester_prefix(sample[:n]) and (n!=len(sample) or summary['complete'] or summary['fail']!=31):
                raise ValueError('held requester capability chain')
        if len(sample)==11 and sample[10]>0xffff:
            raise ValueError('held firmware register width')
    if gate['begin'] and (not guard['checked'] or gate['begin']<guard['begin']):
        raise ValueError('held hardware outside CPU guard')
    if gate['end'] and (not gate['end']>gate['begin']>0 or
        counts!=[11,11] or readers['count']!=42 or
        (guard['end'] and gate['end']>guard['end'])):
        raise ValueError('held hardware incomplete observation')
    if summary['complete'] and bool(c['admitted'])!=bool(gate['end']):
        raise ValueError('missing held hardware observation')
    expected='firmware-transfer' if gate['end'] else 'unresolved'
    if not gate['end'] and c['observed'] and not c['admitted']:expected='cpu-backing'
    if gate['firmware_transfer'] or gate['writes'] or gate['first_unmet']!=expected:
        raise ValueError('held hardware authority')
    result=dict(observation_complete=bool(gate['end']),first_unmet=expected,firmware_transfer=False)
    if gate['end']:
        old=rows['facts'][1]
        prior=[old['fbc'],old['psr_ctl'],old['psr_status']]
        for pipe in rows['reader'][3:]:
            prior.extend(pipe[f'w{i}'] for i in range(13))
        result.update(firmware_endpoints=[dict(asls=s[9],swsci=s[10]) for s in (start,finish)],
                      firmware_endpoint_stable=start[9:]==finish[9:],
                      requester_stable=start[:9]==finish[:9],
                      bus_master=[bool(s[1]&4) for s in (start,finish)],
                      transaction_pending=[bool(s[8]&256) for s in (start,finish)],
                      display_sample_matches=display==prior)
    return result


def stolen_result(text,*,native=False,required_version=None):
    rows={k:[] for k in SCHEMAS}
    for line in text.splitlines():
        if not line.startswith(('intel-stolen-summary:','intel-stolen-range:','intel-stolen-facts:','intel-stolen-run:','intel-stolen-reader:','intel-stolen-policy:','intel-stolen-cpu:','intel-stolen-cpu-span:','intel-stolen-guard:','intel-stolen-guard-walk:','intel-stolen-hardware-gate:','intel-stolen-hardware-config:','intel-stolen-hardware-readers:')):
            if re.match(r'intel-stolen-[^ :]+:',line):raise ValueError('unknown stolen receipt')
            continue
        match=re.fullmatch(r'intel-stolen-(summary|range|facts|run|reader|policy|cpu-span|cpu|guard-walk|guard|hardware-gate|hardware-config|hardware-readers): (.+)',line)
        if not match:raise ValueError('malformed stolen receipt')
        kind=match[1];row={}
        for token in match[2].split():
            k,sep,value=token.partition('=')
            if not sep or k in row:raise ValueError('duplicate stolen field')
            if k in ('ownership','authority','first_unmet'):row[k]=value;continue
            hexadecimal=(k in HEX or k=='aperture_page') and not ((kind=='run' and k=='gpu') or (kind=='cpu-span' and k=='reserved'))
            if not re.fullmatch(r'(?:0|0x[0-9a-fA-F]{1,16})' if hexadecimal else r'[0-9]{1,20}',value):
                raise ValueError('stolen numeric encoding')
            row[k]=int(value,16 if hexadecimal else 10)
            if row[k]>=1<<64:raise ValueError('stolen integer overflow')
        version=row.get('v');extra=WIRE2 if kind=='summary' and version in range(2,14) else ''
        if kind=='summary' and version in range(3,14):extra+=' preflight'
        if kind=='summary' and version in range(4,14):extra+=' root_phase root_reason root_detail'
        if kind=='summary' and version in range(5,14):extra+=' reader_reads'
        if kind=='range' and version in (2,3,4):extra+=' reservation returned'
        if kind=='range' and version in (3,4):extra+=' protected_pages'
        if kind=='range' and version==4:extra+=' aperture_page aperture_alias frame0 frame1 frame_reads'
        if set(row)!=set((SCHEMAS[kind]+' '+extra).split()) or version not in (range(1,14) if kind=='summary' else (1,2,3,4) if kind=='range' else (1,)):raise ValueError('stolen schema')
        rows[kind].append(row)
    if not any(rows.values()):return None
    if len(rows['summary'])!=1 or [r['sample'] for r in rows['facts']]!=[0,1] or [(r['sample'],r['range']) for r in rows['run']]!=[(n,r) for n in range(2) for r in range(3)]:
        raise ValueError('missing/duplicate/unordered stolen rows')
    s=rows['summary'][0];wire=s['v']
    if len(rows['range'])!=int(wire>=9) or (wire==8 and s['fail']>31):
        raise ValueError('stolen range receipt version')
    if wire<9:rows.pop('range')
    if wire<8:
        if any(rows.pop(kind) for kind in ('hardware-gate','hardware-config','hardware-readers')):
            raise ValueError('unexpected held hardware rows')
    if wire==7 and s.get('fail',0)>30:raise ValueError('historical refusal cause')
    if wire<7:
        if rows.pop('guard') or rows.pop('guard-walk'):raise ValueError('unexpected CPU exclusion rows')
    else:verify_guard_progress(rows,s)
    if wire==6 and s.get('fail',0)>29:raise ValueError('historical refusal cause')
    if wire<6:
        if rows.pop('cpu') or rows.pop('cpu-span'):raise ValueError('unexpected CPU backing preflight')
    else:verify_cpu_progress(rows,s)
    hardware = verify_hardware_gate(rows,s) if wire>=8 else None
    if wire==5 and s.get('fail',0)>28:raise ValueError('historical refusal cause')
    if wire<5:
        if rows.pop('reader') or rows.pop('policy'):raise ValueError('unexpected owner preflight')
        if s.get('fail',0)>27:raise ValueError('historical refusal cause')
    if wire>=5:verify_owner_progress(rows,s)
    if required_version is not None and wire!=required_version:raise ValueError('stolen exact wire required')
    if wire>=2:
        if s['cache_index']!=3 or s['fail']>=len(FAILURES) or s['offset']>=0x1000000 or s['offset']&3:
            raise ValueError('stolen admission receipt')
        if any(s[k] not in (0,1,4,5,6,7,255) for k in ('mtrr','pat','effective')):raise ValueError('stolen cache type encoding')
        success=bool(s['complete'] and s['released'] and not s['retained'])
        if success:
            if s['fail'] or s['offset'] or any(s[k] for k in ('mtrr','pat','effective')):raise ValueError('stolen unresolved admission')
        elif not s['fail']:raise ValueError('stolen missing refusal cause')
    if wire>=4:
        phase,reason,detail=(s[k] for k in ('root_phase','root_reason','root_detail'))
        if phase not in (0,1,4) or reason>8:raise ValueError('stolen root observation encoding')
        if phase==0 and (reason or detail):raise ValueError('stolen unobserved root')
        if phase==4 and (reason or detail):raise ValueError('stolen successful root detail')
        if phase==1:
            if not reason or s['fail']!=5 or s['complete']:raise ValueError('stolen failed root outcome')
            if ((reason==1 and detail!=512) or (reason in (2,3,5,6) and detail) or
                (reason==4 and detail&0x000ffffffffff000) or (reason==7 and not 1<=detail<=64) or
                (reason==8 and not detail)):raise ValueError('stolen root detail')
        if s['acquired'] and phase==0:raise ValueError('stolen missing root admission')
        if s['complete'] and phase!=4:raise ValueError('stolen missing successful root')
    if wire>=3:
        if s['preflight'] not in (0,1) or (not s['preflight'] and (s['maps']>1 or s['meta_reads'] or any(row['reads'] for row in rows['run']))):raise ValueError('stolen preflight before read')
        if s['preflight'] and (s['maps']<1 or s['retired']<1 or s['cache_checks']<3 or s['alias_checks']<2):raise ValueError('stolen preflight retirement')
        if s['complete'] and not s['preflight']:raise ValueError('stolen missing preflight')
    if any(s[k]>1 for k in ('complete','acquired','released','retained','classified')) or s['phase']>3 or s['reason']&~62 or s['writes'] or s['ownership']!='none':
        raise ValueError('stolen authority/outcome')
    if s['released']>s['acquired'] or s['retained']!=(s['acquired'] and not s['released']) or s['classified']!=s['complete']:
        raise ValueError('stolen lifetime receipt')
    # Private mode scans at most 105 table pages above 46 MiB and revisits
    # at most four to recheck the selected surface.
    extra_limit=109 if wire==13 else 0
    if s['maps']>128+extra_limit or s['retired']>s['maps'] or s['cache_checks']>256+2*extra_limit or s['meta_reads']>8 or s['checked']>65536 or (not s['retained'] and s['maps']!=s['retired']):
        raise ValueError('stolen mapping progress')
    for f in rows['facts']:
        if any(f[k]>=1<<32 for k in HEX if k in f):raise ValueError('stolen DWORD overflow')
    for r in rows['run']:
        if r['pages']>16384 or r['count']>r['pages'] or not 3*r['count']<=r['reads']<=3*r['count']+3 or r['bytes']!=r['pages']*4096 or (r['gpu']&4095):
            raise ValueError('stolen vector progress')
        if r['bad_index']>r['count'] or (r['bad_index']==r['count'] and r['bad_value']):raise ValueError('stolen failed-entry progress')
        if not r['count'] and (r['first'] or r['last']):raise ValueError('stolen empty prefix')
    complete=bool(s['complete'] and s['released'] and not s['retained'])
    result=dict(complete=complete,native_complete=False,diagnostic_complete=complete,diagnostic_stolen=True,
                hardware_ownership=False,managed_range=False,physical_tearfree=False,version=wire,rows=rows,summary=s,
                failure_stage=FAILURES[s['fail']] if wire>=2 else 'legacy-unspecified')
    if wire>=4:
        names=('none','interrupts-enabled','invalid-expected-root','kernel-root-mismatch','current-CR3-mismatch',
               'missing-root-pointer','root-pointer-mismatch','registered-roots','occupied-root-records')
        result['root_observation']=dict(phase=s['root_phase'],reason=s['root_reason'],detail=s['root_detail'],condition=names[s['root_reason']])
    if hardware is not None:result['hardware_gate']=hardware
    range_reads=0
    if wire>=9:
        r=rows['range'][0]
        if r['v']!=min(wire-8,4):
            raise ValueError('scanout reservation wire mismatch')
        if wire>=12:
            if r['frame_reads']>4097 or max(r['frame0'],r['frame1'])>0xffffffff:
                raise ValueError('invalid primary frame counter')
            if r['verified'] and (r['frame_reads']<2 or not 1<=((r['frame1']-r['frame0']) & 0xffffffff)<=240):
                raise ValueError('original primary did not advance')
            if r['aperture_alias'] not in (0,1) or (r['aperture_alias'] and
                (rows['guard'][0]['reason']!=12 or rows['guard'][0]['acquired'] or r['aperture_page'])):
                raise ValueError('inconsistent aperture alias refusal')
            page=r['aperture_page']
            if (page>=(1<<39) or (page and (page & 0xfffffff)!=0x2200000) or (r['protected_pages'] and not page)
                or (page and not rows['guard'][0]['checked'])):
                raise ValueError('candidate aperture page exclusion')
        if wire>=11:
            protected=r['protected_pages']
            if protected not in (0,8704) or (r['epoch'] and not protected) or (protected and not rows['guard'][0]['checked']):
                raise ValueError('inherited framebuffer protection')
        if wire>=10:
            if r['returned']>1 or (r['reservation'] and not r['epoch']) or (r['returned'] and not r['reservation']):
                raise ValueError('scanout reservation lifetime')
            if (r['fences'] or r['verified']) and not r['reservation']:
                raise ValueError('observed before scanout reservation')
            if r['reservation'] and not r['returned'] and not s['retained']:
                raise ValueError('lost scanout reservation')
        if (r['writes'],r['assurances'],r['obligations'])!=(0,0,255) or r['verified']>1:
            raise ValueError('scanout range authority')
        if r['epoch']:
            v=rows['run'][4]
            if (r['slot'],r['pages'])!=(8704,1484) or r['checked']>1484 or r['fences']>64 or r['closing_readers']>42 or not rows['guard'][0]['checked'] or rows['hardware-readers'][0]['count']!=42:
                raise ValueError('scanout range geometry or guard')
            if (r['checked'] and r['fences']<32) or (r['fences']>32 and r['checked']!=1484) or (r['closing_readers'] and r['fences']!=64):
                raise ValueError('scanout range read order')
            if r['hash']!=_hash(v['first'],r['checked']):
                raise ValueError('scanout range original entries')
        elif any(r[k] for k in ('slot','pages','checked','fences','closing_readers','verified','hash')):
            raise ValueError('scanout range before source')
        if r['missing']!=(31 if r['verified'] else 0) or (r['verified'] and (r['checked'],r['fences'],r['closing_readers'])!=(1484,64,42)):
            raise ValueError('incomplete scanout range')
        if r['verified'] and dict(rows['hardware-config'][0],sample=1)!=rows['hardware-config'][1]:
            raise ValueError('changed scanout requester or firmware endpoint')
        if hardware['observation_complete'] and not r['verified']:
            raise ValueError('missing scanout range observation')
        result['scanout_range']=r
        range_reads=3*(r['fences']+r['checked'])+r['closing_readers']+r.get('frame_reads',0)
    private=[line for line in text.splitlines() if line.startswith('intel-private-range:')]
    if len(private)!=int(wire==13):
        raise ValueError('missing or unexpected private range result')
    extra_maps=extra_aliases=0
    if private:
        from intel_gt_bootstrap import fields
        h=fields(private[0],'intel-private-range:',
                 'v begin first scanned occupied checked batches maps claimed returned exhausted reader_conflict writes'.split())
        if (any(h[k]>1 for k in ('claimed','returned','exhausted','reader_conflict')) or h['writes'] or
                h['returned']>h['claimed'] or h['occupied']>h['scanned'] or h['scanned']>65536-h['begin'] or
                h['checked']>1484 or (h['claimed'] and (h['first']%64 or h['first']<h['begin'] or
                h['first']+1484!=h['begin']+h['scanned'])) or
                (not h['claimed'] and (h['first'] or h['checked']))):
            raise ValueError('private range bounds or reservation')
        if complete:
            last=rows['run'][2]
            begin=((last['gpu']+last['bytes']+0x1fffff)//0x200000)*512
            if h['begin']!=begin or h['returned']!=h['claimed'] or sum(h[k] for k in ('returned','exhausted','reader_conflict'))!=1:
                raise ValueError('private range outcome')
            if h['claimed'] and (h['checked']!=1484 or h['scanned']-h['occupied']<1484):
                raise ValueError('private lease revalidation')
            if h['exhausted'] and h['scanned']!=65536-begin:
                raise ValueError('incomplete aperture search')
            readers=rows['reader'][3:]
            conflict=any(bool(r['w0']>>31)!=(pipe==0) or r['w5']&63 or r['w8']>>31 or
                         any(r['w'+str(w)]&0xfffff000 for w in range(1,5)) for pipe,r in enumerate(readers))
            if bool(h['reader_conflict'])!=conflict:
                raise ValueError('private range reader conflicts differ from hardware snapshot')
            if h['reader_conflict'] and h['scanned']:
                raise ValueError('private range searched with conflicting display reader')
            extra_maps=(h['scanned']+511)//512
            if h['checked']:
                extra_maps+=(h['first']+1483)//512-h['first']//512+1
            batches=(h['scanned']+63)//64+(h['checked']+63)//64
            if (h['maps'],h['batches'])!=(extra_maps,batches):
                raise ValueError('private range MMIO coverage')
            extra_aliases=2*batches
        result['private_range']=h
    if not complete:return result
    if s['reason'] or s['phase']!=3 or not s['acquired'] or s['clock'] or s['previous'] or s['meta_reads']!=8:
        raise ValueError('stolen incomplete success')
    a,b=rows['facts']
    if any(a[k]!=b[k] for k in SCHEMAS['facts'].split() if k not in ('sample','begin','end','fbc','psr_ctl','psr_status')):
        raise ValueError('stolen changed facts')
    if (a['host'],a['gpu'],a['subsystem'])!=(0x16048086,0x16268086,0x011b106b) or a['ggc']>65535 or a['ggc']&63!=1:
        raise ValueError('stolen locked device profile')
    if any(a[k]&0xfffff!=1 for k in ('bdsm','bgsm','tolud')):raise ValueError('stolen PCI bounds')
    data=a['bdsm']&0xfff00000;top=a['tolud']&0xfff00000;gtt=a['bgsm']&0xfff00000
    gms=a['ggc']>>8;ggms=a['ggc']>>6&3;reserved=a['reserved']&0xfff00000
    if not 1<=gms<=16 or not ggms or a['ggtt']!=(1<<ggms)*1048576 or top!=data+gms*33554432 or gtt+a['ggtt']!=data or not gtt:
        raise ValueError('stolen data/GTT bounds')
    if not a['reserved']&1 or a['reserved']&~0xfff00007 or not data+4096<reserved<top:
        raise ValueError('stolen firmware reservation')
    if (a['width'],a['height'],a['stride'])!=(1440,900,6144):raise ValueError('stolen captured display mode')
    if not 1<=a['width']<=4096 or not 1<=a['height']<=4096 or not a['width']*4<=a['stride']<=32768 or a['stride']&63:
        raise ValueError('stolen framebuffer dimensions')
    if (a['fence_begin'],a['fence_end'],a['aperture'],a['ggtt'])!=(0,0x2200000,0x10000000,0x800000):
        raise ValueError('stolen native aperture/fence')
    pixels=a['stride']*a['height'];needed=((pixels+4095)//4096+128)*4096
    size=(needed+262143)//262144*262144
    if size>16*1048576 or a['fence_end']+2*size>a['aperture']:raise ValueError('stolen proposed range overflow')
    spans=[(0,0x2200000),(0x2200000,size),(0x2200000+size,size)]
    maps=2*(3+sum(((gpu//4096*8+size//4096*8+4095)//4096)-(gpu//4096*8//4096) for gpu,size in spans))
    if wire>=3:maps+=1
    entries=sum(size//4096 for _,size in spans)
    if wire>=5:maps+=6
    held_maps=5 if hardware and hardware['observation_complete'] else 0
    maps+=extra_maps+held_maps+(10 if range_reads else 0)
    if (s['maps'],s['retired'],s['cache_checks'],s['alias_checks'],s['checked'])!=(maps,maps,2*maps+(wire>=3),6*entries+8+2*maps+extra_aliases+range_reads+s.get('reader_reads',0)+(42 if held_maps else 0),2*entries):
        raise ValueError('stolen complete census counters')
    physical=[]
    for n in range(2):
        f=rows['facts'][n]
        if not 0<f['begin']<f['end']<=f['begin']+2000000 or (n and f['begin']<a['end']):raise ValueError('stolen bounded sample clock')
        for r,(gpu,size) in enumerate(spans):
            v=rows['run'][n*3+r];pages=size//4096;first=v['first'];base=first&~4095
            if (v['gpu'],v['bytes'],v['pages'],v['count'],v['reads'])!=(gpu,size,pages,pages,3*pages) or first&0xffffffff00000fff!=1:
                raise ValueError('stolen complete affine run')
            if v['bad_index']!=pages or v['bad_value'] or v['last']!=first+4096*(pages-1) or v['hash']!=_hash(first,pages) or not data+4096<=base<base+size<=reserved:
                raise ValueError('stolen vector hash/reservation')
            if n:
                old=rows['run'][r]
                if any(v[k]!=old[k] for k in v if k!='sample'):raise ValueError('stolen changed complete vector')
            else:
                if any(base<end and start<base+size for start,end in physical):raise ValueError('stolen physical overlap')
                physical.append((base,base+size))
    if wire>=5:result['owner_preflight']=owner_preflight(rows,physical)
    if wire>=6:result['cpu_backing_admission']=cpu_admission(rows,physical)
    if wire>=7:result['cpu_mapping_exclusion']=dict(lifetime_complete=bool(rows['guard'][0]['end']),hardware_ownership=False,physical_tearfree=False)
    result.update(physical=[list(v) for v in physical],proposed_ranges=[list(v) for v in spans],auxiliary_disabled=all(not (f['fbc']&0x80000000 or f['psr_ctl']&0x80000000 or f['psr_status']&0xe0000000) for f in (a,b)))
    if native:
        from intel_ggtt_window import native_window_result
        from intel_display_capture import capture_result
        from intel_display_backing import backing_result
        window=native_window_result(text);display=capture_result(text);backing=backing_result(text)
        if not window or not window['native_observation_complete'] or not display:raise ValueError('stolen native prerequisites')
        if wire>=12 and rows['range'][0]['aperture_page']!=window['summary']['aperture_base']+a['fence_end']:
            raise ValueError('excluded page differs from the native PCI aperture')
        for e in backing['rows']['endpoint']:
            if any((e[k]&65535 if k=='ggc' else e[k])!=a[k] for k in ('host','ggc','bdsm','bgsm','tolud')):
                raise ValueError('stolen changed PCI source bounds')
        front=rows['run'][0]['first'];offset=backing['summary']['offset']//4096
        if backing['ptes']!=[front+(offset+i)*4096 for i in range(len(backing['ptes']))]:
            raise ValueError('stolen original framebuffer translations differ')
        if wire>=2:
            from intel_cache import native_cache_result,_snapshot,_page
            cache=native_cache_result(text)
            offsets={0x108000,0x43000,0x6f000}
            if wire>=5:offsets.update((0x70000,0x71000,0x72000))
            for gpu,size in spans:
                start=0x800000+gpu//4096*8;end=start+size//4096*8
                offsets.update(range(start&~4095,(end+4095)&~4095,4096))
            if wire==13 and h['scanned']:
                first=0x800000+h['begin']*8
                offsets.update(range(first,first+((h['scanned']+511)//512)*4096,4096))
            for sample in cache['rows']['sample']:
                n=sample['sample'];regions=_snapshot(sample,[r for r in cache['rows']['variable'] if r['sample']==n],[r for r in cache['rows']['fixed'] if r['sample']==n])
                if any(_page(sample,regions,window['summary']['mmio']+offset,3)!=(0,0,0) for offset in offsets):
                    raise ValueError('stolen original UC3 cache policy mismatch')
                if wire>=5:verify_candidate_policies(rows,sample,regions)
                if wire>=6 and rows['cpu'][0]['bits']!=sample['bits']:raise ValueError('CPU backing width differs from original context')
        fb=display['rows']['framebuffer'][0]
        if (fb['width'],fb['height'],fb['pitch'])!=(a['width'],a['height'],a['stride']) or window['rows']['sample'][1]['end_us']>a['begin']:
            raise ValueError('stolen stale/changed source')
        first=rows['run'][1]['first']
        if window['vectors']!=[[first+4096*i for i in range(16)]]*2:raise ValueError('stolen original window mismatch')
        result['native_observation_complete']=True
    return result

def verify_stolen_checkpoint(preflight,final,required_version=None,**_):
    result=stolen_result(final,native=True,required_version=required_version)
    if not result:return False
    if any(l.startswith(('intel-private-range:','intel-stolen-summary:','intel-stolen-range:','intel-stolen-facts:','intel-stolen-run:','intel-stolen-reader:','intel-stolen-policy:','intel-stolen-cpu:','intel-stolen-cpu-span:','intel-stolen-guard:','intel-stolen-guard-walk:','intel-stolen-hardware-gate:','intel-stolen-hardware-config:','intel-stolen-hardware-readers:')) for l in preflight.splitlines()):
        raise ValueError('stolen late prerequisite checkpoint')
    lines=[l for l in preflight.splitlines() if l.startswith('laptop-capture: boot options=')]
    if len(lines)!=1:raise ValueError('stolen preflight recipe')
    command=lines[0].split('=',1)[1]
    stolen_recipe(command)
    if (('srvros.intel-scanout=range-observation' in command.split())!=(9<=result['version']<=12) or
        ('srvros.intel-scanout=private-range' in command.split())!=(result['version']==13)):
        raise ValueError('stolen range recipe/version mismatch')
    prefixes=('intel-display-','intel-backing-','intel-cache-','intel-window-')
    before=[l for l in preflight.splitlines() if l.startswith(prefixes)]
    after=[l for l in final.splitlines() if l.startswith(prefixes)]
    if not before or before!=after:raise ValueError('stolen preflight source mismatch')
    return True


PROGRESS_STEPS=('invalid','profile','source-snapshot','root-PCI','console-sync',
                'display-exclusion','identity','cache-context','source-revalidation',
                'metadata','vectors','source-final','classification','cleanup',
                'report','mapping','cache-policy','actual-alias','first-MMIO-read','display-readers','candidate-cache','CPU-backing','CPU-guard','hardware-ownership-gate')

def stolen_progress(text,command):
    """Pending operation history only; never a completion or ownership receipt."""
    if len(text.encode('utf-8'))>4096:raise ValueError('stolen progress size')
    lines=text.splitlines()
    if not lines or lines[0]!='laptop-capture: boot options='+command:
        raise ValueError('stolen progress recipe mismatch')
    stolen_recipe(command)
    rows=[]
    pattern=r'intel-stolen: progress v=1 seq=(\d{1,10}) step=(\d{1,2}) edge=([01]) phase=([0-3]) offset=(\d{1,8}) maps=(\d{1,3}) retired=(\d{1,3}) held=([01]) mapped=([01])'
    for line in lines[1:]:
        m=re.fullmatch(pattern,line)
        if not m:raise ValueError('stolen progress encoding')
        row=dict(zip(('seq','step','edge','phase','offset','maps','retired','held','mapped'),map(int,m.groups())))
        if not 0<row['step']<len(PROGRESS_STEPS) or not 0<row['seq']<=4096 or row['offset']>=0x1000000 or row['offset']&3:
            raise ValueError('stolen progress bounds')
        if row['maps']>128 or row['retired']>row['maps'] or row['mapped']>row['held'] or row['maps']-row['retired']!=row['mapped']:
            raise ValueError('stolen progress alias state')
        if rows and row['seq']!=rows[-1]['seq']+1:raise ValueError('stolen progress sequence')
        rows.append(row)
    if not 1<=len(rows)<=16 or rows[-1]['mapped'] or rows[-1]['edge']:
        raise ValueError('stolen progress checkpoint boundary')
    return dict(version=1,rows=rows,pending_operation=PROGRESS_STEPS[rows[-1]['step']],
                native_acceptance=False,completion_evidence=False,physical_tearfree=False)
