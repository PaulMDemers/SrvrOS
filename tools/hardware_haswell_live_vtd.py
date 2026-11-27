"""Bind the same-boot graphics DRHD0 register sample to slot 4096 and IGD status."""

import argparse
import hashlib
import json
from pathlib import Path
import re

from hardware_air61_register_sample import sample_result
from hardware_haswell_access import access_result
from hardware_haswell_candidate import WORD_MAX
from haswell_vtd_capability_audit import decode_sample


def _rows(text, prefix, pattern):
    rows = []
    for line in text.splitlines():
        if not line.startswith(prefix):
            continue
        row = re.fullmatch(pattern, line)
        if not row or int(row[1]) != len(rows):
            raise ValueError("Requester row framing")
        rows.append(tuple(map(int, row.groups()[1:])))
    return rows


def _decode_af(offset, header, control_status):
    if not offset:
        if header or control_status:
            raise ValueError("missing AF framing")
        return dict(offset=None, transactions_pending=None)
    length = header >> 16 & 255
    next_offset = header >> 8 & 255
    if (offset not in range(0x40, 0xfc, 4) or not 0 <= header <= WORD_MAX or header & 255 != 0x13 or
            not 6 <= length <= 256-offset or not 0 <= control_status <= 0xffff or
            (next_offset and (next_offset not in range(0x40, 0x100, 4) or offset <= next_offset < offset+length))):
        raise ValueError("AF capability bounds")
    return dict(offset=offset, header=header, control_status=control_status,
                transactions_pending=bool(control_status & 0x100) if header & (1 << 24) else None)


def _observe_requesters(text, bracket):
    meta = [line for line in text.splitlines() if line.startswith("hsw-live-observe:")]
    rows = _rows(text, "hsw-live-af:", r"hsw-live-af: phase=([01]) igd=(\d+),(\d+),(\d+) hda=(\d+),(\d+),(\d+)")
    if not meta and not rows:
        return None  # Historical live receipts precede this observation.
    if len(meta) != 1:
        raise ValueError("requester observation summary")
    match = re.fullmatch(r"hsw-live-observe: v=1 complete=([01]) cpu=0 begin=(\d+) end=(\d+) "
                         r"smi=(\d+),(\d+) writes=0 authority=none", meta[0])
    if not match:
        raise ValueError("requester observation framing")
    complete, begin, end, before, after = map(int, match.groups())
    if bracket is None:
        if any((complete, begin, end, before, after, rows)):
            raise ValueError("incomplete requester observation")
        return dict(status="NOT_SAMPLED")
    if not complete or (begin, end) != bracket or len(rows) != 2 or max(before, after) > WORD_MAX:
        raise ValueError("requester observation bracket")
    return dict(status="OBSERVED", cpu=0, begin=begin, end=end, smi=[before, after],
                smi_delta=(after-before) & WORD_MAX,
                af=[dict(igd=_decode_af(*row[:3]), hda=_decode_af(*row[3:])) for row in rows],
                requester_drain_proven=False, firmware_exclusion_proven=False)


def _parse(window, ecam, exit_log, session, release):
    if not re.fullmatch(r"[0-9a-f]{16}", release):
        return None
    options = [line.split("boot options=", 1)[1].split() for line in exit_log.splitlines()
               if line.startswith("laptop-capture: boot options=")]
    if not options or any(
        [t for t in row if t.startswith("srvros.desktop-release=")] != [f"srvros.desktop-release={release}"] or
        row.count("srvros.hsw-live-vtd=read-only") != 1 for row in options
    ):
        return None
    summaries = [line for line in exit_log.splitlines() if line.startswith("hsw-live:")]
    if len(summaries) != 1:
        return None
    match = re.fullmatch(r"hsw-live: v=1 observed=([01]) requester_rows=([0-2]) gt_rows=([0-2]) "
                         r"writes=0 authority=none", summaries[0])
    if not match:
        return None
    observed, requester_count, gt_count = map(int, match.groups())
    requester = _rows(exit_log, "hsw-live-requester:", r"hsw-live-requester: phase=([01]) "
                      r"begin=(\d+) end=(\d+) cmd=(\d+) pm=(\d+) cap=(\d+) devsta=(\d+)")
    gt = _rows(exit_log, "hsw-live-gt:", r"hsw-live-gt: phase=([01]) core=(\d+) rc6=(\d+)")
    if len(requester) != requester_count or len(gt) != gt_count:
        return None

    gpu = []
    for begin, end, cmd, pm, cap, devsta in requester:
        if not 0 < begin <= end < 2**64 or not 0 <= cmd < 0xffff or not cmd & 2 or not 0 <= pm < 0xffff or pm & 3:
            return None
        if cap and (cap not in range(0x40, 0xf8, 4) or devsta >= 0xffff) or not cap and devsta != WORD_MAX:
            return None
        gpu.append(dict(begin=begin, end=end, command=cmd, pmcsr=pm, pcie_cap=cap,
                        device_status=devsta if cap else None, bus_master=bool(cmd & 4),
                        transactions_pending=bool(devsta & 0x20) if cap else None))
    if any(value >= WORD_MAX for row in gt for value in row):
        return None

    sample = sample_result(window, ecam, exit_log, session)
    result = dict(slot=4096, session=session, release=release, requesters=gpu,
                  gt=[dict(core=row[0], rc6_raw=row[1]) for row in gt], hardware_authority=False)
    if not observed:
        if "laptop-capture: completed=yes" in exit_log:
            return None
        status = "REFUSED" if sample and sample["status"] == "REFUSED" and not requester and not gt else "INCOMPLETE"
        result.update(status=status, remapper_status=sample["status"] if sample else "unverified")
        result["requester_observation"] = _observe_requesters(exit_log, None)
        return result

    access = access_result(exit_log)
    if (not sample or sample["status"] != "OBSERVED_RETIRED_READ_ONLY_SAMPLE" or
            not sample["retirement_verified"] or not sample["claim_released"] or
            not access or access["status"] != "OBSERVED" or (requester_count, gt_count) != (2, 2)):
        return None
    rows = access["candidate"]["samples"]
    slot_log, = [line for line in exit_log.splitlines() if line.startswith("hsw-candidate:")]
    start, end = map(int, re.search(r" begin=(\d+) end=(\d+) ", slot_log).groups())
    if not requester[0][1] <= sample["begin"] < sample["end"] <= requester[1][0] <= requester[1][1] < start:
        return None
    if any(req[2:5] != tuple(row[key] for key in ("igd_cmd", "igd_pm", "igd_cap"))
           for req, row in zip(requester, rows)):
        return None

    decoded = [decode_sample(row) for row in sample["raw_samples"]]
    if len(decoded) != 2 or decoded[0] != decoded[1]:
        return None
    gpu_scope = {"kind": 1, "bus": 0, "path": [(2, 0)]}
    target_in_igd_rmrr = any(r["segment"] == 0 and gpu_scope in r["scopes"]
                             for r in access["declared_rmrrs_covering_target"])
    result.update(status="OBSERVED", slot_access=access, target_in_igd_rmrr=target_in_igd_rmrr,
                  requester_observation=_observe_requesters(exit_log, (start, end)),
                  remapper="DMAR DRHD0 graphics unit", remapper_sample=decoded[0],
                  remapper_reads=24, remapper_writes=0, candidate_gap_us=start - sample["end"],
                  input_sha256={name: hashlib.sha256(data.encode()).hexdigest()
                                for name, data in (("window", window), ("ecam", ecam), ("exit", exit_log))},
                  unresolved=["live translation of the slot target", "GT queues and DMA retirement",
                              "firmware/SMM access", "other remapping units"])
    return result


def live_result(window, ecam, exit_log, session, release):
    try:
        return _parse(window, ecam, exit_log, session, release)
    except (ValueError, TypeError, KeyError, IndexError):
        return None


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="Matched session containing window.log, ecam.log and exit.log")
    parser.add_argument("--release", required=True)
    args = parser.parse_args()
    logs = [(args.directory / name).read_bytes().decode("utf-8")
            for name in ("window.log", "ecam.log", "exit.log")]
    proof = live_result(*logs, args.directory.name, args.release)
    if proof is None:
        parser.exit(1, "Invalid or missing same-boot requester receipt\n")
    print(json.dumps(proof, indent=2))
