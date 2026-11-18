"""Inspect engine and L3 messages in the original full-GT lifetime."""

from intel_gt_bootstrap import fields

ENGINES = ("rcs", "vcs0", "vecs", "vcs1", "bcs")
WORDS = (*ENGINES, "l3")


def flush_result(text, parent, reset, translation, primary, retire):
    rows = [line for line in text.splitlines() if line.startswith("intel-gt-flush-")]
    if len(rows) != 4 or not rows[0].endswith(" authority=none"):
        raise ValueError("GT flush receipt coverage or authority")
    summary = fields(rows[0].removesuffix(" authority=none"), "intel-gt-flush-summary:",
                     ("v", "samples", "reads", "maps", "retired", "live"), ("live",))
    count, reads, maps, retired, live = (summary[key] for key in ("samples", "reads", "maps", "retired", "live"))
    if (count > 3 or not count * 6 <= reads <= min(18, (count + 1) * 6)
            or not count <= retired <= maps <= min(3, count + 1)
            or (maps != retired and not live)
            or (live and count == 3) or reads > maps * 6):
        raise ValueError("GT flush accounting")

    samples = [fields(row, "intel-gt-flush-sample:", ("v", "n", "begin", "end", *WORDS), WORDS)
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
    return dict(summary=summary, samples=samples[:count], partial=samples[count:],
                engine_ack=engine_ack, engine_ack_rise=engine_ack_rise,
                l3_flush_ack=[bool(row["l3"] & 1) for row in samples[:count]])
