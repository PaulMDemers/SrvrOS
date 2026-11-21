"""Read-only Broadwell auxiliary-display evidence under the original GT owner."""

from intel_gt_bootstrap import fields

WORDS = ("vga", "fbc", "fbc_status2", "ddi", "psr", "psr_status", "trans")


def auxiliary_result(text, parent, primary):
    rows = [line for line in text.splitlines() if line.startswith("intel-aux-")]
    if len(rows) != 3 or not rows[0].endswith(" authority=none"):
        raise ValueError("Auxiliary display receipt coverage or authority")
    summary = fields(
        rows[0].removesuffix(" authority=none"), "intel-aux-summary:",
        "v samples reads maps retired live stable inactive".split(), ("live",),
    )
    count, reads, maps, retired, live = (summary[k] for k in ("samples", "reads", "maps", "retired", "live"))
    if (count > 2 or not count * 7 <= reads <= min(14, (count + 1) * 7)
            or not count * 4 <= retired <= maps <= min(8, (count + 1) * 4)
            or maps - retired > 1 or (maps != retired and not live)):
        raise ValueError("Auxiliary display accounting")
    packets = [fields(line, "intel-aux-sample:", ("v", "n", "begin", "end", "blockers", *WORDS), WORDS)
               for line in rows[1:]]
    if [packet["n"] for packet in packets] != [0, 1]:
        raise ValueError("Auxiliary display sample order")
    original = None
    if count:
        keys = "v sample transcoder htotal vtotal vblank ddi config".split()
        captured = [fields(line, "intel-display-transcoder:", keys, keys[3:])
                    for line in text.splitlines() if line.startswith("intel-display-transcoder:")]
        matches = [row for row in captured if (row["sample"], row["transcoder"]) == (1, 3)]
        if len(matches) != 1:
            raise ValueError("Auxiliary display original eDP route")
        original = (matches[0]["ddi"], matches[0]["config"])
    for n, packet in enumerate(packets):
        if any(packet[key] > 0xffffffff for key in WORDS):
            raise ValueError("Auxiliary display DWORD width")
        start, finish = primary[f"begin{n}"], primary[f"end{n}"]
        begin, end = packet["begin"], packet["end"]
        if begin and not 0 < start < begin <= parent["last"]:
            raise ValueError("Auxiliary display sample outside primary bracket")
        if n < count:
            if not begin or not begin < end <= parent["last"] or (finish and end > finish):
                raise ValueError("Auxiliary display completed clock")
            if any(packet[key] == 0xffffffff for key in WORDS):
                raise ValueError("Auxiliary display floating word")
            route = (packet["ddi"], packet["trans"])
            blocked = (
                int(not packet["vga"] & (1 << 31))
                | (int(bool(packet["fbc"] & (1 << 31))) << 1)
                | (int(bool(packet["fbc_status2"] & 0xfff)) << 2)
                | (int(bool(packet["psr"] & (1 << 31))) << 3)
                | (int(bool(packet["psr_status"] & (7 << 29))) << 4)
                | (int(route != original or route != (0x82200000, 0xc0000000)) << 5)
            )
            if blocked != packet["blockers"]:
                raise ValueError("Auxiliary display blocker classification")
        elif end or packet["blockers"] or (not begin and any(packet[key] for key in WORDS)):
            raise ValueError("Auxiliary display partial sample")
    if count == 2 and (packets[0]["end"] >= packets[1]["begin"]
                       or (maps, retired, live, reads) != (8, 8, 0, 14)):
        raise ValueError("Auxiliary display sample retirement")
    if parent["complete"] and count != 2:
        raise ValueError("Auxiliary display parent returned early")
    stable = count == 2 and all(packets[0][key] == packets[1][key] for key in WORDS)
    inactive = stable and all(not packet["blockers"] for packet in packets)
    if (summary["stable"], summary["inactive"]) != (int(stable), int(inactive)):
        raise ValueError("Auxiliary display disposition")
    return dict(summary=summary, packets=packets)
