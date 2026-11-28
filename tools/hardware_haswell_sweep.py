"""Verify the Air6,1 receipt for the remainder of the GGTT aperture."""

import argparse
import json
from pathlib import Path

from hardware_haswell_backing import backing_result
from hardware_haswell_survey import FIRST_SLOTS, MASK32, MASK64, PAGE_WORDS, _record

TOKEN = "srvros.hsw-sweep=read-only"
PROTECTED = {4096, 16385}
PAGES = tuple(slot for slot in range(4096, 65536, PAGE_WORDS) if slot not in FIRST_SLOTS)
BATCHES = tuple(PAGES[i:i+4] for i in range(0, len(PAGES), 4))
MAX_SHORTLIST = 16


def _records(lines, prefix, pattern):
    return [tuple(map(int, _record(line, prefix + pattern)))
            for line in lines if line.startswith(prefix)]


def _result(text):
    lines = text.splitlines()
    options = [line.split("boot options=", 1)[1].split() for line in lines
               if line.startswith("laptop-capture: boot options=")]
    incompatible = ("srvros.hsw-candidate=", "srvros.hsw-boot-slot=", "srvros.hsw-access=", "srvros.hsw-survey=")
    if not options or any(row.count(TOKEN) != 1 or any(t.startswith(incompatible) for t in row) for row in options):
        raise ValueError("isolated sweep boot profile")
    (state, attempted, completed), = _records(lines, "hsw-sweep:",
        r" v=1 status=([0-5]) batches=(\d+) complete=(\d+) writes=0 authority=none")
    if state == 1:
        if attempted != len(BATCHES) or completed != attempted:
            raise ValueError("incomplete sweep")
    elif attempted > len(BATCHES) or completed != max(0, attempted-1) or "laptop-capture: completed=yes" in lines:
        raise ValueError("impossible failure accounting")
    batches = _records(lines, "hsw-sweep-batch:",
        r" index=(\d+) status=([0-5]) pages=(\d+),(\d+),(\d+),(\d+) begin=(\d+) end=(\d+) frames=(\d+),(\d+)")
    passes = _records(lines, "hsw-sweep-pass:",
        r" index=(\d+) phase=([01]) begin=(\d+) end=(\d+) reads=(\d+) maps=(\d+) retired=(\d+) hash=(\d+)")
    counts = _records(lines, "hsw-sweep-count:",
        r" index=(\d+) present=(\d+) nonzero=(\d+) zero=(\d+) first_image=(\d+) stolen=(\d+)")
    zero_rows = _records(lines, "hsw-sweep-zero:", r" index=(\d+) slot=(\d+)")
    if (len(batches), len(passes), len(counts)) != (attempted, 2*attempted, completed):
        raise ValueError("missing or extra batch records")
    if any(index >= completed for index, _ in zero_rows) or zero_rows != sorted(set(zero_rows)):
        raise ValueError("invalid candidate order or batch")
    if not attempted and state != 0:
        raise ValueError("missing attempted batch")
    result = dict(status=("OBSERVED" if state == 1 else "REFUSED" if not attempted else "FAILED"),
                  source_status=state, batches=[], batches_completed=completed,
                  writes=0, hardware_authority=False)
    prev_end = 0
    for index, values in enumerate(batches):
        ident, batch_state, *bounds = values
        *pages, begin, end, first_frame, last_frame = bounds
        success = index < completed
        expected_state = 1 if success else state
        if (ident != index or tuple(pages) != BATCHES[index] or batch_state != expected_state
                or not 0 <= begin <= end <= MASK64 or (begin and begin < prev_end)
                or max(first_frame, last_frame) > MASK32):
            raise ValueError("invalid batch scope or lifetime")
        prev_end = end
        expected_reads = 4 * PAGE_WORDS - sum(
            any(page <= slot < page+PAGE_WORDS for page in pages) for slot in PROTECTED)
        phases = []
        for phase, (batch, role, start, finish, reads, maps, retired, digest) in enumerate(passes[2*index:2*index+2]):
            if ((batch, role) != (index, phase) or not 0 <= start <= end
                    or (start and start < begin) or (finish and not start <= finish <= end)
                    or not 0 <= retired <= maps <= 4 or reads > min(expected_reads, maps*PAGE_WORDS) or digest > MASK64):
                raise ValueError("invalid phase accounting")
            phases.append(dict(begin=start, end=finish, reads=reads, maps=maps, retired=retired, hash=digest))
        frame_delta = (last_frame-first_frame) & MASK32
        row = dict(index=index, source_status=batch_state, first_slots=pages, expected_reads_per_pass=expected_reads,
                   begin=begin, end=end, frame_delta=frame_delta, passes=phases)
        result["batches"].append(row)
        if not success:
            continue
        a, b = phases
        if (not begin or not 0 < end-begin <= 100000 or not 1 <= frame_delta <= 8
                or not begin <= a["begin"] < a["end"] < b["begin"] < b["end"] <= end
                or b["begin"]-a["end"] < 20000 or a["hash"] != b["hash"]
                or any((p["reads"], p["maps"], p["retired"]) != (expected_reads, 4, 4) for p in phases)):
            raise ValueError("unverified batch")
        ident, present, nonzero, zeros, first_image, stolen = counts[index]
        candidates = [slot for batch, slot in zero_rows if batch == index]
        if (ident != index or present+nonzero+zeros != expected_reads or not 0 <= first_image <= stolen <= present
                or len(candidates) != min(zeros, MAX_SHORTLIST) or any(slot in PROTECTED or
                    not any(page <= slot < page+PAGE_WORDS for page in pages) for slot in candidates)):
            raise ValueError("unverified coverage or candidate")
        row.update(present=present, nonzero_not_present=nonzero, exact_zero=zeros,
                   first_image_aliases=first_image, stolen_aliases=stolen, zero_slot_shortlist=candidates)
    if state == 1:
        backing = backing_result(text)
        if (not backing or backing["status"] != "OBSERVED" or backing.get("source_version") != 3
                or backing["offset"] or backing["count"] != 1056 or backing["tilectl_raw"] != [0, 0]
                or not backing["stolen_range_reserved"] or not backing["all_stolen_candidates"]
                or any(row["fences"] != [0x00fff02b00000001]+[0]*31 for row in backing["samples"])):
            raise ValueError("inherited backing or fence did not qualify")
        rows = result["batches"]
        result.update(entries=sum(row["expected_reads_per_pass"] for row in rows),
                      elapsed_us=rows[-1]["end"]-rows[0]["begin"],
                      zero_count=sum(row["exact_zero"] for row in rows),
                      zero_slot_shortlist=[slot for row in rows for slot in row["zero_slot_shortlist"]][:MAX_SHORTLIST])
    return result


def sweep_result(text):
    try:
        return _result(text)
    except (ValueError, KeyError):
        return None


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("receipt", type=Path)
    args = parser.parse_args()
    result = sweep_result(args.receipt.read_text())
    if not result:
        parser.exit(1, "Invalid or incomplete Haswell sweep receipt\n")
    print(json.dumps(result, indent=2))
