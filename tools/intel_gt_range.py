"""Decode the original full-GT owner's read-only GGTT preservation report."""
import base64
import re
import struct

from intel_gt_bootstrap import fields


def range_result(text, parent, reset, memory, retire, owner_complete, post):
    rows = [line for line in text.splitlines() if line.startswith("intel-range-")]
    if len(rows) != 3:
        raise ValueError("GGTT range receipt coverage")
    if not rows[0].endswith(" authority=none"):
        raise ValueError("GGTT range receipt authority")
    summary = fields(
        rows[0].removesuffix(" authority=none"), "intel-range-summary:",
        "v complete phase maps retired live reads tail candidate begin0 end0 begin1 end1 words0 words1".split(),
        ("live",), versions=(1, 2, 3, 4, 5),
    )
    dual = summary["v"] >= 3
    batches = 3 if dual else 2
    words = 132 if dual else 129
    phase = summary["phase"]
    maps, retired, live = (summary[k] for k in ("maps", "retired", "live"))
    if (phase > 2 or summary["complete"] != int(phase == 2)
            or (summary["tail"], summary["candidate"]) != (128, 8704)
            or not batches * phase <= retired <= maps <= batches * min(2, phase + 1)
            or not 0 <= maps - retired <= 1 or (maps != retired and not live)):
        raise ValueError("GGTT range receipt state")
    vectors = []
    for n, row in enumerate(rows[1:]):
        match = re.fullmatch(r"intel-range-vector: v=1 sample=([01]) count=(\d{1,3}) data=([A-Za-z0-9+/=]*)", row)
        if not match or int(match[1]) != n:
            raise ValueError("GGTT range vector index")
        count = int(match[2])
        if count > words or count != summary[f"words{n}"] or len(match[3]) != (count * 8 + 2) // 3 * 4:
            raise ValueError("GGTT range vector count")
        raw = base64.b64decode(match[3], validate=True)
        if len(raw) != count * 8 or base64.b64encode(raw).decode("ascii") != match[3]:
            raise ValueError("GGTT range vector encoding")
        vector = struct.unpack(f"<{count}Q", raw)
        if (1 << 64) - 1 in vector:
            raise ValueError("GGTT range floating entry")
        vectors.append(vector)
        begin, end = (summary[f"{k}{n}"] for k in ("begin", "end"))
        if n < phase:
            if dual and vector[129:] != (0x8f003001, 0x8f004001, 0x8f005001):
                raise ValueError("Independent Air slot backing changed")
            if count != words or not parent["begin"] <= begin < end <= parent["last"]:
                raise ValueError("GGTT range completed sample")
        elif end or (count and not begin) or (begin and not parent["begin"] <= begin <= parent["last"]):
            raise ValueError("GGTT range partial clock")
    count = sum(map(len, vectors))
    if not 3 * count <= summary["reads"] <= min(6 * words, 3 * count + 3):
        raise ValueError("GGTT range read accounting")
    if reset["begin"] and (phase < 1 or summary["end0"] > reset["begin"]):
        raise ValueError("GGTT range pre-reset order")
    if summary["begin1"] and (
        phase < 1 or not memory["summary"]["end"]
        or not summary["end0"] <= reset["begin"] <= memory["summary"]["end"] <= summary["begin1"]
    ):
        raise ValueError("GGTT range reset and maintenance order")
    if phase == 1 and any(a != b for a, b in zip(vectors[0], vectors[1][:-1])):
        raise ValueError("GGTT range reads after first mismatch")
    if phase == 2 and (
        (maps, retired, live, summary["reads"]) != (2 * batches, 2 * batches, 0, 6 * words)
        or vectors[0] != vectors[1]
        or (retire["guard_begin"] and summary["end1"] > retire["guard_begin"])
    ):
        raise ValueError("GGTT range preservation")
    if owner_complete and phase != 2:
        raise ValueError("GGTT range parent returned early")
    result = dict(summary=summary, vectors=[list(vector) for vector in vectors])
    slots = [line for line in text.splitlines() if line.startswith("intel-slot-summary:")]
    if len(slots) != int(summary["v"] >= 2):
        raise ValueError("GGTT slot receipt coverage")
    if slots:
        slot = fields(slots[0], "intel-slot-summary:", "v reserved rechecked returned candidate pages pte_writes".split())
        if ((slot["candidate"], slot["pages"], slot["pte_writes"]) != (8704, 1, 0)
                or not 0 <= slot["returned"] <= slot["rechecked"] <= slot["reserved"] <= 1
                or (phase >= 1 and not slot["reserved"]) or (phase == 2 and not slot["rechecked"])
                or (owner_complete and not slot["returned"])):
            raise ValueError("GGTT slot reservation lifetime")
        result["slot"] = slot
    independent = [line for line in text.splitlines() if line.startswith("intel-independent-slot:")]
    if len(independent) != int(dual):
        raise ValueError("Independent slot receipt coverage")
    if dual:
        second = fields(independent[0], "intel-independent-slot:",
                        "v slot reserved returned reopen_verified pte_writes".split())
        if ((second["slot"], second["pte_writes"]) != (12291, 0)
                or not slot["returned"] <= second["returned"] <= second["reserved"] <= slot["reserved"]
                or not 0 <= second["reopen_verified"] <= int(parent["pci_samples"] == 4)
                or second["returned"] > second["reopen_verified"]
                or (phase and not second["reserved"]) or (second["returned"] and phase != 2)):
            raise ValueError("Independent slot requester lifetime")
        result["independent_slot"] = second
    backing_rows = [line for line in text.splitlines() if line.startswith("intel-slot-backing:")]
    if len(backing_rows) != int(summary["v"] >= 4):
        raise ValueError("Missing or duplicate slot backing receipt")
    if backing_rows:
        b = fields(backing_rows[0], "intel-slot-backing:",
                   "v physical original planned checks acquired held freed pte_writes".split(),
                   ("physical", "original", "planned"))
        pa, checks = b["physical"], b["checks"]
        if (b["pte_writes"] or not 0 <= b["held"] <= b["acquired"] <= second["reserved"]
                or not 0 <= b["freed"] <= slot["returned"] or not phase <= checks <= 3
                or (checks == 3 and not second["reopen_verified"])
                or (checks and (not b["acquired"] or not 0 < pa < 2**32 or pa % 4096
                                or len(vectors[0]) < 132 or b["original"] != vectors[0][130]
                                or b["planned"] != (pa | 1) or b["planned"] == b["original"]))
                or (b["acquired"] and not b["held"] and not b["freed"])
                or (owner_complete and (checks, b["held"], b["freed"]) != (3, 0, 1))):
            raise ValueError("Invalid backing or submit port lifetime")
        result["slot_backing"] = b
    guard_rows = [line for line in text.splitlines() if line.startswith("intel-slot-widi:")]
    if len(guard_rows) != int(summary["v"] >= 5):
        raise ValueError("Missing or duplicate WiDi guard receipt")
    producers = [line for line in text.splitlines() if line.startswith("intel-widi-producers:")]
    messages = [line for line in text.splitlines() if line.startswith("intel-widi-device:")]
    reopened = [line for line in text.splitlines() if line.startswith("intel-slot-reopened:")]
    queue_rows = [line for line in text.splitlines() if line.startswith("intel-slot-queues:")]
    if not guard_rows and queue_rows:
        raise ValueError("Unexpected queue handoff")
    if len(reopened) != len(guard_rows):
        raise ValueError("Missing or duplicate reopened slot observation")
    if len(producers) != len(guard_rows) or (not guard_rows and messages):
        raise ValueError("Unexpected wireless producer evidence")
    if guard_rows:
        guard = fields(guard_rows[0], "intel-slot-widi:", "v acquired pages closed device_grant".split())
        if (guard["device_grant"] or not 0 <= guard["closed"] <= guard["acquired"] <= second["reserved"]
                or not 0 <= guard["pages"] <= 3 * guard["acquired"]
                or ((phase or b["acquired"]) and guard["pages"] != 3)
                or (guard["closed"] and not second["reopen_verified"]) or (b["freed"] and not guard["closed"])
                or (owner_complete and not guard["closed"])):
            raise ValueError("Invalid WiDi guard lifetime")
        words = "strap function well_before codec_before control codec_after well_after".split()
        prod = fields(producers[0], "intel-widi-producers:",
                      ("v", "disabled", *words, "maps", "retired", "live", "reads"), (*words, "live"))
        if (not 0 <= prod["retired"] <= prod["maps"] <= 5 or prod["reads"] > 7
                or prod["maps"] - prod["retired"] > 1 or (prod["maps"] != prod["retired"] and not prod["live"])
                or not guard["acquired"] <= prod["disabled"] <= 1
                or (prod["disabled"] and ((prod["maps"], prod["retired"], prod["live"], prod["reads"]) != (5,5,0,7)
                    or prod["strap"] & 0xc4100000 != 1 << 20 or prod["function"] >> 31
                    or not prod["well_before"] & prod["well_after"] & (1 << 30)
                    or prod["codec_before"] != 0x80862808 or prod["codec_after"] != 0x80862808
                    or prod["control"] & (1 << 30) or any(prod[k] >= 0xffffffff for k in words)))):
            raise ValueError("Invalid wireless producer preflight")
        samples = [fields(line, "intel-widi-device:", ("v","n","win","reset_flow"), ("win","reset_flow"))
                   for line in messages]
        if (len(samples) > 4 or [row["n"] for row in samples] != list(range(len(samples)))
                or any(max(row["win"], row["reset_flow"]) >= 0xffffffff for row in samples)
                or (samples and guard["pages"] != 3) or (owner_complete and len(samples) != 4)):
            raise ValueError("Incomplete WiDi device samples")
        final = fields(reopened[0],"intel-slot-reopened:", "v matched high0 low high1 reads maps retired live".split(),
                       ("high0","low","high1","live"), versions=(1,2))
        if (not 0 <= guard["closed"] <= final["matched"] <= second["reopen_verified"]
                or not 0 <= final["retired"] <= final["maps"] <= second["reopen_verified"]
                or (final["maps"] != final["retired"] and not final["live"]) or final["reads"] > final["maps"]*3
                or (final["matched"] and (final["reads"],final["maps"],final["retired"],final["live"],
                    final["high0"],final["low"],final["high1"]) != (3,1,1,0,0,0x8f004001,0))):
            raise ValueError("Invalid reopened slot observation")
        if final["v"] == 2:
            matches = 0
            samples_raw = []
            for n, line in enumerate(queue_rows):
                row = re.fullmatch(r"intel-slot-queues: v=1 phase=([01]) matched=([01]) data=([A-Za-z0-9+/]{246}==)", line)
                if (not row or int(row[1]) != n or matches != n or not post or not post["summary"]["complete"]
                        or parent["pci_samples"] < 3+n or not b["acquired"]):
                    raise ValueError("Invalid queue handoff phase")
                expected = [e[f"w{i}"] for e in post["rows"] if e["sample"] == 1 for i in range(8)]
                expected += [post["contexts"][1][f"c{i}"] for i in range(6)]
                words = list(struct.unpack("<46I", base64.b64decode(row[3], validate=True)))
                matched = int(row[2])
                if len(expected) != 46 or 0xffffffff in words or (matched and words != expected):
                    raise ValueError("Changed queue after reset")
                samples_raw.append(dict(phase=n, matched=matched, words=words))
                matches += matched
            if (parent["commands"] == 2 and matches < 1) or (final["matched"] and matches != 2):
                raise ValueError("Queue comparison missing at requester handoff")
            result["queue_handoff"] = samples_raw
        elif queue_rows:
            raise ValueError("Unexpected queue handoff for old slot receipt")
        result["reopened_pte"] = final
        result["widi_producers"] = prod
        result["widi_messages"] = samples
        result["widi_guard"] = guard
    return result
