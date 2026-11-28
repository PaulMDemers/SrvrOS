"""Decode the two Haswell engine and requester samples taken under forcewake."""

from hardware_haswell_live_vtd import _decode_af
from hardware_haswell_survey import MASK32, MASK64, _record

ENGINE_DRAIN_TOKEN = "srvros.hsw-forcewake=engine-drain"
EXCLUSION_AUDIT_TOKEN = "srvros.hsw-forcewake=exclusion-audit"
HANDOFF_AUDIT_TOKEN = "srvros.hsw-forcewake=handoff-audit"
ENGINES = ("render", "video-decode", "blitter", "video-enhancement")
REQUESTERS = ("igd", "display-hda")


def _records(lines, prefix, pattern, expected):
    records = [tuple(map(int, _record(line, pattern))) for line in lines if line.startswith(prefix)]
    if len(records) != expected:
        raise ValueError("engine drain record count")
    return records


def _engine_state(first, second):
    if MASK32 in first or MASK32 in second:
        return "unavailable"
    if first != second:
        return "changed"
    head, tail, ctl, mode, _ = first
    if (head & 0x1ffffc) != (tail & 0x1ffff8) or ctl & 0xc00:
        return "queued-or-waiting"
    return "idle-at-samples" if mode & 0x200 else "idle-unconfirmed"


def _pci_sample(cmd, pm, cap, status, offset, header, flags):
    if (not 0 <= cmd < 0xffff or not 0 <= pm < 0xffff
            or cap and (cap not in range(0x40, 0xf8, 4) or not 0 <= status < 0xffff)
            or not cap and status != MASK32):
        raise ValueError("requester PCI fields")
    return dict(command=cmd, bus_master_enabled=bool(cmd & 4), pmcsr=pm, pcie_offset=cap or None,
                pcie_status=status if cap else None, pcie_pending=bool(status & 0x20) if cap else None,
                advanced_features=_decode_af(offset, header, flags))


def _requester_state(first, last):
    pending = [(p["pcie_pending"], p["advanced_features"]["transactions_pending"]) for p in (first, last)]
    if any(True in signals for signals in pending):
        return "pending-seen"
    for field in ("command", "pmcsr", "pcie_offset"):
        if first[field] != last[field]:
            return "config-changed"
    for field in ("offset", "header"):
        if first["advanced_features"].get(field) != last["advanced_features"].get(field):
            return "config-changed"
    return "clear-at-samples" if all(False in signals for signals in pending) else "unavailable"


def _timing_result(lines, wake, summary):
    timing = [line for line in lines if line.startswith("hsw-engine-drain-timing:")]
    if not timing:
        return None  # The original native receipt predates the IRQ guard.
    line, = timing
    *fields, stage, stop = _record(line,
        r"hsw-engine-drain-timing: v=1 entered=([01]) irq_before=([01]) ticks=(\d+),(\d+) "
        r"preempt=(\d+),(\d+) smi=([01]),(\d+),(\d+) max_gap_us=(\d+) "
        r"gap_stage=(none|entry|pass-one|gap|pass-two) "
        r"stop=(none|complete|clock|hold-deadline|sample-deadline|spin-limit|sample|irq-progress)")
    entered, irq_before, tick_first, tick_last, preempt_first, preempt_last, valid, smi_first, smi_last, gap = map(int, fields)
    count, _, _, reads, begin, end = summary
    clocks = wake["clock_us"]
    if (max(tick_first, tick_last, preempt_first, preempt_last, gap, begin, end) > MASK64
            or max(smi_first, smi_last) > MASK32 or bool(gap) != (stage != "none")):
        raise ValueError("engine timing fields")
    if not entered:
        if any((irq_before, tick_first, tick_last, preempt_first, preempt_last, valid,
                smi_first, smi_last, gap, count, reads, begin, end)) or stop != "none":
            raise ValueError("unentered engine timing window")
    elif (stop == "none" or not clocks["c0"] or not clocks["c0"] <= end
          or clocks["c0"] > (begin or end) or begin > end or gap > end - clocks["c0"]
          or (stop == "complete" and (count != 2 or tick_first != tick_last or preempt_first != preempt_last))
          or (stop == "sample-deadline" and (not begin or end - begin <= 4000))
          or (stop == "hold-deadline" and end - clocks["request"] <= 20000)
          or (stop == "irq-progress" and tick_first == tick_last and preempt_first == preempt_last)):
        raise ValueError("engine timing window")
    if wake["status"] == "VERIFIED" and stop != "complete":
        raise ValueError("successful engine receipt lacks completed timing window")
    return dict(entered=bool(entered), irq_enabled_before=bool(irq_before), timer_ticks=[tick_first, tick_last],
                preemptions=[preempt_first, preempt_last], smi=[smi_first, smi_last] if valid else None,
                smi_delta=(smi_last-smi_first) & MASK32 if valid else None, max_gap_us=gap,
                gap_stage=stage, stop=stop, begin_us=begin, end_us=end)


def engine_drain_result(lines, wake):
    summary, = _records(lines, "hsw-engine-drain:",
        r"hsw-engine-drain: v=1 samples=([0-2]) maps=([0-4]) retired=([0-4]) reads=(\d+) "
        r"begin=(\d+) end=(\d+) authority=none", 1)
    count, maps, retired, reads, begin, end = summary
    times = wake["clock_us"]
    if (retired > maps or not count * 20 <= reads <= min(40, (count+1)*20) or reads != wake["ring_reads"]
            or reads and (maps != 4 or not wake["stores"])
            or wake["status"] == "UNAVAILABLE" and any(summary)
            or wake["source_status"] not in (0, 5) and retired != maps
            or wake["reason"] == "release-unverified" and retired):
        raise ValueError("engine drain mapping or sample extent")
    clock = _records(lines, "hsw-engine-drain-pass:",
        r"hsw-engine-drain-pass: phase=([01]) begin=(\d+) end=(\d+) ack=(\d+),(\d+) "
        r"thread=(\d+),(\d+) smi=(\d+)", count)
    rings = _records(lines, "hsw-engine-drain-ring:",
        r"hsw-engine-drain-ring: phase=([01]) engine=([0-3]) head=(\d+) tail=(\d+) "
        r"ctl=(\d+) mode=(\d+) acthd=(\d+)", 4*count)
    pci = _records(lines, "hsw-engine-drain-pci:",
        r"hsw-engine-drain-pci: phase=([01]) requester=([01]) cmd=(\d+) pm=(\d+) pcie=(\d+) "
        r"status=(\d+) af=(\d+),(\d+),(\d+)", 2*count)
    samples = []
    previous = begin
    for index, (phase, start, stop, ack_first, ack_last, c0_first, c0_last, smi) in enumerate(clock):
        if (phase != index or not 0 < times["c0"] <= begin <= start < stop <= end
                or start - previous < (250 if phase else 0) or stop - begin > 4000
                or (ack_first, ack_last) != (1, 1) or (c0_first | c0_last) & 7
                or max(c0_first, c0_last, smi) > MASK32):
            raise ValueError("engine drain wake bracket")
        previous = stop
        sample = dict(begin_us=start, end_us=stop, ack=[ack_first, ack_last],
                      thread=[c0_first, c0_last], smi=smi, rings={}, requesters={})
        for index, name in enumerate(ENGINES):
            row = rings[phase * 4 + index]
            if row[:2] != (phase, index) or max(row[2:]) > MASK32:
                raise ValueError("engine drain ring identity")
            sample["rings"][name] = row[2:]
        for index, name in enumerate(REQUESTERS):
            row = pci[phase * 2 + index]
            if row[:2] != (phase, index) or index == 0 and (not row[2] & 2 or row[3] & 3):
                raise ValueError("engine drain requester identity")
            sample["requesters"][name] = _pci_sample(*row[2:])
        samples.append(sample)
    result = dict(maps=maps, retired=retired, reads=reads, samples=samples, requester_drain="unverified")
    timing = _timing_result(lines, wake, summary)
    if timing is not None:
        result["timing"] = timing
    if wake["status"] == "VERIFIED":
        if ((count, maps, retired, reads) != (2, 4, 4, 40)
                or end != previous or end > times["clear"] or end - begin > 4000):
            raise ValueError("incomplete held engine observation")
        first, last = samples
        result.update(elapsed_us=end-begin, smi_delta=(last["smi"]-first["smi"]) & MASK32,
                      engines={k: _engine_state(first["rings"][k], last["rings"][k]) for k in ENGINES},
                      requesters={k: _requester_state(first["requesters"][k], last["requesters"][k])
                                  for k in REQUESTERS})
    return result


def exclusion_audit_result(lines, drain):
    count = len(drain["samples"])
    summary, = _records(lines, "hsw-exclusion-audit:",
        r"hsw-exclusion-audit: v=1 rows=([0-2]) firmware_exclusion=unverified requester_drain=unverified", 1)
    if summary != (count,) or "timing" not in drain:
        raise ValueError("unbound exclusion audit")
    rows = _records(lines, "hsw-exclusion-smi:",
        r"hsw-exclusion-smi: phase=([01]) read=([01]) valid=([01]) pmbase=(\d+) "
        r"acpi=(\d+) pmcon=(\d+) en=(\d+) sts=(\d+)", count)
    samples = []
    for index, (phase, read, valid, base, acpi, pmcon, en, status) in enumerate(rows):
        if (phase != index or max(base, en, status) > MASK32 or acpi > 0xff or pmcon > 0xffff
                or read and (base != 0x1801 or acpi == 0xff or not acpi & 0x80 or pmcon == 0xffff)
                or not read and (valid or en or status)
                or valid and MASK32 in (en, status)):
            raise ValueError("SMI route observation")
        samples.append(dict(read=bool(read), valid=bool(valid), pmbase=base, acpi_control=acpi, gen_pmcon1=pmcon,
                            smi_en=en if read else None, smi_sts=status if read else None,
                            pch_global_smi_enabled=bool(en & 1) if valid else None,
                            pch_global_smi_locked=bool(pmcon & 0x10) if valid else None))
    delivery = [sample["pch_global_smi_enabled"] for sample in samples]
    pch_route = ("enabled-observed" if True in delivery else
                 "disabled-at-samples" if delivery == [False, False] else "unavailable")
    return dict(samples=samples, pch_smi_route=pch_route,
                firmware_exclusion="unverified", requester_drain="unverified")


def handoff_audit_result(lines, drain):
    count = len(drain["samples"])
    summary, = _records(lines, "hsw-handoff-audit:",
        r"hsw-handoff-audit: v=1 rows=([0-2]) requester_hold=unavailable firmware_handoff=unavailable authority=none",
        1)
    if summary != (count,):
        raise ValueError("unbound handoff audit")
    rows = _records(lines, "hsw-handoff-opregion:",
        r"hsw-handoff-opregion: phase=([01]) stable=([01]) asls=(\d+) swsci=(\d+) carrier=([0-5])", count)
    carriers = ("unobserved", "acpi-nvs", "reserved", "other", "ambiguous", "undescribed")
    samples = []
    for index, (phase, stable, asls, swsci, carrier) in enumerate(rows):
        extent_ok = asls and not asls & 3 and asls + 8192 <= MASK32 + 1
        if (phase != index or asls > MASK32 or swsci > 0xffff or stable and asls == MASK32
                or carrier and (not stable or not extent_ok)):
            raise ValueError("invalid OpRegion observation")
        sci_known = stable and swsci != 0xffff
        samples.append(dict(stable=bool(stable), asls=asls, swsci=swsci, carrier=carriers[carrier],
                            sci_selected=bool(swsci & 0x8000) if sci_known else None,
                            trigger_set=bool(swsci & 1) if sci_known else None))
    if len(samples) != 2 or not all(row["stable"] for row in samples):
        state = "unavailable"
    elif samples[0] != samples[1]:
        state = "changed"
    else:
        state = "present-at-samples" if samples[0]["asls"] else "absent-at-samples"
    return dict(samples=samples, opregion_state=state, requester_hold="unavailable",
                firmware_handoff="unavailable")
