"""Verify Haswell engine-access and requester observations without granting either."""

import argparse
import json
from pathlib import Path

from hardware_haswell_backing import inherited_pipe_a_backing
from hardware_haswell_live_vtd import _decode_af, _rows
from hardware_haswell_survey import MASK32, MASK64, _record

TOKEN = "srvros.hsw-engine-prereqs=read-only"


def _result(text):
    lines = text.splitlines()
    options = [line.split("boot options=", 1)[1].split() for line in lines
               if line.startswith("laptop-capture: boot options=")]
    incompatible = ("srvros.hsw-candidate=", "srvros.hsw-boot-slot=", "srvros.hsw-access=",
                    "srvros.hsw-survey=", "srvros.hsw-sweep=", "srvros.hsw-ownership=")
    if not options or any(row.count(TOKEN) != 1 or any(t.startswith(incompatible) for t in row) for row in options):
        raise ValueError("isolated engine prerequisite profile")
    summary, = [line for line in lines if line.startswith("hsw-engine-prereqs:")]
    state, count, maps, retired, begin, end, first, last = map(int, _record(summary,
        r"hsw-engine-prereqs: v=1 status=([0-5]) rows=([0-2]) maps=([0-2]) retired=([0-2]) "
        r"begin=(\d+) end=(\d+) frames=(\d+),(\d+) writes=0 ring_reads=0 protected_reads=0 "
        r"awake_owner=unavailable requester_hold=unverified"))
    rows = _rows(text, "hsw-engine-prereqs-row:",
        r"hsw-engine-prereqs-row: phase=([01]) begin=(\d+) end=(\d+) ack=(\d+) "
        r"thread=(\d+) core=(\d+) cmd=(\d+) pm=(\d+) smi=(\d+) af=(\d+),(\d+),(\d+)")
    if len(rows) != count or not count <= retired <= maps or not 0 <= begin <= end <= MASK64 or max(first, last) > MASK32:
        raise ValueError("observation extent")
    result = dict(status="OBSERVED" if state == 1 else "UNAVAILABLE" if state == 0 else "FAILED",
                  source_status=state, begin=begin, end=end, maps=maps, retired=retired, frames=[first,last],
                  writes=0, ring_reads=0, protected_reads=0, hardware_authority=False,
                  awake_owner="unavailable", requester_hold="unverified", rows=[])
    prev_end = begin
    for start, finish, ack, thread, core, cmd, pm, smi, af, header, flags in rows:
        if (not 0 < prev_end <= start < finish <= end or max(ack,thread,core) >= MASK32 or
                not 0 <= smi <= MASK32 or cmd >= 0xffff or not cmd & 2 or pm >= 0xffff or pm & 3):
            raise ValueError("endpoint values")
        result["rows"].append(dict(begin=start,end=finish,ack=ack,kernel_ack=bool(ack&1),
                                   thread=thread,thread_c0_observed=(thread&7)==0,core=core,
                                   command=cmd,bus_master_enabled=bool(cmd&4),pm=pm,smi=smi,
                                   advanced_features=_decode_af(af,header,flags)))
        prev_end = finish
    if state != 1:
        if "laptop-capture: completed=yes" in lines or (not state and any((count,maps,retired,begin,end,first,last))):
            raise ValueError("incomplete observation")
        return result

    frames = (last-first) & MASK32
    if (not inherited_pipe_a_backing(text)
            or (count,maps,retired) != (2,2,2) or not 1 <= frames <= 8
            or not 0 < end-begin <= 100000 or rows[1][0]-rows[0][1] < 20000):
        raise ValueError("live observation or backing did not qualify")
    result.update(frame_delta=frames, elapsed_us=end-begin, smi_delta=(rows[1][7]-rows[0][7]) & MASK32)
    return result


def engine_prereqs_result(text):
    try:
        return _result(text)
    except (ValueError, KeyError):
        return None


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("receipt", type=Path)
    args = parser.parse_args()
    result = engine_prereqs_result(args.receipt.read_text())
    if not result:
        parser.exit(1, "Invalid or incomplete Haswell engine prerequisite receipt\n")
    print(json.dumps(result, indent=2))
