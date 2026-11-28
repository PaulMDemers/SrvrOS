"""Decode the Air6,1 read-only GGTT candidate receipt."""

import argparse
import json
from pathlib import Path
import re
import struct

from hardware_haswell_backing import backing_result, canonical

FIELDS = (
    "frame pte edp_conf edp_func ddi_a primary surf live primary_b primary_c "
    "sprite_a sprite_b sprite_c cursor_a cursor_b cursor_c psr "
    "igd_cmd igd_pm igd_cap igd_devsta hda_cmd hda_pm hda_cap hda_devsta"
).split()
WORD_MAX = 2**32 - 1


def _requester(row, name):
    cmd, pm, cap, status = (row[f"{name}_{field}"] for field in ("cmd", "pm", "cap", "devsta"))
    if cmd >= 0xffff or pm >= 0xffff:
        raise ValueError("Invalid PCI configuration")
    if cap:
        if cap not in range(0x40, 0xf8, 4) or status >= 0xffff:
            raise ValueError("Invalid PCIe status")
    elif status != WORD_MAX:
        raise ValueError("Unqualified PCIe status")
    return {"bus_master": bool(cmd & 4), "d_state": pm & 3,
            "transactions_pending": bool(status & 0x20) if cap else None}


def _parse(text, boot_slot):
    slot, offset = (16385, 0x210004) if boot_slot else (4096, 0x204000)
    if boot_slot:
        options = [line.split("boot options=", 1)[1].split() for line in text.splitlines()
                   if line.startswith("laptop-capture: boot options=")]
        if not options or any(row.count("srvros.hsw-boot-slot=read-only") != 1 or
                              row.count("srvros.hsw-candidate=read-only") != 1 or
                              "srvros.hsw-access=read-only" in row for row in options):
            return None
    summaries = [line for line in text.splitlines() if line.startswith("hsw-candidate:")]
    if len(summaries) != 1:
        return None
    match = re.fullmatch(
        rf"hsw-candidate: v=1 status=([0-5]) rows=([0-2]) slot={slot} mmio={offset:#x} "
        r"begin=(\d+) end=(\d+) mapped=([01]) retired=([01]) writes=0 authority=none", summaries[0])
    if not match:
        return None
    status, count, begin, end, mapped, retired = map(int, match.groups())
    samples = []
    for line in text.splitlines():
        if not line.startswith("hsw-candidate-row:"):
            continue
        row = re.fullmatch(r"hsw-candidate-row: phase=([01]) data=([A-Za-z0-9+/]+={0,2})", line)
        if not row or int(row[1]) != len(samples):
            return None
        values = struct.unpack("<25I", canonical(row[2], 100))
        samples.append(dict(zip(FIELDS, values)))
    if (len(samples) != count or max(begin, end) >= 2**64 or retired > mapped or
            (count and not mapped) or (mapped and not retired and status != 5)):
        return None
    backing = backing_result(text, require_completion=status == 1)
    if not backing:
        return None
    result = dict(slot=slot, mmio_offset=offset, hardware_authority=False, writes=0,
                  status="OBSERVED" if status == 1 else "REFUSED" if status == 0 else "FAILED",
                  source_status=status, mapped=bool(mapped), retired=bool(retired),
                  samples=samples, elapsed_us=end-begin)
    if not status:
        return result if not (count or begin or end or mapped) else None
    if status != 1:
        return result
    if (backing["status"] != "OBSERVED" or backing.get("source_version") != 3 or
            backing["count"] != 1056 or backing["offset"] != 0 or
            not backing["stolen_range_reserved"] or not backing["all_stolen_candidates"] or
            backing["tilectl_raw"] != [0, 0] or count != 2 or not retired or
            not begin or not 20000 <= end - begin <= 100000):
        return None
    for sample in backing["samples"]:
        if sample["fences"] != [0x00fff02b00000001] + [0]*31:
            return None
    before, after = samples
    frame_delta = (after["frame"] - before["frame"]) & WORD_MAX
    if not 1 <= frame_delta <= 8 or any(before[key] != after[key] for key in FIELDS
                                       if key not in ("frame", "igd_devsta", "hda_devsta")):
        return None
    for sample in samples:
        if (tuple(sample[key] for key in ("edp_conf", "edp_func", "ddi_a", "primary", "surf", "live")) !=
                (0xc0000018, 0x82200000, 0x80000011, 0x98004400, 0, 0) or
                any(sample[key] == WORD_MAX for key in FIELDS[1:17]) or
                not sample["igd_cmd"] & 2 or sample["igd_pm"] & 3):
            return None
    result.update(frame_delta=frame_delta, pte=before["pte"], present=bool(before["pte"] & 1),
                  psr_enabled=bool(before["psr"] & (1 << 31)),
                  requesters=[{name: _requester(sample, name) for name in ("igd", "hda")} for sample in samples],
                  unresolved=["firmware/SMM", "engine and display DMA retirement", "VT-d state", "FBC"])
    return result


def candidate_result(text, *, boot_slot=False):
    try:
        return _parse(text, boot_slot)
    except (ValueError, KeyError, struct.error):
        return None


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("receipt", type=Path)
    parser.add_argument("--boot-slot", action="store_true", help="Require the separate slot 16385 profile")
    args = parser.parse_args()
    result = candidate_result(args.receipt.read_text(), boot_slot=args.boot_slot)
    if not result:
        parser.exit(1, "Invalid or incomplete Haswell candidate receipt\n")
    print(json.dumps(result, indent=2))
