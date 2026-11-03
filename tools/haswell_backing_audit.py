#!/usr/bin/env python3
"""Host-only Haswell backing audit arithmetic, never a native receipt/admission.

Intel Haswell Memory Views pp35-36/46, Register Reference pp858-862;
Linux 028ef9c96e96 intel_gtt.h/i915_drm.h/intel_ggtt.c/fencing.c.
No MMIO, data mapping, writer, allocation, pin or hardware authority API.
"""
import argparse
import json
from pathlib import Path


def unsigned(value, bits):
    if type(value) is not int or not 0 <= value < 1 << bits:
        raise ValueError('unsigned field width')
    return value


def pte_fields(raw):
    raw = unsigned(raw, 32)
    if raw == 0xffffffff:
        raise ValueError('ambiguous all-ones read')
    # HSW bit11 is CC[3]. Generic pre-HSW 0xff0 extension is inappropriate.
    present = bool(raw & 1)
    return dict(raw=raw, present=present,
                address_candidate=((raw & 0xfffff000) | ((raw & 0x7f0) << 28)) if present else None,
                cache_control=((raw >> 1) & 7) | ((raw >> 8) & 8),
                ownership=False)


def ggc_fields(raw):
    raw = unsigned(raw, 16)
    ggms, gms = (raw >> 8) & 3, (raw >> 3) & 31
    # Retain numeric fields; do not validate reserved encodings or stolen bounds
    # without the actual unit's host/GPU config and firmware-map evidence.
    return dict(raw=raw, ggms=ggms, gms=gms,
                table_bytes_candidate=ggms << 20,
                stolen_bytes_candidate=gms << 25,
                range_validated=False)


def plane_extent(sample):
    raw, fb = sample['raw'], sample['framebuffer']
    if len(raw) != 9 or len(fb) != 7:
        raise ValueError('fixed v9 field shape')
    for n in raw: unsigned(n, 32)
    for n in fb: unsigned(n, 64)
    control, stride, legacy, surface, offset, source, live, again, surf_again = raw
    count, va, pa, width, height, pitch, span = fb
    if sample['state'] != 1 or sample['reads'] != 9 or sample['bpp'] != 32 or sample['color'] != [1,8,16,8,8,8,0]:
        raise ValueError('complete RGB32 endpoint required')
    if count != 1 or not 0 < width <= 4096 or not 0 < height <= 4096 or not width * 4 <= pitch <= 65536 or span != pitch * height:
        raise ValueError('descriptor extent')
    if control != again or surface != surf_again or surface != live or control & (1 << 31) == 0 or (control >> 26) & 15 != 6:
        raise ValueError('primary endpoint/format')
    if control & ((1 << 15) | (1 << 20) | (1 << 25) | (1 << 9)) or offset or surface & 4095:
        raise ValueError('unsupported pilot route')
    if source != ((width - 1) << 16) | (height - 1) or stride != pitch:
        raise ValueError('geometry mismatch')
    tiled = bool(control & (1 << 10))
    if pitch % (512 if tiled else 64):
        raise ValueError('stride alignment')
    rows = (height + 7) & ~7 if tiled else height
    size = pitch * rows
    pages = (size + 4095) // 4096
    if not 0 < pages <= 1536 or surface + size > 0x10000000:
        raise ValueError('proposed readout capacity; not a change to existing profile')
    pci = sample['pci']
    if len(pci) != 4 or pci[0] != pci[2] or pci[1] or pci[3] or pci[0] & 0xfffffff != 12 or pa != (pci[0] & ~15) + surface:
        raise ValueError('CPU aperture relation')
    return dict(tiled=tiled, offset=surface, padded_rows=rows, span=size, pages=pages,
                pte_width=4, first_mmio=0x200000 + surface // 4096 * 4,
                end_mmio=0x200000 + (surface // 4096 + pages) * 4,
                ggtt_proven=False, swizzle_known=False, hardware_authority=False)


def fence_fields(raw):
    raw = unsigned(raw, 64)
    if raw == (1 << 64) - 1:
        raise ValueError('ambiguous all-ones fence')
    high, low = raw >> 32, raw & 0xffffffff
    return dict(raw=raw, valid=bool(low & 1), y_tiled=bool(low & 2),
                start=low & 0xfffff000, end=(high & 0xfffff000) + 4096,
                prm_pitch=((high & 0x3ff) + 1) * 128,
                linux_pitch=((high & 0x7ff) + 1) * 128,
                reserved_low=bool(low & 0xffc),
                pitch_discrepancy=bool(high & 0x400), reserved_high=bool(high & 0x800))


def fence_candidates(values, extent, pitch):
    if len(values) not in (16, 32):
        raise ValueError('reference fence-table shapes only')
    rows = [fence_fields(x) for x in values]
    start, end = extent['offset'], extent['offset'] + extent['span']
    overlap = [i for i, r in enumerate(rows) if r['valid'] and r['start'] < end and start < r['end']]
    matches = [i for i in overlap if not rows[i]['y_tiled'] and not rows[i]['reserved_low'] and
               not rows[i]['reserved_high'] and not rows[i]['pitch_discrepancy'] and
               rows[i]['start'] == start and rows[i]['end'] >= end and rows[i]['prm_pitch'] == pitch]
    return dict(observed_fences=len(rows), unobserved_upper_fences=len(rows) == 16,
                overlaps=overlap, matches=matches,
                unique_candidate=len(values) == 32 and len(overlap) == len(matches) == 1,
                reference_discrepancy_unresolved=True, ownership=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--backing-json', type=Path, required=True)
    args = parser.parse_args()
    observed = json.loads(args.backing_json.read_text())
    if observed['status'] != 'OBSERVED' or not observed['stable'] or observed['hardware_authority']:
        raise ValueError('stable no-authority v9 observation required')
    extents = [plane_extent(s) for s in observed['samples']]
    if len(extents) != 2 or extents[0] != extents[1]:
        raise ValueError('endpoint extent changed')
    print(json.dumps(dict(status='HOST_AUDIT_ONLY', physical_acceptance=False,
                         extent=extents[0], fences='not captured', ggc='not captured',
                         ptes='not captured', hardware_authority=False), indent=2))


if __name__ == '__main__':
    main()
