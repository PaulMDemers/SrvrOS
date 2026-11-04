"""Host-only inherited-layout proposal and synthetic exclusion model.

No native managed ranges, allocator import, register access, pin or owner grant.
All simulated operations are serialized by the host caller; tokens are local
objects, not hardware completion evidence or a persistence/wire protocol.
"""
import argparse
from dataclasses import dataclass, field
import hashlib
import json
from pathlib import Path

from hardware_haswell_backing import backing_result
from haswell_backing_audit import fence_fields, unsigned

PAGE = 4096
APERTURE = 1 << 28


def layout_proposal(text):
    observed = backing_result(text)
    if not observed or observed['status'] != 'OBSERVED' or observed.get('source_version') != 3:
        raise ValueError('complete version3 observation required')
    e = observed['samples'][0]['endpoint']
    if (e['width'], e['height'], e['pitch'], observed['offset'], observed['bytes'], observed['count']) != (1366, 768, 5632, 0, 4325376, 1056):
        raise ValueError('outside captured Air6,1 layout')
    if not (observed['all_present'] and observed['all_stolen_candidates'] and observed['stolen_range_reserved']):
        raise ValueError('unbound backing candidates')
    if observed['linux_model_fence_candidates'] != [0] or not observed['linux_model_unique_candidate']:
        raise ValueError('ambiguous Linux-model fence candidate')
    if observed['tilectl_raw'] != [0, 0] or (e['disparb'] >> 13) & 3:
        raise ValueError('outside captured raw CPU/display controls')
    pages = observed['samples'][0]['address_candidates']
    if any(p != pages[0] + PAGE * i for i, p in enumerate(pages)):
        raise ValueError('aliased or nonconsecutive backing')
    fence = fence_fields(observed['samples'][0]['fences'][0])
    if (fence['start'], fence['end'], fence['prm_pitch'], fence['linux_pitch']) != (0, 16 << 20, 5632, 5632):
        raise ValueError('outside captured fence layout')
    return dict(status='HOST_PROPOSAL_ONLY', receipt_sha256=hashlib.sha256(text.encode()).hexdigest(),
                surface_ggtt=[0, observed['bytes']], protected_ggtt=[fence['start'], fence['end']],
                physical_pages=pages, physical_surface=[pages[0], pages[-1] + PAGE],
                padded_rows=768, page_count=len(pages),
                extra_fence_bytes=fence['end'] - observed['bytes'],
                extra_fence_physical_pages_known=False, native_managed_ranges=[],
                hardware_authority=False, ownership=False, pin=False, physical_acceptance=False)


@dataclass(frozen=True, eq=False)
class ClaimToken:
    catalog: object
    generation: int
    slot: int


@dataclass(frozen=True, eq=False)
class ReaderToken:
    claim: ClaimToken
    serial: int


@dataclass
class _Claim:
    token: ClaimToken
    ggtt: tuple
    pages: frozenset
    state: str = 'active'
    readers: dict = field(default_factory=dict)


class SyntheticReservations:
    """Finite host simulation only. Quarantined ranges cannot be reclaimed."""
    SLOTS = 8

    def __init__(self):
        self._identity = object()
        self._generation = 0
        self._serial = 0
        self._slots = [None] * self.SLOTS

    def propose(self, ggtt, physical_pages):
        # Validate the entire request before changing the catalog. Protect the
        # full inherited fence, but do not invent physical pages for its excess.
        if not isinstance(ggtt, (tuple, list)) or len(ggtt) != 2:
            raise ValueError('GGTT interval')
        start, end = (unsigned(v, 64) for v in ggtt)
        if start % PAGE or end % PAGE or not 0 <= start < end <= APERTURE:
            raise ValueError('GGTT interval')
        if not isinstance(physical_pages, (tuple, list)) or not 1 <= len(physical_pages) <= 1536:
            raise ValueError('physical page vector')
        pages = tuple(unsigned(v, 39) for v in physical_pages)
        if any(v % PAGE for v in pages) or len(set(pages)) != len(pages):
            raise ValueError('physical alias/alignment')
        pages = frozenset(pages)
        for claim in self._slots:
            if claim and claim.state != 'retired':
                if start < claim.ggtt[1] and claim.ggtt[0] < end:
                    raise ValueError('GGTT collision')
                if pages & claim.pages:
                    raise ValueError('physical alias')
        slot = next((i for i, c in enumerate(self._slots) if c is None or c.state == 'retired'), None)
        if slot is None or self._generation == (1 << 64) - 1:
            raise ValueError('catalog exhausted')
        self._generation += 1
        token = ClaimToken(self._identity, self._generation, slot)
        self._slots[slot] = _Claim(token, (start, end), pages)
        return token

    def _get(self, token):
        if not isinstance(token, ClaimToken) or token.catalog is not self._identity or type(token.slot) is not int or not 0 <= token.slot < self.SLOTS:
            raise ValueError('foreign claim')
        claim = self._slots[token.slot]
        if claim is None or claim.token is not token or claim.state in ('retired', 'quarantined'):
            raise ValueError('stale or quarantined claim')
        return claim

    def hold(self, token):
        claim = self._get(token)
        if claim.state != 'active' or len(claim.readers) == 8 or self._serial == (1 << 64) - 1:
            raise ValueError('reader admission')
        self._serial += 1
        reader = ReaderToken(token, self._serial)
        claim.readers[reader] = False
        return reader

    def _reader(self, reader):
        if not isinstance(reader, ReaderToken):
            raise ValueError('reader token')
        claim = self._get(reader.claim)
        if reader not in claim.readers:
            raise ValueError('stale reader')
        return claim

    def begin(self, reader):
        claim = self._reader(reader)
        if claim.state != 'active' or claim.readers[reader]:
            raise ValueError('reader begin')
        claim.readers[reader] = True

    def end(self, reader):
        claim = self._reader(reader)
        if not claim.readers[reader]:
            raise ValueError('reader not busy')
        claim.readers[reader] = False

    def release(self, reader):
        claim = self._reader(reader)
        if claim.readers[reader]:
            raise ValueError('reader busy')
        del claim.readers[reader]

    def close(self, token):
        claim = self._get(token)
        if claim.state != 'active':
            raise ValueError('already closing')
        claim.state = 'closing'

    def retire(self, token, *, certain):
        claim = self._get(token)
        if claim.state != 'closing' or type(certain) is not bool:
            raise ValueError('retirement state')
        if not certain:
            claim.state = 'quarantined'
            return
        if claim.readers:
            raise ValueError('readers retained')
        claim.state = 'retired'


def simulate(text):
    proposal = layout_proposal(text)
    catalog = SyntheticReservations()
    token = catalog.propose(proposal['protected_ggtt'], proposal['physical_pages'])
    reader = catalog.hold(token)
    catalog.begin(reader)
    catalog.close(token)
    catalog.end(reader)
    catalog.release(reader)
    catalog.retire(token, certain=True)
    proposal['simulated_lifetime'] = 'retired; host only, no hardware completion proof'
    return proposal


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exit-log', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = simulate(args.exit_log.read_text())
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('HOST_PROPOSAL_ONLY: simulated lifetime retired; no native ownership')
