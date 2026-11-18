"""Inspect engine and L3 messages in the original full-GT lifetime."""

from intel_gt_bootstrap import fields

ENGINES = ("rcs", "vcs0", "vecs", "vcs1", "bcs")
WORDS = (*ENGINES, "l3")

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
                     ("v", "samples", "reads", "maps", "retired", "live"), ("live",), versions=(1, 2))
    smi_rows = [line for line in text.splitlines() if line.startswith(SMI_PREFIX)]
    smi = smi_guard_result(smi_rows, summary["v"], parent, memory, retire)
    count, reads, maps, retired, live = (summary[key] for key in ("samples", "reads", "maps", "retired", "live"))
    if (count > 3 or not count * 6 <= reads <= min(18, (count + 1) * 6)
            or not count <= retired <= maps <= min(3, count + 1)
            or (maps != retired and not live)
            or (live and count == 3) or reads > maps * 6):
        raise ValueError("GT flush accounting")

    samples = [fields(row, "intel-flush-sample:", ("v", "n", "begin", "end", *WORDS), WORDS)
               for row in rows[1:]]
    if [row["n"] for row in samples] != [0, 1, 2]:
        raise ValueError("GT flush sample order")
    if ((parent["issued"] and count < 1) or (reset["begin"] and count < 2)
            or (retire["guard_begin"] and count != 3) or (parent["complete"] and count != 3)):
        raise ValueError("GT flush parent skipped an observation")

    starts_after = (translation["end0"], samples[0]["end"], primary["end1"])
    ends_before = (samples[1]["begin"], reset["begin"], retire["guard_begin"])
    for n, sample in enumerate(samples):
        words = [sample[key] for key in WORDS]
        begin, end = sample["begin"], sample["end"]
        if any(word > 0xffffffff for word in words):
            raise ValueError("GT flush DWORD width")
        if begin and (n > count or not parent["begin"] <= starts_after[n] < begin <= parent["last"]):
            raise ValueError("GT flush sample start")
        if n < count:
            if (not 0 < begin < end <= parent["last"] or end - begin > 10000
                    or (ends_before[n] and end > ends_before[n]) or 0xffffffff in words):
                raise ValueError("GT flush completed sample clock or register")
        elif end or (not begin and any(words)):
            raise ValueError("GT flush partial sample")
    if ((samples[0]["begin"] and parent["pci_samples"] < 2)
            or (samples[1]["begin"] and (parent["prepared"], parent["issued"]) != (5, 5))
            or (samples[2]["begin"] and not primary["complete"])):
        raise ValueError("GT flush phase admission")

    engine_ack = [sum((bool(row[key] & 32) << index) for index, key in enumerate(ENGINES))
                  for row in samples[:count]]
    engine_ack_rise = (engine_ack[1] & ~engine_ack[0]) if count >= 2 else 0
    return dict(summary=summary, samples=samples[:count], partial=samples[count:], smi_guard=smi,
                engine_ack=engine_ack, engine_ack_rise=engine_ack_rise,
                l3_flush_ack=[bool(row["l3"] & 1) for row in samples[:count]])
