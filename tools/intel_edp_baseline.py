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
    if (summary["pte_writes"] or summary["reset"] or
            any(summary[key] > 1 for key in ("complete", "same", "changed", "retained")) or
            any(r["status"] > 2 or r["reason"] > 9 or r["events"] > 4 or r["writes"] > 10 or
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
                diagnostic_complete=complete, retained=retained, rows=rows)


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
