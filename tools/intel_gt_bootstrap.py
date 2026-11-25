"""Original full-GT owner/epoch receipts. Reset is not GPU memory authority.

The explicit v37 -> v25 view validates only unchanged historical wake/entry/HDA
prerequisites. It never supplies a live post-reset source. Raw bytes stay raw;
the separate bootstrap/post records and their original order decide completion.
"""
import re

TOKEN='srvros.intel-ring=gt-bootstrap-v1'
SECONDS_TOKEN='srvros.intel-ring=gt-bootstrap-seconds-v1'
from intel_gt_timing import ORIGINAL,SECONDS,PRACTICAL,policy as timing_policy
PRACTICAL_TOKEN='srvros.intel-ring=gt-bootstrap-practical-v1'
MEMORY_TOKEN='srvros.intel-ring=gt-bootstrap-memory-v1'
RANGE_TOKEN='srvros.intel-ring=gt-bootstrap-range-v1'
PRIMARY_TOKEN='srvros.intel-ring=gt-bootstrap-primary-v1'
AUX_TOKEN='srvros.intel-ring=gt-bootstrap-aux-v1'
FLUSH_TOKEN='srvros.intel-ring=gt-bootstrap-flush-v1'
CONTEXT_TOKEN='srvros.intel-ring=gt-bootstrap-context-v1'
ARENA_TOKEN='srvros.intel-ring=gt-bootstrap-arena-v1'
PROFILE='ram-usb-v1-intel-gt-bootstrap-'
SUMMARY='''v state reason complete failed blocked returned active epoch_before epoch_after epoch_calls begin end last root prepare_budget prepared issued commands pci_samples pci_reads guards alias_checks cache_checks native_reads native_writes full_before full_after post_bound clock previous obligations managed_pages pte_writes hardware_ownership'''.split()
BINDING='''v owner provider lifetime root mmio mmio_bytes aperture_bytes epoch sequence prepare gt hda outer policy vendor device display_samples display_before display_after pci0 pci1 pci2 pci3 post_epoch post_parent post_object'''.split()
RESET='''v stage reason complete registered retained uncertain epoch reset_attempts reset_writes reset_completed cancelled changed restored reads writes proofs preserved begin end last settle_begin settle_end clock clock_previous clock_reason'''.split()
CONTROL='v i prepare_first prepare_final prepare_polls before after cancel_first cancel_final cancel_polls'.split()


def fields(line,prefix,keys,hexkeys=(),versions=(1,)):
    if not line.startswith(prefix+' '):raise ValueError('Bootstrap prefix')
    result={}
    for word in line[len(prefix):].split():
        k,sep,value=word.partition('=')
        pattern=r'(?:0|0x[0-9a-fA-F]{1,16})' if k in hexkeys else r'[0-9]{1,20}'
        if not sep or k not in keys or k in result or not re.fullmatch(pattern,value):raise ValueError('Bootstrap field encoding')
        result[k]=int(value,16 if k in hexkeys else 10)
        if result[k]>=1<<64:raise ValueError('Bootstrap overflow')
    if set(result)!=set(keys) or result['v'] not in versions:raise ValueError('Bootstrap schema')
    return result


def _bootstrap_token(seconds,practical,memory,arena,context,range_check,primary,aux,flush):
    if flush and not aux:raise ValueError("Flush receipt requires the held auxiliary capture")
    if aux and not primary:raise ValueError("Auxiliary display requires the original primary capture")
    if primary and not range_check:raise ValueError("Primary receipt requires the range child")
    if range_check and (not memory or arena):raise ValueError('Range requires the isolated original memory child')
    if context and not arena:raise ValueError('Context requires original arena')
    if arena and not memory:raise ValueError('Arena requires the original memory child')
    if memory and not practical:raise ValueError("Memory composition requires practical policy")
    timing_policy(seconds=seconds,practical=practical)
    return FLUSH_TOKEN if flush else AUX_TOKEN if aux else PRIMARY_TOKEN if primary else RANGE_TOKEN if range_check else CONTEXT_TOKEN if context else ARENA_TOKEN if arena else MEMORY_TOKEN if memory else PRACTICAL_TOKEN if practical else SECONDS_TOKEN if seconds else TOKEN


def bootstrap_recipe(cmd,*,seconds=False,practical=False,memory=False,arena=False,context=False,range_check=False,primary=False,aux=False,flush=False):
    token=_bootstrap_token(seconds,practical,memory,arena,context,range_check,primary,aux,flush)
    from intel_ring_entry import entry_recipe
    if not isinstance(cmd,str) or cmd.split().count(token)!=1:raise ValueError('Bootstrap exact recipe')
    return entry_recipe(cmd.replace(token,'srvros.intel-ring=entry-v1'))


def historical_view(text,*,seconds=False,practical=False,memory=False,arena=False,context=False,range_check=False,primary=False,aux=False,flush=False):
    token=_bootstrap_token(seconds,practical,memory,arena,context,range_check,primary,aux,flush)
    lines=text.splitlines();out=[]
    wakes=[l for l in lines if l.startswith('intel-wake-summary:')]
    if len(wakes)>1:raise ValueError('Bootstrap duplicate wake')
    if wakes:
        raw=wakes[0]
        if not raw.startswith('intel-wake-summary: v=37 '):raise ValueError('Bootstrap wake image')
        # Check actual new mapping counters before projecting the unchanged
        # fourteen aliases. Partial/faulted lifetimes never acquire completion.
        values=dict(w.split('=',1) for w in raw.split()[1:])
        if len(values)!=len(raw.split())-1:raise ValueError('Bootstrap duplicate wake field')
        maps,retired,cache=(int(values[k]) for k in ('maps','retired','cache_checks'))
        if not 0<=retired<=maps<=15 or not 0<=cache<=15:raise ValueError('Bootstrap actual alias counts')
        if int(values['attempts']) and (maps!=15 or cache!=15):raise ValueError('Bootstrap missing fence alias')
        if int(values['complete']) and (maps,retired,cache)!=(15,15,15):raise ValueError('Bootstrap false alias retirement')
        for key,value in [('v',25),('maps',min(maps,14)),('retired',min(retired,14)),('cache_checks',min(cache,14))]:
            raw=re.sub(r'\b'+key+r'=\d+(?= |$)',key+'='+str(value),raw,count=1)
    for line in lines:
        if line.startswith('intel-wake-summary:'):line=raw
        if line.startswith('laptop-capture: boot options='):
            bootstrap_recipe(line.split('=',1)[1],seconds=seconds,practical=practical,memory=memory,arena=arena,context=context,range_check=range_check,primary=primary,aux=aux,flush=flush);line=line.replace(token,'srvros.intel-ring=entry-v1')
        line=line.replace('init: deferred=gt-bootstrap-preinit;','init: deferred=ring-entry-preinit;')
        out.append(line)
    return '\n'.join(out)


def bootstrap_result(text,*,native=False):
    lines=[l for l in text.splitlines() if l.startswith('intel-bootstrap-')]
    if not lines:return None
    allowed=('summary','binding','reset','retire','timing','maintain','admission','cost','fence','control','pulse','pci','display','post-row','post-context','post-summary')
    if any(l.split(':',1)[0] not in ['intel-bootstrap-'+k for k in allowed] for l in lines):raise ValueError('Bootstrap unknown receipt')
    def one(kind,keys,hexkeys=(),versions=(1,)):
        selected=[l for l in lines if l.startswith('intel-bootstrap-'+kind+':')]
        if len(selected)!=1:raise ValueError('Bootstrap '+kind+' coverage')
        return fields(selected[0],'intel-bootstrap-'+kind+':',keys,hexkeys,versions)
    def rows(kind,keys,hexkeys=()):
        return [fields(l,'intel-bootstrap-'+kind+':',keys,hexkeys) for l in lines if l.startswith('intel-bootstrap-'+kind+':')]
    s=one('summary',SUMMARY,('root',),(1,2,3,4,5,6,7,8,9,10,11,12));b=one('binding',BINDING,('root','mmio','policy','vendor','device'))
    r=one('reset',RESET,('changed','restored'))
    retire=one('retire','v stage guard_begin guard_end return_begin return_end final_begin final_end'.split())
    flush=s['v']==12;aux=flush or s['v']==11;primary=aux or s['v']==10;range_check=primary or s['v']==9;context=s['v']==8;arena=s['v'] in (7,8);memory=range_check or s['v'] in (6,7,8);seconds=s['v']==4;practical=memory or s['v']==5;diagnostic=seconds or practical;policy=timing_policy(seconds=seconds,practical=practical);timing=None
    if diagnostic:
        timing=one('timing','v diagnostic prepare gt_phase gt hda outer hardware_phase poll_cap gt_end hda_end outer_end'.split())
        if {k:v for k,v in timing.items() if k not in ('gt_end','hda_end','outer_end')}!=dict(v=1,diagnostic=1,**policy,hardware_phase=10000,poll_cap=256):raise ValueError('Bootstrap seconds policy')
    elif any(l.startswith('intel-bootstrap-timing:') for l in lines):raise ValueError('Bootstrap foreign timing policy')
    maintain=None
    if s['v']>=3:
        maintain=one('maintain','v complete begin end phase before after'.split())
        if maintain['complete']>1 or max(maintain['before'],maintain['after'])>=1<<32:raise ValueError('Bootstrap maintenance bounds')
        if maintain['complete']:
            if not s['begin']<=r['begin']<=maintain['begin']<maintain['end']<=s['last'] or not 0<maintain['phase']<=maintain['begin'] or maintain['end']-maintain['phase']>policy['gt_phase'] or maintain['after']!=maintain['before']+1:raise ValueError('Bootstrap maintenance clocks/snapshot')
    elif any(l.startswith('intel-bootstrap-maintain:') for l in lines):raise ValueError('Bootstrap historical maintenance receipt')
    admission=None;cost=[]
    if s['v']>=2:
        admission=one('admission','v issued consumed rejected active stage begin partial_begin'.split())
        cost=rows('cost','v i calls total max'.split())
        if [q['i'] for q in cost]!=list(range(5)):raise ValueError('Bootstrap cost coverage')
        if admission['active']>1 or admission['stage']>5:raise ValueError('Bootstrap admission bounds')
        if admission['consumed']>s['native_reads']+1 or admission['issued']>r['proofs']+1 or admission['rejected']>1:raise ValueError('Bootstrap admission counts')
        if admission['partial_begin']<admission['begin'] or admission['partial_begin']>s['last']:raise ValueError('Bootstrap cost clocks')
        for q in cost:
            if q['calls']>s['cache_checks']+1 or q['max']>q['total'] or (not q['calls'] and (q['total'] or q['max'])) or q['total']>policy['prepare']:raise ValueError('Bootstrap cost bounds')
    elif any(l.startswith(('intel-bootstrap-admission:','intel-bootstrap-cost:')) for l in lines):raise ValueError('Bootstrap v1 admission receipt')
    fences=rows('fence','v i before after final'.split(),('before','after','final'))
    controls=rows('control',CONTROL,('prepare_first','prepare_final','before','after','cancel_first','cancel_final'))
    pulses=rows('pulse','v i first final polls'.split(),('first','final'))
    pci=rows('pci','v n i raw'.split(),('raw',));display=rows('display','v n p i raw'.split(),('raw',))
    boolean='complete failed blocked returned active full_before full_after post_bound'.split()
    reasons=2047 if arena else 1023 if memory else 511 if s['v']>=2 else 255
    if flush:
        from intel_gt_flush import SMI_FAULT
        reasons |= SMI_FAULT
    if s['state']>2 or s['reason']&~reasons or any(s[k]>1 for k in boolean) or s['epoch_calls']>1 or s['epoch_after']>=1<<32 or s['epoch_before']>=1<<32:
        raise ValueError('Bootstrap owner range')
    if s['prepare_budget']!=policy['prepare'] or s['obligations']!=255 or any(s[k] for k in ('managed_pages','pte_writes','hardware_ownership')):raise ValueError('Bootstrap false authority/policy')
    if s['prepared']>s['issued'] or s['issued']>5 or s['commands']>2 or s['pci_samples']>4 or s['pci_reads']>36 or s['guards']>20000 or s['alias_checks']>300000 or s['cache_checks']>20000:
        raise ValueError('Bootstrap owner counters')
    if retire['stage']>3:raise ValueError('Bootstrap retirement phase')
    if r['stage']>10 or r['reason']&~511 or any(r[k]>1 for k in ('complete','registered','retained','uncertain','epoch','preserved')) or r['reads']>10000 or r['writes']>103 or r['proofs']>20000 or r['changed']>=1<<32 or r['restored']>=1<<32:
        raise ValueError('Bootstrap reset range')
    if any(b[k]>1 for k in ('owner','provider','lifetime','post_parent','post_object')) or b['display_samples']>2 or max(b['display_before'],b['display_after'])>39 or max(b['pci'+str(n)] for n in range(4))>9:
        raise ValueError('Bootstrap binding range')
    if [q['i'] for q in fences]!=list(range(32)) or [q['i'] for q in controls]!=list(range(5)) or [q['i'] for q in pulses]!=[0,1]:raise ValueError('Bootstrap fixed vector order/coverage')
    if [(q['n'],q['i']) for q in pci]!=[(n,i) for n in range(4) for i in range(b['pci'+str(n)])] or len(pci)!=s['pci_reads']:
        raise ValueError('Bootstrap PCI vector coverage')
    if [(q['n'],q['p'],q['i']) for q in display]!=[(n,k//13,k%13) for n,count in enumerate((b['display_before'],b['display_after'])) for k in range(count)]:raise ValueError('Bootstrap display vector coverage')
    if any(q[k]>=1<<32 for q in controls for k in ('prepare_first','prepare_final','before','after','cancel_first','cancel_final')) or any(q['raw']>=1<<32 for q in pci+display) or any(q['first']>=1<<32 or q['final']>=1<<32 for q in pulses):raise ValueError('Bootstrap DWORD width')
    kinds=['summary','binding','reset','retire']+(['timing'] if diagnostic else [])+(['maintain'] if s['v']>=3 else [])+(['admission']+['cost']*5 if s['v']>=2 else [])+['fence']*32+['control']*5+['pulse']*2+['pci']*len(pci)+['display']*len(display)
    actual=[l.split(':',1)[0].removeprefix('intel-bootstrap-') for l in lines if not l.startswith('intel-bootstrap-post-')]
    if actual!=kinds:raise ValueError('Bootstrap receipt order')
    view=historical_view(text,seconds=seconds,practical=practical,memory=memory,arena=arena,context=context,range_check=range_check,primary=primary,aux=aux,flush=flush)
    from intel_ring_entry import entry_result,native_entry_result
    entry=(native_entry_result if native else entry_result)(view,seconds=seconds,practical=practical)
    from intel_completion_audit import completion_result
    post_text='\n'.join(l.replace('intel-bootstrap-post-','intel-completion-',1) for l in view.splitlines() if l.startswith(('intel-bootstrap-post-','intel-wake-summary:','intel-clock:')))
    post=completion_result(post_text,seconds=seconds,practical=practical)
    complete=bool(s['complete'])
    stop_lines = [line for line in text.splitlines() if line.startswith('intel-bcs-stop:')]
    stop = None
    if stop_lines:
        if not flush or len(stop_lines) != 1:
            raise ValueError('Unexpected blitter stop receipt')
        stop = fields(stop_lines[0], 'intel-bcs-stop:', ('v','writes','verified','ctl','before','stopped','at_reset','after'),
                      ('ctl','before','stopped','at_reset','after'))
        if (max(stop['writes'],stop['verified']) > 1 or max(stop.values()) > 0xffffffff or
            (stop['writes'] and (stop['ctl'] & 1 or stop['before'] & 0x300 != 0x200)) or
            (r['reset_writes'] and (not stop['writes'] or stop['at_reset'] != stop['before'] | 0x100 or
                                    stop['stopped'] != stop['at_reset'])) or
            (stop['verified'] and (not r['complete'] or stop['after'] != stop['before'])) or
            (complete and not stop['verified'])):
            raise ValueError('Invalid blitter stop or reset readback')
    if complete:
        if maintain is not None and (maintain['complete']!=1 or maintain['end']>r['settle_begin']):raise ValueError('Bootstrap missing safe pre-reset maintenance')
        required=dict(state=2,reason=0,failed=0,blocked=1,returned=1,active=0,epoch_calls=1,prepared=5,issued=5,commands=2,pci_samples=4,pci_reads=36,full_before=1,full_after=1,post_bound=1,clock=0,previous=0)
        if any(s[k]!=v for k,v in required.items()) or s['epoch_after']!=s['epoch_before']+1 or not entry or not entry['summary']['complete'] or not post or not post['summary']['complete']:
            raise ValueError('Bootstrap false completed owner')
        if (r['stage'],r['reason'],r['complete'],r['registered'],r['retained'],r['uncertain'],r['epoch'],r['reset_attempts'],r['reset_writes'],r['reset_completed'],r['cancelled'],r['preserved'],r['clock'],r['clock_previous'],r['clock_reason'])!=(9,0,1,0,0,0,1,2,2,2,5,1,0,0,0):raise ValueError('Bootstrap false reset completion')
        if (b['owner'],b['provider'],b['lifetime'],b['sequence'],b['policy'],b['vendor'],b['device'],b['mmio_bytes'],b['display_samples'],b['display_before'],b['display_after'])!=(1,1,1,1,0x47544231,0x8086,0x1626,0x200000,2,39,39):raise ValueError('Bootstrap source identity')
        if b['root']!=s['root'] or not b['root'] or b['root']&4095 or b['epoch']!=s['epoch_before'] or b['prepare']!=s['begin'] or b['mmio']&0xffffff or not b['mmio'] or not 0<b['aperture_bytes']<=1<<32 or b['aperture_bytes']&4095:raise ValueError('Bootstrap original resource')
        h=entry['owner']['summary'];e=entry['summary'];p=post['summary'];w=entry['wake']['summary']
        if stop and (any(row['c3'] != stop['before'] for row in entry['post']['contexts']) or
                     any(row['w3'] != stop['ctl'] for row in entry['post']['rows'] if row['engine'] == 4) or
                     any(row['c3'] != stop['after'] for row in post['contexts'])):
            raise ValueError('Blitter stop differs from the independent queue census')
        if diagnostic:
            if not s['end']<=timing['gt_end']<=timing['hda_end']<=timing['outer_end']<=w['end_us'] or any(timing[k+'_end']<b[k] or timing[k+'_end']-b[k]>policy[k] for k in ('gt','hda','outer')):raise ValueError('Bootstrap seconds parent closing clocks')
        if b['outer']!=h['origin'] or b['hda']!=h['hda_begin'] or not b['outer']<=b['hda']<=b['gt']<=e['source_begin']:
            raise ValueError('Bootstrap original parent clocks')
        if b['post_epoch']!=s['epoch_after'] or b['post_parent']!=1 or b['post_object']!=1 or s['epoch_before']!=e['epoch_after'] or not 0<e['end']<=s['begin']<=r['begin']<r['end']==r['last']<=p['begin_us']<p['end_us']<=s['end']==s['last']<=w['end_us']:
            raise ValueError('Bootstrap original/new epoch order')
        if not b['outer']<=b['hda']<=b['gt']<=b['prepare'] or any(s['end']-b[k]>cap for k,cap in [(k,policy[k]) for k in ('prepare','gt','hda','outer')]):raise ValueError('Bootstrap original deadline')
        if retire['stage']!=3 or not p['end_us']<=retire['guard_begin']<=retire['guard_end']==retire['return_begin']<=retire['return_end']==retire['final_begin']<=retire['final_end']==s['end']:raise ValueError('Bootstrap complete guarded/returned/final clocks')
        if not r['begin']<=r['settle_begin']+50<=r['settle_end']<=r['end'] or r['settle_end']-r['settle_begin']>10000:raise ValueError('Bootstrap settling')
        changed=0
        for q in fences:
            v=q['before'];i=q['i']
            if v&~0xfffff7fffffff003 or q['final']!=v or q['after'] not in (0,v):raise ValueError('Bootstrap fence restoration')
            if v&1:
                low=v&0xfffff000;high=(v>>32)&0xfffff000
                if low>high or high+4096>b['aperture_bytes'] or not(v&2) and (((v>>32)&2047)+1)&3:raise ValueError('Bootstrap fence geometry')
                for old in fences[:i]:
                    x=old['before']
                    if x&1 and low<((x>>32)&0xfffff000)+4096 and (x&0xfffff000)<high+4096:raise ValueError('Bootstrap overlapping fences')
            if q['after']!=v:changed|=1<<i
        count=changed.bit_count()
        if (r['changed'],r['restored'],r['writes'],r['reads'])!=(changed,changed,7+3*count,308+2*count+sum(q['polls']-1 for q in pulses)+sum(q['cancel_polls']-1 for q in controls)):
            raise ValueError('Bootstrap derived reset counts')
        if admission:
            if s['guards']<r['proofs'] or admission['active'] or admission['rejected'] or admission['stage']!=5 or admission['consumed']!=r['reads'] or admission['issued']!=r['proofs']:raise ValueError('Bootstrap completed admission lifetime')
            if any(q['calls']!=s['guards'] for q in cost) or sum(q['total'] for q in cost)>s['end']-s['begin']:raise ValueError('Bootstrap completed cost accounting')
        if s['native_reads']!=r['reads']+5+sum(q['prepare_polls'] for q in controls) or s['native_writes']!=r['writes']+5+(stop['writes'] if stop else 0) or s['alias_checks']!=15*s['guards'] or s['cache_checks']!=s['guards'] or r['proofs']<2*(r['reads']+r['writes']):raise ValueError('Bootstrap native adapter counts')
        for q in controls:
            if not 1<=q['prepare_polls']<=256 or not 1<=q['cancel_polls']<=256 or q['prepare_final']&65535!=3 or q['before']&65535!=3 or q['after']&65535 or q['cancel_final']&3 or q['prepare_first']&~0xffff0003 or q['cancel_first']&~0xffff0003:
                raise ValueError('Bootstrap control endpoints')
        if any(not 1<=q['polls']<=256 or q['final'] or q['first'] not in (0,1) for q in pulses):raise ValueError('Bootstrap reset acknowledgement')
        matrix=[[q['raw'] for q in pci if q['n']==n] for n in range(4)]
        if any(b['pci'+str(n)]!=9 for n in range(4)) or [row[1]&65535 for row in matrix]!=[7,3,3,7] or matrix[0]!=matrix[3] or matrix[1]!=matrix[2] or any(matrix[1][i]!=(matrix[0][i]&(~4 if i==1 else 0xffffffff)) for i in range(9)):raise ValueError('Bootstrap requester restoration')
        from intel_gpu_hold import _prefix
        if not all(_prefix(row) for row in matrix) or any(row[8] for row in matrix):raise ValueError('Bootstrap requester exact AF profile')
        planes=[[q['raw'] for q in display if q['n']==n] for n in range(2)]
        if planes[0]!=planes[1] or any(v==0xffffffff for v in planes[0]):raise ValueError('Bootstrap display preservation')
        for pipe in range(3):
            old=entry['readers'][pipe]['words'];sample=planes[0][pipe*13:(pipe+1)*13]
            if [sample[i] for i in (0,1,2,5,6,7,8,9,10)]!=old:raise ValueError('Bootstrap inherited programmed/live/stereo source')
        if any(p[k] for k in ('enabled','pending','flip_wait','tlb_pending')):raise ValueError('Bootstrap post-reset source not quiet')
        if native:
            from intel_display_backing import backing_result
            from intel_ggtt_window import window_result
            backing=backing_result(view);window=window_result(view)
            if not backing or [q['raw'] for q in sorted(backing['rows']['fence'],key=lambda q:q['index'])]!=[q['before'] for q in fences]:raise ValueError('Bootstrap original native fence vector')
            if not window or b['aperture_bytes']!=window['summary']['aperture_bytes'] or b['mmio']!=window['summary']['mmio']:raise ValueError('Bootstrap native resources')
    elif s['state']==2 or s['returned'] and s['active'] or (s['returned'] or s['active'] or r['complete']) and not s['failed']:
        raise ValueError('Bootstrap incomplete owner retirement')
    owner_complete=complete
    complete=bool(owner_complete and entry and entry['complete'] and post and post['observation_complete'])
    if entry and entry['wake']['summary']['complete'] and not complete:raise ValueError('Bootstrap false enclosing completion')
    memory_proof=None
    if memory:
        from intel_gt_memory import memory_result
        memory_proof=memory_result(text,s,b,r,retire,post,view,complete)
    elif any(l.startswith('intel-memory-') for l in text.splitlines()):raise ValueError('Foreign memory receipt')
    arena_proof=None
    if arena:
        from intel_gt_arena import arena_result
        arena_proof=arena_result(text,s,b,memory_proof,entry,complete)
        complete=bool(complete and arena_proof['complete'])
    elif any(l.startswith('intel-arena-') for l in text.splitlines()):raise ValueError('Foreign arena receipt')
    context_proof=None
    if context:
        from intel_bcs_context import context_result
        context_proof=context_result(text,arena_proof,complete)
        complete=bool(complete and context_proof['complete'])
    elif any(l.startswith('intel-bcs-context:') for l in text.splitlines()):raise ValueError('Foreign context receipt')
    range_proof=None
    if range_check:
        from intel_gt_range import range_result
        range_proof=range_result(text,s,r,memory_proof,retire,owner_complete,post)
    elif any(l.startswith('intel-range-') for l in text.splitlines()):raise ValueError('Foreign GGTT range receipt')
    primary_proof=None
    if primary:
        from intel_gt_primary import primary_result,verify_primary_backing
        primary_proof=primary_result(text,s,range_proof,retire,display)
        if native and owner_complete:
            verify_primary_backing(backing,range_proof,primary_proof)
    elif any(l.startswith('intel-primary-') for l in text.splitlines()):raise ValueError('Foreign primary receipt')
    aux_proof=None
    if aux:
        from intel_gt_aux import auxiliary_result
        aux_proof=auxiliary_result(text,s,primary_proof['summary'])
    elif any(l.startswith('intel-aux-') for l in text.splitlines()):raise ValueError('Foreign auxiliary display receipt')
    cold_rows = [line for line in text.splitlines() if line.startswith("intel-air-rc6:")]
    if cold_rows and not (flush and stop):
        raise ValueError("Unexpected active panel RC6 observation")
    flush_proof=None
    if flush:
        from intel_gt_flush import flush_result
        flush_proof=flush_result(text,s,r,range_proof['summary'],primary_proof['summary'],retire,memory_proof['summary'],range_proof.get('widi_messages'))
        if stop:
            if flush_proof['summary']['v'] != 9:
                raise ValueError('Blitter stop lacks a compatible baseline')
            if stop['verified'] and (not flush_proof.get('stop_messages') or
                                     len(flush_proof.get('prepare_messages', [])) != 5):
                raise ValueError('Missing messages during verified blitter stop')
            flush_proof['bcs_stop'] = stop
        if cold_rows:
            from intel_gt_flush import cold_rc6_result
            flush_proof['cold_rc6'] = cold_rc6_result(
                cold_rows, b, timing, entry['wake']['summary'] if entry else None,
                range_proof.get('efi_handoff', {}).get('display', []))
        cold = flush_proof.get('cold_rc6', {})
        has_change = any(line.startswith(('intel-idle-change:', 'intel-idle-engine:')) for line in text.splitlines())
        if has_change or cold.get('version') == 3:
            from intel_gt_flush import idle_change_result
            if cold and cold['version'] != 3:
                raise ValueError('MAX_IDLE trial sample version')
            flush_proof['idle_change'] = idle_change_result(text, flush_proof.get('idle_policy', []),
                                                           cold, timing, bool(entry and entry['wake']['summary']['complete']))
    elif any(l.startswith(('intel-flush-','intel-l3-power:','intel-smi-guard:','intel-widi-','intel-slot-widi:','intel-slot-reopened:','intel-slot-queues:','intel-command-slots:')) for l in text.splitlines()):raise ValueError('Foreign GT flush receipt')
    return dict(version=s['v'],diagnostic_seconds=seconds,timing=timing,diagnostic_complete=bool(diagnostic and native and complete and entry['native_complete']),maintain=maintain,admission=admission,cost=cost,owner_complete=owner_complete,complete=complete,native_complete=bool(not diagnostic and native and complete and entry['native_complete']),summary=s,
                binding=b,reset=r,retire=retire,fences=fences,controls=controls,pulses=pulses,pci=pci,display=display,entry=entry,post=post,
                hardware_ownership=False,managed_pages=0,physical_tearfree=False,
                scope='one original full-GT reset/fence restoration/new engine epoch; DMA/cache/all-reader authority unproved',**({'diagnostic_practical':True} if practical else {}),**({'diagnostic_memory':True,'memory':memory_proof} if memory else {}),**({'diagnostic_arena':True,'arena':arena_proof} if arena else {}),**({'diagnostic_context':True,'context':context_proof} if context else {}),**({'diagnostic_range':True,'range':range_proof} if range_check else {}),**({'diagnostic_primary':True,'primary':primary_proof} if primary else {}),**({'diagnostic_aux':True,'aux':aux_proof} if aux else {}),**({'diagnostic_flush':True,'flush':flush_proof} if flush else {}))


def verify_bootstrap_checkpoint(pre,text,*,native=False):
    from intel_ring_entry import verify_entry_checkpoint
    seconds=any(l.startswith('intel-bootstrap-summary: v=4 ') for l in text.splitlines())
    flush=any(l.startswith('intel-bootstrap-summary: v=12 ') for l in text.splitlines())
    aux=flush or any(l.startswith('intel-bootstrap-summary: v=11 ') for l in text.splitlines())
    primary=aux or any(l.startswith('intel-bootstrap-summary: v=10 ') for l in text.splitlines())
    range_check=primary or any(l.startswith('intel-bootstrap-summary: v=9 ') for l in text.splitlines())
    context=any(l.startswith('intel-bootstrap-summary: v=8 ') for l in text.splitlines())
    arena=context or any(l.startswith('intel-bootstrap-summary: v=7 ') for l in text.splitlines())
    memory=range_check or arena or any(l.startswith('intel-bootstrap-summary: v=6 ') for l in text.splitlines())
    practical=memory or any(l.startswith('intel-bootstrap-summary: v=5 ') for l in text.splitlines())
    for value in (pre,text):
        boots=re.findall(r'^laptop-capture: boot options=(.*)$',value,re.M)
        if len(boots)!=1:raise ValueError('Bootstrap boot provenance')
        bootstrap_recipe(boots[0],seconds=seconds,practical=practical,memory=memory,arena=arena,context=context,range_check=range_check,primary=primary,aux=aux,flush=flush)
        if value.count('init: deferred=gt-bootstrap-preinit;')!=1 or 'init: started pid=' in value:raise ValueError('Bootstrap preinit provenance')
    if any(l.startswith(('intel-bootstrap-','intel-range-','intel-primary-','intel-aux-','intel-flush-','intel-l3-power:','intel-smi-guard:','intel-slot-widi:','intel-widi-','intel-slot-reopened:','intel-slot-queues:','intel-command-slots:')) for l in pre.splitlines()):raise ValueError('Bootstrap late prerequisite checkpoint')
    verify_entry_checkpoint(historical_view(pre,seconds=seconds,practical=practical,memory=memory,arena=arena,context=context,range_check=range_check,primary=primary,aux=aux,flush=flush),historical_view(text,seconds=seconds,practical=practical,memory=memory,arena=arena,context=context,range_check=range_check,primary=primary,aux=aux,flush=flush),native=native,seconds=seconds,practical=practical)
    if not bootstrap_result(text,native=native):raise ValueError('Bootstrap missing final receipt')
    return True
