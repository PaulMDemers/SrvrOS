"""Source-bound HDA/GPU composition; no DMA/cache or address-transfer grant."""
import base64
import re
import struct

SUMMARY = 'v entry admission state reason pinned retained complete gpu_complete origin begin end last clock_at clock_prev clock_cause checks attached hda_state hda_reason hda_epoch hda_begin hda_end hda_phases hda_checks hda_attempts hda_writes hda_pins hda_retained hda_complete hda_clock_at hda_clock_prev hda_clock_cause root_maps root_retired borrows returns inspection cache obligations assurances pte_writes managed_pages hardware_ownership'.split()
CLAIM = 'v state reason pinned origin owner_origin begin end last epoch expected clock_at clock_prev complete obligations assurances pte_writes managed_pages hardware_ownership'.split()
ENDPOINT = 'id class_rev subsystem command flags pm state bar bar_high reserved'.split()
PHASE = 'maps retired retained samples reads stable run dma reason begin last end complete'.split()
RECIPE = '''srvros.storage=ram srvros.capture=intel-display srvros.intel-backing=read-only
srvros.intel-cache=read-only srvros.intel-window=read-only srvros.intel-handoff=qualify
srvros.intel-gt-preflight=read-only srvros.intel-firmware=read-only srvros.intel-wake=trial
srvros.capture-failure=reboot srvros.intel-engines=read-only srvros.intel-completion=read-only
srvros.intel-takeover=preflight srvros.intel-engines-prepare=request-cancel
srvros.intel-held-requester=read-only srvros.intel-bcs-reset=held-blitter-only
srvros.intel-interval=read-only srvros.intel-rehold=request-cancel srvros.intel-joint=hda-claim'''.split()


def joint_recipe(cmd, *, requester_hold=False,entry_admit=False):
    if not isinstance(cmd, str) or len(cmd) > 2048 or any(c.isspace() and c != ' ' for c in cmd):
        raise ValueError('joint recipe encoding')
    tokens = cmd.split()
    if requester_hold and entry_admit:raise ValueError('conflicting joint consumers')
    expected=22 if requester_hold or entry_admit else 21
    if len(tokens) != expected or len(set(tokens)) != expected or len(cmd) + len(tokens) > 2048:
        raise ValueError('joint exact21 recipe')
    extras = set(tokens) - (set(RECIPE) | ({"srvros.intel-gpu-requester=hold"} if requester_hold else {"srvros.intel-admission=disabled-bcs-v1"} if entry_admit else set()))
    volume = [t for t in extras if re.fullmatch(r'srvros.workspace-volume=[0-9a-f]{8}', t)]
    release = [t for t in extras if re.fullmatch(r'srvros.desktop-release=[0-9a-f]{16}', t)]
    if len(extras) != 2 or len(volume) != 1 or len(release) != 1:
        raise ValueError('joint recipe source/volume')
    return dict(volume=volume[0].split('=')[1], release=release[0].split('=')[1])


def numbers(text, count=None):
    values = text.split(',')
    if count is not None and len(values) != count:
        raise ValueError('joint vector width')
    if any(not re.fullmatch(r'0|[1-9][0-9]{0,19}', v) or int(v) >= 1 << 64 for v in values):
        raise ValueError('joint canonical integer')
    return list(map(int, values))


def fields(line, prefix, names):
    if not line.startswith(prefix + ': '):
        raise ValueError('joint prefix')
    result = {}
    for token in line[len(prefix) + 2:].split():
        key, sep, value = token.partition('=')
        if not sep or key in result:
            raise ValueError('joint duplicate field')
        result[key] = numbers(value, 1)[0]
    if set(result) != set(names) or result['v'] != 1:
        raise ValueError('joint schema')
    return result


def authority(s):
    if s['obligations'] != 255 or any(s[k] for k in ('assurances', 'pte_writes', 'managed_pages', 'hardware_ownership')):
        raise ValueError('joint unsupported authority')


def hda_prefix(samples, reads):
    """Check which initialized fields could have been written by native loads."""
    left = reads
    for sample in samples:
        globals = sample['globals']; raw = sample['raw']
        for i in range(11):
            if left: left -= 1
            elif globals[i]: raise ValueError('joint HDA data past read prefix')
        if sample['streams']:
            if reads < 11: raise ValueError('joint HDA topology before globals')
            count = ((globals[0] >> 12) & 15) + ((globals[0] >> 8) & 15) + ((globals[0] >> 3) & 31)
            if sample['streams'] != count: raise ValueError('joint HDA raw topology')
        for i in range(8):
            w = raw[i * 5:i * 5 + 5]
            for value in (w[0] & 65535, w[0] >> 16, *w[1:]):
                if i < sample['streams'] and left: left -= 1
                elif value: raise ValueError('joint HDA stream past read prefix')
        reads = left
    if left: raise ValueError('joint HDA excess reads')


def hda_phase(line):
    m = re.fullmatch(r'intel-joint-hda: v=1 p=([0-3]) e=([0-9,]+) d=([0-9,]+) b=([A-Za-z0-9+/]+={0,2})', line)
    if not m:
        raise ValueError('joint HDA row')
    e = dict(zip(ENDPOINT, numbers(m[2], 10)))
    p = dict(zip(PHASE, numbers(m[3], 13)))
    try:
        raw = base64.b64decode(m[4], validate=True)
    except ValueError as error:
        raise ValueError('joint HDA base64') from error
    if len(raw) != 416 or base64.b64encode(raw).decode() != m[4]:
        raise ValueError('joint HDA canonical buffer')
    words = struct.unpack('<104I', raw)
    if any(e[k] >= 1 << 32 for k in ENDPOINT) or e['reserved'] > 1:
        raise ValueError('joint endpoint width')
    if any(p[k] > 1 for k in ('maps', 'retired', 'retained', 'stable', 'complete')) or p['retired'] > p['maps'] or p['samples'] > 2 or p['reads'] > 118 or p['reason'] > 7 or p['run'] > 255 or p['dma'] > 7:
        raise ValueError('joint HDA progress')
    samples = [dict(globals=list(words[n:n + 11]), streams=words[n + 11], raw=list(words[n + 12:n + 52])) for n in (0, 52)]
    if p['reads'] and (not p['maps'] or not p['begin']) or p['retained'] and p['retired']:
        raise ValueError('joint HDA load/borrow prefix')
    hda_prefix(samples, p['reads'])
    quiet = False
    if p['complete']:
        if p['reason'] or p['retained'] or p['maps'] != 1 or p['retired'] != 1 or p['samples'] != 2 or not 0 < p['begin'] < p['end'] <= p['begin'] + 200000 or not p['begin'] <= p['last'] <= p['end']:
            raise ValueError('joint HDA completion')
        if e['id'] != 0x160c8086 or e['class_rev'] != 0x04030009 or e['subsystem'] != 0x011b106b or e['command'] not in (2, 6) or e['state'] != 1 or not e['flags'] & 1 or e['pm'] & 3 or e['bar'] != 0xc1610004 or e['bar_high'] or not e['reserved']:
            raise ValueError('joint HDA endpoint')
        streams = samples[0]['streams']
        if not 1 <= streams <= 8 or samples[1]['streams'] != streams or p['reads'] != 2 * (11 + 6 * streams):
            raise ValueError('joint HDA read coverage')
        for s in samples:
            g = s['globals']
            if ((g[0] >> 12) & 15) + ((g[0] >> 8) & 15) + ((g[0] >> 3) & 31) != streams or g[0] & 6 == 6:
                raise ValueError('joint HDA topology')
            if any(v >= (65535 if i == 0 else 255 if i in (5, 8) else 0xffffffff) for i, v in enumerate(g)):
                raise ValueError('joint HDA raw global width')
            if any(s['raw'][streams * 5:]):
                raise ValueError('joint HDA unobserved tail')
            for i in range(streams):
                w = s['raw'][i * 5:i * 5 + 5]
                if w[0] >= 0xffffff or w[0] & 65535 == 65535 or w[0] >> 16 == 255 or w[2] >= 65535 or w[1] == 0xffffffff or max(w[3:]) == 0xffffffff:
                    raise ValueError('joint HDA raw stream width')
        stable = samples[0] == samples[1]
        run = sum(1 << i for i in range(streams) if any(s['raw'][i * 5] & 2 for s in samples))
        dma = (1 if any(s['globals'][5] & 2 for s in samples) else 0) | (2 if any(s['globals'][8] & 2 for s in samples) else 0) | (4 if any(s['globals'][9] & 1 for s in samples) else 0)
        if (p['stable'], p['run'], p['dma']) != (int(stable), run, dma):
            raise ValueError('joint HDA derived state')
        quiet = stable and streams == 3 and all(s['globals'] == [0x3001, 1] + [0] * 9 and s['raw'] == [0x40000, 0, 0, 0, 0] * 3 + [0] * 25 for s in samples)
    return dict(index=int(m[1]), endpoint=e, summary=p, samples=samples, quiet=quiet)


def joint_capture(text):
    summaries = []; phases = []; order = []
    for line in text.splitlines():
        if line.startswith('intel-joint-hda:'):
            p = hda_phase(line); phases.append(p); order.append(('p', p['index']))
        elif line.startswith('intel-joint-summary:'):
            summaries.append(fields(line, 'intel-joint-summary', SUMMARY)); order.append(('s',))
        elif line.startswith('intel-joint-'):
            raise ValueError('unknown joint record')
    if not summaries:
        if phases: raise ValueError('joint missing summary')
        return None
    if len(summaries) != 1 or order != [('p', n) for n in range(len(phases))] + [('s',)] or len(phases) > 4:
        raise ValueError('joint phase order/coverage')
    s = summaries[0]; authority(s)
    if s['entry'] > 1 or s['admission'] > 6 or s['admission'] and not s['entry'] or s['state'] > 5 or s['reason'] > 255 or s['clock_cause'] > 31 or s['hda_state'] > 3 or s['hda_reason'] > 5 or s['hda_clock_cause'] > 15 or s['checks'] > 200000 or s['hda_epoch'] >= 1 << 64:
        raise ValueError('joint range')
    if any(s[k] > 1 for k in ('pinned', 'retained', 'complete', 'gpu_complete', 'attached', 'hda_pins', 'hda_retained', 'hda_complete', 'root_maps', 'root_retired', 'inspection')) or not 0 <= s['hda_writes'] <= s['hda_attempts'] <= 2 or not 0 <= s['returns'] <= s['borrows'] <= 4 or s['root_retired'] > s['root_maps'] or s['cache'] > 22 or s['hda_phases'] > 3 or s['hda_checks'] > 1:
        raise ValueError('joint lifetime progress')
    if s['pinned'] and s['state'] not in (1, 2, 3, 5) or s['hda_pins'] and not s['pinned'] or s['borrows']-s['returns']!=s['inspection'] or s['root_retired'] and s['inspection'] or s['gpu_complete']!=s['complete']:
        raise ValueError('joint pin/borrow lifecycle')
    if sum(p['summary']['maps'] for p in phases) != s['borrows'] or sum(p['summary']['retired'] for p in phases) != s['returns']:
        raise ValueError('joint logical inspection coverage')
    if s['clock_cause'] and not s['reason'] & 4 or not s['clock_cause'] and (s['clock_at'] or s['clock_prev']):
        raise ValueError('joint clock binding')
    if s['hda_clock_cause'] and s['hda_reason'] != 2:
        raise ValueError('joint HDA clock binding')
    if s['complete']:
        if not s['entry'] or s['admission'] or s['state'] != 4 or s['reason'] or s['pinned'] or s['retained'] or not s['gpu_complete'] or not s['attached'] or s['hda_state'] != 2 or s['hda_reason'] or not s['hda_epoch'] or s['hda_phases'] != 3 or s['hda_checks'] != 1 or s['hda_pins'] or s['hda_retained'] or not s['hda_complete'] or s['hda_attempts'] != 2 or s['hda_writes'] != 2 or s['root_maps'] != 1 or s['root_retired'] != 1 or s['borrows'] != 4 or s['returns'] != 4 or s['inspection'] or s['cache'] != 22 or not s['checks'] or s['clock_cause'] or s['hda_clock_cause']:
            raise ValueError('joint false completion')
        if not 0 < s['origin'] <= s['begin'] <= s['hda_begin'] < s['hda_end'] <= s['end'] == s['last'] <= s['origin'] + 500000 or s['end'] > s['hda_begin'] + 400000:
            raise ValueError('joint original deadlines')
        if len(phases) != 4 or not all(p['quiet'] and p['summary']['complete'] for p in phases) or [p['endpoint']['command'] for p in phases] != [6, 2, 2, 6]:
            raise ValueError('joint HDA phase proof')
        previous = s['hda_begin']
        for p in phases:
            if not previous <= p['summary']['begin'] < p['summary']['end'] <= s['hda_end']:
                raise ValueError('joint HDA phase clocks')
            previous = p['summary']['end']
            if any(p['endpoint'][k] != phases[0]['endpoint'][k] for k in ENDPOINT if k != 'command'):
                raise ValueError('joint HDA resource drift')
    return dict(summary=s, phases=phases, scope_complete=bool(s['complete']), complete=False, native_complete=False, hardware_ownership=False, managed_pages=0, physical_tearfree=False)


def claim_capture(text):
    from intel_held_interval import interval_capture
    lines = []; sums = []
    for line in text.splitlines():
        if line.startswith('intel-claim-final-'):
            lines.append(line.replace('intel-claim-final-', 'intel-held-interval-', 1))
        elif line.startswith('intel-claim-summary:'):
            sums.append(fields(line, 'intel-claim-summary', CLAIM))
        elif line.startswith('intel-claim-'):
            raise ValueError('unknown claim receipt')
    if len(sums) != 1: raise ValueError('claim summary coverage')
    s = sums[0]; authority(s)
    if s['state'] > 5 or s['reason'] > 127 or s['pinned'] > 1 or s['complete'] > 1 or s['epoch'] >= 1 << 32:
        raise ValueError('claim range')
    final = interval_capture('\n'.join(lines)) if lines else None
    if bool(final) != bool(s['state']): raise ValueError('claim final coverage')
    if s['complete'] and (s['state'] != 4 or s['reason'] or s['pinned'] or not final or not final['complete'] or not 0 < s['owner_origin'] <= s['origin'] <= s['begin'] < s['end'] == s['last'] <= s['origin'] + 100000 or s['end'] > s['owner_origin'] + 300000):
        raise ValueError('claim false completion/deadline')
    if not s['reason'] & 4 and (s['clock_at'] or s['clock_prev']): raise ValueError('claim unexplained clock')
    return dict(summary=s, final=final)


def joint_result(text):
    r = joint_capture(text)
    if not r: return None
    from intel_engine_rehold import rehold_result
    from intel_gt_wake import wake_result
    from intel_held_interval import interval_capture
    wake = wake_result(text); held = rehold_result(text); claim = claim_capture(text)
    if not wake or wake['summary']['v'] not in (20,21,23,26,27) or not held: raise ValueError('joint image/source graph')
    s = r['summary']; w = wake['summary']; h = held['summary']; c = claim['summary']; opening = interval_capture(text)
    if not opening: raise ValueError('joint opening coverage')
    if not text.index('intel-joint-summary:')<text.index('intel-completion-summary:') or not text.index('intel-held-interval-summary:')<text.index('intel-claim-summary:')<text.index('intel-prepare-summary:'):
        raise ValueError('joint raw record ordering')
    if claim['final'] and not text.index('intel-held-interval-summary:')<text.index('intel-claim-final-row:')<text.index('intel-claim-final-summary:')<text.index('intel-claim-summary:'):
        raise ValueError('joint closing report order')
    if s['pinned'] and w['released'] or s['retained'] and w['released'] or c['pinned'] and (not h['child_retained'] or not s['hda_pins'] or s['hda_attempts'] > 1):
        raise ValueError('joint released live source')
    if s['origin'] and s['origin'] != w['begin_us']: raise ValueError('joint replacement outer origin')
    if h['child_returns'] and not c['complete']:raise ValueError('joint released incomplete claim')
    if c['state']:
        if not s['attached'] or c['origin'] != h['origin'] or c['owner_origin'] != h['owner_origin'] or c['epoch'] != h['epoch']:
            raise ValueError('joint claim source binding')
        window=held['interval']['window']
        if c['origin'] and (not window or not window['readout_complete'] or c['expected']!=window['vectors'][1][0]):raise ValueError('joint claim captured original')
        final = claim['final']; f = final['summary']; a = opening['summary']
        if f['pages'] and (f['origin'], f['epoch_before'], f['epoch_after']) != (a['origin'], a['epoch_before'], a['epoch_after']):
            raise ValueError('joint final original binding')
        if f['begin'] and not c['begin'] <= f['begin'] <= f['end'] <= c['end']:
            raise ValueError('joint closing sample outside claim')
        for observation in (opening, final):
            words = [v for row in observation['rows'] for v in row['words']]
            expected = [c['expected'] >> 32, c['expected'] & 0xffffffff, c['expected'] >> 32] * 2
            drift = [n for n, v in enumerate(words) if v != expected[n]]
            if drift and (drift != [len(words) - 1] or not observation['summary']['reason'] & 32): raise ValueError('joint unexpected original entry')
        if c['complete'] and (not opening['complete'] or not final['complete'] or not c['begin'] <= a['begin'] < a['end'] < f['begin'] < f['end'] <= c['end'] < h['end'] or not h['child_returns']):
            raise ValueError('joint terminal child proof')
        if c['begin'] and (not held['acks'] or c['begin']<held['acks'][0]['end']):raise ValueError('joint claim before ready')
        if len(held['acks'])==2 and (not c['complete'] or c['end']>held['acks'][1]['begin']):raise ValueError('joint re-arm cancellation before claim retirement')
    complete = bool(s['complete'] and c['complete'] and held['complete'] and wake['lease_complete'])
    if w['complete'] and not complete or s['complete'] and not complete: raise ValueError('joint enclosing false completion')
    if complete:
        prep = held['interval']['reset']['prepare']
        p1, p2 = r['phases'][1]['summary'], r['phases'][2]['summary']
        if not p1['end'] <= h['owner_origin'] < h['origin'] <= c['begin'] < c['end'] < prep['summary']['end'] < p2['begin']:
            raise ValueError('joint inhibition/retirement ordering')
        for audio in prep['audio']:
            e = r['phases'][1]['endpoint']; words = audio['words']
            if words[:5] != [e['id'], e['class_rev'] >> 8, 2, e['flags'], e['pm']] or words[9] != e['state']:
                raise ValueError('joint phase-correct GPU HDA preservation')
        if not s['end'] <= w['end_us']:
            raise ValueError('joint report/outer completion order')
    entry=None
    if w['v']==23:
        from intel_entry_admission import entry_capture
        entry=entry_capture(text)
        if not entry:raise ValueError('joint admission missing child')
        if s['complete'] and not entry['complete']:raise ValueError('joint admission false completion')
        complete=complete and entry['complete']
    elif w['v'] in (26,27):
        from intel_ring_reset import entry_capture
        entry=entry_capture(text)
        if not entry or s['complete'] and not entry['complete']:raise ValueError('joint reset entry false completion')
        complete=complete and entry['complete']
    elif 'intel-compose-entry-' in text:raise ValueError('entry receipt on legacy joint image')
    return dict(r, complete=complete, claim=claim, held=held, wake=wake,entry=entry)


def native_hda_context(text):
    from intel_cache import native_cache_result, _snapshot, _page
    cache = native_cache_result(text)
    if not cache or not cache['native_observation_complete']: raise ValueError('joint native cache prerequisite')
    for n, s in enumerate(cache['rows']['sample']):
        regions = _snapshot(s, [x for x in cache['rows']['variable'] if x['sample'] == n], [x for x in cache['rows']['fixed'] if x['sample'] == n])
        d = _page(s, regions, 0xc1610000, 2)
        if d is None or d[0] != 0 or d[2] != 0: raise ValueError('joint HDA independent UC')


def native_joint_result(text):
    r = joint_result(text)
    if not r or not r['complete']: return r
    from intel_gt_wake import native_wake_result
    w = native_wake_result(text)
    if not w or not w['native_wake_complete']: raise ValueError('joint native prerequisites')
    r['native_complete'] = True
    return r


def verify_joint_checkpoint(pre, text, native=False):
    from intel_engine_rehold import verify_rehold_checkpoint
    if any(l.startswith(('intel-joint-', 'intel-claim-')) for l in pre.splitlines()): raise ValueError('joint late checkpoint')
    verify_rehold_checkpoint(pre, text, native=native)
    boots = [[line.split('=', 1)[1] for line in payload.splitlines() if line.startswith('laptop-capture: boot options=')] for payload in (pre, text)]
    if any(len(b) != 1 for b in boots) or boots[0] != boots[1]:
        raise ValueError('joint checkpoint boot provenance')
    from intel_gt_wake import wake_result
    if wake_result(text)['summary']['v'] in (26,27):
        from intel_ring_reset import reset_recipe
        reset_recipe(boots[0][0],requester=wake_result(text)['summary']['v']==27)
    else:
        joint_recipe(boots[0][0],requester_hold=wake_result(text)['summary']['v']==21,entry_admit=wake_result(text)['summary']['v']==23)
    return True
