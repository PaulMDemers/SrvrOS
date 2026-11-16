"""Paired native provider witnesses; never GPU interval or completion authority."""
import struct
from intel_source_audit import _raw,shared_view

FIELDS='''state reason registered complete begin last end clock previous root physical serial epoch kind
prepare gt hda outer samples reads groups verified missing budget first pages guard_reads bad_command issued
pte_writes managed_pages hardware_ownership'''.split()
SAMPLE_WORDS=98


def ownership_result(text,command,parent,*,native=False):
    lines=[l for l in text.splitlines() if l.startswith('intel-command-ownership:')]
    if len(lines)!=1:raise ValueError('Command ownership receipt coverage')
    raw=_raw(lines[0],'intel-command-ownership:',1824)
    words=struct.unpack('<228Q',raw);d=dict(zip(FIELDS,words[:32]))
    if len(FIELDS)!=32 or d['state']>3 or d['reason']&~63 or d['registered']>1 or d['complete']>1:
        raise ValueError('Command ownership schema')
    if (d['budget'],d['first'],d['pages'],d['pte_writes'],d['managed_pages'],d['hardware_ownership'])!=(150000,8704,4,0,0,0):
        raise ValueError('Command ownership policy or false authority')
    if d['samples']>2 or d['reads']>180 or d['groups']>22 or d['guard_reads']>162 or d['bad_command']>0xffffffff or d['verified']&~6 or d['missing'] not in (0,25):
        raise ValueError('Command ownership counters or proof scope')
    expected=('intel-command-provider: state={state} reason={reason} samples={samples} reads={reads} groups={groups} checked={verified} '
              'missing={missing} complete={complete} clock={clock} previous={previous}; no GPU grant').format(**d)
    if [l for l in text.splitlines() if l.startswith('intel-command-provider:')]!=[expected]:
        raise ValueError('Command ownership progress/result mismatch')
    samples=[]
    for n in range(2):
        w=words[32+n*SAMPLE_WORDS:32+(n+1)*SAMPLE_WORDS]
        samples.append(dict(begin=w[0],end=w[1],pci=list(w[2:11]),
            engines=[list(w[11+e*6:17+e*6]) for e in range(5)],
            planes=[list(w[41+p*13:54+p*13]) for p in range(3)],pte=list(w[80:84]),
            alias=list(w[84:91]),cache=list(w[91:94]),backing=w[94],cache_begin=w[95],cache_end=w[96],cache_reads=w[97]))
    c=command['summary'];e=parent['engines']['summary']
    complete=bool(d['complete'])
    if complete:
        if (d['state'],d['reason'],d['registered'],d['clock'],d['previous'],d['samples'],d['reads'],d['groups'],d['verified'],d['missing'],d['guard_reads'],d['bad_command'],d['issued'])!=(2,0,0,0,0,2,180,22,6,25,162,0,10):
            raise ValueError('Command ownership false completion')
        if not command['complete'] or not parent['complete']:
            raise ValueError('Command ownership enclosing retirement')
        for key in ('root','physical','serial','epoch','prepare','gt','hda','outer'):
            if d[key]!=c[key]:raise ValueError('Command ownership original '+key)
        if d['kind']!=e['kind'] or not c['end']<=d['begin']<d['end']==d['last']<=e['child_end']:
            raise ValueError('Command ownership source order')
        if any(d['end']-d[key]>cap for key,cap in (('prepare',150000),('gt',300000),('hda',400000),('outer',500000))):
            raise ValueError('Command ownership original deadline')
        from intel_ggtt_window import window_result
        window=window_result(shared_view(text))
        if not window or not window['readout_complete']:raise ValueError('Command ownership original window')
        reset=parent['engines']['requester']['transfer']['buffers']['source']['reset']
        engines={row['engine']:[row['w'+str(i)] for i in range(6)] for row in reset['post']['rows'] if row['sample']==1}
        from intel_cache import cache_result
        context=cache_result(shared_view(text))
        if native and not context:raise ValueError('Command ownership missing native cache context')
        cache_reads=context['rows']['sample'][1]['reads'] if context else None
        old=parent['samples'][1];previous=d['begin'];old_plane=(0,1,2,5,6,7,8,9,10)
        for q in samples:
            if not previous<=q['begin']<q['end']<=d['end'] or not q['begin']<=q['cache_begin']<q['cache_end']<=q['end'] or not 6<=q['cache_reads']<=49 or (cache_reads is not None and q['cache_reads']!=cache_reads):
                raise ValueError('Command ownership sample timing')
            previous=q['end']
            if q['pci']!=old['pci'] or q['pci'][1]&65535!=3 or q['pci'][8]:raise ValueError('Command ownership requester/AF')
            if q['engines']!=[engines[i] for i in range(5)]:raise ValueError('Command ownership engine preservation')
            if q['pte']!=window['vectors'][1][:4]:raise ValueError('Command ownership exact four PTEs')
            if q['alias']!=[4,4,0,1,0,1,1] or q['cache']!=[6,6,6] or q['backing']!=1:
                raise ValueError('Command ownership CPU backing/alias/cache')
            for p in range(3):
                if any(v>=1<<32 or v==0xffffffff for v in q['planes'][p]) or [q['planes'][p][i] for i in old_plane]!=old['planes'][p]:
                    raise ValueError('Command ownership programmed/live reader')
        for key in ('pci','engines','planes','pte','alias','cache','backing'):
            if samples[0][key]!=samples[1][key]:raise ValueError('Command ownership changed '+key)
    elif d['state']==2 or d['verified'] or d['missing']:
        raise ValueError('Command ownership partial positive proof')
    if not d['state'] and (any(words[i] for i in range(32) if i not in (23,24,25)) or any(words[32:])):
        raise ValueError('Command ownership unstarted state')
    return dict(summary=d,samples=samples,complete=complete,native_complete=bool(native and complete and command['native_complete']),
        hardware_ownership=False,managed_pages=0,physical_tearfree=False,
        scope='paired native provider prerequisites only; interval/DMA-cache/all-reader completion unproved')
