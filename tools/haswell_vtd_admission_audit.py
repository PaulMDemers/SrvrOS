"""Host-only exact-Air6,1 cache/resource admission audit. Never maps or reads MMIO."""
import argparse
import hashlib
import json
from pathlib import Path

from hardware_haswell_cache import cache_result, effective, UNIT, WINDOW
from hardware_haswell_firmware import firmware_result
from haswell_vtd_read_audit import candidate_resource
from haswell_backing_audit import unsigned

# A prospective fresh 4KiB PTE: P, PWT, PCD, XD. PAT=0 selects PAT slot3.
# This is a model, not a PTE store or proof of EFER.NXE/current permissions.
CANDIDATE_LEAF = UNIT | 0x19 | (1 << 63)
REQUIRED = (
    'exclusive-exact-page-resource-lease-and-competing-client-exclusion',
    'complete-relevant-root-and-CPU-alias-coverage-or-refusal',
    'fresh-CPU0-kernel-root-and-mapping-mutation-exclusion',
    'NX-capability-and-current-EFER-NXE-before-XD-entry',
    'fresh-cache-and-firmware-guards-around-each-pass',
    'actual-supervisor-RO-NX-nonglobal-4KiB-leaf-and-effective-UC',
    'verified-clear-and-local-TLB-retirement-before-reuse',
    'uncertain-publication-or-retirement-poisons-window-until-reboot',
)


def model_leaf(cpu, leaf):
    """Check only the proposed encoding against a decoded historical CPU context."""
    unsigned(leaf, 64)
    if leaf != CANDIDATE_LEAF or not cpu['raw'][6] & (1 << 16):
        raise ValueError('exact prospective leaf and observed CR0.WP required')
    classification = effective(cpu, 3)
    if classification.get('status') != 'CLASSIFIED_OBSERVATION' or classification.get('effective') != 0:
        raise ValueError('prospective PAT3 leaf is not classified strong UC')
    return dict(raw=leaf, physical=UNIT, size=4096, pat_index=3,
                supervisor=True, writable=False, execute_disable_bit=True,
                global_bit=False, classification=classification,
                encoding_only=True, actual_leaf_observed=False,
                nx_enablement_proven=False, native_mapping_authorized=False)


def audit(text):
    context = cache_result(text)
    if not context or context['status'] != 'OBSERVED_CONTEXT' or not context['source_completed']:
        raise ValueError('complete exact-Air61 raw cache/context receipt required')
    fw = firmware_result(text)
    resource = candidate_resource(fw['samples'][0]['catalog'])
    models = [model_leaf(c, CANDIDATE_LEAF) for c in context['cpu_samples']]
    if models[0] != models[1]:
        raise ValueError('changed prospective leaf classification')
    hhdm = [w for w in context['walks'] if w['alias'] == 0]
    private = [w for w in context['walks'] if w['alias'] == 1]
    # Point queries cannot establish a reverse-map census. A saved receipt also
    # cannot grant a lease, prove current EFER or exclude a future writer/CPU.
    return dict(status='HOST_ADMISSION_AUDIT_BLOCKED',
                receipt_sha256=hashlib.sha256(text.encode()).hexdigest(),
                resource_candidate=resource, proposed_window=WINDOW,
                historical_cache_context_observed=True,
                proposed_leaf=models[0],
                known_aliases=dict(hhdm_state=context['hhdm_alias_state'],
                                   private_window_absent=context['proposed_window_absent'],
                                   hhdm_raw=[w['raw'] for w in hhdm],
                                   private_raw=[w['raw'] for w in private]),
                historical_cr0_wp=True,
                catalog_usable_intersections=0,
                resource_lease_proven=False, all_aliases_proven=False,
                live_root_exclusion_proven=False, nx_enablement_proven=False,
                actual_leaf_observed=False, effective_uc_mapping_proven=False,
                retirement_proven=False, native_mapping_authorized=False,
                native_read_authorized=False, translation_state_observed=False,
                hardware_authority=False, ownership=False,
                unresolved_prerequisites=list(REQUIRED),
                next_step='bounded read-only relevant-root/alias and NX-state diagnostics; no device mapping')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exit-log', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = audit(args.exit_log.read_text())
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(report['status'] + ': historical context classified; live resource/alias/leaf admission remains unproved')
