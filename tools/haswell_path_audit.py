"""Host-only interpretation of a complete Haswell pilot receipt; no MMIO access."""
import argparse
import json
from pathlib import Path

from hardware_haswell_backing import backing_result
from haswell_backing_audit import fence_fields, unsigned


def display_mode(raw):
    raw = unsigned(raw, 32)
    if raw == 0xffffffff:
        raise ValueError('unavailable display register')
    field = (raw >> 13) & 3
    return dict(raw=raw, field=field, valid=field < 2,
                mode=('none', 'bit6-xor9-xor10', 'reserved', 'reserved')[field])


def access_class(offset):
    """Pinned Gen6/7 Linux forcewake model, NOT a register permission grant."""
    offset = unsigned(offset, 32)
    if offset == 0x4030:
        return 'render-forcewake-required; defer'
    if offset == 0x101000:
        return 'outside-render-forcewake-range; candidate-read-only'
    return 'not-audited'


def next_readout(offset, count):
    offset = unsigned(offset, 64)
    count = unsigned(count, 32)
    if offset % 4096 or not 1 <= count <= 1536 or offset + count * 4096 > 0x10000000:
        raise ValueError('pilot extent')
    # One additional TILECTL page per pass. Retain the existing ten-map ceiling;
    # layouts requiring three PTE pages must refuse this proposed extension.
    pages = ((offset // 4096 * 4 % 4096) + count * 4 + 4095) // 4096
    maps = 2 * (pages + 3)
    return dict(proposed_only=True, offset=0x101000, width_bytes=4,
                samples=2, proposed_maps=maps, existing_map_cap=10,
                fits_existing_map_cap=maps <= 10, pass_gap_limit_us=100000,
                pte_cap=1536, receipt_tail_bytes=65536,
                scope='Two raw CPU/aperture TILECTL observations, not full-path equivalence',
                gt_read_deferred=0x4030, upper_fence_reads_authorized=False,
                requires_new_adapter_codec_retention_and_VM_validation=True)


def audit(text):
    observed = backing_result(text)
    if not observed or observed['status'] != 'OBSERVED':
        raise ValueError('complete matching pilot receipt required')
    samples = observed['samples']
    modes = [display_mode(s['endpoint']['disparb']) for s in samples]
    if modes[0] != modes[1]:
        raise ValueError('changed display field')
    start, end = observed['offset'], observed['offset'] + observed['bytes']
    rows = [fence_fields(raw) for raw in samples[0]['fences']]
    overlaps = [i for i, row in enumerate(rows)
                if row['valid'] and row['start'] < end and start < row['end']]
    candidates = [dict(index=i, start=rows[i]['start'], end=rows[i]['end'],
                       prm_pitch=rows[i]['prm_pitch'], linux_pitch=rows[i]['linux_pitch'],
                       pitch_models_agree=not rows[i]['pitch_discrepancy'],
                       bytes_outside_surface=rows[i]['end'] - rows[i]['start'] - observed['bytes'])
                  for i in observed['lower_fence_candidates']]
    addresses = samples[0]['address_candidates']
    consecutive = all(addresses[i] is not None and addresses[0] is not None and
                      addresses[i] == addresses[0] + 4096 * i for i in range(len(addresses)))
    return dict(status='HOST_AUDIT_ONLY', physical_acceptance=False,
                source_readout_completed=True, count=observed['count'],
                display=modes[0], cpu_aperture_swizzle='unobserved', gt_swizzle='unobserved',
                full_path_equivalence=False,
                linux_inherited_tiled_policy=modes[0]['mode'] if modes[0]['valid'] else 'refuse-reserved',
                policy_is_not_hardware_consistency_proof=True,
                fence=dict(observed=16, intel_documented=16, linux_exact_hsw_model=32,
                           upper_unobserved=True, reference_count_discrepancy_resolved=False,
                           overlaps=overlaps, candidates=candidates, uniqueness_proven=False),
                address_candidates_consecutive=consecutive,
                all_present=observed['all_present'], all_stolen_candidates=observed['all_stolen_candidates'],
                firmware_reserved=observed['stolen_range_reserved'],
                backing_bytes_read=0, ownership=False, pin=False, hardware_authority=False,
                next_readout=next_readout(observed['offset'], observed['count']))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exit-log', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.exit_log.read_text())
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
