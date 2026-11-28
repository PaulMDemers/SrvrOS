"""Strict receipts for the inherited Air7,2 eDP baseline trial."""
import re

from intel_display_backing import backing_result

PROFILE = "intel-edp-baseline-v1"
CYCLE_PROFILE = "intel-edp-cycle-v1"
FLR_PREFLIGHT_PROFILE = "intel-edp-flr-preflight-v1"
DARK_WELL_PROFILE = "intel-edp-dark-well-v1"
PCI_HOLD_PROFILE = "intel-edp-pci-hold-v1"
DARK_PWM_PROFILE = "intel-edp-dark-pwm-v1"
DARK_PPS_PROFILE = "intel-edp-dark-pps-v1"
PREFIX = "ram-usb-v1-intel-edp-baseline-"
SUMMARY = "intel-edp-baseline"
SCHEMAS = {
    SUMMARY: "v complete same changed retained pte_writes reset",
    "intel-edp-window": "v sample status reason events writes restored retained hash port buf tp pll cdclk",
    "intel-edp-poll": "v sample iir",
    "intel-edp-crc": "v sample event frame value",
    "intel-edp-phy": "v sample entry low high",
    "intel-edp-pattern": "v repeat",
    "intel-edp-platform": "v samples writes reason acquired released retained begin end",
    "intel-edp-platform-word": ("v phase bios driver kvmr debug fuse pll cdclk gate chicken "
                               "pps_status pps_ctl pps_on pps_off pps_div cpu_pwm cpu_duty pch_pwm pch_duty"),
    "intel-edp-cycle-preflight": ("v complete hash blockers imr ier iir aux fitter audio "
                                 "ddi_b ddi_c ddi_d ddi_e wm1 wm2 wm3"),
    "intel-edp-cycle": ("v attempted complete stage shutdown_error restore_error restore_result offset word panel "
                        "aux dark_hash crc_checks crc0 crc1 mask_before mask_live mask_after mask_writes "
                        "mask_restored shutdown_writes restore_writes well_released retired retained"),
    "intel-edp-panel-off": "v outcome samples first last elapsed_us mask_error",
    "intel-edp-flr-preflight": "v complete pixels configs bytes released reset",
    "intel-edp-flr-pixels": "v phase digest elapsed_us",
    "intel-edp-flr-config": "v phase index word",
    "intel-edp-dark-well": "v complete samples writes elapsed_us",
    "intel-edp-dark-well-word": "v phase bios driver pps",
    "intel-edp-pci-hold": "v complete samples writes polls elapsed_us settle_us first_clear_us cpu_copy digest backing retained",
    "intel-edp-pci-hold-word": "v phase command af",
    "intel-edp-rc6": "v seq begin_us end_us core residency wake_ack",
    "intel-edp-candidate": "v phase slot complete reads retired begin_us end_us command af left target right",
    "intel-edp-dark-census": ("v first end_slot complete entries empty first_empty framebuffer_aliases "
                             "maps retired begin_us end_us command pte_writes"),
    "intel-edp-reopen": "v seq check at ok read got want",
    "intel-edp-pwm": "v complete retained writes elapsed_us before_ctl before_data after_ctl after_data",
    "intel-edp-pps": "v complete retained samples writes wait_us program_us initial_status idle_status",
    "intel-edp-pps-word": "v phase on off div",
}
FLR_ROWS = ("intel-edp-flr-preflight", "intel-edp-flr-pixels", "intel-edp-flr-config")
WELL_ROWS = ("intel-edp-dark-well", "intel-edp-dark-well-word")
HOLD_ROWS = ("intel-edp-pci-hold", "intel-edp-pci-hold-word")
PPS_ROWS = ("intel-edp-pps", "intel-edp-pps-word")
DARK_CENSUS_STOPS = ("complete", "owner", "geometry", "clock", "cache", "map", "alias", "read", "retire")


def recipe(command, *, cycle=False, flr=False, dark_well=False, pci_hold=False, dark_pwm=False, dark_pps=False):
    flr = flr or dark_well or pci_hold or dark_pwm or dark_pps
    cycle = cycle or flr
    subtest = ("dark-pps" if dark_pps else "dark-pwm" if dark_pwm else "pci-hold" if pci_hold else
               "dark-well" if dark_well else "preflight")
    match = re.fullmatch(
        r"srvros.storage=ram srvros.workspace-volume=([0-9a-f]{8}) "
        r"srvros.desktop-release=([0-9a-f]{16}) srvros.capture=intel-display "
        r"srvros.intel-backing=read-only srvros.intel-cache=read-only "
        r"srvros.intel-edp-baseline=on" +
        (r" srvros.intel-edp-cycle=on" if cycle else "") +
        (r" srvros.intel-edp-flr=" + subtest if flr else "") +
        r" srvros.capture-failure=reboot", command)
    if not match:
        raise ValueError("Invalid eDP baseline recipe")
    return dict(volume=match[1], release=match[2])


def rows_of(text):
    rows = {name: [] for name in SCHEMAS}
    for line in text.splitlines():
        name, _, data = line.partition(": v=")
        if name not in rows or not data:
            continue
        row = {}
        for item in ("v=" + data).split():
            key, _, value = item.partition("=")
            if key in row or not re.fullmatch(r"0x[0-9a-fA-F]{1,16}|[0-9]{1,20}", value):
                raise ValueError("Malformed eDP field")
            row[key] = int(value, 16 if value.startswith("0x") else 10)
        versions = (1, 2, 3, 4, 5) if name == "intel-edp-pci-hold" else (1,)
        keys = SCHEMAS[name].split()
        if name == "intel-edp-dark-census":
            versions = (1, 2)
            if row["v"] == 2:
                keys.append("stop")
        if row.keys() != set(keys) or row["v"] not in versions or any(v >= 1 << 64 for v in row.values()):
            raise ValueError("Unsupported eDP row")
        rows[name].append(row)
    return rows


def platform_result(rows, *, native):
    summaries = rows["intel-edp-platform"]
    samples = rows["intel-edp-platform-word"]
    if not summaries and not samples:
        return False
    if len(summaries) != 1 or not rows["intel-edp-pattern"]:
        raise ValueError("Missing eDP platform summary or reference pattern")
    p = summaries[0]
    if (p["samples"] > 4 or p["writes"] > 2 or p["reason"] > 6 or
            [s["phase"] for s in samples] != list(range(p["samples"])) or
            p["end"] < p["begin"] or any(p[k] > 1 for k in ("acquired", "released", "retained"))):
        raise ValueError("Invalid eDP platform result")
    if any(s[k] >= 1 << 32 for s in samples for k in s if k not in ("v", "phase")):
        raise ValueError("Invalid eDP platform register")
    if not native and (p["writes"] or p["acquired"] or p["released"] or p["samples"]):
        raise ValueError("VM reached native eDP platform access")
    if p["retained"] != bool(p["writes"] and not p["released"]):
        raise ValueError("Unretired eDP driver request")
    if p["acquired"] and (p["samples"] < 2 or not p["writes"]):
        raise ValueError("Missing eDP driver acquisition evidence")
    if not p["released"]:
        return False
    if (p["samples"] != 4 or p["writes"] != 2 or not p["acquired"] or p["retained"] or
            not 0 < p["end"] - p["begin"] < 600000 or p["reason"] not in (0, 5)):
        raise ValueError("Invalid eDP driver release")
    first = samples[0]
    period, duty = first["pch_duty"] >> 16, first["pch_duty"] & 0xffff
    if ((first["bios"], first["driver"], first["kvmr"], first["debug"]) !=
            (0xc0000000, 0x40000000, 0x40000000, 0x40000000) or
            first["fuse"] & (1 << 24) or (first["pll"], first["cdclk"]) != (0x4c000000, 674) or
            first["pps_status"] & 0xb800000f != 0x80000008 or first["pps_ctl"] & 0xd != 5 or
            first["cpu_pwm"] & (1 << 31) or not first["pch_pwm"] & (1 << 31) or
            not 0 < period or duty > period):
        raise ValueError("Invalid inherited eDP platform profile")
    for phase, row in enumerate(samples):
        driver = 0xc0000000 if phase in (1, 2) else 0x40000000
        if row != {**first, "phase": phase, "driver": driver}:
            raise ValueError("eDP platform changed during the well request")
    return True


def cycle_preflight(rows, *, native):
    entries = rows["intel-edp-cycle-preflight"]
    if not entries:
        return None
    if len(entries) != 1 or not rows["intel-edp-platform"]:
        raise ValueError("Missing eDP cycle platform evidence")
    r = entries[0]
    if (r["complete"] > 1 or r["blockers"] > 255 or
            any(r[k] >= 1 << 32 for k in r if k != "hash")):
        raise ValueError("Invalid eDP cycle preflight result")
    if not native and any(r[k] for k in r if k != "v"):
        raise ValueError("VM reached native eDP cycle preflight")
    if r["complete"]:
        tests = [r["imr"] >> 31, r["ier"] >> 31, r["iir"] >> 31, r["aux"] >> 31,
                 r["fitter"] >> 31, r["audio"] & 0x444,
                 (r["ddi_b"] | r["ddi_c"] | r["ddi_d"] | r["ddi_e"]) >> 31,
                 (r["wm1"] | r["wm2"] | r["wm3"]) >> 31]
        if not r["hash"] or r["blockers"] != sum(bool(test) << bit for bit, test in enumerate(tests)):
            raise ValueError("eDP cycle blockers contradict the register values")
    return r


def cycle_result(rows, *, native, baseline_complete, preflight):
    entries = rows["intel-edp-cycle"]
    if len(entries) != 1 or preflight is None:
        raise ValueError("Missing native eDP cycle evidence")
    c = entries[0]
    flags = ("attempted", "complete", "mask_restored", "well_released", "retired", "retained")
    if (any(c[k] > 1 for k in flags) or any(c[k] >= 1 << 32 for k in c if k != "dark_hash") or
            c["stage"] > 8 or c["shutdown_error"] > 9 or c["restore_error"] > 9 or
            c["restore_result"] > 12 or c["panel"] > 6 or c["crc_checks"] > 2 or
            c["mask_writes"] > 2 or c["shutdown_writes"] > 9 or c["restore_writes"] > 128):
        raise ValueError("Invalid native eDP cycle values")
    if not native and any(c[k] for k in c if k != "v"):
        raise ValueError("VM reached native eDP cycle access")
    if (c["retired"] and c["retained"] or
            (not c["attempted"] and any(c[k] for k in c if k not in ("v", "retired", "retained"))) or
            (c["attempted"] and (not baseline_complete or not (c["well_released"] or c["retained"]))) or
            (c["mask_writes"] and c["mask_before"] != preflight["imr"]) or
            (c["mask_restored"] and c["mask_after"] != c["mask_before"])):
        raise ValueError("Contradictory native eDP cycle lifetime")
    finished = c["stage"] == 8 and c["retired"] and not c["retained"]
    if c["complete"] != finished:
        raise ValueError("Contradictory native eDP cycle completion")
    if finished:
        reference = rows["intel-edp-crc"][10]["value"]
        if (not c["attempted"] or not c["well_released"] or
                any(c[k] for k in ("shutdown_error", "restore_error", "restore_result")) or
                c["panel"] != 6 or c["crc_checks"] != 2 or
                (c["crc0"], c["crc1"]) != (reference, reference) or
                c["dark_hash"] != preflight["hash"] or preflight["blockers"] != 1 or
                preflight["ier"] or preflight["iir"] or
                c["mask_writes"] != 2 or not c["mask_restored"] or
                c["mask_live"] != c["mask_before"] & 0x7fffffff or
                c["shutdown_writes"] != 9 or not c["restore_writes"]):
            raise ValueError("Native eDP cycle lacks dark backing, CRC or mask restoration proof")
    return c


def panel_off_result(rows, cycle, *, native):
    entries = rows["intel-edp-panel-off"]
    if not entries:
        return None
    if len(entries) != 1:
        raise ValueError("Duplicate eDP panel-off evidence")
    off = entries[0]
    outcome = off["outcome"]
    if (outcome > 3 or off["samples"] > 3501 or off["mask_error"] > 9 or
            any(off[key] >= 1 << 32 for key in ("first", "last")) or
            (not native and any(off[k] for k in off if k != "v")) or
            (off["mask_error"] and (not cycle["attempted"] or cycle["mask_restored"])) or
            (not off["samples"] and (off["first"] or off["last"])) or
            (not outcome and any(off[key] for key in ("samples", "elapsed_us"))) or
            (outcome and (cycle["stage"] < 5 or cycle["shutdown_writes"] < 8)) or
            (cycle["stage"] > 5 and outcome != 3)):
        raise ValueError("Invalid eDP panel-off receipt")
    if outcome == 2 and (not off["samples"] or off["elapsed_us"] < 350000 or cycle["shutdown_error"] != 4):
        raise ValueError("Missing eDP panel-off deadline evidence")
    if outcome == 3 and (not off["samples"] or off["elapsed_us"] >= 350000 or off["last"] & 0xb0000000):
        raise ValueError("Missing completed eDP panel-off status")
    return off


def dark_well_result(rows, *, native, display):
    summaries, samples = (rows[k] for k in WELL_ROWS)
    if len(summaries) != 1:
        raise ValueError("Missing dark well summary")
    s = summaries[0]
    complete = len(samples) == 3 and s["writes"] == 2 and bool(s["elapsed_us"])
    if (s["samples"] > 3 or [w["phase"] for w in samples] != list(range(s["samples"])) or
            s["writes"] > 2 or not s["samples"] - 1 <= s["writes"] <= s["samples"] or
            s["elapsed_us"] >= 100000 or (not samples and s["elapsed_us"]) or
            s["complete"] != complete or (display["complete"] and not complete) or
            ((samples or s["writes"]) and (not native or display["stage"] < 6)) or
            (s["writes"] and not complete and not display["retained"])):
        raise ValueError("Invalid dark well coverage")
    for w in samples:
        if (w["bios"] != 0xc0000000 or w["driver"] != (0x40000000 if w["phase"] == 1 else 0xc0000000) or
                w["pps"] >= 1 << 32 or w["pps"] & 0xb0000000):
            raise ValueError("Display well or panel power changed unexpectedly")
    return s


def pci_reopen_result(rows, summary):
    checks = rows["intel-edp-reopen"]
    if summary["v"] == 1:
        if checks:
            raise ValueError("Unexpected legacy PCI reopen receipt")
        return None
    if bool(checks) != (summary["samples"] >= 4):
        raise ValueError("Missing or premature PCI reopen receipt")
    platform = (0x45400, 0x45408, 0x4540c, 0x130040, 0x46200, 0xc2020, 0xc2004,
                0xc7208, 0xc720c, 0xc7210, 0x45404, 0x48250, 0xc8250, 0xc8254)
    fences = [(9, 0x100000), (10, 0), (11, 0x100000)]
    plan = [(1, 0), *((2, offset) for offset in platform), (3, 0), (4, 0), (5, 0),
            (6, 0), (7, 0), (8, 0), *fences]
    if summary["v"] >= 3:
        plan[1 + len(platform):1 + len(platform)] = [(21, 0), (22, 4)]
    for first in (0, 512, 1024):
        page = 0x800000 + first * 8
        plan.extend(((12, page), (13, first), (14, page)))
    plan.extend((*fences, (6, 1), (7, 1), (8, 1), (15, 0), (16, 0), (17, 0), (18, 0), (19, 0), (20, 0)))
    if len(checks) > len(plan) or (summary["backing"] and len(checks) != len(plan)):
        raise ValueError("Invalid PCI reopen receipt coverage")
    for seq, (row, (check, at)) in enumerate(zip(checks, plan)):
        where = row["at"]
        allowed = (row["check"], where) == (check, at)
        if not row["ok"]:
            if check == 22:
                allowed = row["check"] == 22 and where <= 4
            elif check == 10:
                allowed = row["check"] == check and where < 32
            elif check == 13:
                allowed = row["check"] == check and at <= where < min(at + 512, 1484)
            elif check == 17:
                allowed = ((row["check"] in (17, 18) and where < 1350) or
                           (row["check"] == 19 and where < 1382400))
        elif check in (21, 22):
            driver = 0x40000000 if check == 21 else 0xc0000000
            allowed = allowed and row["read"] and row["got"] == row["want"] == driver
        elif check in (17, 18, 19):
            count = 1350 if check != 19 else 1382400
            allowed = allowed and row["read"] and row["got"] == row["want"] == count
        if (not allowed or row["seq"] != seq or row["ok"] > 1 or row["read"] > 1 or
                (not row["read"] and row["got"]) or
                (not row["ok"] and (seq != len(checks) - 1 or summary["backing"]))):
            raise ValueError("Invalid PCI reopen check")
    check_names = (
        "display lease", "platform register", "framebuffer setup", "display source ownership",
        "translation admission", "live endpoint read", "endpoint PCI Command", "endpoint match",
        "fence mapping", "fence contents", "fence unmap", "GGTT mapping", "PTE contents",
        "GGTT unmap", "translation close", "backing digest", "framebuffer page guard",
        "framebuffer page mapping", "framebuffer contents", "framebuffer close",
        "release driver well", "recover display resources",
    )
    if checks and not checks[-1]["ok"]:
        return dict(checks[-1], name=check_names[checks[-1]["check"] - 1])
    if len(checks) not in (0, len(plan)):
        raise ValueError("Truncated PCI reopen receipt")
    return None


def dark_rc6_result(points, hold):
    if hold["v"] < 4:
        if points:
            raise ValueError("Unexpected legacy RC6 samples")
        return None
    if (len(points) > 2 or (points and hold["samples"] < 2) or
            (hold["writes"] >= 2 and len(points) != 2)):
        raise ValueError("Invalid dark RC6 sample coverage")
    for i, p in enumerate(points):
        if (p["seq"] != i or not hold["settle_us"] <= p["begin_us"] <= p["end_us"] <= hold["elapsed_us"]
                or p["end_us"] >= 2000000 or (i and p["begin_us"] < points[0]["end_us"] + 2000)
                or any(p[k] >= 1 << 32 for k in ("core", "residency", "wake_ack"))):
            raise ValueError("Invalid dark RC6 sample")
    observation = dict(samples=points)
    if len(points) == 2:
        observation["counter_delta_ticks_mod32"] = (points[1]["residency"] - points[0]["residency"]) & 0xffffffff
    return observation


def candidate_result(points, config, hold, display, *, native):
    if hold["v"] < 5:
        if points:
            raise ValueError("Unexpected legacy eDP candidate evidence")
        return None
    if (len(points) > 3 or (points and not native) or
            (len(points) >= 2 and (hold["samples"] < 4 or display["stage"] < 6)) or
            (len(points) == 3 and not hold["complete"]) or
            (display["complete"] and len(points) != 3)):
        raise ValueError("Invalid eDP candidate coverage")
    words = ("left", "target", "right")
    original = next((r["word"] for r in config if r["phase"] == 0 and r["index"] == 3), None)
    for i, point in enumerate(points):
        reads, command = point["reads"], point["command"]
        begin, end = point["begin_us"], point["end_us"]
        expected = (original & 0xffff0000) | (0x402 if i == 1 else 7) if original is not None else None
        valid_complete = (reads == 3 and point["retired"] == 1 and 0 < begin < end < begin + 100000)
        if (point["phase"] != i or point["slot"] != 12292 or reads > 3 or point["retired"] > 1 or
                point["complete"] > 1 or (point["complete"] and not valid_complete) or point["af"] or
                (reads and not command) or (end and (not begin or end < begin)) or
                (command and (not begin or command != expected)) or
                (i and begin and points[i-1]["end_us"] and begin < points[i-1]["end_us"]) or
                any(point[k] for k in words[reads:]) or any(point[k] == 2**64-1 for k in words[:reads])):
            raise ValueError("Invalid eDP candidate sample")
    preserved = len(points) == 3 and all(p["complete"] for p in points) and all(
        p[k] == points[0][k] for p in points[1:] for k in words)
    return dict(samples=points, preserved=preserved)


def dark_census_result(rows, points, hold, pixels, *, native):
    if not rows:
        return None
    if not native or len(rows) != 1 or hold["samples"] < 4 or len(points) < 2:
        raise ValueError("Missing dark GGTT census ownership boundary")
    census = rows[0]
    first, end_slot = census["first"], census["end_slot"]
    entries, empty, first_empty = (census[key] for key in ("entries", "empty", "first_empty"))
    maps, retired = census["maps"], census["retired"]
    begin, end = census["begin_us"], census["end_us"]
    dark = points[1]
    limit = end_slot - first
    if ((first, end_slot) != (8704, 65536) or census["pte_writes"] or census["complete"] > 1 or
            not 0 < dark["begin_us"] <= begin <= end or
            (dark["end_us"] and begin < dark["end_us"]) or
            (len(points) == 3 and points[2]["begin_us"] and end > points[2]["begin_us"]) or
            not 0 <= retired <= maps <= limit // 512 or maps - retired > 1 or
            not 0 <= entries <= maps * 512 or not 0 <= empty + census["framebuffer_aliases"] <= entries or
            (empty == 0 and first_empty) or (empty and not first <= first_empty < first + entries) or
            (census["command"] and (census["command"] & 0xffff) != 0x402) or
            (census["command"] and dark["command"] and census["command"] != dark["command"]) or
            (maps and not census["command"]) or (hold["complete"] and maps != retired)):
        raise ValueError("Invalid dark GGTT census scope")
    if census["complete"] and (entries != limit or maps != limit // 512 or retired != maps or
                               not 0 < end - begin < 500000):
        raise ValueError("Incomplete dark GGTT census marked complete")
    report = dict(survey=census, write_authority=False)
    if census["v"] == 2:
        stop = census["stop"]
        if (stop >= len(DARK_CENSUS_STOPS) or bool(census["complete"]) != (stop == 0) or
                (stop == 8) != (maps != retired) or (not census["command"] and stop != 1) or
                len(pixels) < 2 or [p["phase"] for p in pixels[:2]] != [0, 1] or
                not pixels[0]["digest"] or pixels[0]["digest"] != pixels[1]["digest"]):
            raise ValueError("Invalid dark GGTT census stop or framebuffer verification")
        report["stop"] = DARK_CENSUS_STOPS[stop]
    return report


def pci_hold_result(rows, *, native, display):
    summaries, samples = (rows[k] for k in HOLD_ROWS)
    if len(summaries) != 1:
        raise ValueError("Missing PCI decode hold summary")
    s = summaries[0]
    n = s["samples"]
    if (n > 5 or [w["phase"] for w in samples] != list(range(n)) or
            s["writes"] > 4 or not n - 1 <= s["writes"] <= n or s["polls"] > 99 or
            not s["polls"] * 100 <= s["settle_us"] < 10000 or s["elapsed_us"] < s["settle_us"] or
            s["first_clear_us"] > s["settle_us"] or
            any(s[key] > 1 for key in ("cpu_copy", "backing", "complete", "retained")) or
            (not native and any(v for k, v in s.items() if k != "v")) or
            (n and display["stage"] < 6) or (s["retained"] and not display["retained"]) or
            s["retained"] != bool(s["writes"] and not s["complete"])):
        raise ValueError("Invalid PCI decode hold coverage")
    expected = (7, 0x403, 0x400, 0x402, 7)
    if n:
        first = [r["word"] for r in rows["intel-edp-flr-config"] if r["phase"] == 0 and r["index"] == 3]
        if first != [samples[0]["command"]] or first[0] >= 1 << 32 or first[0] & 0x10ffff != 0x100007:
            raise ValueError("PCI decode hold is detached from the original configuration")
        for w, command in zip(samples, expected):
            if (w["command"] != (first[0] & 0xffff0000) | command or w["af"] not in (0, 0x100) or
                    (w["phase"] and w["af"])):
                raise ValueError("Invalid PCI command or AF status")
    pixels = rows["intel-edp-flr-pixels"]
    if ((n >= 2 and (s["polls"] < 2 or not 100 <= s["first_clear_us"] <= s["settle_us"] - 100)) or
            (s["digest"] and n < 3) or
            (s["cpu_copy"] and (n < 3 or not pixels or s["digest"] != pixels[0]["digest"])) or
            (n >= 4 and not s["cpu_copy"]) or (s["backing"] and (n < 4 or len(pixels) < 2)) or
            (n == 5 and (not s["backing"] or s["elapsed_us"] >= 2000000)) or
            s["complete"] != (n == 5) or (display["complete"] and not s["complete"])):
        raise ValueError("Contradictory PCI decode hold completion")
    return s


def flr_result(rows, *, native, display, handoff_complete=True):
    summaries, pixels, config = (rows[key] for key in FLR_ROWS)
    if len(summaries) != 1:
        raise ValueError("Missing eDP FLR preflight summary")
    s = summaries[0]
    if (s["pixels"] > 3 or s["configs"] > 3 or s["bytes"] not in (0, 6144 * 900) or
            s["released"] > 1 or s["complete"] > 1 or s["reset"] or
            (not native and any(s[k] for k in s if k != "v")) or
            ((pixels or s["released"]) and not s["bytes"]) or
            [p["phase"] for p in pixels] != list(range(s["pixels"])) or
            [(r["phase"], r["index"]) for r in config] !=
            [(i, j) for i in range(s["configs"]) for j in range(16)]):
        raise ValueError("Invalid eDP FLR preflight coverage")
    if any(not p["digest"] or not 0 < p["elapsed_us"] < 2000000 or
           p["digest"] != pixels[0]["digest"] for p in pixels):
        raise ValueError("eDP FLR pixel copy differs")
    for n in range(s["configs"]):
        w = [r["word"] for r in config[16*n:16*(n+1)]]
        if (any(value >= 0xffffffff for value in w) or
                w[:3] != [0x16268086, 0x03000009, 0x011b106b] or
                w[3] & 0x100007 != 0x100007 or
                w[9] & 255 != 0x90 or w[10] & 65535 != 0xd005 or
                w[11] & 0x7ffff != 0x2a401 or w[12] & 3 or
                w[13] != 0x03060013 or w[14] & ~0x100 or
                (n and w != [r["word"] for r in config[:16]])):
            raise ValueError("eDP FLR config differs")
    complete = (display["complete"] and s["pixels"] == s["configs"] == 3 and
                bool(s["released"]) and handoff_complete)
    if complete != s["complete"] or (s["released"] and not display["retired"]):
        raise ValueError("Contradictory eDP FLR preflight completion")
    return s


def pwm_result(rows, *, native, display):
    if len(rows["intel-edp-pwm"]) != 1:
        raise ValueError("Missing eDP PWM result")
    pwm = rows["intel-edp-pwm"][0]
    registers = ("before_ctl", "before_data", "after_ctl", "after_data")
    done = bool(pwm["complete"])
    if (pwm["complete"] > 1 or pwm["retained"] != bool(pwm["writes"] and not done) or
            pwm["writes"] > 5 or any(pwm[key] >= 1 << 32 for key in registers) or
            (not native and any(value for key, value in pwm.items() if key != "v")) or
            (pwm["writes"] and display["stage"] < 6) or
            (display["complete"] and not done) or (pwm["retained"] and not display["retained"])):
        raise ValueError("Invalid eDP PWM result")
    if done:
        before = (pwm["before_ctl"], pwm["before_data"])
        platform = rows["intel-edp-platform-word"]
        if (pwm["writes"] != 5 or not 0 < pwm["elapsed_us"] < 25000 or
                before[0] != 0xc0000000 or before[1] >> 16 != 0x0ad9 or
                before[1] & 0xffff > before[1] >> 16 or not platform or
                before != (platform[0]["pch_pwm"], platform[0]["pch_duty"]) or
                before != (pwm["after_ctl"], pwm["after_data"])):
            raise ValueError("PWM did not restore the original state")
    return pwm


def pps_result(rows, *, native, display, panel_off):
    summaries, samples = (rows[key] for key in PPS_ROWS)
    if len(summaries) != 1:
        raise ValueError("Missing eDP panel timing summary")
    s = summaries[0]
    writes = s["writes"]
    if (s["samples"] > 3 or [w["phase"] for w in samples] != list(range(s["samples"])) or
            writes > 5 or (writes and not samples) or (len(samples) >= 2 and writes < 2) or
            (len(samples) == 3 and writes != 5) or
            any(s[k] > 1 for k in ("complete", "retained")) or
            s["retained"] != bool(writes and not s["complete"]) or
            (s["retained"] and not display["retained"]) or
            (display["complete"] and not s["complete"]) or
            (any(s[key] for key in ("samples", "writes", "wait_us", "program_us", "initial_status", "idle_status"))
             and (not native or display["stage"] < 6)) or
            any(w[k] >= 1 << 32 for w in samples for k in ("on", "off", "div")) or
            any(s[k] >= 1 << 32 for k in ("initial_status", "idle_status"))):
        raise ValueError("Invalid eDP panel timing coverage")
    if s["complete"]:
        platform = rows["intel-edp-platform-word"]
        if not platform:
            raise ValueError("Missing inherited panel timings")
        original = tuple(platform[0][k] for k in ("pps_on", "pps_off", "pps_div"))
        observed = [tuple(w[k] for k in ("on", "off", "div")) for w in samples]
        if (original != (0x007d0834, 0x02e409c4, 0x0004af06) or
                observed != [original, (0, 0, original[2]), original] or writes != 5 or
                s["wait_us"] >= 525000 or not 0 < s["program_us"] < 25000 or
                s["initial_status"] & 0xf0000000 or s["idle_status"] & 0xf8000000 or
                not panel_off or panel_off["outcome"] != 3 or
                (s["initial_status"] & 0x08000000 and
                 (s["wait_us"] < 1000 or not panel_off["last"] & 0x08000000))):
            raise ValueError("Panel timings did not restore after T12")
    return s


def result(text, *, native=True, cycle=False, flr=False, dark_well=False, pci_hold=False, dark_pwm=False, dark_pps=False):
    flr = flr or dark_well or pci_hold or dark_pwm or dark_pps
    cycle = cycle or flr
    rows = rows_of(text)
    summaries = rows[SUMMARY]
    windows = rows["intel-edp-window"]
    poll = rows["intel-edp-poll"]
    crc = rows["intel-edp-crc"]
    phy = rows["intel-edp-phy"]
    pattern = rows["intel-edp-pattern"]
    samples = list(range(3 if pattern else 2))
    if len(summaries) != 1 or [r["sample"] for r in windows] != samples or len(pattern) > 1:
        raise ValueError("Missing eDP summary/windows")
    if ((pattern or poll) and [r["sample"] for r in poll] != samples) or any(r["iir"] >= 1 << 32 for r in poll):
        raise ValueError("Invalid eDP IRQ polling rows")
    if [(r["sample"], r["event"]) for r in crc] != [(s, n) for s in samples for n in range(4)]:
        raise ValueError("Missing eDP CRC rows")
    if [(r["sample"], r["entry"]) for r in phy] != [(s, n) for s in samples for n in range(9)]:
        raise ValueError("Missing eDP PHY rows")
    summary = summaries[0]
    platform = rows["intel-edp-platform"]
    well_cycle = platform_result(rows, native=native)
    preflight = cycle_preflight(rows, native=native)
    if (summary["pte_writes"] or summary["reset"] or
            any(summary[key] > 1 for key in ("complete", "same", "changed", "retained")) or
            any(r["status"] > 2 or r["reason"] > (11 if preflight else 10 if platform else 9) or r["events"] > 4 or r["writes"] > 10 or
                r["restored"] > 1 or r["retained"] > 1 for r in windows) or
            any(r[key] >= 1 << 32 for r in crc for key in ("frame", "value"))):
        raise ValueError("Impossible eDP outcome")
    for sample, window in enumerate(windows):
        if window["status"] != 1:
            continue
        c = crc[sample * 4:sample * 4 + 4]
        if (window["reason"] or window["events"] != 4 or window["restored"] != 1 or window["retained"] or
                window["writes"] < 8 or not window["hash"] or window["port"] != 0x20000000 or
                not 0 < c[2]["value"] < 0xffffffff or c[2]["value"] != c[3]["value"] or
                not all(1 <= ((b["frame"]-a["frame"]) & 0xffffffff) <= 2 for a, b in zip(c, c[1:]))):
            raise ValueError("Invalid completed eDP window")
        if poll and poll[sample]["iir"] & 0xb0000000 != 0x10000000:
            raise ValueError("Completed eDP capture lacks clean CRC-done")
        for row in phy[sample * 9:sample * 9 + 9]:
            if (not row["low"] & 0xffffff or row["low"] & ~0x80ffffff or
                    not row["high"] & 31 or row["high"] & ~0x001f001f):
                raise ValueError("Invalid captured eDP PHY value")

    route = ("hash", "port", "buf", "tp", "pll", "cdclk")
    phy_words = [(r["low"], r["high"]) for r in phy]
    same = all(r["status"] == 1 and all(r[k] == windows[0][k] for k in route) and
               phy_words[s*9:s*9+9] == phy_words[:9] for s, r in enumerate(windows))
    if pattern:
        same = same and all(0 < ((crc[s*4]["frame"] - crc[s*4-1]["frame"]) & 0xffffffff) < 1 << 31
                            for s in (1, 2))
    changed = same and crc[2]["value"] != crc[6]["value"]
    repeat = bool(pattern and same and crc[2]["value"] == crc[10]["value"])
    if pattern and pattern[0]["repeat"] != repeat:
        raise ValueError("eDP repeat verdict contradicts the CRC")
    complete = changed and (not pattern or repeat)
    if preflight is not None:
        complete = complete and bool(preflight["complete"]) and preflight["hash"] == windows[-1]["hash"]
    retained = any(r["retained"] for r in windows)
    if platform and ((complete and (not well_cycle or platform[0]["reason"])) or
                     (platform[0]["retained"] and not windows[-1]["retained"])):
        raise ValueError("eDP platform result contradicts the CRC")
    if (summary["same"], summary["changed"], summary["complete"], summary["retained"]) != (same, changed, complete, retained):
        raise ValueError("eDP summary contradicts the raw rows")
    if cycle:
        observed = cycle_result(rows, native=native, baseline_complete=complete, preflight=preflight)
        panel_off = panel_off_result(rows, observed, native=native)
        retained = retained or bool(observed["retained"])
        complete = bool(observed["complete"])
    elif rows["intel-edp-cycle"] or rows["intel-edp-panel-off"]:
        raise ValueError("Unexpected display-cycle evidence for baseline")
    if dark_well:
        well = dark_well_result(rows, native=native, display=observed)
    else:
        for key in WELL_ROWS:
            if rows.pop(key):
                raise ValueError("Unexpected dark well evidence")
    if pci_hold:
        hold = pci_hold_result(rows, native=native, display=observed)
        reopen_failure = pci_reopen_result(rows, hold)
        dark_rc6 = dark_rc6_result(rows["intel-edp-rc6"], hold)
        candidate = candidate_result(rows["intel-edp-candidate"], rows["intel-edp-flr-config"], hold,
                                     observed, native=native)
        census = dark_census_result(rows["intel-edp-dark-census"], rows["intel-edp-candidate"], hold,
                                    rows["intel-edp-flr-pixels"], native=native)
        if census is None:
            del rows["intel-edp-dark-census"]
        if dark_rc6 is None:
            del rows["intel-edp-rc6"]
        if candidate is None:
            del rows["intel-edp-candidate"]
    else:
        for key in (*HOLD_ROWS, "intel-edp-reopen", "intel-edp-rc6", "intel-edp-candidate", "intel-edp-dark-census"):
            if rows.pop(key):
                raise ValueError("Unexpected PCI decode hold evidence")
    if dark_pwm:
        pwm = pwm_result(rows, native=native, display=observed)
    elif rows.pop("intel-edp-pwm"):
        raise ValueError("Unexpected eDP PWM evidence")
    if dark_pps:
        pps = pps_result(rows, native=native, display=observed, panel_off=panel_off)
    else:
        for key in PPS_ROWS:
            if rows.pop(key):
                raise ValueError("Unexpected eDP panel timing evidence")
    if flr:
        handoff = (not dark_well or bool(well["complete"])) and (not pci_hold or bool(hold["complete"]))
        handoff = handoff and (not pci_hold or candidate is None or candidate["preserved"])
        handoff = handoff and (not dark_pwm or bool(pwm["complete"])) and (not dark_pps or bool(pps["complete"]))
        flr_preflight = flr_result(rows, native=native, display=observed,
                                   handoff_complete=handoff)
        complete = bool(flr_preflight["complete"])
    else:
        for key in FLR_ROWS:
            if rows.pop(key):
                raise ValueError("Unexpected eDP FLR preflight evidence")
    if ("laptop-capture: completed=yes" in text.splitlines()) != complete:
        raise ValueError("Incomplete eDP terminal record")
    if not native and any(r["writes"] or r["restored"] for r in windows):
        raise ValueError("VM eDP access reached CRC writes")
    if complete:
        backing = backing_result(text)
        if not native or not backing or not backing["observation_complete"]:
            raise ValueError("Missing native framebuffer observation")
        b = backing["summary"]
        if (b["count"], b["offset"], b["tiled"], b["fence_matches"]) != (1356, 0, 1, 1):
            raise ValueError("Unexpected eDP framebuffer geometry")
    if not cycle:
        del rows["intel-edp-cycle"]  # Keep the original parser receipt shape.
    report = dict(version=1, complete=complete, native_complete=False, pattern_repeated=repeat,
                  well_cycle=well_cycle, cycle_preflight=preflight, diagnostic_complete=complete,
                  retained=retained, rows=rows)
    if cycle:
        report["display_cycle"] = observed
    if cycle and panel_off is not None:
        report["panel_off"] = panel_off
    else:
        del rows["intel-edp-panel-off"]
    if flr:
        report["flr_preflight"] = flr_preflight
    if dark_well:
        report["dark_well"] = well
    if dark_pwm:
        report["pwm"] = pwm
    if dark_pps:
        report["pps"] = pps
    if pci_hold:
        report["pci_hold"] = hold
        if dark_rc6 is not None:
            report["dark_rc6"] = dark_rc6
        if candidate is not None:
            report["candidate"] = candidate
        if census is not None:
            report["dark_census"] = census
        if hold["v"] >= 2:
            report["pci_reopen_failure"] = reopen_failure
        else:
            del rows["intel-edp-reopen"]
    return report


def checkpoint(pre, text, native=False, *, cycle=False, flr=False, dark_well=False, pci_hold=False, dark_pwm=False,
               dark_pps=False):
    boot = "laptop-capture: boot options="
    a = [line[len(boot):] for line in pre.splitlines() if line.startswith(boot)]
    b = [line[len(boot):] for line in text.splitlines() if line.startswith(boot)]
    if len(a) != 1 or a != b or any(rows_of(pre).values()):
        raise ValueError("Invalid eDP baseline preflight checkpoint")
    mode = dict(cycle=cycle, flr=flr, dark_well=dark_well, pci_hold=pci_hold, dark_pwm=dark_pwm, dark_pps=dark_pps)
    recipe(a[0], **mode)
    prefixes = ("intel-display-", "intel-backing-", "intel-cache-")
    original = [line for line in pre.splitlines() if line.startswith(prefixes)]
    terminal = [line for line in text.splitlines() if line.startswith(prefixes)]
    if not original or original != terminal:
        raise ValueError("Changed eDP baseline preflight evidence")
    result(text, native=native, **mode)
    return True
