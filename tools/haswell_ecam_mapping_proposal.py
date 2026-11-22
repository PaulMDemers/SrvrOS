"""Host-only Air61 ECAM large-leaf proposal and synthetic lifetime model.

No native mapper, MMIO operation, frame allocation or authority is implemented.
"""
import argparse
import hashlib
import json
from pathlib import Path
from hardware_haswell_cache import cache_result, effective_at
from haswell_alias_layout_audit import audit_mcfg

VA = 0xffffc00300000000
PA = 0xe0000000
SIZE = 156 << 20
LARGE = 1 << 21
PTE_FLAGS = (1 << 63) | 0x13  # NX, supervisor RW, PCD, present; PAT[2]
PDE_FLAGS = PTE_FLAGS | 0x80  # PS is bit7; large-leaf PAT bit12 remains zero
LEAVES = SIZE//LARGE
GRANULES = SIZE//4096
REQUIRED = (
    'fresh-exact-unit-MCFG-and-cache-NX-feature-guards',
    'CPU0-current-sole-kernel-root-and-exclusive-mutation-epoch',
    'no-other-executing-CPU-or-root-client',
    'borrowed-root-PDPT-full-raw-path-and-RAM-source-validation',
    'exact-zero-PDPT-slot-no-existing-4KiB-or-large-mapping-replacement',
    'fresh-owned-detached-zeroed-PD-frame-no-borrowed-frame-free',
    'fresh-per-4KiB-cache-equivalence-with-existing-ECAM-policy',
    'ordered-single-parent-publication-and-exact-readback',
    'local-invalidation-of-every-4KiB-VA-before-PCI-handoff',
    'verified-unlink-and-full-invalidation-before-pre-handoff-free',
    'terminal-retention-and-no-fallback-after-uncertain-publication',
    'permanent-boot-ownership-after-PCI-handoff',
)


def large_leaf(physical):
    if type(physical) is not int or physical % LARGE or not PA <= physical < PA+SIZE:
        raise ValueError('exact aligned ECAM leaf')
    return physical | PDE_FLAGS


def propose(text, mcfg):
    cpu = cache_result(text)
    if not cpu or cpu['status'] != 'OBSERVED_CONTEXT' or not cpu['source_completed']:
        raise ValueError('complete exact-unit raw cache receipt required')
    layout = audit_mcfg(mcfg)
    if (len(layout['windows']) != 1 or
            any(layout['windows'][0][k] != v for k, v in
                dict(physical=PA, virtual=VA, bytes=SIZE, segment=0, start_bus=0, end_bus=155).items())):
        raise ValueError('exact captured Air61 ECAM geometry required')
    if any(not c['raw'][6] & (1 << 16) for c in cpu['cpu_samples']):
        raise ValueError('historical CR0.WP observation required')
    leaves = []
    for at in range(0, SIZE, LARGE):
        base = PA+at
        first = None
        for small in range(0, LARGE, 4096):
            a, b = [effective_at(c, 2, base+small) for c in cpu['cpu_samples']]
            if (a != b or a['status'] != 'CLASSIFIED_OBSERVATION' or a['effective'] != 0):
                raise ValueError('changed, undefined or non-UC historical cache classification')
            if first is not None and a != first:
                raise ValueError('nonuniform MTRR/PAT type inside a large leaf')
            first = a
            entry = large_leaf(base)
            # Independent address/attribute expansion, including both PAT-bit locations.
            if ((entry & 0x000fffffffe00000)+small != base+small or
                    entry & (1 << 12) or (entry & ~0x80) != base | PTE_FLAGS or
                    ((entry >> 12) & 1)*4 + ((entry >> 4) & 1)*2 + ((entry >> 3) & 1) != 2):
                raise ValueError('large-leaf encoding differs from 4KiB mapping attributes')
        leaves.append(dict(virtual=VA+at, physical=base, raw=large_leaf(base),
                           size=LARGE, original_first_4k=base | PTE_FLAGS,
                           original_last_4k=(base+LARGE-4096) | PTE_FLAGS,
                           historical_cache=first))
    return dict(status='HOST_ECAM_MAPPING_PROPOSAL_ONLY',
                cache_receipt_sha256=hashlib.sha256(text.encode()).hexdigest(),
                mcfg_sha256=layout['mcfg_sha256'], virtual=VA, physical=PA, bytes=SIZE,
                proposed_leaves=leaves, detached_PD_entries=512,
                proposed_large_leaves=LEAVES, zero_unused_PD_entries=512-LEAVES,
                modeled_equivalent_4k_leaves=GRANULES,
                original_metadata_registration_attempts=GRANULES,
                prospective_PT_pages_avoided=78, PAT_index=2,
                invlpg_addresses_per_complete_publication_or_retirement=GRANULES,
                single_parent_slot=dict(pml4=(VA >> 39) & 511, pdpt=(VA >> 30) & 511,
                                        required_initial_raw=0),
                # Historical/host results cannot satisfy any live prerequisite.
                live_admission=False, native_mapping_authorized=False,
                all_aliases_proven=False, fresh_NX_state_proven=False,
                ownership=False, hardware_authority=False, mappings=0, device_reads=0,
                msr_writes=0, page_table_writes=0, native_retirement_proven=False,
                native_backend_implemented=False, unresolved=list(REQUIRED))


class PublicationModel:
    """Synthetic one-link transaction, never a backend or live proof.

    Model assumes an existing admitted borrowed PDPT. A missing PDPT is refused
    by the design; do not quietly allocate/publish another borrowed-path level.
    Exact raw equality includes A/D bits. Unexpected changes retain the frame.
    """
    def __init__(self, plan):
        expected = [large_leaf(PA+i*LARGE) for i in range(LEAVES)]
        if (plan.get('status') != 'HOST_ECAM_MAPPING_PROPOSAL_ONLY' or
                [x['raw'] for x in plan.get('proposed_leaves', [])] != expected):
            raise ValueError('exact modeled leaf vector required')
        self.expected = tuple(expected+[0]*(512-LEAVES))
        self.state = 'NEW'
        self.parent = 0
        self.frame = 0x2000000  # synthetic owned frame, never a real allocation
        self.link = self.frame | 3
        self.private = None
        self.owned = False
        self.freed = []
        self.invalidations = 0
        self.guards = dict(current_root=True, sole_root=True, CPU0=True,
                           irqs_masked=True, mutation_excluded=True, RAM_path_valid=True,
                           fresh_cache_NX=True, same_ECAM_coverage_and_cache_policy=True)

    def _refuse(self):
        if self.owned:
            self.freed.append(self.frame)
            self.owned = False
        self.private = None
        self.state = 'REFUSED'
        return False

    def _poison(self):
        self.state = 'POISONED'
        return False

    def prepare(self, fault=None):
        if fault not in (None, 'allocate', 'stage'):
            raise ValueError('modeled prepare fault')
        if self.state != 'NEW':
            return False
        if fault == 'allocate':
            return self._refuse()
        self.owned = True
        self.private = self.expected
        if fault == 'stage':
            return self._refuse()
        self.state = 'STAGED'
        return True

    def _invalidate(self, fail_at):
        # Count conceptual addresses VA+i*4096. No instruction or I/O executes.
        self.invalidations = GRANULES if fail_at is None else fail_at+1
        return fail_at is None

    def publish(self, fault=None, fail_invalidate_at=None):
        if fault not in (None, 'precheck', 'link', 'readback'):
            raise ValueError('modeled publication fault')
        if fail_invalidate_at is not None and (type(fail_invalidate_at) is not int or
                                             not 0 <= fail_invalidate_at < GRANULES):
            raise ValueError('modeled invalidation cursor')
        if self.state != 'STAGED':
            return False
        if (not all(self.guards.values()) or self.parent != 0 or
                self.private != self.expected or fault == 'precheck'):
            return self._refuse()
        self.parent = self.link
        if fault == 'link':
            return self._poison()
        if not self._invalidate(fail_invalidate_at) or fault == 'readback':
            return self._poison()
        self.state = 'READY'
        return True

    def retire_before_handoff(self, fault=None, fail_invalidate_at=None):
        if fault not in (None, 'unlink', 'readback'):
            raise ValueError('modeled retirement fault')
        if fail_invalidate_at is not None and (type(fail_invalidate_at) is not int or
                                             not 0 <= fail_invalidate_at < GRANULES):
            raise ValueError('modeled invalidation cursor')
        if self.state != 'READY':
            return False
        if self.parent != self.link or self.private != self.expected:
            return self._poison()
        if not all(self.guards.values()):
            return self._poison()
        self.parent = 0
        if fault == 'unlink':
            return self._poison()
        if not self._invalidate(fail_invalidate_at) or fault == 'readback':
            return self._poison()
        self.freed.append(self.frame)
        self.owned = False
        self.private = None
        self.state = 'RETIRED'
        return True

    def handoff(self):
        if (self.state != 'READY' or not all(self.guards.values()) or
                self.parent != self.link or self.private != self.expected):
            return self._poison() if self.state == 'READY' else False
        self.state = 'ACTIVE_UNTIL_REBOOT'
        return True


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exit-log', type=Path, required=True)
    parser.add_argument('--mcfg', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = propose(args.exit_log.read_text(), args.mcfg.read_bytes())
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print('HOST_ECAM_MAPPING_PROPOSAL_ONLY: 78 large leaves model 39936 unchanged 4KiB mappings; native admission blocked')
