"""Verify the fixed read-only Air6,1 GGTT survey receipt."""

import argparse
import json
from pathlib import Path
import re
import struct

from hardware_haswell_backing import backing_result, canonical
from haswell_backing_audit import pte_fields

FIRST_SLOTS = (8192, 24576, 40960, 57344)
PAGE_WORDS = 1024
CHUNK_WORDS = 128
MASK64 = (1 << 64) - 1
MASK32 = (1 << 32) - 1
TOKEN = "srvros.hsw-survey=read-only"


def _record(line, spec):
    match = re.fullmatch(spec, line)
    if not match:
        raise ValueError("invalid survey record")
    return match.groups()


def _result(text):
    lines = text.splitlines()
    options = [line.split("boot options=", 1)[1].split() for line in lines
               if line.startswith("laptop-capture: boot options=")]
    if not options or any(row.count(TOKEN) != 1 or any(
            token.startswith(("srvros.hsw-candidate=", "srvros.hsw-boot-slot=", "srvros.hsw-access="))
            for token in row) for row in options):
        raise ValueError("isolated survey boot profile")
    summary, = [line for line in lines if line.startswith("hsw-survey:")]
    state, begin, end, first, last = map(int, _record(summary,
        r"hsw-survey: v=1 status=([0-5]) begin=(\d+) end=(\d+) frames=(\d+),(\d+) writes=0 authority=none"))
    pass_lines = [line for line in lines if line.startswith("hsw-survey-pass:")]
    data_lines = [line for line in lines if line.startswith("hsw-survey-data:")]
    if len(pass_lines) != 2 or not 0 <= begin <= end <= MASK64 or max(first, last) > MASK32:
        raise ValueError("survey extent")
    passes = []
    for index, line in enumerate(pass_lines):
        phase, start, finish, reads, maps, retired, digest = map(int, _record(line,
            r"hsw-survey-pass: phase=([01]) begin=(\d+) end=(\d+) reads=(\d+) maps=(\d+) retired=(\d+) hash=(\d+)"))
        if (phase != index or not 0 <= start <= end or (finish and not start <= finish <= end)
                or not 0 <= retired <= maps <= len(FIRST_SLOTS) or reads > maps * PAGE_WORDS or digest > MASK64):
            raise ValueError("survey pass extent")
        passes.append(dict(begin=start, end=finish, reads=reads, maps=maps, retired=retired, hash=digest))
    result = dict(status=("REFUSED" if state == 0 else "OBSERVED" if state == 1 else "FAILED"),
                  source_status=state, begin=begin, end=end, frames=[first, last], passes=passes,
                  writes=0, hardware_authority=False)
    if state != 1:
        if data_lines or "laptop-capture: completed=yes" in lines:
            raise ValueError("incomplete survey data")
        if state == 0 and (begin or end or first or last or any(any(p.values()) for p in passes)):
            raise ValueError("refusal contains survey accesses")
        return result

    backing = backing_result(text)
    frames = (last-first) & MASK32
    a, b = passes
    if (not backing or backing["status"] != "OBSERVED" or backing.get("source_version") != 3
            or not begin or not 1 <= frames <= 8 or not 0 < end-begin <= 100000
            or not begin <= a["begin"] < a["end"] < b["begin"] < b["end"] <= end
            or b["begin"]-a["end"] < 20000 or len(data_lines) != len(FIRST_SLOTS)*PAGE_WORDS//CHUNK_WORDS
            or any((p["reads"], p["maps"], p["retired"]) != (4096, 4, 4) for p in passes)):
        raise ValueError("unverified complete survey")
    if (backing["offset"] or backing["count"] != 1056 or backing["tilectl_raw"] != [0, 0]
            or not backing["stolen_range_reserved"] or not backing["all_stolen_candidates"]
            or any(row["fences"] != [0x00fff02b00000001]+[0]*31 for row in backing["samples"])):
        raise ValueError("inherited backing or fence did not qualify")

    image = set(backing["samples"][0]["address_candidates"])
    stolen_start, stolen_end = backing["stolen_bounds"]
    digest = 14695981039346656037
    by_range = []
    shortlist = []
    records = iter(data_lines)
    for start in FIRST_SLOTS:
        words = []
        for slot in range(start, start+PAGE_WORDS, CHUNK_WORDS):
            ident, payload = _record(next(records), r"hsw-survey-data: first=(\d+) data=([A-Za-z0-9+/]+={0,2})")
            if ident != str(slot):
                raise ValueError("survey slot order or scope")
            raw = canonical(payload, CHUNK_WORDS*4)
            for byte in raw:
                digest = ((digest ^ byte) * 1099511628211) & MASK64
            words.extend(struct.unpack(f"<{CHUNK_WORDS}I", raw))
        if MASK32 in words:
            raise ValueError("unavailable survey word")
        zero = [start+index for index, word in enumerate(words) if word == 0]
        shortlist.extend(zero[:16-len(shortlist)])
        present = [pte_fields(word)["address_candidate"] for word in words if word & 1]
        by_range.append(dict(first_slot=start, last_slot=start+PAGE_WORDS-1, exact_zero=len(zero),
                             present=len(present), nonzero_not_present=PAGE_WORDS-len(zero)-len(present),
                             primary_image_aliases=sum(address in image for address in present),
                             stolen_aliases=sum(stolen_start <= address < stolen_end for address in present)))
    if any(p["hash"] != digest for p in passes):
        raise ValueError("survey pass hashes differ")
    result.update(frame_delta=frames, elapsed_us=end-begin, ranges=by_range,
                  zero_slot_shortlist=shortlist, zero_count=sum(row["exact_zero"] for row in by_range))
    return result


def survey_result(text):
    try:
        return _result(text)
    except (ValueError, KeyError, struct.error):
        return None


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("receipt", type=Path)
    args = parser.parse_args()
    result = survey_result(args.receipt.read_text())
    if not result:
        parser.exit(1, "Invalid or incomplete Haswell survey receipt\n")
    print(json.dumps(result, indent=2))
