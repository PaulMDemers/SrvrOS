"""Host-only read-lifetime audit/models. No device, mapping or native callback API."""
import argparse
import base64
import hashlib
import json
from pathlib import Path

from hardware_air61_window import LEAF, decode, window_result
from haswell_backing_audit import unsigned
from haswell_vtd_read_audit import (GPU_UNIT, OTHER_UNIT, SEQUENCE, SAMPLE_COUNT,
    MAP_CAP, PASS_LIMIT_US, GAP_MIN_US, GAP_MAX_US, GAP_ITERATION_CAP, RECEIPT_TAIL_BYTES)

ADDR = ((1 << 39) - 1) & ~4095
A = 1 << 5
READ_LEAF = LEAF | A
PARENT_FLAGS = 3 | A
DENIED = dict(native_mapping_authorized=False, native_read_authorized=False,
    register_writes_authorized=False, root_table_walk_authorized=False,
    root_register_is_loaded_table_proof=False, full_interval_stability_proven=False,
    physical_acceptance=False, hardware_authority=False, ownership=False,
    translation_state_observed=False, actual_leaf_observed=False,
    effective_uc_proven=False, nx_enablement_proven=False, existing_alias_compatibility_proven=False)


def _page(value, owned=False):
    unsigned(value, 64)
    if not value or value & 4095 or value > ADDR or value in (GPU_UNIT, OTHER_UNIT):
        raise ValueError('RAM frame geometry')
    if owned and value < 0x2000000:
        raise ValueError('private frame minimum')
    return value


def _root_word(value):
    unsigned(value, 64)
    if value & 7 != 3 or not value & A or value & ~(ADDR | 0x800000000000003b):
        raise ValueError('borrowed root must already have A, exact supervisor path')
    _page(value & ADDR)
    return value


def expected_path(root, borrowed_word, pd, pt):
    """Synthetic expected words, never a frame-ownership/RAM admission proof."""
    _page(root);_root_word(borrowed_word);_page(pd, True);_page(pt, True)
    if len({root, borrowed_word & ADDR, pd, pt}) != 4:
        raise ValueError('borrowed/duplicate frame')
    return (borrowed_word, pd | PARENT_FLAGS, pt | PARENT_FLAGS, READ_LEAF)


def exact_path(expected, observed):
    if not isinstance(expected, (tuple, list)) or len(expected) != 4:
        raise ValueError('expected path shape')
    _root_word(expected[0])
    for word in expected[1:3]:
        unsigned(word, 64);_page(word & ADDR, True)
        if word & ~ADDR != PARENT_FLAGS:
            raise ValueError('exact A-preseeded parent flags')
    unsigned(expected[3], 64)
    if expected[3] != READ_LEAF:
        raise ValueError('exact A-preseeded RO/NX/UC leaf')
    if not isinstance(observed, (tuple, list)) or len(observed) != 4:
        raise ValueError('observed path shape')
    for word in observed:
        unsigned(word, 64)
    if tuple(expected) != tuple(observed):
        raise ValueError('raw path changed; no A/D masks')


def sample_words(rows):
    """Validate synthetic DWORD observations, not actual reads or device state."""
    if not isinstance(rows, (list, tuple)) or len(rows) != len(SEQUENCE):
        raise ValueError('exactly twelve modeled reads')
    words = []
    for row, offset in zip(rows, SEQUENCE):
        if not isinstance(row, (tuple, list)) or len(row) != 2:
            raise ValueError('DWORD shape')
        unsigned(row[0], 32);unsigned(row[1], 32)
        if row[0] != offset or row[1] == 0xffffffff:
            raise ValueError('sequence/unavailable DWORD')
        words.append(row[1])
    if not words[1] or any(words[a] != words[b] for a, b in ((0,11),(2,4),(5,7),(8,10))):
        raise ValueError('unavailable version/torn or changed bracket')
    return tuple(words)


def model_series(passes, gap_iterations=0):
    """Pure structural/timing model. Inputs cannot supply live gate authority."""
    unsigned(gap_iterations, 32)
    if gap_iterations > GAP_ITERATION_CAP or not isinstance(passes, (list, tuple)) or len(passes) != SAMPLE_COUNT:
        raise ValueError('bounded two-pass series')
    fields = {'owner','epoch','root','borrowed_word','pd','pt','begin','published',
        'sample_begin','sample_end','retired','words','before','after','receipt_bytes'}
    tokens, samples = [], []
    previous = None
    for p in passes:
        if not isinstance(p, dict) or set(p) != fields:
            raise ValueError('model fields; no caller-supplied grants')
        for key in fields - {'words','before','after'}:
            unsigned(p[key], 64)
        if not p['owner'] or not 0 < p['epoch'] <= p['begin'] <= p['published'] <= p['sample_begin'] < p['sample_end'] <= p['retired']:
            raise ValueError('epoch/time ordering')
        if p['retired'] - p['begin'] >= PASS_LIMIT_US or not 0 < p['receipt_bytes'] <= RECEIPT_TAIL_BYTES:
            raise ValueError('per-pass lifetime/receipt cap')
        path = expected_path(p['root'], p['borrowed_word'], p['pd'], p['pt'])
        if any(not isinstance(p[k], (tuple,list)) or len(p[k]) != len(SEQUENCE) for k in ('before','after')):
            raise ValueError('pre/post path for every modeled read')
        for pre, post in zip(p['before'], p['after']):
            exact_path(path, pre);exact_path(path, post)
        words = sample_words(p['words'])
        token = (p['owner'], p['epoch'], p['root'])
        if previous is not None:
            if token[0] != tokens[0][0] or token[2] != tokens[0][2] or token[1] <= tokens[0][1]:
                raise ValueError('owner/root/stale epoch')
            if p['borrowed_word'] != previous['borrowed_word'] or p['begin'] < previous['retired']:
                raise ValueError('borrowed mutation/overlapping mapping lifetimes')
            gap = p['sample_begin'] - previous['sample_end']
            if not GAP_MIN_US <= gap <= GAP_MAX_US or words != samples[0]:
                raise ValueError('inter-sample gap/changed raw sample')
        tokens.append(token);samples.append(words);previous=p
    return dict(status='HOST_SYNTHETIC_SERIES_ONLY', model_only=True,
        simulated_mapping_count=MAP_CAP, simulated_dword_count=len(SEQUENCE)*SAMPLE_COUNT,
        raw_simulated_samples=samples, gap_us=passes[1]['sample_begin']-passes[0]['sample_end'],
        pass_lifetimes_us=[p['retired']-p['begin'] for p in passes], **DENIED)


def guard_budget(baseline_words, coalesce_one=False, full_post_guards=False):
    """Host arithmetic only; coalescing requires a separate actual-source proof."""
    unsigned(baseline_words,32)
    if not 0 < baseline_words <= 65536 or type(coalesce_one) is not bool or type(full_post_guards) is not bool:
        raise ValueError('guard budget model inputs')
    full_guards=len(SEQUENCE)*(2 if full_post_guards else 1)
    coalesced=int(coalesce_one)
    # Current complete window consumes1840 bytes. Each stable source copy is
    # DMAR136 + MCFG60. Worst unaligned copies require18+9 guarded RAM words;
    # each costs at most four HHDM words plus the actual source word.
    firmware_bytes=1840+(full_guards-coalesced)*196
    word_bound=baseline_words+(full_guards-coalesced)*27*5+len(SEQUENCE)*2*4*5
    return dict(status='HOST_GUARD_BUDGET_ONLY',baseline_words=baseline_words,
        extra_full_guards=full_guards,proposed_adjacent_coalescing=coalesced,
        firmware_bytes=firmware_bytes,firmware_cap=4096,mapper_word_upper_bound=word_bound,
        mapper_word_cap=65536,fits_byte_word_caps=firmware_bytes<=4096 and word_bound<=65536,
        coalescing_implemented=False,**DENIED)


def audit(window, ecam, exit_log, session):
    proof = window_result(window, ecam, exit_log, session)
    if not proof or proof['status'] != 'OBSERVED_RETIRED_WINDOW_ONLY' or not proof['source_completed']:
        raise ValueError('complete strict matching window/ecam/exit history required')
    if not proof['retirement_verified'] or not proof['claim_released'] or proof['target_reads'] or proof['target_writes']:
        raise ValueError('retired no-access history required')
    blob = b''.join(base64.b64decode(line.split('data=')[1], validate=True)
        for line in window.splitlines() if line.startswith('hsw-window-data:'))
    raw = decode(blob)
    borrowed = _root_word(raw['_graph'][raw['root']]['raw'][384])
    from hardware_haswell_cache import effective_at
    cache=[effective_at(cpu,3,GPU_UNIT) for cpu in raw['_cpus']]
    if any(c['status']!='CLASSIFIED_OBSERVATION' or c['effective']!=0 for c in cache):
        raise ValueError('historical PAT3 classification')
    return dict(status='HOST_READ_LIFETIME_AUDIT_ONLY',
        receipt_sha256={name:hashlib.sha256(value.encode()).hexdigest()
            for name,value in [('window',window),('ecam',ecam),('exit',exit_log)]},
        session=session, historical_window=dict(elapsed_us=proof['end']-proof['begin'],
            pages=proof['pages'], mapper_words=proof['words'], retired_claim_released=True),
        historical_borrowed_root_word=borrowed, proposed_leaf=READ_LEAF,
        proposed_parent_flags=PARENT_FLAGS, historical_pat3_classification=cache, dirty_must_remain_zero=True,
        raw_A_D_mask=0, samples=SAMPLE_COUNT, reads_per_sample=len(SEQUENCE),
        sequence=list(SEQUENCE), mapping_cap=MAP_CAP, per_pass_limit_us=PASS_LIMIT_US,
        gap_min_us=GAP_MIN_US, gap_max_us=GAP_MAX_US, gap_iteration_cap=GAP_ITERATION_CAP,
        receipt_tail_bytes=RECEIPT_TAIL_BYTES,
        guard_budgets=dict(naive_pre=guard_budget(proof['words']),
            proposed_pre=guard_budget(proof['words'],coalesce_one=True),
            full_pre_post=guard_budget(proof['words'],coalesce_one=True,full_post_guards=True)),
        gap_definition='last DWORD completion in pass1 to first DWORD start in pass2, including retirement and fresh preparation',
        required_implementation=['separate two-context one-attempt-per-pass engine; never reset qualified no-read state',
            'fresh full native CPU/AP/root/cache/firmware/catalog/PCI admission in each pass',
            'borrowed root A already set; preseed only newly owned entries; no hidden target touch',
            'exact full pre/post path and live checks for every aligned volatile DWORD',
            'qualify one adjacent full-source guard coalescing: retain source comparisons, cap bytes4096 and words65536',
            'do not use full private-page verification per read; retain whole-page comparisons around the batch',
            'verified retirement and release before pass2; no retries after refusal or poison',
            'gap minimum wait only while unpublished, counted within pass2 budget when claimed',
            'preserve both full raw receipts without expanding per-file tail or using hashes alone',
            'maximum actual-producer/fault/VM qualification before separate A-preseed/two-window no-read native trial'],
        runtime_changed=False, boot_test_prepared=False, **DENIED)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('window-log','ecam-log','exit-log','output'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--session',required=True)
    a = p.parse_args()
    result = audit(a.window_log.read_text(),a.ecam_log.read_text(),a.exit_log.read_text(),a.session)
    a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(result['status']+': no native target access or new boot profile')
