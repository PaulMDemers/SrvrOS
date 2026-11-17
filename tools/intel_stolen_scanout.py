"""Read-only full-vector layout receipts. A fitting range is never free memory."""
import re

SCHEMAS={
    'summary':'v complete phase reason acquired released retained maps retired cache_checks alias_checks meta_reads classified checked clock previous writes ownership',
    'facts':'v sample begin end host gpu subsystem ggc bdsm bgsm tolud reserved width height stride aperture ggtt fence_begin fence_end fbc psr_ctl psr_status',
    'run':'v sample range gpu bytes pages count reads first last hash bad_index bad_value',
}
HEX=set('host gpu subsystem ggc bdsm bgsm tolud reserved fbc psr_ctl psr_status first last hash bad_value'.split())
FIXED=('srvros.storage=ram','srvros.capture=intel-display','srvros.intel-backing=read-only',
       'srvros.intel-cache=read-only','srvros.intel-window=read-only','srvros.intel-stolen=read-only','srvros.capture-failure=reboot')

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

def stolen_result(text,*,native=False):
    rows={k:[] for k in SCHEMAS}
    for line in text.splitlines():
        if not line.startswith(('intel-stolen-summary:','intel-stolen-facts:','intel-stolen-run:')):
            if re.match(r'intel-stolen-[^ :]+:',line):raise ValueError('unknown stolen receipt')
            continue
        match=re.fullmatch(r'intel-stolen-(summary|facts|run): (.+)',line)
        if not match:raise ValueError('malformed stolen receipt')
        kind=match[1];row={}
        for token in match[2].split():
            k,sep,value=token.partition('=')
            if not sep or k in row:raise ValueError('duplicate stolen field')
            if k=='ownership':row[k]=value;continue
            hexadecimal=k in HEX and not (kind=='run' and k=='gpu')
            if not re.fullmatch(r'(?:0|0x[0-9a-fA-F]{1,16})' if hexadecimal else r'[0-9]{1,20}',value):
                raise ValueError('stolen numeric encoding')
            row[k]=int(value,16 if hexadecimal else 10)
            if row[k]>=1<<64:raise ValueError('stolen integer overflow')
        if set(row)!=set(SCHEMAS[kind].split()) or row['v']!=1:raise ValueError('stolen schema')
        rows[kind].append(row)
    if not any(rows.values()):return None
    if len(rows['summary'])!=1 or [r['sample'] for r in rows['facts']]!=[0,1] or [(r['sample'],r['range']) for r in rows['run']]!=[(n,r) for n in range(2) for r in range(3)]:
        raise ValueError('missing/duplicate/unordered stolen rows')
    s=rows['summary'][0]
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
                hardware_ownership=False,managed_range=False,physical_tearfree=False,version=1,rows=rows,summary=s)
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
    entries=sum(size//4096 for _,size in spans)
    if (s['maps'],s['retired'],s['cache_checks'],s['alias_checks'],s['checked'])!=(maps,maps,2*maps,6*entries+8+2*maps,2*entries):
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
        fb=display['rows']['framebuffer'][0]
        if (fb['width'],fb['height'],fb['pitch'])!=(a['width'],a['height'],a['stride']) or window['rows']['sample'][1]['end_us']>a['begin']:
            raise ValueError('stolen stale/changed source')
        first=rows['run'][1]['first']
        if window['vectors']!=[[first+4096*i for i in range(16)]]*2:raise ValueError('stolen original window mismatch')
        result['native_observation_complete']=True
    return result

def verify_stolen_checkpoint(preflight,final,**_):
    result=stolen_result(final,native=True)
    if not result or not result['complete']:return False
    lines=[l for l in preflight.splitlines() if l.startswith('laptop-capture: boot options=')]
    if len(lines)!=1:raise ValueError('stolen preflight recipe')
    stolen_recipe(lines[0].split('=',1)[1])
    prefixes=('intel-display-','intel-backing-','intel-cache-','intel-window-')
    before=[l for l in preflight.splitlines() if l.startswith(prefixes)]
    after=[l for l in final.splitlines() if l.startswith(prefixes)]
    if not before or before!=after:raise ValueError('stolen preflight source mismatch')
    return True
