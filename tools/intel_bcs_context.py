"""Private CPU context-image lifetime, not an executable GPU context receipt."""
from intel_gt_bootstrap import fields
KEYS='v initialized verified held erased fault physical root descriptor begin end erase_begin erase_end ring context completion aperture fence_begin fence_end nonce id vendor device mapped submissions ownership'.split()
HEX='physical root descriptor ring context completion nonce vendor device'.split()
def context_result(text,arena,parent_complete):
    lines=[l for l in text.splitlines() if l.startswith('intel-bcs-context:')]
    if len(lines)!=1:raise ValueError('Context image receipt coverage')
    protected = 'v=3' in lines[0].split()
    independent = protected or 'v=2' in lines[0].split()
    keys = KEYS + ['pte_reads', 'reserved', 'released'] if independent else KEYS
    if protected:
        keys=keys+['sealed','reopened','ro_mask','alias_checks','traffic_samples']
    version=3 if protected else 2 if independent else 1
    c=fields(lines[0],'intel-bcs-context:',keys,HEX,versions=(version,))
    registers=['request','vcs0_refs','vcs0_prefetch','vcs1_refs','vcs1_prefetch']
    traffic=[]
    for line in text.splitlines():
        if not line.startswith('intel-context-traffic:'):
            continue
        row=fields(line,'intel-context-traffic:', ['v','sample',*registers],registers)
        if row['sample']!=len(traffic) or any(row[k]>=0xffffffff for k in registers):
            raise ValueError('Invalid context traffic sample')
        traffic.append(row)
    if len(traffic)!=c.get('traffic_samples',0) or len(traffic)>2:
        raise ValueError('Context traffic coverage')
    if protected:
        sealed,reopened=c['sealed'],c['reopened']
        if (not 0<=reopened<=sealed<=5 or (reopened and sealed!=5) or
                c['ro_mask']!=(1<<(sealed-reopened))-1 or
                (traffic and (sealed!=5 or c['alias_checks']<3)) or
                (c['pte_reads'] and not traffic) or
                (len(traffic)==2 and c['pte_reads']!=30) or
                (reopened and len(traffic)!=2) or (c['pte_reads'] and sealed!=5) or (reopened and c['pte_reads']!=30) or
                not 0<=c['alias_checks']<=6 or
                (c['pte_reads'] and c['alias_checks']<3) or
                (c['pte_reads']>15 and c['alias_checks']!=6)):
            raise ValueError('Context engine write exclusion prefix')
    if independent and not (0 <= c['released'] <= c['erased'] <= c['initialized'] <= c['reserved'] <= 1):
        raise ValueError('Context address reservation lifetime')
    if any(c[k]>1 for k in ('initialized','erased','fault')) or c['verified']>4 or c['held']>2 or c['erased']>c['initialized'] or any(c[k] for k in ('mapped','submissions','ownership')):
        raise ValueError('Context image prefix/false authority')
    if independent and not (15*c['held'] <= c['pte_reads'] <= min(30,15*(c['held']+1))):
        raise ValueError('Context live translation read coverage')
    if c['initialized']:
        ring = 0x2201000 if independent else 0x2200000
        if independent:
            from intel_ggtt_window import window_result
            window = window_result(text)
            if not window or not window['readout_complete']:
                raise ValueError('Missing independent command address capture')
            entries = window['vectors'][0][:5]
            if (entries != window['vectors'][1][:5] or len(set(entries)) != 5 or
                    any(e >> 39 or e & 4095 != 1 or e < 4096 or
                        0 <= (e & ~4095) - c['physical'] < 16384 for e in entries)):
                raise ValueError('Aliased or invalid command backing')
        if (c['ring'],c['context'],c['completion'],c['aperture'],c['fence_begin'],c['fence_end'],c['nonce'],c['id'],c['vendor'],c['device'])!=(ring,ring+4096,ring+3*4096,0x10000000,0,0x2200000,0x435458494d414745,1,0x8086,0x1626):
            raise ValueError('Context image prospective layout')
        a=arena['summary']
        if c['physical']!=a['physical'] or c['root']!=a['root'] or c['descriptor']!=(1 << 32 | (ring+4096) | 0x0d):
            raise ValueError('Context original CPU arena binding')
        if not a['storage_begin']<=c['begin']<=c['end']<=a['storage_end']:
            raise ValueError('Context initialization outside CPU preparation')
    complete=bool(parent_complete and c['initialized'] and c['verified']==4 and c['held']==2 and c['erased'] and not c['fault'])
    if complete:
        a=arena['summary']
        if protected and (c['alias_checks'],c['sealed'],c['reopened'],c['ro_mask'])!=(6,5,5,0):
            raise ValueError('Context incomplete engine permission restoration')
        if independent and not c['released']:
            raise ValueError('Context slot reservation still held')
        if not arena['complete'] or not a['close_begin']<=c['erase_begin']<=c['erase_end']<=a['close_end']:
            raise ValueError('Context erase before actual parent retirement')
    if parent_complete and not complete:raise ValueError('Context false enclosing completion')
    return dict(summary=c,traffic=traffic,complete=complete,independent_layout=bool(independent and c['initialized']),cpu_image_only=True,hardware_ownership=False,physical_tearfree=False)
