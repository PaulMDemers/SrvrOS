"""Read-only full-vector layout receipts. A fitting range is never free memory."""
import re

SCHEMAS={
    'summary':'v complete phase reason acquired released retained maps retired cache_checks alias_checks meta_reads classified checked clock previous writes ownership',
    'facts':'v sample begin end host gpu subsystem ggc bdsm bgsm tolud reserved width height stride aperture ggtt fence_begin fence_end fbc psr_ctl psr_status',
    'run':'v sample range gpu bytes pages count reads first last hash bad_index bad_value',
    'reader':'v sample pipe count '+ ' '.join('w'+str(i) for i in range(13)),
    'policy':'v sample range index physical pages queried ok mtrr pat effective checks',
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
          'changed-facts','source-window','allowed-range','progress-save','invalid-reader-word','CPU-backing-guard','CPU-mapping-exclusion')
HEX=set('host gpu subsystem ggc bdsm bgsm tolud reserved fbc psr_ctl psr_status first last hash bad_value'.split())
HEX.update(['virtual','entry','physical']+['w'+str(i) for i in range(13)])
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
        elif any(q[k] not in (0,1,4,5,6,7) for k in ('mtrr','pat','effective')) or not 1<=q['checks']<=33:
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

def stolen_result(text,*,native=False,required_version=None):
    rows={k:[] for k in SCHEMAS}
    for line in text.splitlines():
        if not line.startswith(('intel-stolen-summary:','intel-stolen-facts:','intel-stolen-run:','intel-stolen-reader:','intel-stolen-policy:','intel-stolen-cpu:','intel-stolen-cpu-span:','intel-stolen-guard:','intel-stolen-guard-walk:')):
            if re.match(r'intel-stolen-[^ :]+:',line):raise ValueError('unknown stolen receipt')
            continue
        match=re.fullmatch(r'intel-stolen-(summary|facts|run|reader|policy|cpu-span|cpu|guard-walk|guard): (.+)',line)
        if not match:raise ValueError('malformed stolen receipt')
        kind=match[1];row={}
        for token in match[2].split():
            k,sep,value=token.partition('=')
            if not sep or k in row:raise ValueError('duplicate stolen field')
            if k in ('ownership','authority'):row[k]=value;continue
            hexadecimal=k in HEX and not ((kind=='run' and k=='gpu') or (kind=='cpu-span' and k=='reserved'))
            if not re.fullmatch(r'(?:0|0x[0-9a-fA-F]{1,16})' if hexadecimal else r'[0-9]{1,20}',value):
                raise ValueError('stolen numeric encoding')
            row[k]=int(value,16 if hexadecimal else 10)
            if row[k]>=1<<64:raise ValueError('stolen integer overflow')
        version=row.get('v');extra=WIRE2 if kind=='summary' and version in (2,3,4,5,6,7) else ''
        if kind=='summary' and version in (3,4,5,6,7):extra+=' preflight'
        if kind=='summary' and version in (4,5,6,7):extra+=' root_phase root_reason root_detail'
        if kind=='summary' and version in (5,6,7):extra+=' reader_reads'
        if set(row)!=set((SCHEMAS[kind]+' '+extra).split()) or version not in ((1,2,3,4,5,6,7) if kind=='summary' else (1,)):raise ValueError('stolen schema')
        rows[kind].append(row)
    if not any(rows.values()):return None
    if len(rows['summary'])!=1 or [r['sample'] for r in rows['facts']]!=[0,1] or [(r['sample'],r['range']) for r in rows['run']]!=[(n,r) for n in range(2) for r in range(3)]:
        raise ValueError('missing/duplicate/unordered stolen rows')
    s=rows['summary'][0];wire=s['v']
    if wire<7:
        if rows.pop('guard') or rows.pop('guard-walk'):raise ValueError('unexpected CPU exclusion rows')
    else:verify_guard_progress(rows,s)
    if wire==6 and s.get('fail',0)>29:raise ValueError('historical refusal cause')
    if wire<6:
        if rows.pop('cpu') or rows.pop('cpu-span'):raise ValueError('unexpected CPU backing preflight')
    else:verify_cpu_progress(rows,s)
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
    if s['maps']>128 or s['retired']>s['maps'] or s['cache_checks']>256 or s['meta_reads']>8 or s['checked']>65536 or (not s['retained'] and s['maps']!=s['retired']):
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
    if (s['maps'],s['retired'],s['cache_checks'],s['alias_checks'],s['checked'])!=(maps,maps,2*maps+(wire>=3),6*entries+8+2*maps+s.get('reader_reads',0),2*entries):
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
    if any(l.startswith(('intel-stolen-summary:','intel-stolen-facts:','intel-stolen-run:','intel-stolen-reader:','intel-stolen-policy:','intel-stolen-cpu:','intel-stolen-cpu-span:','intel-stolen-guard:','intel-stolen-guard-walk:')) for l in preflight.splitlines()):
        raise ValueError('stolen late prerequisite checkpoint')
    lines=[l for l in preflight.splitlines() if l.startswith('laptop-capture: boot options=')]
    if len(lines)!=1:raise ValueError('stolen preflight recipe')
    stolen_recipe(lines[0].split('=',1)[1])
    prefixes=('intel-display-','intel-backing-','intel-cache-','intel-window-')
    before=[l for l in preflight.splitlines() if l.startswith(prefixes)]
    after=[l for l in final.splitlines() if l.startswith(prefixes)]
    if not before or before!=after:raise ValueError('stolen preflight source mismatch')
    return True


PROGRESS_STEPS=('invalid','profile','source-snapshot','root-PCI','console-sync',
                'display-exclusion','identity','cache-context','source-revalidation',
                'metadata','vectors','source-final','classification','cleanup',
                'report','mapping','cache-policy','actual-alias','first-MMIO-read','display-readers','candidate-cache','CPU-backing','CPU-guard')

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
