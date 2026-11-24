"""Decode the original full-GT owner's read-only GGTT preservation report."""
import base64
import re
import struct
import uuid

from intel_gt_bootstrap import fields


def boot_handoff(text, slots):
    def rows(prefix, keys, hexkeys=()):
        return [fields(line, prefix, keys.split(), hexkeys) for line in text.splitlines() if line.startswith(prefix)]
    summaries = rows("intel-efi-handoff:", "v table state step backing drivers children protocols files ram grants", ("ram",))
    if len(summaries) != 1:
        raise ValueError("Missing or ambiguous boot producer")
    h = summaries[0]
    if (h["table"] > 1 or h["state"] > 2 or h["step"] > 6 or h["grants"] or h["backing"] > h["table"]
            or h["protocols"] > 32 or h["files"] > min(4,h["drivers"])
            or bool(h["ram"]) != bool(h["table"])
            or (h["ram"] and (h["ram"] % 4096 or h["ram"] < 0x100000 or h["ram"] > (1<<32)-19*4096))
            or (not h["table"] and any(h[k] for k in ("state","step","drivers","children","protocols","files")))):
        raise ValueError("Invalid EFI handoff state")
    ids = rows("intel-efi-guid:", "v kind index low high", ("low","high"))
    expected = [(kind, i) for kind, count in enumerate((h["protocols"], h["files"])) for i in range(count)]
    if [(r["kind"],r["index"]) for r in ids] != expected:
        raise ValueError("Incomplete EFI protocol or bound-driver inventory")
    for row in ids:
        row["guid"] = str(uuid.UUID(bytes_le=struct.pack("<QQ", row["low"], row["high"])))
    keys = "id subsystem mmio aperture framebuffer bytes plane surface"
    surfaces = rows("intel-efi-display:", "v "+keys, tuple(k for k in keys.split() if k!="bytes"))
    entries = rows("intel-efi-entry:", "v slot pte", ("pte",))
    captured = h["state"]==2
    if len(surfaces)!=int(captured) or len(entries)!=7*int(captured):
        raise ValueError("Incomplete firmware display or GGTT snapshot")
    if captured:
        d = surfaces[0]
        if (h["step"]!=6 or d["id"]!=0x16268086 or d["subsystem"]!=0x011b106b
                or not d["mmio"] or not d["aperture"] or not d["plane"] & (1<<31)
                or d["surface"] % 4096 or d["framebuffer"]!=d["aperture"]+d["surface"]
                or not d["bytes"] or d["surface"]+d["bytes"]>11744*4096
                or any(row["slot"]!=11744+i or not row["pte"] & 1 for i,row in enumerate(entries))):
            raise ValueError("Invalid boot display or command mappings")
    if slots["samples"] and (not captured or not h["backing"] or h["ram"]!=slots["ram"]
                             or any(row["pte"]!=0x8ede1001+i*4096 for i,row in enumerate(entries))):
        raise ValueError("Kernel command reservation disagrees with its boot producer")
    return dict(summary=h, guids=ids, display=surfaces, entries=entries)


def range_result(text, parent, reset, memory, retire, owner_complete, post):
    rows = [line for line in text.splitlines() if line.startswith("intel-range-")]
    if len(rows) != 3:
        raise ValueError("GGTT range receipt coverage")
    if not rows[0].endswith(" authority=none"):
        raise ValueError("GGTT range receipt authority")
    summary = fields(
        rows[0].removesuffix(" authority=none"), "intel-range-summary:",
        "v complete phase maps retired live reads tail candidate begin0 end0 begin1 end1 words0 words1".split(),
        ("live",), versions=(1, 2, 3, 4, 5, 6, 7, 8),
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
        message_words = ("win", "reset_flow")
        if summary["v"] >= 8:
            message_words += ("clock_before", "clock_ack", "clock_after")
        samples = [fields(line, "intel-widi-device:", ("v", "n", *message_words), message_words,
                          versions=(2,) if summary["v"] >= 8 else (1,)) for line in messages]
        if (len(samples) > 4 or [row["n"] for row in samples] != list(range(len(samples)))
                or any(row[k] >= 0xffffffff for row in samples for k in message_words)
                or (samples and guard["pages"] != 3) or (owner_complete and len(samples) != 4)):
            raise ValueError("Incomplete WiDi device samples")
        port_fields = ("pinned", "restored", "ro_mask") if summary["v"] >= 6 else ()
        writer_fields = ("writer", "hwsp", "hwstam") if reopened[0].startswith("intel-slot-reopened: v=4 ") else ()
        final = fields(reopened[0], "intel-slot-reopened:",
                       ("v", *port_fields, *writer_fields, "matched", "high0", "low", "high1", "reads", "maps", "retired", "live"),
                       ("high0", "low", "high1", "live", *writer_fields[1:]), versions=(3,4) if port_fields else (1,2))
        if bool(writer_fields) != (final["v"] == 4):
            raise ValueError("Missing blitter register loan")
        if writer_fields:
            stage = final["writer"]
            if (stage not in range(4) or (stage and final["pinned"] < 4)
                    or (stage < 2 and (final["hwsp"] or final["hwstam"]))
                    or max(final["hwsp"], final["hwstam"]) > 0xffffffff
                    or (final["ro_mask"] == 31 or final["pinned"] == 5) and stage != 3):
                raise ValueError("Invalid blitter register loan")
        if (not 0 <= guard["closed"] <= final["matched"] <= second["reopen_verified"]
                or not 0 <= final["retired"] <= final["maps"] <= second["reopen_verified"]
                or (final["maps"] != final["retired"] and not final["live"]) or final["reads"] > final["maps"]*3
                or (final["matched"] and (final["reads"],final["maps"],final["retired"],final["live"],
                    final["high0"],final["low"],final["high1"]) != (3,1,1,0,0,0x8f004001,0))):
            raise ValueError("Invalid reopened slot observation")
        if port_fields:
            pinned, restored, mask = (final[k] for k in port_fields)
            # Pages become read-only in engine order and are restored in reverse.
            if (pinned not in (0,2,4,5) or restored > 5 or mask not in (0,1,3,7,15,31)
                    or (restored and (not slot["returned"] or not guard["closed"] or pinned != 5))
                    or (restored and mask != (1 << (5-restored))-1)
                    or (not restored and not pinned <= mask.bit_count() <= min(5,pinned+2))
                    or ((queue_rows or parent["commands"] == 2) and pinned != 5)
                    or (owner_complete and (pinned,restored,mask) != (5,5,0))):
                raise ValueError("Invalid engine submission page lifetime")
        if final["v"] >= 2:
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
    commands = [line for line in text.splitlines() if line.startswith("intel-command-slots:")]
    if len(commands) != int(summary["v"] >= 7):
        raise ValueError("Command slot reservation receipt coverage")
    if commands:
        returned_key="boot_returned" if commands[0].startswith("intel-command-slots: v=2 ") else "freed"
        slots = fields(commands[0], "intel-command-slots:",
                       f"v first pages samples reserved returned ram {returned_key} mapped pinned observations retired device_grant".split(), ("ram",),
                       versions=(2,) if returned_key=="boot_returned" else (1,))
        samples = slots["samples"]
        if ((slots["first"], slots["pages"], slots["device_grant"]) != (11744, 7, 0)
                or samples > 3 or slots["reserved"] not in (0, 1) or int(samples > 0) > slots["reserved"]
                or (slots["ram"] and (not slots["reserved"] or slots["ram"] < 0x100000
                    or slots["ram"] % 4096 or slots["ram"] > (1 << 32) - 19*4096))
                or (samples and not slots["ram"]) or not 0 <= slots[returned_key] <= slots["returned"]
                or not 0 <= slots["pinned"] <= slots["mapped"] <= 7
                or not 0 <= slots["retired"] <= slots["mapped"]
                or (slots["retired"] and not guard["closed"])
                or not int(samples > 0) + int(samples == 3) <= slots["observations"] <= 2
                or (slots["observations"] and slots["pinned"] != 7)
                or (slots["observations"] == 2 and parent["pci_samples"] != 4)
                or (owner_complete and (not slots[returned_key] or slots["retired"] != 7))
                or not phase <= samples <= phase + 1
                or (samples == 3 and parent["pci_samples"] != 4)
                or not 0 <= slots["returned"] <= int(samples == 3)
                or (owner_complete and not slots["returned"])):
            raise ValueError("Command slot reservation lifetime")
        result["command_slots"] = slots
        if slots["v"]==2:
            result["efi_handoff"] = boot_handoff(text, slots)
    return result
