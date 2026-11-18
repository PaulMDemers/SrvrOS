"""Original full-GT epoch child: remapper/GGTT operations, no GPU grant.

Wire6 is isolated from legacy source graphs and wire1--5. Raw evidence remains
unchanged; these records cannot discharge pipeline/cache/all-reader obligations.
"""
import base64,re,struct
from intel_cache import SCHEMAS as CACHE_SCHEMAS,_snapshot,_page
from intel_gt_bootstrap import fields
SCHEMAS={'summary': ['state', 'reason', 'complete', 'records', 'object', 'owner', 'child', 'borrows', 'returns', 'root', 'epoch', 'begin', 'end', 'last', 'maps', 'retired', 'guards', 'reads', 'writes', 'cache_checks', 'alias_checks', 'resource'], 'vtd': ['state', 'reason', 'complete', 'busy', 'retained', 'begin', 'end', 'last', 'reads', 'guards', 'attempts', 'writes', 'polls', 'uncertain', 'initial', 'final', 'clock', 'at', 'previous'], 'ggtt': ['state', 'reason', 'complete', 'busy', 'retained', 'begin', 'end', 'last', 'reads', 'guards', 'attempts', 'writes', 'initial', 'posted', 'uncertain', 'clock', 'at', 'previous'], 'maintain': ['i', 'begin', 'end', 'phase', 'before', 'after'], 'alias': ['i', 'physical', 'va', 'mapped', 'pages', 'leaves', 'pat_index', 'writable', 'user', 'no_execute', 'leaf_sizes', 'mtrr', 'pat', 'effective'], 'context': ['n', 'i', 'raw'], 'binding': ['owner', 'provider', 'lifetime', 'vtd_source', 'ggtt_source', 'root', 'epoch', 'sequence', 'hash', 'unit', 'prepare', 'gt', 'hda', 'outer', 'mmio', 'mmio_bytes', 'va']}

SCHEMAS.update(cache=CACHE_SCHEMAS['sample'].split()[1:],variable=CACHE_SCHEMAS['variable'].split()[1:],fixed=CACHE_SCHEMAS['fixed'].split()[1:])


def memory_result(text,parent,binding,reset,retire,initial,view,parent_complete):
    lines=[l for l in text.splitlines() if l.startswith('intel-memory-')]
    allowed=set(SCHEMAS)|{'polls','post-row','post-context','post-summary'}
    if any(l.split(':',1)[0].removeprefix('intel-memory-') not in allowed for l in lines):raise ValueError('Memory unknown receipt')
    def rows(kind):
        return [fields(l,'intel-memory-'+kind+':',['v',*SCHEMAS[kind]]) for l in lines if l.startswith('intel-memory-'+kind+':')]
    cached=rows('cache')
    if len(cached)!=1 or cached[0]['count']>16 or cached[0]['fixed']>11:raise ValueError('Memory CPU cache coverage')
    if any(cached[0][k]>0xffffffff for k in ('signature','features','vendor0','vendor1','vendor2')):raise ValueError('Memory CPU word width')
    values={}
    for kind in SCHEMAS:
        q=rows(kind)
        count=cached[0]['count'] if kind=='variable' else cached[0]['fixed'] if kind=='fixed' else 2 if kind in ('maintain','alias') else 16 if kind=='context' else 1
        if len(q)!=count:raise ValueError('Memory '+kind+' coverage')
        values[kind]=q if kind in ('maintain','alias','context','variable','fixed') else q[0]
    order=['summary','vtd','ggtt']+['maintain']*2+['alias']*2+['context']*16+['binding','cache']+['variable']*cached[0]['count']+['fixed']*cached[0]['fixed']+['polls']
    actual=[l.split(':',1)[0].removeprefix('intel-memory-') for l in lines if not l.startswith('intel-memory-post-')]
    if actual!=order:raise ValueError('Memory receipt order')
    polls=[l for l in lines if l.startswith('intel-memory-polls:')]
    if len(polls)!=1 or not re.fullmatch(r'intel-memory-polls: v=1 b=[A-Za-z0-9+/]{1366}==',polls[0]):raise ValueError('Memory poll encoding')
    encoded=polls[0].split(' b=',1)[1]
    raw=base64.b64decode(encoded,validate=True)
    if len(raw)!=1024 or base64.b64encode(raw).decode()!=encoded:raise ValueError('Memory poll bytes')
    observed=list(struct.unpack('<256I',raw));values['polls']=observed
    s,v,g,b=(values[k] for k in ('summary','vtd','ggtt','binding'))
    if s['state']>3 or s['reason']&~511 or any(s[k]>1 for k in ('complete','records','object','owner','child')) or not 0<=s['returns']<=s['borrows']<=1 or not 0<=s['retired']<=s['maps']<=2 or s['resource']>2:raise ValueError('Memory owner range')
    if s['complete']!=(s['state']==2) or s['writes']>2 or s['reads']>1840 or s['guards']>20000 or s['alias_checks']>40000 or s['cache_checks']>40000:raise ValueError('Memory counters')
    if [q['i'] for q in values['maintain']]!=[0,1] or [q['i'] for q in values['alias']]!=[0,1] or [(q['n'],q['i']) for q in values['context']]!=[(n,i) for n in range(2) for i in range(8)]:raise ValueError('Memory vector coverage/order')
    if v['polls']>256 or any(observed[v['polls']:]):raise ValueError('Memory poll tail')
    from intel_completion_audit import completion_result
    post_text='\n'.join(l.replace('intel-memory-post-','intel-completion-',1) for l in view.splitlines() if l.startswith(('intel-memory-post-','intel-wake-summary:','intel-clock:')))
    post=completion_result(post_text,practical=True);values['post']=post
    if s['complete']:
        expected=dict(state=2,reason=0,records=1,object=1,owner=1,child=0,borrows=1,returns=1,maps=2,retired=2,writes=2)
        if any(s[k]!=x for k,x in expected.items()) or s['resource'] not in (1,2):raise ValueError('Memory false completed child')
        if not parent['begin']<=reset['end']<=s['begin']<=v['begin']<v['end']==v['last']<=g['begin']<g['end']==g['last']<=s['end']==s['last']<=parent['last']:raise ValueError('Memory original clock order')
        if (s['root'],s['epoch'])!=(binding['root'],parent['epoch_after']):raise ValueError('Memory new epoch owner')
        if any(b[k]!=1 for k in ('owner','provider','lifetime','vtd_source','ggtt_source','sequence')) or (b['root'],b['epoch'],b['hash'],b['unit'],b['mmio'],b['mmio_bytes'],b['va'])!=(s['root'],s['epoch'],0x722775ad66626863,0xfed90000,binding['mmio'],0x200000,0xffffc001f5180000):raise ValueError('Memory source/resource binding')
        from intel_gt_timing import PRACTICAL
        if any(b[k]!=binding[k] or s['end']<b[k] or s['end']-b[k]>PRACTICAL[k] for k in ('prepare','gt','hda','outer')):raise ValueError('Memory original deadlines')
        if (v['state'],g['state'])!=(5,5):raise ValueError('Memory protocol states')
        for q in (v,g):
            if q['complete']!=1 or any(q[k] for k in ('reason','busy','retained','uncertain','clock','at','previous')) or q['attempts']!=q['writes'] or q['writes']!=1:raise ValueError('Memory operation not complete')
        if not 1<=v['polls']<=256 or (v['reads'],v['guards'])!=(43+7*v['polls'],45+8*v['polls']) or v['initial'] not in (0x0200000000000000,0x1203000000000000) or v['final']!=0x1203000000000000:raise ValueError('Memory remapper completion')
        if observed[v['polls']-1]!=0x12030000 or any(x&~0x06000000!=0x90030000 for x in observed[:v['polls']-1]):raise ValueError('Memory remapper prefix')
        if (g['reads'],g['guards'],g['initial'],g['posted'])!=(2,4,0,0) or s['reads']!=v['reads']+g['reads']:raise ValueError('Memory GGTT posting sequence')
        # Every core guard + each independently guarded load/store, plus two
        # fresh map admissions and two retire admissions. Access rechecks add
        # a second alias walk; policy checks bracket each map and retirement.
        own_guards=v['guards']+g['guards']+2*(s['reads']+s['writes'])+4
        if (s['guards'],s['cache_checks'],s['alias_checks'])!=(own_guards,own_guards+4,own_guards+s['reads']+s['writes']+2):raise ValueError('Memory actual alias/cache accounting')
        expected_context=[0,0x10,0x01c0000c40660462,0x7e1ff0505e,0,0x0800000000000000,0,0]*2
        if [q['raw'] for q in values['context']]!=expected_context:raise ValueError('Memory remapper policy preservation')
        cache=values['cache']
        if cache['sample'] or cache['status']!=1 or not parent['begin']<=cache['begin_us']<cache['end_us']<=reset['begin'] or any(q['sample'] for q in values['variable']+values['fixed']):raise ValueError('Memory original CPU cache context')
        regions=_snapshot(cache,values['variable'],values['fixed'])
        for i,a in enumerate(values['alias']):
            if tuple(a[k] for k in ('physical','va','mapped','pages','leaves','pat_index','writable','user','no_execute','leaf_sizes','effective'))!=((0xfed90000 if i==0 else binding['mmio']+0x101000),b['va']+i*4096,0,1,1,2,1,0,1,1,0) or a['pat'] not in (0,7) or a['mtrr'] not in (0,1,4,5,6):raise ValueError('Memory UC alias retirement')
            if _page(cache,regions,a['physical'],2)!=(a['mtrr'],a['pat'],a['effective']):raise ValueError('Memory CPU cache decode disagreement')
        m0,m1=values['maintain']
        for m in (m0,m1):
            if not s['begin']<=m['begin']<=m['end']<=s['end'] or not 0<m['phase']<=m['begin'] or m['end']-m['phase']>PRACTICAL['gt_phase'] or m['after']!=m['before']+1:raise ValueError('Memory maintained original GT lease')
        # The range child runs after this census while the same owner is held.
        # A later failure can retain the outer lease while the raw memory
        # census still contains both independently checked samples.
        retained_range_parent=parent['v']==9 and not parent_complete and post and post['summary']['complete']
        if not post or not (post['observation_complete'] or retained_range_parent):raise ValueError('Memory missing post-operation census')
        p=post['summary'];first=initial['summary']
        if not m0['end']<=first['begin_us']<first['end_us']<=v['begin'] or not g['end']<=p['begin_us']<p['end_us']<=m1['begin'] or any(p[k] for k in ('enabled','pending','flip_wait','tlb_pending')):raise ValueError('Memory census clock/quiet order')
        # Compare exact words/context, retaining distinct actual timestamps.
        if post['rows']!=initial['rows'] or post['contexts']!=initial['contexts']:raise ValueError('Memory post-operation source preservation')
    if parent_complete and (not s['complete'] or s['end']>retire['guard_begin']):raise ValueError('Memory parent returned before child')
    return values
