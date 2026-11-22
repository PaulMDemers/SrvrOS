"""Interpret saved legacy VT-d bytes; never access hardware or grant authority.

Intel VT-d 4.1 sections 11.4.1--11.4.5. Only the captured VER=1.0 subset is
interpreted. Uninterpreted raw bits are retained, not silently treated as zero.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

from hardware_air61_register_sample import sample_result

OFFSETS = (0x1c, 0, 0xc, 8, 0xc, 0x14, 0x10, 0x14, 0x24, 0x20, 0x24, 0x1c)
REFERENCE = 'https://cdrdv2-public.intel.com/774206/vt-directed-io-spec%20.pdf'


def decode_sample(rows):
    """Decode a synthetic or historical sample; this function is no admission gate."""
    if not isinstance(rows, (list, tuple)) or len(rows) != 12:
        raise ValueError('fixed twelve-DWORD sample required')
    values = []
    for expected, row in zip(OFFSETS, rows):
        if not isinstance(row, (list, tuple)) or len(row) != 2:
            raise ValueError('offset/value pair required')
        offset, value = row
        if type(offset) is not int or offset != expected or type(value) is not int or not 0 <= value < 0xffffffff:
            raise ValueError('offset or DWORD unavailable/out of range')
        values.append(value)
    if any(values[a] != values[b] for a, b in ((0, 11), (2, 4), (5, 7), (8, 10))):
        raise ValueError('changed bracket')
    gsts, ver = values[:2]
    if ver != 0x10:
        raise ValueError('only captured legacy VER=1.0 is interpreted')
    cap = values[2] << 32 | values[3]
    ecap = values[5] << 32 | values[6]
    rtaddr = values[8] << 32 | values[9]
    nd = cap & 7
    if nd == 7:
        raise ValueError('reserved domain encoding')
    sagaw = cap >> 8 & 31
    return dict(
        interpretation='LEGACY_CAPTURED_BYTES_ONLY',
        raw=dict(VER=f'0x{ver:08x}', CAP=f'0x{cap:016x}', ECAP=f'0x{ecap:016x}',
                 GSTS=f'0x{gsts:08x}', RTADDR=f'0x{rtaddr:016x}'),
        version=dict(major=1, minor=0),
        capabilities=dict(domain_id_bits=4+2*nd, domains=1 << (4+2*nd),
                          sagaw_raw=sagaw,
                          known_adjusted_guest_widths=[w for b, w in ((1, 39), (2, 48)) if sagaw & (1 << b)],
                          sagaw_uninterpreted_bits=sagaw & ~6,
                          maximum_guest_address_bits=(cap >> 16 & 63)+1,
                          page_walk_coherent=bool(ecap & 1),
                          queued_invalidation=bool(ecap & 2),
                          interrupt_remapping=bool(ecap & 8),
                          extended_interrupt_mode=bool(ecap & 16),
                          pass_through=bool(ecap & 64)),
        sampled_status=dict(translation_enabled=bool(gsts & (1 << 31)),
                            root_pointer_status=bool(gsts & (1 << 30)),
                            queued_invalidation_enabled=bool(gsts & (1 << 26)),
                            interrupt_remapping_enabled=bool(gsts & (1 << 25)),
                            interrupt_table_pointer_status=bool(gsts & (1 << 24))),
        root_address_zero=rtaddr == 0,
        register_read_authorized=False, register_writes_authorized=False,
        root_table_walk_authorized=False, hardware_authority=False, ownership=False,
        full_interval_stability_proven=False, dma_quiescence_proven=False,
        firmware_exclusion_proven=False, root_register_is_loaded_table_proof=False)


def audit(window_text, ecam_text, exit_text, session, release):
    """Bind interpretation to the unchanged strict same-session native receipt."""
    if not re.fullmatch(r'[0-9a-f]{16}', release):
        raise ValueError('release identity')
    proof = sample_result(window_text, ecam_text, exit_text, session)
    if not proof or proof['status'] != 'OBSERVED_RETIRED_READ_ONLY_SAMPLE' or not proof['source_completed']:
        raise ValueError('complete strict R2 receipt required')
    options = [line.split('boot options=', 1)[1].split() for line in exit_text.splitlines()
               if line.startswith('laptop-capture: boot options=')]
    for row in options:
        if [s for s in row if s.startswith('srvros.desktop-release=')] != ['srvros.desktop-release='+release]:
            raise ValueError('release mismatch/ambiguity')
    if not options:
        raise ValueError('missing release binding')
    decoded = [decode_sample(rows) for rows in proof['raw_samples']]
    if len(decoded) != 2 or decoded[0] != decoded[1]:
        raise ValueError('changed interpretation')
    if not proof['retirement_verified'] or not proof['claim_released']:
        raise ValueError('retirement required')
    return dict(status='HISTORICAL_CAPABILITY_AUDIT', session=session, release=release,
                reference=REFERENCE, reference_sections='11.4.1--11.4.5',
                unit='DMAR DRHD0 graphics-specific unit only; not DRHD1',
                strict_receipt_status=proof['status'], target_reads=24, target_writes=0,
                input_sha256={name: hashlib.sha256(text.encode()).hexdigest()
                              for name, text in (('window', window_text), ('ecam', ecam_text), ('exit', exit_text))},
                sample=decoded[0], samples_equal=True,
                lifetimes_us=[p['end']-p['begin'] for p in proof['passes']],
                mapper_words=[p['words'] for p in proof['passes']], gap_us=proof['gap_us'],
                retirement_verified=True, claim_released=True,
                register_read_authorized=False, register_writes_authorized=False,
                root_table_walk_authorized=False, hardware_authority=False, ownership=False,
                next_prerequisite='Ordinary desktop regression for shared mapping registry; separate ownership and DMA/firmware exclusion before any mutation')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('window', 'ecam', 'exit'):
        p.add_argument('--'+name, required=True, type=Path)
    p.add_argument('--session', required=True)
    p.add_argument('--release', required=True)
    args = p.parse_args()
    # Decode exactly the bytes whose hashes are retained; refuse newline rewriting.
    inputs = [path.read_bytes().decode('utf-8') for path in (args.window, args.ecam, args.exit)]
    try:
        result = audit(*inputs, args.session, args.release)
    except ValueError as error:
        p.exit(1, 'audit refused: '+str(error)+'\n')
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == '__main__':
    main()
