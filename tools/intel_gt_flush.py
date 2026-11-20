"""Inspect engine and L3 messages in the original full-GT lifetime."""

from intel_gt_bootstrap import fields

ENGINES = ("rcs", "vcs0", "vecs", "vcs1", "bcs")
WORDS = (*ENGINES, "l3")
GAM_WORDS = ("gam_go", "gam_eco")
MEDIA_WORDS = ("vcs0_ref", "vcs0_prefetch", "vcs1_ref", "vcs1_prefetch")
POWER_WORDS = ("gam_req", "rc_control", "rc_target", "rc6_threshold")
GAM_GO_MASK = 0x5f  # RCS, VCS0, VCS1, VECS, BCS and WiDi (bit 6).

SMI_PREFIX = "intel-smi-guard:"
SMI_FAULT = 2048


def smi_guard_result(rows, version, parent, memory, retire):
    if version == 1:
        if rows or parent["reason"] & SMI_FAULT:
            raise ValueError("Foreign GT SMI receipt")
        return None
    if len(rows) != 1 or not rows[0].endswith(" authority=none"):
        raise ValueError("GT SMI receipt coverage or authority")
    guard = fields(rows[0].removesuffix(" authority=none"), SMI_PREFIX,
                   ("v", "reads", "point", "fault", "first", "last"))
    reads, point, fault, first, last = (guard[key] for key in ("reads", "point", "fault", "first", "last"))
    writes = parent["commands"] + parent["native_writes"] + memory["writes"]
    if (max(first, last) >= 1 << 32 or point > 6 or fault > 2
            or bool(fault) != bool(parent["reason"] & SMI_FAULT)
            or (fault and (not parent["failed"] or parent["complete"]))):
        raise ValueError("GT SMI counter or fault")
    if not reads:
        if (first or last or writes or retire["stage"]
                or (point, fault) not in ((0, 0), (1, 1))):
            raise ValueError("GT SMI missing initial sample")
        return guard
    minimum = 1 + writes + (retire["stage"] >= 1)
    if (not point or (point == 1 and reads != 1)
            or (point == 6 and retire["stage"] < 2)
            or ((first != last) != (fault == 2))
            or (fault == 1 and reads != minimum)
            or (fault == 2 and reads != minimum + 1)
            or not minimum <= reads <= minimum + 1):
        raise ValueError("GT SMI checkpoint accounting")
    if parent["complete"] and (point != 6 or reads != minimum + 1 or fault):
        raise ValueError("GT SMI missing final sample")
    return guard


def flush_result(text, parent, reset, translation, primary, retire, memory):
    rows = [line for line in text.splitlines() if line.startswith("intel-flush-")]
    if len(rows) != 4 or not rows[0].endswith(" authority=none"):
        raise ValueError("GT flush receipt coverage or authority")
    summary = fields(rows[0].removesuffix(" authority=none"), "intel-flush-summary:",
                     ("v", "samples", "reads", "maps", "retired", "live"), ("live",), versions=(1, 2, 3, 4, 5))
    smi_rows = [line for line in text.splitlines() if line.startswith(SMI_PREFIX)]
    smi = smi_guard_result(smi_rows, summary["v"], parent, memory, retire)
    power = summary["v"] >= 5
    media = summary["v"] >= 4
    gam = summary["v"] >= 3
    words = (*WORDS, *GAM_WORDS) if gam else WORDS
    if media:
        words += MEDIA_WORDS
    if power:
        words += POWER_WORDS
    width = len(words)
    offsets = (0, 8, 10, 12, 13, width) if power else (0, 8, 10, width) if media else (0, width)
    pages = len(offsets) - 1
    count, reads, maps, retired, live = (summary[key] for key in ("samples", "reads", "maps", "retired", "live"))
    if (count > 3 or not count * width <= reads <= min(3, count + 1) * width
            or not count * pages <= retired <= maps <= min(3, count + 1) * pages
            or (maps != retired and not live)
            or (live and count == 3) or reads > maps * width):
        raise ValueError("GT flush accounting")

    if media:
        # Each later alias requires every read on the preceding pages.
        extra_maps = maps - count * pages
        extra_reads = reads - count * width
        if (maps - retired > 1 or
                not offsets[max(0, extra_maps - 1)] <= extra_reads <= offsets[extra_maps]):
            raise ValueError("GT media page accounting")
    sample_version = 4 if power else 3 if media else 2 if gam else 1
    samples = [fields(row, "intel-flush-sample:", ("v", "n", "begin", "end", *words), words,
                      versions=(sample_version,))
               for row in rows[1:]]
    if [row["n"] for row in samples] != [0, 1, 2]:
        raise ValueError("GT flush sample order")
    if ((parent["issued"] and count < 1) or (reset["begin"] and count < 2)
            or (retire["guard_begin"] and count != 3) or (parent["complete"] and count != 3)):
        raise ValueError("GT flush parent skipped an observation")

    starts_after = (translation["end0"], samples[0]["end"], primary["end1"])
    ends_before = (samples[1]["begin"], reset["begin"], retire["guard_begin"])
    for n, sample in enumerate(samples):
        values = [sample[key] for key in words]
        begin, end = sample["begin"], sample["end"]
        if any(word > 0xffffffff for word in values):
            raise ValueError("GT flush DWORD width")
        if begin and (n > count or not parent["begin"] <= starts_after[n] < begin <= parent["last"]):
            raise ValueError("GT flush sample start")
        if n < count:
            if (not 0 < begin < end <= parent["last"] or end - begin > 10000
                    or (ends_before[n] and end > ends_before[n]) or 0xffffffff in values):
                raise ValueError("GT flush completed sample clock or register")
        elif end or (not begin and any(values)):
            raise ValueError("GT flush partial sample")
    if ((samples[0]["begin"] and parent["pci_samples"] < 2)
            or (samples[1]["begin"] and (parent["prepared"], parent["issued"]) != (5, 5))
            or (samples[2]["begin"] and not primary["complete"])):
        raise ValueError("GT flush phase admission")

    engine_ack = [sum((bool(row[key] & 32) << index) for index, key in enumerate(ENGINES))
                  for row in samples[:count]]
    engine_ack_rise = (engine_ack[1] & ~engine_ack[0]) if count >= 2 else 0
    gam_state = [{"allowed": row["gam_go"] & GAM_GO_MASK, "idle": bool(row["gam_eco"] & 1)}
                 for row in samples[:count]] if gam else None
    result = dict(summary=summary, samples=samples[:count], partial=samples[count:],
                  engine_ack=engine_ack, engine_ack_rise=engine_ack_rise,
                  l3_flush_ack=[bool(row["l3"] & 1) for row in samples[:count]])
    if media:
        current = [{key: (row[key] >> 16) & 0xff for key in MEDIA_WORDS} for row in samples[:count]]
        for n, queues in enumerate(current):
            later_phase = (parent["issued"], reset["begin"], retire["guard_begin"])[n]
            if any(queues.values()) and (n + 1 != count or later_phase or parent["complete"] or not parent["failed"]):
                raise ValueError("GT advanced past outstanding media requests")
        result["media_outstanding"] = current
    if power:
        result["power_controls"] = [
            dict(hardware_control=bool(row["rc_control"] & (1 << 31)),
                 rc6_enabled=bool(row["rc_control"] & (1 << 18)),
                 timeout_mode=bool(row["rc_control"] & (1 << 28)),
                 evaluation_mode=bool(row["rc_control"] & (1 << 27)),
                 sw_target=(row["rc_target"] >> 16) & 7,
                 threshold_ticks=row["rc6_threshold"] & 0xffffff,
                 requested_clients=row["gam_req"] & GAM_GO_MASK)
            for row in samples[:count]
        ]
    if smi is not None:
        result["smi_guard"] = smi
    if gam:
        result["gam"] = gam_state
    return result
