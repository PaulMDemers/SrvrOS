"""Classify the bounded Haswell slot-access observations without inferring ownership."""
import argparse
from pathlib import Path
import json
import re
import struct

from hardware_haswell_backing import backing_result, canonical
from hardware_haswell_candidate import candidate_result, WORD_MAX
from hardware_haswell_firmware import firmware_result
from haswell_backing_audit import pte_fields

SLOT = 4096
ENTRIES = 16
PRIOR_PTE = 0x8EA00001


def _rmrrs(raw):
    result = []
    offset = 48  # Fixed ACPI DMAR header and reserved fields.
    while offset < len(raw):
        kind, size = struct.unpack_from("<HH", raw, offset)
        end = offset + size
        if size < 4 or end > len(raw):
            raise ValueError("Invalid DMAR structure length")
        if kind == 1:
            if size <= 24:
                raise ValueError("Short RMRR or missing scope")
            reserved, segment, base, limit = struct.unpack_from("<HHQQ", raw, offset + 4)
            if reserved or base & 4095 or (limit + 1) & 4095 or limit <= base or limit == 2**64 - 1:
                raise ValueError("Invalid RMRR range")
            scopes = []
            i = offset + 24
            while i < end:
                if i + 6 > end:
                    raise ValueError("Short DMAR device scope")
                dtype, length, _, _, _, bus = struct.unpack_from("<6B", raw, i)
                if length < 8 or length % 2 or i + length > end:
                    raise ValueError("Invalid DMAR device scope length")
                path = [tuple(raw[j:j + 2]) for j in range(i + 6, i + length, 2)]
                scopes.append(dict(kind=dtype, bus=bus, path=path))
                i += length
            result.append(dict(segment=segment, base=base, limit=limit, scopes=scopes))
        offset = end
    return result


def _parse(text):
    summaries = [line for line in text.splitlines() if line.startswith("hsw-access:")]
    if len(summaries) != 1:
        return None
    match = re.fullmatch(r"hsw-access: v=1 observed=([01]) fbc_rows=([0-2]) entry_rows=([0-2]) "
                         r"fbc_maps=([0-2]) fbc_retired=([0-2]) writes=0 authority=none", summaries[0])
    if not match:
        return None
    observed, fbc_count, entry_count, maps, retired = map(int, match.groups())
    rows = {"fbc": [], "entries": []}
    for line in text.splitlines():
        if not line.startswith(("hsw-access-fbc:", "hsw-access-entries:")):
            continue
        record = re.fullmatch(r"hsw-access-(fbc|entries): phase=([01]) data=([A-Za-z0-9+/]+={0,2})", line)
        if not record or int(record[2]) != len(rows[record[1]]):
            return None
        width = 3 if record[1] == "fbc" else ENTRIES
        data = struct.unpack(f"<{width}I", canonical(record[3], width * 4))
        if WORD_MAX in data:
            return None
        rows[record[1]].append(data)

    candidate = candidate_result(text)
    if (not candidate or len(rows["fbc"]) != fbc_count or len(rows["entries"]) != entry_count or
            fbc_count > retired or retired > maps):
        return None
    status = "OBSERVED" if observed else "REFUSED" if candidate["status"] == "REFUSED" else "FAILED"
    result = dict(status=status, slot=SLOT, fbc_maps=maps, fbc_retired=retired,
                  raw=rows, hardware_authority=False)
    if not observed:
        if candidate["status"] == "OBSERVED" or (status == "REFUSED" and (maps or entry_count)):
            return None
        return result

    firmware = firmware_result(text)
    backing = backing_result(text)
    if (candidate["status"] != "OBSERVED" or (fbc_count, entry_count, maps, retired) != (2, 2, 2, 2) or
            not firmware or firmware["status"] != "OBSERVED" or not backing or
            rows["entries"][0] != rows["entries"][1] or
            rows["entries"][0][0] != candidate["pte"] or rows["fbc"][0][:2] != rows["fbc"][1][:2]):
        return None
    target = pte_fields(candidate["pte"])["address_candidate"]
    stolen_start, stolen_end = backing["stolen_bounds"]
    ctl, base, _ = rows["fbc"][0]
    # The programmed value is a stolen-memory offset. Reserved low/high bits
    # or an offset outside stolen RAM make its address unavailable.
    fbc_base = stolen_start + base if not base & ~0x0ffff000 and stolen_start + base < stolen_end else None
    relation = "unavailable" if target is None or fbc_base is None else (
        "buffer_start" if target == fbc_base else "below_buffer_start" if target < fbc_base else "extent_unproved")

    raw_dmar = canonical(firmware["samples"][0]["dmar_base64"], 136)
    covers = [rmrr for rmrr in _rmrrs(raw_dmar)
              if target is not None and rmrr["base"] <= target and target + 4095 <= rmrr["limit"]]
    aliases = [SLOT + index for index, pte in enumerate(rows["entries"][0][1:], 1)
               if target is not None and pte_fields(pte)["address_candidate"] == target]
    inherited_aliases = [index for index, pa in enumerate(backing["samples"][0]["address_candidates"])
                         if target is not None and pa == target]
    result.update(candidate=candidate, target=target, prior_entry_matches=candidate["pte"] == PRIOR_PTE,
                  declared_rmrrs_covering_target=covers, neighboring_aliases=aliases,
                  inherited_image_aliases=inherited_aliases,
                  fbc=dict(enabled=bool(ctl & 1 << 31), buffer_base=fbc_base, relation=relation),
                  unresolved=["GGTT aliases outside the sampled windows", "FBC allocation extent",
                              "live VT-d translation", "GT and display DMA retirement", "firmware/SMM"])
    return result


def access_result(text):
    try:
        return _parse(text)
    except (ValueError, KeyError, struct.error):
        return None


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("receipt", type=Path)
    args = parser.parse_args()
    result = access_result(args.receipt.read_text())
    if result is None:
        parser.exit(1, "Invalid or incomplete Haswell slot-access receipt\n")
    print(json.dumps(result, indent=2))
