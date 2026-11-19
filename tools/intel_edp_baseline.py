"""Strict receipts for the inherited Air7,2 eDP baseline trial."""
import re

from intel_display_backing import backing_result

PROFILE = "intel-edp-baseline-v1"
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
}


def recipe(command):
    match = re.fullmatch(
        r"srvros.storage=ram srvros.workspace-volume=([0-9a-f]{8}) "
        r"srvros.desktop-release=([0-9a-f]{16}) srvros.capture=intel-display "
        r"srvros.intel-backing=read-only srvros.intel-cache=read-only "
        r"srvros.intel-edp-baseline=on srvros.capture-failure=reboot", command)
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
        if row.keys() != set(SCHEMAS[name].split()) or row["v"] != 1 or any(v >= 1 << 64 for v in row.values()):
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


def result(text, *, native=True):
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
    if (summary["pte_writes"] or summary["reset"] or
            any(summary[key] > 1 for key in ("complete", "same", "changed", "retained")) or
            any(r["status"] > 2 or r["reason"] > (10 if platform else 9) or r["events"] > 4 or r["writes"] > 10 or
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
    retained = any(r["retained"] for r in windows)
    if platform and ((complete and (not well_cycle or platform[0]["reason"])) or
                     (platform[0]["retained"] and not windows[-1]["retained"])):
        raise ValueError("eDP platform result contradicts the CRC")
    if (summary["same"], summary["changed"], summary["complete"], summary["retained"]) != (same, changed, complete, retained):
        raise ValueError("eDP summary contradicts the raw rows")
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
    return dict(version=1, complete=complete, native_complete=False, pattern_repeated=repeat,
                well_cycle=well_cycle, diagnostic_complete=complete, retained=retained, rows=rows)


def checkpoint(pre, text, native=False):
    boot = "laptop-capture: boot options="
    a = [line[len(boot):] for line in pre.splitlines() if line.startswith(boot)]
    b = [line[len(boot):] for line in text.splitlines() if line.startswith(boot)]
    if len(a) != 1 or a != b or any(rows_of(pre).values()):
        raise ValueError("Invalid eDP baseline preflight checkpoint")
    recipe(a[0])
    prefixes = ("intel-display-", "intel-backing-", "intel-cache-")
    original = [line for line in pre.splitlines() if line.startswith(prefixes)]
    terminal = [line for line in text.splitlines() if line.startswith(prefixes)]
    if not original or original != terminal:
        raise ValueError("Changed eDP baseline preflight evidence")
    result(text, native=native)
    return True
