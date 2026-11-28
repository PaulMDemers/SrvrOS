"""Verify the native read-only Haswell populated-range ownership refusal."""

import argparse
import json
from pathlib import Path
import re
import struct

from hardware_haswell_backing import backing_result, canonical
from hardware_haswell_survey import MASK32, MASK64, _record
from haswell_backing_audit import pte_fields

TOKEN = "srvros.hsw-ownership=read-only"
HOLD_TOKEN = "srvros.hsw-ownership=requester-hold"
FIRST_SLOT, COUNT = 24576, 16


def _software_hold(lines, result):
    (line,) = [line for line in lines if line.startswith("hsw-requester-hold:")]
    state, *raw = _record(line,
        r"hsw-requester-hold: v=1 state=(unavailable|retired|retained) begin=(\d+) denied=(\d+),(\d+) "
        r"end=(\d+) refusals=([0-2]) firmware_handoff=unavailable hardware_hold=unverified "
        r"protected_reads=0 authority=none")
    begin, first, last, end, refusals = map(int, raw)
    a, b = result["passes"]
    status = result["source_status"]
    expected = "unavailable" if status == 0 else "retained" if status == 5 else "retired"
    if (state != expected or (begin, end) != (result["begin"], result["end"])
            or bool(begin) != (state != "unavailable") or not 0 <= end - begin <= 100000
            or refusals != bool(first) + bool(last)
            or first and not begin <= first <= (a["begin"] or end)
            or last and (not first or not b["end"] or not b["end"] <= last == end)
            or (refusals == 2) != (status == 1)):
        raise ValueError("unverified software hold or admission ordering")
    result["software_hold"] = dict(state=state, begin_us=begin, end_us=end,
        refusals=[clock for clock in (first, last) if clock], firmware_handoff="unavailable",
        hardware_hold="unverified", protected_reads=0)


def _result(text):
    lines = text.splitlines()
    options = [line.split("boot options=", 1)[1].split() for line in lines
               if line.startswith("laptop-capture: boot options=")]
    hold = any(HOLD_TOKEN in row for row in options)
    fixed = {HOLD_TOKEN, "srvros.storage=ram", "srvros.capture=haswell-backing", "srvros.capture-failure=reboot",
             "srvros.hsw-cpu-control=read-only", "srvros.hsw-fences=linux-model-32"}
    if hold and any(len(row) != 8 or not fixed.issubset(row)
                    or sum(bool(re.fullmatch(r"srvros\.workspace-volume=[a-f0-9]{8}", t)) for t in row) != 1
                    or sum(bool(re.fullmatch(r"srvros\.desktop-release=[a-f0-9]{16}", t)) for t in row) != 1 for row in options):
        raise ValueError("isolated requester hold profile")
    incompatible = ("srvros.hsw-candidate=", "srvros.hsw-boot-slot=", "srvros.hsw-access=", "srvros.hsw-survey=",
                    "srvros.hsw-sweep=", "srvros.hsw-engine-prereqs=", "srvros.hsw-forcewake=")
    if not options or not hold and any(row.count(TOKEN) != 1 or any(t.startswith(incompatible) for t in row) for row in options):
        raise ValueError("isolated ownership profile")
    summary, = [line for line in lines if line.startswith("hsw-ownership:")]
    state, first, count, begin, end, before, after = map(int, _record(summary,
        r"hsw-ownership: v=1 status=([0-6]) first=(\d+) count=(\d+) begin=(\d+) end=(\d+) frames=(\d+),(\d+) writes=0 authority=none"))
    if ((first, count) != (FIRST_SLOT, COUNT) or not 0 <= begin <= end <= MASK64 or max(before, after) > MASK32):
        raise ValueError("ownership range or lifetime")
    pass_rows = [line for line in lines if line.startswith("hsw-ownership-pass:")]
    if len(pass_rows) != 2:
        raise ValueError("missing or extra passes")
    passes = []
    for index, line in enumerate(pass_rows):
        phase, start, finish, reads, maps, retired, digest = map(int, _record(line,
            r"hsw-ownership-pass: phase=([01]) begin=(\d+) end=(\d+) reads=(\d+) maps=(\d+) retired=(\d+) hash=(\d+)"))
        if (phase != index or not 0 <= start <= end or (start and start < begin)
                or (finish and not start <= finish <= end)
                or not 0 <= retired <= maps <= 1 or reads > maps * COUNT or digest > MASK64):
            raise ValueError("invalid pass extent")
        passes.append(dict(begin=start, end=finish, reads=reads, maps=maps, retired=retired, hash=digest))
    refusal_rows = [line for line in lines if line.startswith("hsw-ownership-refusal:")]
    data_rows = [line for line in lines if line.startswith("hsw-ownership-data:")]
    result = dict(status=("REFUSAL_VERIFIED" if state == 1 else "UNAVAILABLE" if state == 0 else "FAILED"),
                  source_status=state, first_slot=first, count=count, begin=begin, end=end,
                  frames=[before, after], passes=passes, writes=0, hardware_authority=False)
    if hold:
        _software_hold(lines, result)
    elif any(line.startswith("hsw-requester-hold:") for line in lines):
        raise ValueError("unexpected requester hold receipt")
    if state != 1:
        if refusal_rows or data_rows or "laptop-capture: completed=yes" in lines:
            raise ValueError("incomplete observation claims a refusal")
        if not state and (begin or end or before or after or any(any(p.values()) for p in passes)):
            raise ValueError("unavailable observation contains accesses")
        return result

    backing = backing_result(text)
    if (not backing or backing["status"] != "OBSERVED" or backing.get("source_version") != 3
            or backing["offset"] or backing["count"] != 1056 or backing["tilectl_raw"] != [0, 0]
            or not backing["stolen_range_reserved"] or not backing["all_stolen_candidates"]
            or any(row["fences"] != [0x00fff02b00000001]+[0]*31 for row in backing["samples"])):
        raise ValueError("inherited backing or fence did not qualify")
    a, b = passes
    frame_delta = (after-before) & MASK32
    if (not begin or not 0 < end-begin <= 100000 or not 1 <= frame_delta <= 8
            or not begin <= a["begin"] < a["end"] < b["begin"] < b["end"] <= end
            or b["begin"]-a["end"] < 20000
            or any((p["reads"], p["maps"], p["retired"]) != (COUNT, 1, 1) for p in passes)):
        raise ValueError("unverified live observation")
    refusal, = refusal_rows
    aliases, = map(int, _record(refusal,
        r"hsw-ownership-refusal: admission=refused image_aliases=(\d+) range_grant=unavailable "
        r"requester_hold=unavailable drain=unverified awake_engines=unavailable firmware_exclusion=unverified"))
    data, = data_rows
    payload, = _record(data, r"hsw-ownership-data: data=([A-Za-z0-9+/]+={0,2})")
    raw = canonical(payload, COUNT*4)
    words = struct.unpack(f"<{COUNT}I", raw)
    digest = 14695981039346656037
    for byte in raw:
        digest = ((digest ^ byte) * 1099511628211) & MASK64
    image = set(backing["samples"][0]["address_candidates"])
    if (any(word == MASK32 or not word & 1 for word in words) or any(p["hash"] != digest for p in passes)
            or aliases != sum(pte_fields(word)["address_candidate"] in image for word in words)):
        raise ValueError("unverified populated entries or alias count")
    result.update(admission="refused", image_aliases=aliases, words=list(words), frame_delta=frame_delta,
                  elapsed_us=end-begin, range_grant="unavailable", requester_hold="unavailable",
                  drain="unverified", awake_engines="unavailable", firmware_exclusion="unverified")
    return result


def ownership_result(text):
    try:
        return _result(text)
    except (ValueError, KeyError, struct.error):
        return None


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("receipt", type=Path)
    args = parser.parse_args()
    result = ownership_result(args.receipt.read_text())
    if not result:
        parser.exit(1, "Invalid or incomplete Haswell ownership receipt\n")
    print(json.dumps(result, indent=2))
