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


def smi_guard_result(rows, version, parent, memory, retire, selector_writes=0):
    if version == 1:
        if rows or parent["reason"] & SMI_FAULT:
            raise ValueError("Foreign GT SMI receipt")
        return None
    if len(rows) != 1 or not rows[0].endswith(" authority=none"):
        raise ValueError("GT SMI receipt coverage or authority")
    guard = fields(rows[0].removesuffix(" authority=none"), SMI_PREFIX,
                   ("v", "reads", "point", "fault", "first", "last"))
    reads, point, fault, first, last = (guard[key] for key in ("reads", "point", "fault", "first", "last"))
    writes = selector_writes + parent["commands"] + parent["native_writes"] + memory["writes"]
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


def bank_result(text, version, samples, reset_started):
    summaries = [line for line in text.splitlines() if line.startswith("intel-l3-steer:")]
    rows = [line for line in text.splitlines() if line.startswith("intel-l3-bank:")]
    if version < 9:
        if summaries or rows:
            raise ValueError("Unexpected L3 bank receipt")
        return None
    if len(summaries) != 1:
        raise ValueError("Missing or duplicate L3 bank summary")
    s = fields(summaries[0], "intel-l3-steer:",
               ("v", "opened", "mapped", "captured", "original", "final", "writes", "banks", "restored"),
               ("original", "final"))
    orig, count, writes = s["original"], s["banks"], s["writes"]
    if (len(rows) != count or count > 8 or writes > 9 or orig >= 0xffffffff or s["final"] > 0xffffffff
            or s["opened"] not in (0, 1, 3) or s["mapped"] not in (0, 1, 3) or s["mapped"] & ~s["opened"]
            or max(s["captured"], s["restored"]) > 1 or (s["opened"] and samples == 0)
            or (s["captured"] and s["opened"] != 3)
            or (not s["captured"] and (orig or writes or count or s["final"] or s["restored"]))
            or (s["restored"] and s["final"] != orig)):
        raise ValueError("Invalid L3 steering accounting")
    banks = []
    for i, line in enumerate(rows):
        b = fields(line, "intel-l3-bank:", ("v", "n", "selector", "sqc", "scratch"),
                   ("selector", "sqc", "scratch"))
        if (b["n"] != i or b["selector"] != ((orig & ~(15 << 24)) | (i << 24))
                or max(b["sqc"], b["scratch"]) >= 0xffffffff):
            raise ValueError("Invalid steered L3 bank observation")
        banks.append(b)
    if count == 8 and s["restored"]:
        expected = 7 + (((orig >> 24) & 15) != 0) + (((orig >> 24) & 15) != 7)
        if writes != expected:
            raise ValueError("Wrong L3 selector write count")
    if (samples >= 2 or reset_started) and (count != 8 or not s["restored"] or s["mapped"]):
        raise ValueError("Advanced without full L3 bank coverage and selector return")
    return dict(summary=s, banks=banks)


def flush_result(text, parent, reset, translation, primary, retire, memory):
    rows = [line for line in text.splitlines() if line.startswith("intel-flush-")]
    if not rows or not rows[0].endswith(" authority=none"):
        raise ValueError("GT flush receipt coverage or authority")
    summary = fields(rows[0].removesuffix(" authority=none"), "intel-flush-summary:",
                     ("v", "samples", "reads", "maps", "retired", "live"), ("live",), versions=(1, 2, 3, 4, 5, 6, 7, 8, 9))
    smi_rows = [line for line in text.splitlines() if line.startswith(SMI_PREFIX)]
    bank = bank_result(text, summary["v"], summary["samples"], reset["begin"])
    smi = smi_guard_result(smi_rows, summary["v"], parent, memory, retire,
                           bank["summary"]["writes"] if bank else 0)
    traced = summary["v"] >= 6
    if (summary["v"] >= 7) != (translation["v"] >= 2):
        raise ValueError("GT flush and slot reservation receipt versions disagree")
    slots = 4 if traced else 3
    if len(rows) != slots + 1:
        raise ValueError("GT flush sample coverage")
    power = summary["v"] >= 5
    media = summary["v"] >= 4
    gam = summary["v"] >= 3
    words = (*WORDS, *GAM_WORDS) if gam else WORDS
    if media:
        words += MEDIA_WORDS
    if power:
        words += POWER_WORDS
    if traced:
        words += ("l3_close", "go_close", *(key + "_close" for key in ENGINES), "eco_close", "req_close")
    width = len(words)
    offsets = (0, 8, 10, 12, 13, width) if power else (0, 8, 10, width) if media else (0, width)
    if traced:
        offsets = (0, 1, 9, 11, 13, 16, 24, width)
    pages = len(offsets) - 1
    count, reads, maps, retired, live = (summary[key] for key in ("samples", "reads", "maps", "retired", "live"))
    if (count > slots or not count * width <= reads <= min(slots, count + 1) * width
            or not count * pages <= retired <= maps <= min(slots, count + 1) * pages
            or (maps != retired and not live)
            or (live and count == slots) or reads > maps * width):
        raise ValueError("GT flush accounting")

    if media:
        # Each later alias requires every read on the preceding pages.
        extra_maps = maps - count * pages
        extra_reads = reads - count * width
        if (maps - retired > 1 or
                not offsets[max(0, extra_maps - 1)] <= extra_reads <= offsets[extra_maps]):
            raise ValueError("GT media page accounting")
    sample_version = 5 if traced else 4 if power else 3 if media else 2 if gam else 1
    samples = [fields(row, "intel-flush-sample:", ("v", "n", "begin", "end", *words), words,
                      versions=(sample_version,))
               for row in rows[1:]]
    if [row["n"] for row in samples] != list(range(slots)):
        raise ValueError("GT flush sample order")
    if ((parent["issued"] and count < 1) or (reset["begin"] and count < 2)
            or (traced and memory["begin"] and count < 3)
            or (retire["guard_begin"] and count != slots) or (parent["complete"] and count != slots)):
        raise ValueError("GT flush parent skipped an observation")

    starts_after = (translation["end0"], samples[0]["end"], primary["end1"])
    ends_before = (samples[1]["begin"], reset["begin"], retire["guard_begin"])
    if traced:
        starts_after = (*starts_after[:2], reset["end"], primary["end1"])
        ends_before = (*ends_before[:2], memory["begin"], retire["guard_begin"])
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
            or (traced and samples[2]["begin"] and not reset["complete"])
            or (samples[-1]["begin"] and not primary["complete"])):
        raise ValueError("GT flush phase admission")

    engine_ack = [sum((bool(row[key] & 32) << index) for index, key in enumerate(ENGINES))
                  for row in samples[:count]]
    engine_ack_rise = (engine_ack[1] & ~engine_ack[0]) if count >= 2 else 0
    gam_state = [{"allowed": row["gam_go"] & GAM_GO_MASK, "idle": bool(row["gam_eco"] & 1)}
                 for row in samples[:count]] if gam else None
    result = dict(summary=summary, samples=samples[:count], partial=samples[count:],
                  engine_ack=engine_ack, engine_ack_rise=engine_ack_rise,
                  l3_flush_ack=[bool(row["l3"] & 1) for row in samples[:count]])
    slice_rows = [line for line in text.splitlines() if line.startswith("intel-l3-power:")]
    if len(slice_rows) != (count if summary["v"] >= 8 else 0):
        raise ValueError("GT slice power sample coverage")
    if summary["v"] >= 8:
        decoded = []
        for n, line in enumerate(slice_rows):
            sample = fields(line, "intel-l3-power:", ("v", "n", "fuse", "before", "after"),
                            ("fuse", "before", "after"))
            if sample["n"] != n or any(sample[k] >= 0xffffffff for k in ("fuse", "before", "after")):
                raise ValueError("Invalid slice power observation")
            available = (sample["fuse"] >> 25) & 7
            before, after = sample["before"] & 7, sample["after"] & 7
            if (before | after) & ~available:
                raise ValueError("Powered slice outside available topology")
            decoded.append(dict(available=available, before=before, after=after))
        result["slice_power"] = decoded
    if bank:
        if count >= 2 and result["slice_power"][1] != dict(available=3, before=3, after=3):
            raise ValueError("L3 bank observation lacks slice power")
        result["l3_banks"] = bank
    if media:
        current = [{key: (row[key] >> 16) & 0xff for key in MEDIA_WORDS} for row in samples[:count]]
        for n, queues in enumerate(current):
            phases = [parent["issued"], reset["begin"]]
            if traced:
                phases.append(memory["begin"])
            later_phase = (phases + [retire["guard_begin"]])[n]
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
    trace_rows = [line for line in text.splitlines() if line.startswith("intel-gam-")]
    if traced:
        result["request_trace"] = request_trace(trace_rows, parent, reset, samples[:count], memory)
    elif trace_rows:
        raise ValueError("Foreign GAM trace")
    return result


def request_trace(rows, parent, reset, samples, memory):
    if not rows:
        raise ValueError("Missing GAM trace")
    summary = fields(rows[0], "intel-gam-trace:", ("v", "reads", "spans", "omitted", "max_gap_us"))
    size, reads, omitted, gap = (summary[k] for k in ("spans", "reads", "omitted", "max_gap_us"))
    if (len(rows) != size + 1 or size > 32 or reads > parent["guards"] or (omitted and size != 32)
            or bool(gap) != (reads > 1) or gap > parent["last"] - parent["begin"]):
        raise ValueError("GAM trace coverage")
    spans = []
    changes = []
    for i, line in enumerate(rows[1:]):
        s = fields(line, "intel-gam-span:", ("v", "n", "pulse", "word", "count", "first_begin",
                                            "first_end", "last_begin", "last_end"), ("word",))
        a, b, c, d = (s[k] for k in ("first_begin", "first_end", "last_begin", "last_end"))
        if (s["n"] != i or not s["count"] or s["word"] >= 0xffffffff or s["pulse"] > reset["reset_completed"]
                or (s["pulse"] and (not reset["begin"] or a < reset["begin"]))
                or (reset["end"] and s["pulse"] < reset["reset_completed"] and d > reset["end"])
                or not parent["begin"] <= a <= b <= d <= parent["last"] or not a <= c <= d
                or (s["count"] == 1 and (a, b) != (c, d))
                or (s["count"] > 1 and (b > c or d - a > (s["count"] - 1) * gap))):
            raise ValueError("GAM span fields or clock")
        if spans:
            prev = spans[-1]
            if (prev["last_end"] > a or b - prev["last_begin"] > gap or s["pulse"] < prev["pulse"]
                    or (prev["pulse"], prev["word"]) == (s["pulse"], s["word"])):
                raise ValueError("GAM span order or gap")
            old, new = (word >> 8 & 0xff for word in (prev["word"], s["word"]))
            if old != new:
                changes.append(dict(cleared=old & ~new, asserted=new & ~old,
                                    earliest_us=prev["last_begin"], latest_us=b,
                                    pulses_before=prev["pulse"], pulses_after=s["pulse"]))
        spans.append(s)
    if reads != sum(s["count"] for s in spans) + omitted:
        raise ValueError("GAM trace count")
    if parent["complete"] and not omitted and (
            {s["pulse"] for s in spans} != {0, 1, 2} or spans[0]["first_begin"] > samples[0]["begin"]
            or spans[-1]["last_end"] < samples[-1]["end"]):
        raise ValueError("GAM trace missed reset or recovery")
    windows = []
    for i, row in enumerate(samples):
        # Require an actual closed observation inside the packet. Any conflicting
        # sampled permission within it rules out even a sampled block.
        observed = [s for s in spans if s["last_end"] >= row["begin"] and s["first_begin"] <= row["end"]]
        allowed = (row["gam_req"] | row["req_close"] | row["gam_go"] | row["go_close"]) & GAM_GO_MASK
        trace_open = any(s["word"] & GAM_GO_MASK for s in observed)
        closed = bool(observed) and not (allowed or trace_open or omitted)
        ack = all(row[k] & 0x31 == 0x20 and row[k + "_close"] & 0x31 == 0x20 for k in ENGINES)
        l3 = row["l3"] & 3 == 1 and row["l3_close"] & 3 == 1
        idle = bool(row["gam_eco"] & row["eco_close"] & 1)
        empty = all((row[k] & 0xff0000) == 0 for k in MEDIA_WORDS)
        fresh = i > 0 and all(not samples[i - 1][k + "_close"] & 32 for k in ENGINES)
        fresh = fresh and not samples[i - 1]["l3_close"] & 1
        windows.append(dict(sample=i, observed_block=closed, fresh_flush_ack=bool(fresh and ack and l3),
                            gam_idle=idle, media_empty=empty,
                            sampled_block_and_flush=bool(closed and fresh and ack and l3 and idle and empty)))
    maintenance = None
    if len(samples) == 4 and memory["complete"]:
        before, after = samples[2:]
        # Packet 2 ends before the first maintenance write. Packet 3 starts
        # after maintenance has finished. Include every intervening request sample.
        interior = [s for s in spans if s["last_end"] >= before["end"] and s["first_begin"] <= after["begin"]]
        open_mask = 0
        for span in interior:
            open_mask |= span["word"] & GAM_GO_MASK
        blocked = bool(interior and windows[2]["observed_block"] and windows[3]["observed_block"] and not open_mask)
        maintenance = dict(begin_us=memory["begin"], end_us=memory["end"], observed_open_clients=open_mask,
                           observed_block=blocked, sampled_block_and_flush=blocked and windows[3]["sampled_block_and_flush"])
    return dict(summary=summary, spans=spans, reason_changes=changes,
                last_observed_reasons=(spans[-1]["word"] >> 8) & 0xff if spans and not omitted else None,
                windows=windows, maintenance=maintenance)
