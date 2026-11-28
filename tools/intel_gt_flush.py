"""Inspect engine and L3 messages in the original full-GT lifetime."""

import base64
import re
import struct

from intel_gt_bootstrap import fields

ENGINES = ("rcs", "vcs0", "vecs", "vcs1", "bcs")
WORDS = (*ENGINES, "l3")
GAM_WORDS = ("gam_go", "gam_eco")
MEDIA_WORDS = ("vcs0_ref", "vcs0_prefetch", "vcs1_ref", "vcs1_prefetch")
POWER_WORDS = ("gam_req", "rc_control", "rc_target", "rc6_threshold")
GAM_GO_MASK = 0x5f  # RCS, VCS0, VCS1, VECS, BCS and WiDi (bit 6).
CPD_POLICY_LOCK = 1 << 31
CPD_GAM_GO_MESSAGES = 1 << 25

SMI_PREFIX = "intel-smi-guard:"
SMI_FAULT = 2048
DRIVER_RANGE_PREFIX = 'intel-driver-range-preflight:'
DRIVER_TRAFFIC_PREFIX = 'intel-driver-range-traffic:'
DRIVER_CLOSE_PREFIX = 'intel-driver-range-close:'
DRIVER_CLOSE_STEPS = (
    'pte_sample_count', 'traffic_alias_retired', 'panel_owner_before', 'panel_alias_before',
    'panel_clock_before', 'panel_reads', 'panel_control', 'panel_programmed_surface',
    'panel_live_surface', 'panel_alias_after', 'panel_owner_after', 'panel_clock_after',
    'panel_frame_delta', 'owner_guard', 'smi_available', 'smi_count', 'closing_clock',
    'first_pte_high_pair', 'second_pte_high_pair', 'pte_high_match', 'pte_low_match',
    'pte_alias_retired', 'positive_elapsed', 'phase_limit',
)


def driver_range_close(line, sample, parent, baseline, frame_before, phase_us):
    panel = ('frame', 'plane', 'programmed', 'live')
    close = fields(line, DRIVER_CLOSE_PREFIX, ('v', 'attempted', 'passed', 'panel_words', *panel, 'smi'), panel)
    attempted, passed, count = (close[k] for k in ('attempted', 'passed', 'panel_words'))
    total = len(DRIVER_CLOSE_STEPS)
    if (not 1 <= attempted <= total or passed not in (attempted, attempted - 1) or
            bool(sample['stable']) != (passed == total) or (passed != total and passed == attempted) or
            not 0 <= count <= 4 or (attempted < 6 and count) or (passed >= 6 and count != 4) or
            (attempted == 6 and passed == 5 and count == 4) or
            any(close[k] >= 1 << 32 for k in (*panel, 'smi')) or any(close[k] for k in panel[count:]) or
            (attempted < 15 and close['smi']) or (attempted >= 15 and close['smi'] != parent['smi1'])):
        raise ValueError('Driver range closing sequence')
    frame_delta = (close['frame'] - frame_before) & 0xffffffff if count else None
    expected = {1: sample['samples'] == 2, 2: not sample['traffic_live'],
                7: close['plane'] == baseline[0], 8: close['programmed'] == baseline[1],
                9: close['live'] == baseline[2], 13: frame_delta is not None and frame_delta <= 8,
                16: close['smi'] == parent['smi0'], 18: sample['a0'] == sample['a2'],
                19: sample['b0'] == sample['b2'], 20: sample['a0'] == sample['b0'],
                21: sample['a1'] == sample['b1'], 22: not sample['live'],
                23: sample['end'] > sample['begin'], 24: sample['end'] - parent['begin'] <= phase_us}
    if any(ok != (step <= passed) for step, ok in expected.items() if step <= attempted):
        raise ValueError('Driver range closing evidence disagrees with its checks')
    return dict(raw=close, checks=dict(zip(DRIVER_CLOSE_STEPS,
                ('passed' if i <= passed else 'failed' if i == attempted else 'not_run'
                 for i in range(1, total + 1)))), frame_delta=frame_delta,
                first_failure=DRIVER_CLOSE_STEPS[passed] if passed < attempted else None)


def driver_range_preflight_result(rows, traffic_rows, close_rows, parent, baseline, frame_before, phase_us):
    if not rows:
        if traffic_rows or close_rows:
            raise ValueError('Driver range preflight missing traffic bracket')
        return None
    suffix = ' authority=none'
    if len(rows) != 1 or not rows[0].endswith(suffix):
        raise ValueError('Driver range preflight authority or record count')
    words = ('a0', 'a1', 'a2', 'b0', 'b1', 'b2')
    keys = ('v', 'slot', 'begin', 'end', 'maps', 'retired', 'reads', 'live', 'samples', 'stable', *words)
    hexkeys = ('live', *words)
    version = re.match(r'intel-driver-range-preflight: v=([1-4]) ', rows[0])
    if not version:
        raise ValueError('Driver range preflight version')
    version = int(version[1])
    if version >= 2:
        keys += ('packets', 'traffic_maps', 'traffic_retired', 'traffic_reads', 'traffic_live')
        hexkeys += ('traffic_live',)
    if version == 4:
        keys += ('traffic0_end', 'pte_end', 'traffic1_end', 'wake_checks', 'wake_us')
    sample = fields(rows[0].removesuffix(suffix), DRIVER_RANGE_PREFIX, keys, hexkeys, versions=(version,))
    maps, retired, reads, count = (sample[key] for key in ('maps', 'retired', 'reads', 'samples'))
    if (sample['slot'] != 16385 or parent['stage'] not in (4, 5) or
            not parent['begin'] <= sample['begin'] <= sample['end'] <= parent['end'] or
            not 0 <= count <= retired <= maps <= 2 or not count * 3 <= reads <= maps * 3 or
            sample['stable'] != (parent['stage'] == 5) or any(sample[key] >= 1 << 32 for key in words)):
        raise ValueError('Driver range preflight scope')
    first = tuple(sample[key] for key in words[:3])
    second = tuple(sample[key] for key in words[3:])
    stable = bool(sample['stable'])
    if stable and ((maps, retired, reads, count) != (2, 2, 6, 2) or sample['live'] or
                   sample['end'] <= sample['begin'] or first != second or first[0] != first[2] or
                   0xffffffff in first):
        raise ValueError('Driver range preflight closing proof')
    report = dict(summary=sample, pte=(first[0] << 32 | first[1]) if stable else None,
                  empty_encoding=first == (0, 0, 0) if stable else None, write_authority=False)
    if version >= 2:
        packets, maps, retired, reads, live = (sample[k] for k in
            ('packets', 'traffic_maps', 'traffic_retired', 'traffic_reads', 'traffic_live'))
        if (packets != len(traffic_rows) or not 0 <= packets <= 2 or
                not packets * 2 <= retired <= maps <= 4 or not packets * 9 <= reads <= 18 or
                (stable and (live or maps != retired)) or
                (packets == 2 and (count, maps, retired, reads, live) != (2, 4, 4, 18, 0))):
            raise ValueError('Driver range preflight traffic bracket')
        keys = ('gam', 'reply', *ENGINES, 'widi', 'l3')
        traffic = []
        for n, line in enumerate(traffic_rows):
            raw = fields(line, DRIVER_TRAFFIC_PREFIX, ('v', 'n', *keys), keys)
            if raw['n'] != n or any(raw[k] >= 0xffffffff for k in keys):
                raise ValueError('Driver range preflight traffic packet')
            traffic.append(dict(raw=raw, engine_ack_bits=sum(1 << i for i, k in enumerate(ENGINES) if raw[k] & 0x20),
                                l3_message=raw['l3'] & 3, widi_go=bool(raw['widi'] & 0x10)))
        report['traffic'] = dict(samples=traffic, bracket_complete=packets == 2 and stable, request_issued=False)
    elif traffic_rows:
        raise ValueError('Driver range preflight unexpected traffic bracket')
    if version >= 3:
        if len(close_rows) != 1:
            raise ValueError('Driver range closing receipt count')
        report['closing'] = driver_range_close(close_rows[0], sample, parent, baseline, frame_before, phase_us)
        traffic_open = any((s['raw']['gam'] | s['raw']['reply']) & GAM_GO_MASK or s['widi_go'] or
                           any(s['raw'][engine] & 0x10 for engine in ENGINES) for s in traffic)
        reason = ('traffic_open' if traffic_open else 'no_owned_request' if report['traffic']['bracket_complete']
                  else 'incomplete_bracket')
        report['traffic'].update(drain_verified=False, drain_disposition=reason)
    elif close_rows:
        raise ValueError('Unexpected driver range closing receipt')
    if version == 4:
        begin, first, pte, last, end = (sample[k] for k in ('begin', 'traffic0_end', 'pte_end', 'traffic1_end', 'end'))
        wake_checks, wake_us = sample['wake_checks'], sample['wake_us']
        if (not begin <= first <= pte <= last <= end or not 2 * packets <= wake_checks <= 4 or
                wake_us > first - begin + last - pte or
                ((count != 2 or not packets) and last != pte)):
            raise ValueError('Driver range timing evidence')
        report['timing'] = dict(first_traffic_us=first-begin, pte_us=pte-first, last_traffic_us=last-pte,
                                closing_us=end-last, wake_checks=wake_checks, wake_us=wake_us)
    return report


def boot_page_preflight_result(text, parent, range_proof, primary, flush, phase_us):
    prefix = 'intel-boot-page-'
    lines = text.splitlines()
    rows = [line for line in lines if line.startswith(prefix)]
    driver_rows = [line for line in lines if line.startswith(DRIVER_RANGE_PREFIX)]
    traffic_rows = [line for line in lines if line.startswith(DRIVER_TRAFFIC_PREFIX)]
    close_rows = [line for line in lines if line.startswith(DRIVER_CLOSE_PREFIX)]
    if not rows:
        if driver_rows or traffic_rows or close_rows:
            raise ValueError('Driver range preflight lacks the boot-page record')
        return None
    if flush['summary']['v'] != 9 or range_proof['summary']['v'] != 8:
        raise ValueError('Boot page preflight lacks the current GT flush baseline')
    suffix = ' authority=none'
    if not rows[0].endswith(suffix):
        raise ValueError('Boot page preflight authority')
    keys = ('v', 'slot', 'stage', 'epoch', 'begin', 'end', 'smi0', 'smi1', 'maps', 'retired', 'reads', 'live')
    has_policy = rows[0].startswith((prefix + 'preflight: v=2 ', prefix + 'preflight: v=3 '))
    summary = fields(rows[0].removesuffix(suffix), prefix + 'preflight:',
                     keys + (('policy_samples', 'policy0', 'policy1') if has_policy else ()),
                     ('live', 'policy0', 'policy1'), versions=(2, 3) if has_policy else (1,))
    stage = summary['stage']
    begin, end = summary['begin'], summary['end']
    maps, retired, reads = (summary[key] for key in ('maps', 'retired', 'reads'))
    if (summary['slot'] != 12292 or stage not in range(1, 6) or
            len(rows) != 1 + (stage >= 2) + (stage >= 3) + (stage >= 4) or
            summary['epoch'] != parent['epoch_after'] or not parent['post_bound'] or parent['pci_samples'] < 3 or
            not primary['summary']['complete'] or not 0 < primary['summary']['end1'] <= begin <= end <= parent['last'] or
            any(summary[key] >= 1 << 32 for key in ('smi0', 'smi1')) or
            not 0 <= retired <= maps <= 4 or reads > 19 or (stage < 4 and summary['smi1']) or
            (stage != 5 and (parent['commands'] != 1 or parent['pci_samples'] != 3))):
        raise ValueError('Boot page preflight scope')
    translation = None
    if stage >= 3:
        keys = ('gam0', 'wtcr0', 'valid0', 'valid1', 'wtcr1', 'gam1', 'high0', 'low', 'high1')
        translation = fields(rows[1], prefix + 'translation:', ('v', *keys), keys)
        if any(translation[key] >= 1 << 32 for key in keys):
            raise ValueError('Boot page translation word')
    keys = ('req', 'ack', 'win', 'reset', 'gam', 'frame', 'plane', 'programmed', 'live')
    wireless = []
    for index, line in enumerate(rows[1 + (translation is not None):]):
        sample = fields(line, prefix + 'widi:', ('v', 'n', *keys), keys)
        baseline = primary['vectors'][1]
        if (sample['n'] != index or any(sample[key] >= 1 << 32 for key in keys) or
                tuple(sample[key] for key in keys[-3:]) != tuple(baseline[30:33])):
            raise ValueError('Boot page preflight surface or sample')
        wireless.append(sample)
    frame_delta = (wireless[1]['frame'] - wireless[0]['frame']) & 0xffffffff if stage >= 4 else None
    if stage == 5 and (not 0 < end - begin <= phase_us or (maps, retired, reads) != (4, 4, 19) or summary['live'] or
                       summary['smi0'] != summary['smi1'] or summary['smi0'] != (flush['smi_guard'] or {}).get('first') or
                       frame_delta > 8 or translation['high0'] != translation['high1'] or
                       (translation['high0'] << 32 | translation['low']) != range_proof['vectors'][1][-1]):
        raise ValueError('Boot page preflight closing proof')
    policy = None
    if has_policy:
        count = summary['policy_samples']
        words = [summary['policy0'], summary['policy1']]
        if (count > 2 or (stage == 1 and count > 1) or (stage in (2, 3) and count != 1) or
                (stage >= 4 and count != 2) or any(word >= 0xffffffff for word in words[:count]) or
                any(words[count:]) or (stage == 5 and words[0] != words[1])):
            raise ValueError('Boot page power policy bracket')
        stable = count == 2 and words[0] == words[1]
        locked = bool(words[0] & CPD_POLICY_LOCK) if stable else None
        enabled = bool(words[0] & CPD_GAM_GO_MESSAGES) if stable else None
        policy = dict(words=words[:count], stable=stable, cpd_locked=locked, cpd_go_enabled=enabled,
                      cpd_go_locked_out=locked and not enabled if stable else None)
    if (summary['v'] == 3 and stage == 5 and not driver_rows) or (summary['v'] < 3 and driver_rows):
        raise ValueError('Driver range preflight version or missing record')
    driver_range = driver_range_preflight_result(driver_rows, traffic_rows, close_rows, summary,
                                                primary['vectors'][1][30:33] if driver_rows else (),
                                                wireless[0]['frame'] if wireless else None, phase_us)
    if driver_range is not None and 'traffic' in driver_range:
        pinned = range_proof.get('scanout_backing_guard', {}).get('physical')
        pte = driver_range['pte']
        driver_range['matches_pinned_framebuffer'] = pte == (pinned | 1) if pinned and pte is not None else None
    return dict(summary=summary, translation=translation, wireless=wireless, frame_delta=frame_delta, capture_complete=stage == 5,
                widi_go=[bool(sample['win'] & 16) for sample in wireless],
                win_clock_gated=[bool(sample['req'] & sample['ack'] & 32) for sample in wireless],
                widi_tlb_empty=translation is not None and not any(translation[key] for key in ('wtcr0', 'wtcr1', 'valid0', 'valid1')),
                **({'power_policy': policy} if policy is not None else {}),
                **({'driver_range': driver_range} if driver_range is not None else {}),
                write_authority=False)


def pipe_writer_result(text, parent, primary, aux, retire):
    rows = [line for line in text.splitlines() if line.startswith(('intel-pipe-a-writer:', 'intel-pipe-a-writer-frame:'))]
    if not rows:
        return None
    summary = fields(rows[0], 'intel-pipe-a-writer:', ('v', 'stage', 'before', 'after', 'returned', 'writable', 'writes'))
    stage, before, after, returned, writable, writes = (summary[key] for key in
        ('stage', 'before', 'after', 'returned', 'writable', 'writes'))
    if (stage > 6 or writable > 1 or writes or (stage in (0, 6) and writable) or
            (1 <= stage <= 4 and not writable) or bool(before) != (stage >= 3) or
            (stage >= 4 and not after) or (after and (stage < 3 or parent['pci_samples'] != 4)) or
            bool(returned) != (stage == 6) or len(rows) != 1 + bool(before) + bool(after)):
        raise ValueError('Pipe A writer lifetime')
    if stage and (not primary['summary']['complete'] or not aux['summary']['inactive'] or parent['commands'] == 0):
        raise ValueError('Pipe A writer lacks its original display or PCI owner')
    if before and not 0 < primary['summary']['end1'] <= before <= parent['last']:
        raise ValueError('Pipe A writer admission clock')
    if after and not before <= after <= parent['last']:
        raise ValueError('Pipe A writer reopen clock')
    frames = []
    keys = ('frame', 'plane', 'stride', 'programmed', 'live')
    for n, line in enumerate(rows[1:]):
        row = fields(line, 'intel-pipe-a-writer-frame:', ('v', 'n', *keys), keys)
        baseline = primary['vectors'][1]
        if (row['n'] != n or row['frame'] >= 1 << 32 or
                tuple(row[key] for key in keys[1:]) != (baseline[30], baseline[6], baseline[31], baseline[32])):
            raise ValueError('Pipe A writer observed a different surface')
        frames.append(row)
    delta = ((frames[1]['frame'] - frames[0]['frame']) & 0xffffffff) if after else None
    if stage >= 4 and (not before < after <= before + 100000 or delta > 8):
        raise ValueError('Pipe A writer display progress')
    if returned and (not after < returned <= min(before + 100000, parent['last']) or
                     (retire['guard_begin'] and returned > retire['guard_begin'])):
        raise ValueError('Pipe A writer retirement clock')
    if parent['complete'] and stage != 6:
        raise ValueError('Pipe A writer omitted retirement')
    return dict(summary=summary, frames=frames, frame_delta=delta, complete=stage == 6)


def smi_guard_result(rows, version, parent, memory, retire, selector_writes=0):
    if version == 1:
        if rows or parent["reason"] & SMI_FAULT:
            raise ValueError("Foreign GT SMI receipt")
        return None
    if len(rows) != 1 or not rows[0].endswith(" authority=none"):
        raise ValueError("GT SMI receipt coverage or authority")
    guard = fields(rows[0].removesuffix(" authority=none"), SMI_PREFIX,
                   ("v", "reads", "point", "fault", "first", "last"))
    reads, point, fault, first, last = (guard[key] for key in ("reads", "point", "fault", "first", "last"))
    writes = selector_writes + parent["commands"] + parent["native_writes"] + memory["writes"]
    if (max(first, last) >= 1 << 32 or point > 6 or fault > 2
            or bool(fault) != bool(parent["reason"] & SMI_FAULT)
            or (fault and (not parent["failed"] or parent["complete"]))):
        raise ValueError("GT SMI counter or fault")
    if not reads:
        if (first or last or writes or retire["stage"]
                or (point, fault) not in ((0, 0), (1, 1))):
            raise ValueError("GT SMI missing initial sample")
        return guard
    minimum = 1 + writes + (retire["stage"] >= 1)
    if (not point or (point == 1 and reads != 1)
            or (point == 6 and retire["stage"] < 2)
            or ((first != last) != (fault == 2))
            or (fault == 1 and reads != minimum)
            or (fault == 2 and reads != minimum + 1)
            or not minimum <= reads <= minimum + 1):
        raise ValueError("GT SMI checkpoint accounting")
    if parent["complete"] and (point != 6 or reads != minimum + 1 or fault):
        raise ValueError("GT SMI missing final sample")
    return guard


def bank_result(text, version, samples, reset_started):
    summaries = [line for line in text.splitlines() if line.startswith("intel-l3-steer:")]
    rows = [line for line in text.splitlines() if line.startswith("intel-l3-bank:")]
    if version < 9:
        if summaries or rows:
            raise ValueError("Unexpected L3 bank receipt")
        return None
    if len(summaries) != 1:
        raise ValueError("Missing or duplicate L3 bank summary")
    s = fields(summaries[0], "intel-l3-steer:",
               ("v", "opened", "mapped", "captured", "original", "final", "writes", "banks", "restored"),
               ("original", "final"))
    orig, count, writes = s["original"], s["banks"], s["writes"]
    if (len(rows) != count or count > 8 or writes > 9 or orig >= 0xffffffff or s["final"] > 0xffffffff
            or s["opened"] not in (0, 1, 3) or s["mapped"] not in (0, 1, 3) or s["mapped"] & ~s["opened"]
            or max(s["captured"], s["restored"]) > 1 or (s["opened"] and samples == 0)
            or (s["captured"] and s["opened"] != 3)
            or (not s["captured"] and (orig or writes or count or s["final"] or s["restored"]))
            or (s["restored"] and s["final"] != orig)):
        raise ValueError("Invalid L3 steering accounting")
    banks = []
    for i, line in enumerate(rows):
        b = fields(line, "intel-l3-bank:", ("v", "n", "selector", "sqc", "scratch"),
                   ("selector", "sqc", "scratch"))
        if (b["n"] != i or b["selector"] != ((orig & ~(15 << 24)) | (i << 24))
                or max(b["sqc"], b["scratch"]) >= 0xffffffff):
            raise ValueError("Invalid steered L3 bank observation")
        banks.append(b)
    if count == 8 and s["restored"]:
        expected = 7 + (((orig >> 24) & 15) != 0) + (((orig >> 24) & 15) != 7)
        if writes != expected:
            raise ValueError("Wrong L3 selector write count")
    if (samples >= 2 or reset_started) and (count != 8 or not s["restored"] or s["mapped"]):
        raise ValueError("Advanced without full L3 bank coverage and selector return")
    return dict(summary=s, banks=banks)


def cold_rc6_result(rows, binding, timing, wake, display):
    points = []
    counts = {1: 4, 2: 8, 3: 9}
    for i, line in enumerate(rows):
        p = fields(line, 'intel-air-rc6:', ('v', 'n', 'begin', 'end', 'core', 'residency', 'ack',
                                             'plane', 'programmed', 'live'),
                   ('core', 'residency', 'ack', 'plane', 'programmed', 'live'), versions=(1, 2, 3))
        released = 6 if p['v'] == 3 else 5
        delays = (0, 2000, 0, 2000, 18000) + ((80000,) if p['v'] == 3 else ()) + (0, 20000, 80000)
        if (len(display) != 1 or p['plane'] != display[0]['plane'] or
                p['programmed'] & ~8 != display[0]['surface'] or p['live'] != display[0]['surface']):
            raise ValueError('Active panel RC6 scanout differs from EFI')
        if (p['n'] != i or i >= counts[p['v']] or p['ack']
                or max(p['core'], p['residency']) >= 0xffffffff
                or not 0 < p['begin'] < p['end'] <= p['begin'] + 10000
                or (points and (p['v'] != points[0]['v'] or p['begin'] < points[-1]['end']))):
            raise ValueError('Active panel RC6 sample order, clock or register')
        if delays[i] and not delays[i] <= p['begin'] - points[-1]['end'] <= delays[i] + 10000:
            raise ValueError('Active panel RC6 idle interval')
        if (wake and p['begin'] < wake['begin_us']) or (i < 2 and binding['gt'] and p['end'] > binding['gt']):
            raise ValueError('Active panel RC6 sample crosses wake acquisition')
        if i >= 2 and (not timing['gt_end'] or p['begin'] < timing['gt_end']
                       or not wake or not wake['closed'] or p['end'] > wake['end_us']):
            raise ValueError('Active panel RC6 sample crosses wake release')
        if p['v'] >= 2 and 2 <= i < released and timing['hda_end'] and p['end'] > timing['hda_end']:
            raise ValueError('Active panel RC6 sample crosses audio release')
        if i >= released and (not timing['outer_end'] or not timing['hda_end'] or
                       p['begin'] < max(timing['outer_end'], timing['hda_end'])):
            raise ValueError('Active panel RC6 sample precedes audio release')
        points.append(p)
    version = points[0]['v']
    if wake and wake['complete'] and len(points) != counts[version]:
        raise ValueError('Active panel RC6 coverage')
    def delta(start, end):
        return (points[end]['residency'] - points[start]['residency']) & 0xffffffff
    result = dict(version=version, samples=points,
                  delta_ticks_mod32=[delta(i, i+1) for i in (0, 2) if i+1 < len(points)])
    if version >= 2:
        intervals = [('audio_held_20ms', 2, 4), ('audio_released_20ms', 5, 6), ('audio_released_100ms', 5, 7)]
        if version == 3:
            intervals = [('max_idle_10_20ms', 2, 4), ('max_idle_10_100ms', 2, 5),
                         ('restored_audio_released_20ms', 6, 7), ('restored_audio_released_100ms', 6, 8)]
        result['idle_intervals'] = {
            name: dict(elapsed_us=points[end]['begin'] - points[start]['end'],
                       residency_delta_mod32=delta(start, end))
            for name, start, end in intervals if end < len(points)}
    return result


def idle_change_result(text, policy, cold, timing, complete):
    lines = text.splitlines()
    headers = [line for line in lines if line.startswith('intel-idle-change:')]
    engines = [line for line in lines if line.startswith('intel-idle-engine:')]
    if len(headers) != 1:
        raise ValueError('MAX_IDLE trial header coverage')
    h = fields(headers[0], 'intel-idle-change:', ('v', 'captured', 'attempted', 'verified', 'persisted', 'returned',
                                                  'apply_end', 'restore_begin', 'restore_end', 'closed'))
    captured, attempted, verified, returned = (h[k] for k in ('captured', 'attempted', 'verified', 'returned'))
    if (captured > 31 or h['persisted'] & ~verified or attempted & ~captured or verified & ~attempted or returned & ~attempted
            or captured & (captured + 1) or attempted & (attempted + 1) or verified & (verified + 1)
            or (attempted and captured != 31) or len(engines) != captured.bit_count() or h['closed'] > 1):
        raise ValueError('MAX_IDLE register coverage')
    evidence = {}
    for i, line in enumerate(engines):
        row = fields(line, 'intel-idle-engine:', ('v', 'n', 'saved', 'applied', 'at_return', 'restored'),
                     ('saved', 'applied', 'at_return', 'restored'))
        name = ENGINES[i]
        if (row['n'] != i or len(policy) != 2 or row['saved'] != policy[1][name]
                or (verified & (1 << i) and row['applied'] != 10)
                or (h['persisted'] & (1 << i) and row['at_return'] != 10)
                or (returned & (1 << i) and row['restored'] != row['saved'])):
            raise ValueError('MAX_IDLE per-engine readback')
        evidence[name] = row
    points = cold.get('samples', [])
    if ((h['apply_end'] and (verified != 31 or h['apply_end'] < policy[1]['end']))
            or (len(points) > 2 and not 0 < h['apply_end'] <= timing['gt_end'] <= points[2]['begin'])
            or (len(points) > 2 and h['restore_begin'] and points[min(len(points)-1,5)]['end'] > h['restore_begin'])
            or (h['restore_end'] and (returned != attempted or not 0 < h['restore_begin'] < h['restore_end']))
            or (len(points) > 6 and not 0 < h['restore_end'] <= timing['hda_end'] <= points[6]['begin'])
            or (complete and not (len(points) == 9 and returned == verified == h['persisted'] == 31 and h['apply_end'] and h['restore_end'] and h['closed']))):
        raise ValueError('MAX_IDLE measurement or restoration lifetime')
    return dict(summary=h, engines=evidence)


def prepare_messages(text, baseline, following):
    rows = []
    phases = ('bcs-stop', *ENGINES) if any(line.startswith('intel-bcs-stop:') for line in text.splitlines()) else ENGINES
    version = 2 if len(phases) == 6 else 1
    previous = [baseline[k + '_close'] for k in ENGINES]
    last_l3, end = baseline['l3_close'], baseline['end']
    for line in text.splitlines():
        if not line.startswith('intel-gpm-step:'):
            continue
        m = re.fullmatch(rf'intel-gpm-step: v={version} n=(\d+) begin=(\d+) end=(\d+) data=(\S+)', line)
        if not m:
            raise ValueError('Malformed preparation message')
        n, begin, close = map(int, m.groups()[:3])
        data = base64.b64decode(m[4], validate=True)
        if len(data) != 44 or n != len(rows) or n >= len(phases) or not end <= begin < close or close-begin > 10000:
            raise ValueError('Preparation message order or deadline')
        words = struct.unpack('<11I', data)
        if 0xffffffff in words or (following and close > following['begin']):
            raise ValueError('Invalid preparation sample')
        req, reply, *engines, widi, l3, reply_end, req_end = words
        permitted = (req | reply | reply_end | req_end) & GAM_GO_MASK
        fresh = [name for bit, name, old, new in zip((0, 1, 3, 2, 4), ENGINES, previous, engines)
                 if not permitted & (1 << bit) and not old & 32 and new & 0x31 == 0x20]
        blocked = not permitted and not widi & 0x11 and not (req | req_end) & 0xff00
        rows.append(dict(after_prepare=phases[n], begin=begin, end=close, request=[req, req_end],
                         arbiter=[reply, reply_end], engines=engines, widi=widi, l3=l3,
                         fresh_engine_ack=fresh, observed_closed=blocked,
                         fresh_l3_under_block=blocked and last_l3 & 3 == 0 and l3 & 3 == 1))
        previous, last_l3, end = engines, l3, close
    if rows and following and len(rows) != len(phases):
        raise ValueError('Missing preparation message')
    return rows


def flush_result(text, parent, reset, translation, primary, retire, memory, widi=None):
    rows = [line for line in text.splitlines() if line.startswith("intel-flush-")]
    if not rows or not rows[0].endswith(" authority=none"):
        raise ValueError("GT flush receipt coverage or authority")
    summary = fields(rows[0].removesuffix(" authority=none"), "intel-flush-summary:",
                     ("v", "samples", "reads", "maps", "retired", "live"), ("live",), versions=(1, 2, 3, 4, 5, 6, 7, 8, 9))
    smi_rows = [line for line in text.splitlines() if line.startswith(SMI_PREFIX)]
    bank = bank_result(text, summary["v"], summary["samples"], reset["begin"])
    smi = smi_guard_result(smi_rows, summary["v"], parent, memory, retire,
                           bank["summary"]["writes"] if bank else 0)
    traced = summary["v"] >= 6
    if (summary["v"] >= 7) != (translation["v"] >= 2):
        raise ValueError("GT flush and slot reservation receipt versions disagree")
    slots = 4 if traced else 3
    if len(rows) != slots + 1:
        raise ValueError("GT flush sample coverage")
    power = summary["v"] >= 5
    media = summary["v"] >= 4
    gam = summary["v"] >= 3
    words = (*WORDS, *GAM_WORDS) if gam else WORDS
    if media:
        words += MEDIA_WORDS
    if power:
        words += POWER_WORDS
    if traced:
        words += ("l3_close", "go_close", *(key + "_close" for key in ENGINES), "eco_close", "req_close")
    width = len(words)
    offsets = (0, 8, 10, 12, 13, width) if power else (0, 8, 10, width) if media else (0, width)
    if traced:
        offsets = (0, 1, 9, 11, 13, 16, 24, width)
    pages = len(offsets) - 1
    count, reads, maps, retired, live = (summary[key] for key in ("samples", "reads", "maps", "retired", "live"))
    if widi is not None and not count <= len(widi) <= min(slots, count + 1):
        raise ValueError("WiDi sample count outside engine observation")
    if (count > slots or not count * width <= reads <= min(slots, count + 1) * width
            or not count * pages <= retired <= maps <= min(slots, count + 1) * pages
            or (maps != retired and not live)
            or (live and count == slots) or reads > maps * width):
        raise ValueError("GT flush accounting")

    if media:
        # Each later alias requires every read on the preceding pages.
        extra_maps = maps - count * pages
        extra_reads = reads - count * width
        if (maps - retired > 1 or
                not offsets[max(0, extra_maps - 1)] <= extra_reads <= offsets[extra_maps]):
            raise ValueError("GT media page accounting")
    sample_version = 5 if traced else 4 if power else 3 if media else 2 if gam else 1
    samples = [fields(row, "intel-flush-sample:", ("v", "n", "begin", "end", *words), words,
                      versions=(sample_version,))
               for row in rows[1:]]
    if [row["n"] for row in samples] != list(range(slots)):
        raise ValueError("GT flush sample order")
    if ((parent["issued"] and count < 1) or (reset["begin"] and count < 2)
            or (traced and memory["begin"] and count < 3)
            or (retire["guard_begin"] and count != slots) or (parent["complete"] and count != slots)):
        raise ValueError("GT flush parent skipped an observation")

    starts_after = (translation["end0"], samples[0]["end"], primary["end1"])
    ends_before = (samples[1]["begin"], reset["begin"], retire["guard_begin"])
    if traced:
        starts_after = (*starts_after[:2], reset["end"], primary["end1"])
        ends_before = (*ends_before[:2], memory["begin"], retire["guard_begin"])
    for n, sample in enumerate(samples):
        values = [sample[key] for key in words]
        begin, end = sample["begin"], sample["end"]
        if any(word > 0xffffffff for word in values):
            raise ValueError("GT flush DWORD width")
        if begin and (n > count or not parent["begin"] <= starts_after[n] < begin <= parent["last"]):
            raise ValueError("GT flush sample start")
        if n < count:
            if (not 0 < begin < end <= parent["last"] or end - begin > 10000
                    or (ends_before[n] and end > ends_before[n]) or 0xffffffff in values):
                raise ValueError("GT flush completed sample clock or register")
        elif end or (not begin and any(values)):
            raise ValueError("GT flush partial sample")
    if ((samples[0]["begin"] and parent["pci_samples"] < 2)
            or (samples[1]["begin"] and (parent["prepared"], parent["issued"]) != (5, 5))
            or (traced and samples[2]["begin"] and not reset["complete"])
            or (samples[-1]["begin"] and not primary["complete"])):
        raise ValueError("GT flush phase admission")

    engine_ack = [sum((bool(row[key] & 32) << index) for index, key in enumerate(ENGINES))
                  for row in samples[:count]]
    engine_ack_rise = (engine_ack[1] & ~engine_ack[0]) if count >= 2 else 0
    gam_state = [{"allowed": row["gam_go"] & GAM_GO_MASK, "idle": bool(row["gam_eco"] & 1)}
                 for row in samples[:count]] if gam else None
    result = dict(summary=summary, samples=samples[:count], partial=samples[count:],
                  engine_ack=engine_ack, engine_ack_rise=engine_ack_rise,
                  l3_flush_ack=[bool(row["l3"] & 1) for row in samples[:count]])
    slice_rows = [line for line in text.splitlines() if line.startswith("intel-l3-power:")]
    if len(slice_rows) != (count if summary["v"] >= 8 else 0):
        raise ValueError("GT slice power sample coverage")
    if summary["v"] >= 8:
        decoded = []
        for n, line in enumerate(slice_rows):
            sample = fields(line, "intel-l3-power:", ("v", "n", "fuse", "before", "after"),
                            ("fuse", "before", "after"))
            if sample["n"] != n or any(sample[k] >= 0xffffffff for k in ("fuse", "before", "after")):
                raise ValueError("Invalid slice power observation")
            available = (sample["fuse"] >> 25) & 7
            before, after = sample["before"] & 7, sample["after"] & 7
            if (before | after) & ~available:
                raise ValueError("Powered slice outside available topology")
            decoded.append(dict(available=available, before=before, after=after))
        result["slice_power"] = decoded
    if bank:
        if count >= 2 and result["slice_power"][1] != dict(available=3, before=3, after=3):
            raise ValueError("L3 bank observation lacks slice power")
        result["l3_banks"] = bank
    if media:
        current = [{key: (row[key] >> 16) & 0xff for key in MEDIA_WORDS} for row in samples[:count]]
        for n, queues in enumerate(current):
            phases = [parent["issued"], reset["begin"]]
            if traced:
                phases.append(memory["begin"])
            later_phase = (phases + [retire["guard_begin"]])[n]
            if any(queues.values()) and (n + 1 != count or later_phase or parent["complete"] or not parent["failed"]):
                raise ValueError("GT advanced past outstanding media requests")
        result["media_outstanding"] = current
    idle_rows = [line for line in text.splitlines() if line.startswith('intel-idle-policy:')]
    if idle_rows:
        phases = list(range(0, count, 2))
        if summary['v'] != 9 or len(idle_rows) != len(phases):
            raise ValueError('Idle policy phase coverage')
        names = ('wake_rate', 'evaluation', 'hysteresis', 'sleep', *ENGINES)
        result['idle_policy'] = []
        for phase, line in zip(phases, idle_rows):
            p = fields(line, 'intel-idle-policy:', ('v', 'phase', 'begin', 'end', *names), names)
            sample = samples[phase]
            if (p['phase'] != phase or p['begin'] != sample['begin'] or
                    not p['begin'] < p['end'] <= sample['end'] or any(p[k] >= 0xffffffff for k in names)):
                raise ValueError('Idle policy phase, clock or register')
            result['idle_policy'].append(p)
    if power:
        result["power_controls"] = [
            dict(hardware_control=bool(row["rc_control"] & (1 << 31)),
                 rc6_enabled=bool(row["rc_control"] & (1 << 18)),
                 timeout_mode=bool(row["rc_control"] & (1 << 28)),
                 evaluation_mode=bool(row["rc_control"] & (1 << 27)),
                 sw_target=(row["rc_target"] >> 16) & 7,
                 threshold_ticks=row["rc6_threshold"] & 0xffffff,
                 requested_clients=row["gam_req"] & GAM_GO_MASK)
            for row in samples[:count]
        ]
    if smi is not None:
        result["smi_guard"] = smi
    if any(line.startswith("intel-gpm-step:") for line in text.splitlines()):
        if summary["v"] < 9 or not count:
            raise ValueError("Preparation messages lack a compatible baseline")
        result['prepare_messages'] = prepare_messages(text, samples[0], samples[1] if count >= 2 else None)
        if result['prepare_messages'][0]['after_prepare'] == 'bcs-stop':
            result['stop_messages'] = result['prepare_messages'].pop(0)
    if gam:
        result["gam"] = gam_state
    trace_rows = [line for line in text.splitlines() if line.startswith("intel-gam-")]
    if traced:
        result["request_trace"] = request_trace(trace_rows, parent, reset, samples[:count], memory, widi)
    elif trace_rows:
        raise ValueError("Foreign GAM trace")
    return result


def request_trace(rows, parent, reset, samples, memory, widi=None):
    if not rows:
        raise ValueError("Missing GAM trace")
    summary = fields(rows[0], "intel-gam-trace:", ("v", "reads", "spans", "omitted", "max_gap_us"))
    size, reads, omitted, gap = (summary[k] for k in ("spans", "reads", "omitted", "max_gap_us"))
    if (len(rows) != size + 1 or size > 32 or reads > parent["guards"] or (omitted and size != 32)
            or bool(gap) != (reads > 1) or gap > parent["last"] - parent["begin"]):
        raise ValueError("GAM trace coverage")
    spans = []
    changes = []
    for i, line in enumerate(rows[1:]):
        s = fields(line, "intel-gam-span:", ("v", "n", "pulse", "word", "count", "first_begin",
                                            "first_end", "last_begin", "last_end"), ("word",))
        a, b, c, d = (s[k] for k in ("first_begin", "first_end", "last_begin", "last_end"))
        if (s["n"] != i or not s["count"] or s["word"] >= 0xffffffff or s["pulse"] > reset["reset_completed"]
                or (s["pulse"] and (not reset["begin"] or a < reset["begin"]))
                or (reset["end"] and s["pulse"] < reset["reset_completed"] and d > reset["end"])
                or not parent["begin"] <= a <= b <= d <= parent["last"] or not a <= c <= d
                or (s["count"] == 1 and (a, b) != (c, d))
                or (s["count"] > 1 and (b > c or d - a > (s["count"] - 1) * gap))):
            raise ValueError("GAM span fields or clock")
        if spans:
            prev = spans[-1]
            if (prev["last_end"] > a or b - prev["last_begin"] > gap or s["pulse"] < prev["pulse"]
                    or (prev["pulse"], prev["word"]) == (s["pulse"], s["word"])):
                raise ValueError("GAM span order or gap")
            old, new = (word >> 8 & 0xff for word in (prev["word"], s["word"]))
            if old != new:
                changes.append(dict(cleared=old & ~new, asserted=new & ~old,
                                    earliest_us=prev["last_begin"], latest_us=b,
                                    pulses_before=prev["pulse"], pulses_after=s["pulse"]))
        spans.append(s)
    if reads != sum(s["count"] for s in spans) + omitted:
        raise ValueError("GAM trace count")
    if parent["complete"] and not omitted and (
            {s["pulse"] for s in spans} != {0, 1, 2} or spans[0]["first_begin"] > samples[0]["begin"]
            or spans[-1]["last_end"] < samples[-1]["end"]):
        raise ValueError("GAM trace missed reset or recovery")
    windows = []
    for i, row in enumerate(samples):
        # Require an actual closed observation inside the packet. Any conflicting
        # sampled permission within it rules out even a sampled block.
        observed = [s for s in spans if s["last_end"] >= row["begin"] and s["first_begin"] <= row["end"]]
        allowed = (row["gam_req"] | row["req_close"] | row["gam_go"] | row["go_close"]) & GAM_GO_MASK
        trace_open = any(s["word"] & GAM_GO_MASK for s in observed)
        closed = bool(observed) and not (allowed or trace_open or omitted)
        if widi is not None:
            closed = closed and (widi[i]["win"] & 0x11) == 0 and not widi[i]["reset_flow"] & (1 << 10)
        ack = all(row[k] & 0x31 == 0x20 and row[k + "_close"] & 0x31 == 0x20 for k in ENGINES)
        l3 = row["l3"] & 3 == 1 and row["l3_close"] & 3 == 1
        idle = bool(row["gam_eco"] & row["eco_close"] & 1)
        empty = all((row[k] & 0xff0000) == 0 for k in MEDIA_WORDS)
        fresh = i > 0 and all(not samples[i - 1][k + "_close"] & 32 for k in ENGINES)
        fresh = fresh and not samples[i - 1]["l3_close"] & 1
        windows.append(dict(sample=i, observed_block=closed, fresh_flush_ack=bool(fresh and ack and l3),
                            gam_idle=idle, media_empty=empty,
                            sampled_block_and_flush=bool(closed and fresh and ack and l3 and idle and empty)))
    maintenance = None
    if len(samples) == 4 and memory["complete"]:
        before, after = samples[2:]
        # Packet 2 ends before the first maintenance write. Packet 3 starts
        # after maintenance has finished. Include every intervening request sample.
        interior = [s for s in spans if s["last_end"] >= before["end"] and s["first_begin"] <= after["begin"]]
        open_mask = 0
        for span in interior:
            open_mask |= span["word"] & GAM_GO_MASK
        blocked = bool(interior and windows[2]["observed_block"] and windows[3]["observed_block"] and not open_mask)
        maintenance = dict(begin_us=memory["begin"], end_us=memory["end"], observed_open_clients=open_mask,
                           observed_block=blocked, sampled_block_and_flush=blocked and windows[3]["sampled_block_and_flush"])
    return dict(summary=summary, spans=spans, reason_changes=changes,
                last_observed_reasons=(spans[-1]["word"] >> 8) & 0xff if spans and not omitted else None,
                windows=windows, maintenance=maintenance)
