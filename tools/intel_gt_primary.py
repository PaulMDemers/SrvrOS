"""Verify the post-maintenance primary observation against the original capture."""

import base64
import re
import struct

from intel_gt_bootstrap import fields


def primary_result(text, parent, protection, retire, display):
    rows = [line for line in text.splitlines() if line.startswith("intel-primary-")]
    if not rows or not rows[0].endswith(" authority=none"):
        raise ValueError("Primary receipt coverage or authority")
    keys = "v complete samples reads maps retired live begin0 end0 begin1 end1 image_end pte_end candidate reason"
    summary = fields(rows[0].removesuffix(" authority=none"), "intel-primary-summary:", keys.split(), ("live",))
    complete, samples, reads, maps, retired, live = (
        summary[key] for key in ("complete", "samples", "reads", "maps", "retired", "live")
    )
    if (complete > 1 or samples > 2 or len(rows) != samples + 1 or summary["reason"] > 7
            or not samples * 56 <= reads <= min(112, (samples + 1) * 56)
            or not samples * 5 <= retired <= maps <= min(10, (samples + 1) * 5)
            or maps - retired > 1 or (maps != retired and not live)):
        raise ValueError("Primary receipt state")
    if summary["begin0"]:
        if protection["summary"]["complete"] != 1 or not (
                protection["summary"]["end1"] <= summary["begin0"] <= parent["last"]):
            raise ValueError("Primary sample before range comparison")
    elif any(summary[key] for key in keys.split() if key != "v"):
        raise ValueError("Primary absent observation")
    for n in range(2):
        begin, end = (summary[f"{key}{n}"] for key in ("begin", "end"))
        if n < samples and not 0 < begin < end <= parent["last"]:
            raise ValueError("Primary sample clock")
        if n >= samples and (end or begin > parent["last"]):
            raise ValueError("Primary partial clock")
    if summary["begin1"] and (samples < 1 or summary["begin1"] < summary["end0"]):
        raise ValueError("Primary sample order")
    vectors = []
    for n, row in enumerate(rows[1:]):
        match = re.fullmatch(rf"intel-primary-vector: v=1 sample={n} data=([A-Za-z0-9+/=]{{412}})", row)
        if not match:
            raise ValueError("Primary vector schema")
        raw = base64.b64decode(match[1], validate=True)
        if len(raw) != 308 or base64.b64encode(raw).decode("ascii") != match[1]:
            raise ValueError("Primary vector encoding")
        words = list(struct.unpack("<77I", raw))
        if 0xffffffff in words or any(words[index:index + 2] != [0, 0] for index in (3, 12, 21)):
            raise ValueError("Primary vector word")
        vectors.append(words)
    if vectors:
        pipe_keys = "v sample pipe source line frames control stride surface offset live cursor_control cursor_base".split()
        capture = [fields(line, "intel-display-pipe:", pipe_keys, pipe_keys[3:])
                   for line in text.splitlines() if line.startswith("intel-display-pipe:")]
        capture = [row for row in capture if row["sample"] == 1]
        well_keys = ("v sample begin_us end_us reads " + " ".join(
            f"{when}{i}" for when in ("before", "after") for i in range(4))).split()
        wells = [fields(line, "intel-display-sample:", well_keys, well_keys[5:])
                 for line in text.splitlines() if line.startswith("intel-display-sample:")]
        wells = [row for row in wells if row["sample"] == 1]
        readers = [row["raw"] for row in display if row["n"] == 1]
        if [row["pipe"] for row in capture] != [0, 1, 2] or len(wells) != 1 or len(readers) != 39:
            raise ValueError("Primary original capture coverage")
        power = [wells[0][f"after{i}"] for i in range(4)]
        if any(not word & (1 << 30) for word in power):
            raise ValueError("Primary power-well state")
        old = [row["source"] for row in capture]
        old.extend(row[key] if key else 0 for row in capture for key in (
            None, None, "control", "stride", "surface", "offset", "live", "cursor_control", "cursor_base",
        ))
        if any(vector != old + readers + power * 2 for vector in vectors):
            raise ValueError("Primary footprint differs from original display")
    if complete:
        if ((samples, reads, maps, retired, live, summary["reason"]) != (2, 112, 10, 10, 0, 0)
                or (retire["guard_begin"] and summary["end1"] > retire["guard_begin"])
                or (summary["image_end"], summary["pte_end"], summary["candidate"]) != (5554176, 6078464, 35651584)):
            raise ValueError("Primary completed footprint")
        source, control, stride = vectors[0][0], vectors[0][5], vectors[0][6]
        if (source, control & ~0x41000000, stride) != (0x059f0383, 0x98000400, 6144):
            raise ValueError("Primary supported geometry")
        if any(vectors[0][index] for index in (7, 8, 9, 10, 11, 14, 23)):
            raise ValueError("Primary supported pipe state")
        for pipe in range(3):
            start = 30 + pipe * 13
            reader = vectors[0][start:start + 13]
            if (pipe and reader[0]) or reader[5] or reader[8] or (not pipe and any(reader[1:5])):
                raise ValueError("Primary auxiliary reader is active or unsupported")
    elif parent["complete"]:
        raise ValueError("Primary parent returned before completing geometry")
    return dict(summary=summary, vectors=vectors)


def verify_primary_backing(backing, protection, primary):
    """Cross-check the raw original targets without granting the candidate."""
    retained = protection["vectors"][1]
    if protection["summary"]["v"] in (3, 4, 5, 6, 7, 8):
        # The last three entries cover the independent slot and its neighbours.
        if len(retained) != 132:
            raise ValueError("Primary physical backing coverage")
        retained = retained[:-3]
    image = backing["ptes"]
    aperture = primary["summary"]
    original = backing["rows"]["summary"][0]
    base, end = original["stolen_base"], original["stolen_end"]
    image_end = (aperture["image_end"] + 4095) & ~4095
    if (len(retained) != 129 or not 0 < base < end <= 2**32
            or (base | end) & 4095
            or original["offset"] & 4095 or not original["offset"] < aperture["image_end"] <= 2**32
            or not 1 <= len(image) <= 1536 or original["offset"] + len(image) * 4096 != image_end
            or aperture["pte_end"] != image_end + 128 * 4096):
        raise ValueError("Primary physical backing coverage")
    protected = image + retained[:-1]
    for word in protected + retained[-1:]:
        if word & 4095 != 1 or word >= 2**32 or not base <= word - 1 < end:
            raise ValueError("Primary physical backing encoding")
    if protected != list(range(protected[0], protected[0] + len(protected) * 4096, 4096)):
        raise ValueError("Primary physical backing continuity")
    if retained[-1] in protected:
        raise ValueError("Candidate original PTE aliases primary physical backing")
