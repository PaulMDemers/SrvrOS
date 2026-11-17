"""Private CPU context-image lifetime, not an executable GPU context receipt."""
from intel_gt_bootstrap import fields
KEYS='v initialized verified held erased fault physical root descriptor begin end erase_begin erase_end ring context completion aperture fence_begin fence_end nonce id vendor device mapped submissions ownership'.split()
HEX='physical root descriptor ring context completion nonce vendor device'.split()
def context_result(text,arena,parent_complete):
    lines=[l for l in text.splitlines() if l.startswith('intel-bcs-context:')]
    if len(lines)!=1:raise ValueError('Context image receipt coverage')
    c=fields(lines[0],'intel-bcs-context:',KEYS,HEX)
    if any(c[k]>1 for k in ('initialized','erased','fault')) or c['verified']>4 or c['held']>2 or c['erased']>c['initialized'] or any(c[k] for k in ('mapped','submissions','ownership')):
        raise ValueError('Context image prefix/false authority')
    if c['initialized']:
        if (c['ring'],c['context'],c['completion'],c['aperture'],c['fence_begin'],c['fence_end'],c['nonce'],c['id'],c['vendor'],c['device'])!=(0x2200000,0x2201000,0x2203000,0x10000000,0,0x2200000,0x435458494d414745,1,0x8086,0x1626):
            raise ValueError('Context image prospective layout')
        a=arena['summary']
        if c['physical']!=a['physical'] or c['root']!=a['root'] or c['descriptor']!=0x000000010220100d:
            raise ValueError('Context original CPU arena binding')
        if not a['storage_begin']<=c['begin']<=c['end']<=a['storage_end']:
            raise ValueError('Context initialization outside CPU preparation')
    complete=bool(parent_complete and c['initialized'] and c['verified']==4 and c['held']==2 and c['erased'] and not c['fault'])
    if complete:
        a=arena['summary']
        if not arena['complete'] or not a['close_begin']<=c['erase_begin']<=c['erase_end']<=a['close_end']:
            raise ValueError('Context erase before actual parent retirement')
    if parent_complete and not complete:raise ValueError('Context false enclosing completion')
    return dict(summary=c,complete=complete,cpu_image_only=True,hardware_ownership=False,physical_tearfree=False)
