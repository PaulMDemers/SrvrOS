"""Decode the original full-GT owner's read-only GGTT preservation report."""
import base64
import re
import struct

from intel_gt_bootstrap import fields


def range_result(text, parent, reset, memory, retire, owner_complete):
    rows = [line for line in text.splitlines() if line.startswith("intel-range-")]
    if len(rows) != 3:
        raise ValueError("GGTT range receipt coverage")
    if not rows[0].endswith(" authority=none"):
        raise ValueError("GGTT range receipt authority")
    summary = fields(
        rows[0].removesuffix(" authority=none"), "intel-range-summary:",
        "v complete phase maps retired live reads tail candidate begin0 end0 begin1 end1 words0 words1".split(),
        ("live",),
    )
    phase = summary["phase"]
    maps, retired, live = (summary[k] for k in ("maps", "retired", "live"))
    if (phase > 2 or summary["complete"] != int(phase == 2)
            or (summary["tail"], summary["candidate"]) != (128, 8704)
            or not 2 * phase <= retired <= maps <= min(4, 2 * (phase + 1))
            or not 0 <= maps - retired <= 1 or (maps != retired and not live)):
        raise ValueError("GGTT range receipt state")
    vectors = []
    for n, row in enumerate(rows[1:]):
        match = re.fullmatch(r"intel-range-vector: v=1 sample=([01]) count=(\d{1,3}) data=([A-Za-z0-9+/=]*)", row)
        if not match or int(match[1]) != n:
            raise ValueError("GGTT range vector index")
        count = int(match[2])
        if count > 129 or count != summary[f"words{n}"] or len(match[3]) != (count * 8 + 2) // 3 * 4:
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
            if count != 129 or not parent["begin"] <= begin < end <= parent["last"]:
                raise ValueError("GGTT range completed sample")
        elif end or (count and not begin) or (begin and not parent["begin"] <= begin <= parent["last"]):
            raise ValueError("GGTT range partial clock")
    count = sum(map(len, vectors))
    if not 3 * count <= summary["reads"] <= min(774, 3 * count + 3):
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
        (maps, retired, live, summary["reads"]) != (4, 4, 0, 774)
        or vectors[0] != vectors[1]
        or (retire["guard_begin"] and summary["end1"] > retire["guard_begin"])
    ):
        raise ValueError("GGTT range preservation")
    if owner_complete and phase != 2:
        raise ValueError("GGTT range parent returned early")
    return dict(summary=summary, vectors=[list(vector) for vector in vectors])
