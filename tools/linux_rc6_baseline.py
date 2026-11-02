#!/usr/bin/env python3
"""Observe Linux i915 RC6 sysfs without changing graphics or power policy.

This host-only comparison never accepts a SrvrOS platform receipt. Kernel sysfs
rounds cumulative residency to milliseconds and its read can acquire runtime PM.
Linux CLOCK_MONOTONIC_RAW is not an independent ACPI PM timer calibration.
"""
import argparse
import json
import platform
import time
from pathlib import Path


def interval(a, b, clock="raw_ns"):
    low = b[clock][0] - a[clock][1]
    high = b[clock][1] - a[clock][0]
    delta = b["rc6_ms"] - a["rc6_ms"]
    if low <= 0 or high < low or delta < 0:
        raise ValueError("clock overlap/regression or residency counter reset")
    nominal = delta * 1_000_000
    return dict(elapsed_ns_bounds=[low, high], rc6_delta_ms=delta,
                nominal_excess_ns=max(0, nominal-high),
                # Conservative two-endpoint millisecond quantization envelope.
                # Diagnostic annotation only, not a new SrvrOS tolerance.
                excess_beyond_2ms_ns=max(0, nominal-high-2_000_000),
                nominal_fraction_bounds=[nominal/high, nominal/low])


def summarize(rows, steps):
    if steps < 1 or len(rows) < 2 or (len(rows)-1) % steps:
        raise ValueError("incomplete windows")
    for i, row in enumerate(rows):
        if row["seq"] != i or row["rc6_ms"] < 0:
            raise ValueError("invalid sequence/counter")
        for clock in ("raw_ns", "mono_ns"):
            lo, hi = row[clock]
            if not 0 < lo <= hi:
                raise ValueError("invalid read bracket")
    gaps = [dict(start=a["seq"], end=b["seq"],
                 raw=interval(a, b), mono=interval(a, b, "mono_ns"))
            for a, b in zip(rows, rows[1:])]
    windows = [dict(start=i, end=i+steps,
                    raw=interval(rows[i], rows[i+steps]),
                    mono=interval(rows[i], rows[i+steps], "mono_ns"))
               for i in range(0, len(rows)-1, steps)]
    return dict(status="OBSERVED", acceptance=False, intervals=gaps,
                windows=windows, total_raw=interval(rows[0], rows[-1]),
                scope="Linux rounded sysfs residency versus OS clocks; no independent PM calibration, GPU activity control or SrvrOS acceptance")


def sample(path, seq):
    # Both clock pairs enclose the actual sysfs read, not sleep or presentation.
    m0 = time.monotonic_ns()
    r0 = time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)
    value = int(path.read_text().strip())
    r1 = time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)
    m1 = time.monotonic_ns()
    return dict(seq=seq, rc6_ms=value, raw_ns=[r0, r1], mono_ns=[m0, m1])


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--card", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--steps", type=int, default=9)
    ap.add_argument("--interval-seconds", type=float, default=10)
    ap.add_argument("--windows", type=int, default=3)
    args = ap.parse_args()
    if not 1 <= args.steps <= 60 or not 1 <= args.windows <= 10 or not 1 <= args.interval_seconds <= 60:
        ap.error("bounded capture requires 1..60 steps, 1..10 windows, 1..60 seconds")
    if args.output.exists():
        ap.error("output already exists; preserve the previous capture")
    args.output.mkdir(parents=True)
    card = args.card.resolve(strict=True)
    device = card / "device"
    driver = (device / "driver").resolve(strict=True).name
    if driver != "i915":
        ap.error("only the i915 RC6 sysfs interface is supported")
    gt = card / "gt/gt0"
    path = gt / "rc6_residency_ms"
    enabled = (gt / "rc6_enable").read_text().strip()
    meta = dict(model=Path("/sys/class/dmi/id/product_name").read_text().strip(),
                kernel=platform.release(), card=str(card), driver=driver,
                pci_vendor=(device / "vendor").read_text().strip(),
                pci_device=(device / "device").read_text().strip(),
                clocksource=Path("/sys/devices/system/clocksource/clocksource0/current_clocksource").read_text().strip(),
                rc6_enable_before=enabled,
                runtime_before=(device / "power/runtime_status").read_text().strip(),
                interval_seconds=args.interval_seconds, steps=args.steps,
                windows=args.windows, utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                interface=str(path), policy_writes=0)
    (args.output / "metadata.json").write_text(json.dumps(meta, indent=2)+"\n")
    rows = []
    target = time.monotonic()
    with (args.output / "samples.jsonl").open("x") as out:
        for seq in range(args.steps*args.windows+1):
            delay = target-time.monotonic()
            if delay > 0:
                time.sleep(delay)
            row = sample(path, seq)
            rows.append(row)
            out.write(json.dumps(row)+"\n")
            out.flush()
            print(f"RC6 baseline {seq}/{args.steps*args.windows}: {row['rc6_ms']} ms", flush=True)
            target += args.interval_seconds
    meta.update(rc6_enable_after=(gt / "rc6_enable").read_text().strip(),
                runtime_after=(device / "power/runtime_status").read_text().strip())
    (args.output / "metadata.json").write_text(json.dumps(meta, indent=2)+"\n")
    if meta["rc6_enable_before"] != meta["rc6_enable_after"]:
        raise ValueError("RC6 enable state changed during capture")
    result = summarize(rows, args.steps)
    (args.output / "result.json").write_text(json.dumps(result, indent=2)+"\n")
    print("Complete: observations saved; no SrvrOS acceptance inferred.", flush=True)


if __name__ == "__main__":
    main()
