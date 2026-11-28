"""Check the isolated Haswell kernel forcewake acquire/release receipt."""

import argparse
import json
from pathlib import Path
import re

from hardware_haswell_backing import inherited_pipe_a_backing
from hardware_haswell_engine_drain import (
    BLITTER_PROVIDER_TOKEN, ENGINE_DRAIN_TOKEN, EXCLUSION_AUDIT_TOKEN, HANDOFF_AUDIT_TOKEN, RESET_FREE_AUDIT_TOKEN,
    blitter_provider_result, engine_drain_result, exclusion_audit_result, handoff_audit_result, reset_free_audit_result,
)
from hardware_haswell_survey import MASK32, MASK64, _record

TOKEN = "srvros.hsw-forcewake=acquire-release"
ENGINE_HOLD_TOKEN = "srvros.hsw-forcewake=engine-hold"
COMPLETION_PAGE_TOKEN = "srvros.hsw-forcewake=completion-page"
REQUIRED = {"srvros.storage=ram", "srvros.capture=haswell-backing",
            "srvros.hsw-cpu-control=read-only", "srvros.hsw-fences=linux-model-32",
            "srvros.capture-failure=reboot"}
FAILURES = {"map", "preflight", "baseline-ack", "request", "acquire-ack", "thread-c0",
            "held-display", "held-ack", "release-unverified", "alias-retirement", "hold-deadline",
            "display"}


def _result(text):
    lines = text.splitlines()
    options = [line.partition("boot options=")[2].split() for line in lines
               if line.startswith("laptop-capture: boot options=")]
    if not options:
        raise ValueError("missing forcewake profile")
    provider_modes = (BLITTER_PROVIDER_TOKEN, COMPLETION_PAGE_TOKEN)
    hold_modes = (ENGINE_HOLD_TOKEN, *provider_modes)
    reset_free_modes = (*hold_modes, RESET_FREE_AUDIT_TOKEN)
    handoff_modes = (*reset_free_modes, HANDOFF_AUDIT_TOKEN)
    exclusion_modes = (*handoff_modes, EXCLUSION_AUDIT_TOKEN)
    modes = (*exclusion_modes, ENGINE_DRAIN_TOKEN)
    mode = next((token for token in modes if token in options[0]), TOKEN)
    with_engines = mode != TOKEN
    required = REQUIRED | {mode}
    for row in options:
        identity = set(row) - required
        if (len(row) != 8 or not required.issubset(row)
                or sum(bool(re.fullmatch(r"srvros.workspace-volume=[0-9a-f]{8}", t)) for t in identity) != 1
                or sum(bool(re.fullmatch(r"srvros.desktop-release=[0-9a-f]{16}", t)) for t in identity) != 1):
            raise ValueError("isolated forcewake profile")
    summary, = [line for line in lines if line.startswith("hsw-forcewake:")]
    state, reason, *counts = _record(summary,
        r"hsw-forcewake: v=1 status=([0-5]) reason=([a-z-]+) maps=([0-2]) retired=([0-2]) "
        r"stores=([0-2]) released=([01]) frames=(\d+),(\d+) ring_reads=(\d+) protected_reads=0 "
        r"requester_hold=unverified")
    state = int(state)
    maps, retired, stores, released, first, last, ring_reads = map(int, counts)
    if not with_engines and (ring_reads or any(line.startswith("hsw-engine-drain") for line in lines)):
        raise ValueError("wake-only profile issued ring reads")
    proof, = [line for line in lines if line.startswith("hsw-forcewake-proof:")]
    begin, request, ack, c0, clear, release, end, baseline, acquired, thread, held, final = map(int,
        _record(proof, r"hsw-forcewake-proof: begin=(\d+) request=(\d+) ack=(\d+) c0=(\d+) "
                       r"clear=(\d+) release=(\d+) end=(\d+) baseline=(\d+) acquired=(\d+) "
                       r"thread=(\d+) held=(\d+) final=(\d+)"))
    if (retired > maps or stores and maps != 2 or max(first, last, baseline, acquired, thread, held, final) > MASK32
            or not 0 <= begin <= end <= MASK64 or max(request, ack, c0, clear, release) > MASK64):
        raise ValueError("forcewake receipt extent")
    if released and (stores != 2 or final or not 0 < clear <= release <= clear + 10000):
        raise ValueError("invalid release observation")
    status = "VERIFIED" if state == 1 else "UNAVAILABLE" if state == 0 else "FAILED"
    result = dict(status=status, source_status=state, reason=reason, maps=maps, retired=retired,
                  stores=stores, released=bool(released), frames=[first, last],
                  clock_us=dict(begin=begin, request=request, ack=ack, c0=c0, clear=clear, release=release, end=end),
                  raw=dict(baseline=baseline, acquired=acquired, thread=thread, held=held, final=final),
                  ring_reads=ring_reads, protected_reads=0, requester_hold="unverified")
    if with_engines:
        result["engine_drain"] = engine_drain_result(lines, result)
    if mode in exclusion_modes:
        result["exclusion_audit"] = exclusion_audit_result(lines, result["engine_drain"])
    elif any(line.startswith("hsw-exclusion-") for line in lines):
        raise ValueError("unexpected exclusion observation")
    if mode in handoff_modes:
        result["handoff_audit"] = handoff_audit_result(lines, result["engine_drain"])
    elif any(line.startswith("hsw-handoff-") for line in lines):
        raise ValueError("unexpected handoff observation")
    if mode in reset_free_modes:
        result["reset_free_audit"] = reset_free_audit_result(lines, result["engine_drain"])
    elif any(line.startswith("hsw-reset-free-") for line in lines):
        raise ValueError("unexpected reset-free observation")
    hold_rows = [line for line in lines if line.startswith("hsw-engine-hold:")]
    if mode in hold_modes:
        (hold_row,) = hold_rows
        hold_state, hold_reason = _record(hold_row,
            r"hsw-engine-hold: v=1 state=(refused|unavailable) reason=(no-provider|baseline) "
            r"polls=0 attempted=0 completion_us=0 authority=none")
        expected = ("refused", "no-provider") if state == 1 else ("unavailable", "baseline")
        if (hold_state, hold_reason) != expected:
            raise ValueError("unbound Haswell hold refusal")
        result["engine_hold"] = dict(state=hold_state, reason=hold_reason, polls=0,
                                     attempted=0, completion_us=0)
    elif hold_rows:
        raise ValueError("unexpected Haswell hold receipt")
    if mode in provider_modes:
        result["blitter_provider"] = blitter_provider_result(lines, result)
    elif any(line.startswith(("hsw-blitter-provider:", "hsw-blitter-status-route:")) for line in lines):
        raise ValueError("unexpected blitter provider observation")
    completion_rows = [line for line in lines if line.startswith("hsw-completion-page:")]
    if mode == COMPLETION_PAGE_TOKEN:
        (row,) = completion_rows
        completion_state, completion_reason = _record(row,
            r"hsw-completion-page: v=1 state=(refused|unavailable) reason=(firmware-exclusion|baseline) "
            r"zero=0 pte=0 pga=0 authority=none")
        expected = ("refused", "firmware-exclusion") if state == 1 else ("unavailable", "baseline")
        if (completion_state, completion_reason) != expected:
            raise ValueError("unbound completion page refusal")
        result["completion_page"] = dict(state=completion_state, reason=completion_reason,
                                         zero_attempts=0, pte_attempts=0, pga_attempts=0)
    elif completion_rows:
        raise ValueError("unexpected completion page observation")
    complete = lines.count("laptop-capture: completed=yes")
    if complete and "laptop-capture: completed=NO" in lines:
        raise ValueError("conflicting completion markers")
    if state == 0:
        if reason != "none" or complete or any((maps, retired, stores, released, first, last, begin, request,
                                                 ack, c0, clear, release, end, baseline, acquired, thread, held, final)):
            raise ValueError("unavailable forcewake issued operations")
        return result
    if state != 1:
        reasons = FAILURES | {"engine-drain-map", "engine-drain-sample"} if with_engines else FAILURES
        if reason not in reasons or complete or (state != 5 and (retired != maps or (stores and not released))):
            raise ValueError("failed forcewake cleanup")
        if reason == "release-unverified" and (state != 5 or retired or released or not stores):
            raise ValueError("unverified release altered mapping state")
        return result
    frames = (last - first) & MASK32
    if (reason != "none" or complete != 1 or (maps, retired, stores, released) != (2, 2, 2, 1)
            or (baseline, acquired, held, final) != (0, 1, 1, 0) or thread & 7
            or not 0 < begin <= request <= ack <= c0 <= clear <= release <= end
            or request - begin > 50000 or ack - request > 10000 or c0 - ack > 5000
            or clear - request > 20000 or end - release < 20000 or end - begin > 100000
            or not 1 <= frames <= 8 or not inherited_pipe_a_backing(text)):
        raise ValueError("forcewake acquire, release or Pipe A did not qualify")
    result.update(frame_delta=frames, elapsed_us=end-begin, ack_latency_us=ack-request,
                  c0_latency_us=c0-ack, release_latency_us=release-clear)
    return result


def forcewake_result(text):
    try:
        return _result(text)
    except ValueError:
        return None


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("receipt", type=Path)
    args = parser.parse_args()
    result = forcewake_result(args.receipt.read_text())
    if result is None:
        parser.exit(1, "Invalid Haswell forcewake receipt\n")
    print(json.dumps(result, indent=2))
