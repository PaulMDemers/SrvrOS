"""Offline ECAM layout lower bound; never a native mapping/alias permission."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def audit_mcfg(data):
    """Model the current 4KiB PCI mapper from a complete archived ACPI MCFG.

    Count unique virtual PT regions, not devices or public mapping-list entries.
    This is a source-derived lower bound, not a reconstruction of live tables.
    """
    if (len(data) < 60 or data[:4] != b'MCFG' or
            struct.unpack_from('<I', data, 4)[0] != len(data) or
            (len(data)-44) % 16 or sum(data) % 256 or data[8] != 1 or
            any(data[36:44])):
        raise ValueError('complete checksummed revision-1 MCFG required')
    count = (len(data)-44)//16
    if count > 8:
        raise ValueError('unmodeled allocations beyond mapper capacity')
    windows = []
    regions = set()
    for i in range(count):
        base, segment, start, end, reserved = struct.unpack_from('<QHBBI', data, 44+16*i)
        if not base or base & ((1 << 20)-1) or end < start or reserved:
            raise ValueError('invalid MCFG allocation')
        buses = end-start+1
        size = buses << 20
        if base+size > 1 << 39:
            raise ValueError('outside modeled Air61 physical width')
        if any(base < w['physical']+w['bytes'] and w['physical'] < base+size for w in windows):
            raise ValueError('overlapping physical ECAM windows are unmodeled')
        va = 0xffffc00300000000 + i*0x10000000
        spans = set(range(va >> 21, ((va+size-1) >> 21)+1))
        regions.update(spans)
        windows.append(dict(segment=segment, start_bus=start, end_bus=end,
                            physical=base, virtual=va, bytes=size,
                            mapped_4k_leaves=size//4096, leaf_table_pages=len(spans)))
    leaf_pages = len(regions)
    # Every distinct 2MiB VA region needs its own PT: these contiguous physical
    # windows have different PTE contents, so sharing one cannot implement them.
    minimum_pages = leaf_pages + 3  # at least root, one PDPT, one PD
    return dict(status='HOST_LAYOUT_AUDIT_ONLY', mcfg_sha256=hashlib.sha256(data).hexdigest(),
                source_assumption='current pci.map_ecam_windows and VMM 4KiB mapper complete successfully',
                windows=windows, ecam_leaf_table_pages=leaf_pages,
                minimum_root_graph_pages=minimum_pages,
                minimum_entry_loads_per_full_pass=512*minimum_pages,
                table_cap=64, entry_cap_per_pass=32768,
                fits_current_full_graph_bounds=minimum_pages <= 64,
                live_table_graph_observed=False, all_aliases_proven=False,
                native_mapping_authorized=False, hardware_authority=False, ownership=False,
                device_reads=0, mappings=0, policy_writes=0)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mcfg', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = audit_mcfg(args.mcfg.read_bytes())
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(f"HOST_LAYOUT_AUDIT_ONLY: ECAM needs {result['ecam_leaf_table_pages']} leaf tables; "
          f"full graph >= {result['minimum_root_graph_pages']} pages; no native authority")
