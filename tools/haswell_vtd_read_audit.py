"""Host-only VT-d candidate contract. No MMIO, map, writer or native admission."""
import argparse
import hashlib
import json
from pathlib import Path

from hardware_haswell_firmware import PIN, catalog_valid, firmware_result
from haswell_backing_audit import unsigned

GPU_UNIT = 0xfed90000
OTHER_UNIT = 0xfed91000
PAGE = 4096
SAMPLE_COUNT = 2
MAP_CAP = 2
PASS_LIMIT_US = 100000
GAP_MIN_US, GAP_MAX_US = 20000, 100000
GAP_ITERATION_CAP = 1000000
RECEIPT_TAIL_BYTES = 65536
PREDECESSOR = (0xfed1c000, 0x4000, 1)
SUCCESSOR = (0xffe10000, 0x30000, 1)
OFFSETS = (0x00, 0x08, 0x0c, 0x10, 0x14, 0x1c, 0x20, 0x24)
SEQUENCE = (0x1c, 0x00, 0x0c, 0x08, 0x0c, 0x14, 0x10, 0x14,
            0x24, 0x20, 0x24, 0x1c)


def candidate_resource(rows):
    """Exact observed gap candidate only; generic RESERVED coverage refuses."""
    if not isinstance(rows, (tuple, list)) or not 1 <= len(rows) <= 128:
        raise ValueError('complete bounded catalog required')
    checked = []
    for row in rows:
        if not isinstance(row, (tuple, list)) or len(row) != 3:
            raise ValueError('catalog shape')
        checked.append(tuple(unsigned(v, 64) for v in row))
    catalog_valid(checked)
    if any(b < GPU_UNIT + PAGE and GPU_UNIT < b + n for b, n, _ in checked):
        raise ValueError('GPU unit intersects catalog')
    before = [r for r in checked if r[0] + r[1] <= GPU_UNIT]
    after = [r for r in checked if r[0] >= GPU_UNIT + PAGE]
    if not before or not after:
        raise ValueError('missing gap edges')
    lo, hi = max(before, key=lambda r: r[0] + r[1]), min(after, key=lambda r: r[0])
    if lo != PREDECESSOR or hi != SUCCESSOR:
        raise ValueError('changed unit-specific gap edges')
    return dict(proposed_base=GPU_UNIT, proposed_bytes=PAGE,
                predecessor=list(lo), successor=list(hi), catalog_intersections=[],
                source='exact-DMAR-declaration-and-observed-gap-candidate',
                enclosing_reserved_grant=False, native_mapping_authorized=False)


def candidate_read(offset):
    unsigned(offset, 32)
    if offset not in OFFSETS:
        raise ValueError('outside audited read offsets')
    return dict(offset=offset, width_bytes=4, proposed_only=True,
                register_writes_authorized=False, native_read_authorized=False)


def audit(text):
    proof = firmware_result(text)
    if not proof or proof['status'] != 'OBSERVED' or not proof['source_completed'] or proof['dmar_sha256'] != PIN:
        raise ValueError('complete exact-unit pinned firmware receipt required')
    resources = [candidate_resource(s['catalog']) for s in proof['samples']]
    if len(resources) != 2 or resources[0] != resources[1]:
        raise ValueError('matching resource prerequisites required')
    return dict(status='HOST_CONTRACT_AUDIT_ONLY', receipt_sha256=hashlib.sha256(text.encode()).hexdigest(),
                dmar_sha256=PIN, resource_candidate=resources[0],
                proposed_reads=[candidate_read(v) for v in OFFSETS],
                per_sample_sequence=list(SEQUENCE), reads_per_sample=len(SEQUENCE),
                sample_count=SAMPLE_COUNT, proposed_map_cap=MAP_CAP, pass_limit_us=PASS_LIMIT_US,
                gap_min_us=GAP_MIN_US, gap_max_us=GAP_MAX_US, gap_iteration_cap=GAP_ITERATION_CAP,
                receipt_tail_bytes=RECEIPT_TAIL_BYTES,
                mapping_contract=['kernel-root-only', 'supervisor-read-only-NX-non-global',
                                  'effective-strong-UC-from-current-PAT-MTRR-and-actual-leaf',
                                  'compatible-existing-aliases-or-refuse',
                                  'existing-or-poisoned-window-refuses',
                                  'fresh-pinned-firmware-catalog-and-PCI-guard-each-pass',
                                  'exact-page-translation-before-each-read',
                                  'verified-unmap-before-reuse',
                                  'uncertain-retirement-quarantines-until-reboot'],
                excluded_unit=OTHER_UNIT, root_table_walk_authorized=False,
                register_writes_authorized=False, native_mapping_authorized=False,
                native_read_authorized=False, translation_state_observed=False,
                root_register_is_loaded_table_proof=False, full_interval_stability_proven=False,
                physical_acceptance=False, hardware_authority=False, ownership=False,
                cache_state_observed=False, effective_uc_proven=False, existing_alias_compatibility_proven=False,
                unresolved_prerequisites=['Air61-current-PAT-MTRR-CR0-context',
                                          'effective-UC-and-actual-leaf-alias-proof',
                                          'native-adapter-codec-retention-and-VM-validation'],
                next_step='read-only exact-Air61 CPU cache/alias context before VT-d window implementation')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exit-log', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.exit_log.read_text())
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('HOST_CONTRACT_AUDIT_ONLY: exact-unit resource candidate; no native read/map authority')
