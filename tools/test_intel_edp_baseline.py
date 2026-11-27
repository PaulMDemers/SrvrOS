from pathlib import Path
import importlib
import json
import os
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from hardware_boot import EDP_CONFIGS, limine_config
from hardware_boot_smoke import intel_probe_cpu_args, intel_probe_profile
from intel_edp_baseline import PREFIX, candidate_result, checkpoint, dark_rc6_result, pci_reopen_result, recipe, result, rows_of

ROOT = Path(__file__).resolve().parent.parent
RELEASE = "1" * 16
VOLUME = "01234567"
BOOT = "laptop-capture: boot options="


def cmdline(cycle=False, flr=False, dark_well=False, pci_hold=False, dark_pwm=False, dark_pps=False):
    profile = 6 if dark_pps else 5 if dark_pwm else 4 if pci_hold else 3 if dark_well else 2 if flr else cycle
    return next(line.partition("cmdline:")[2].strip() for line in
                limine_config(RELEASE, EDP_CONFIGS[profile] + "laptop-probe-" +
                              VOLUME + "-auto-return").splitlines()
                if "cmdline:" in line)


def packet(positive, patterned=False):
    text = [BOOT + cmdline(), "intel-display-observe: preflight"]
    for sample in range(3 if patterned else 2):
        status, reason, events, writes, restored = (1, 0, 4, 8, 1) if positive else (0, int(sample == 0), 0, 0, 0)
        text.append(f"intel-edp-window: v=1 sample={sample} status={status} reason={reason} "
                    f"events={events} writes={writes} restored={restored} retained=0 "
                    f"hash={int(positive)} port={0x20000000 * positive} buf=0 tp=0 pll=0 cdclk=0")
        text.append(f"intel-edp-poll: v=1 sample={sample} iir={0x10000000 * positive}")
        for n in range(4):
            text.append(f"intel-edp-crc: v=1 sample={sample} event={n} "
                        f"frame={(n + sample*10 if patterned else n)*positive} "
                        f"value={(0x1234 + sample%2)*positive}")
        for n in range(9):
            text.append(f"intel-edp-phy: v=1 sample={sample} entry={n} low={1*positive} high={2*positive}")
    value = int(positive)
    if patterned:
        text.append(f"intel-edp-pattern: v=1 repeat={value}")
    text.append(f"intel-edp-baseline: v=1 complete={value} same={value} changed={value} retained=0 pte_writes=0 reset=0")
    text.append("laptop-capture: completed=" + ("yes" if positive else "NO"))
    return "\n".join(text) + "\n"


class EdpBaseline(unittest.TestCase):
    def test_dark_candidate_coverage_and_recovery(self):
        hold = dict(v=5, samples=5, complete=1)
        display = dict(stage=8, complete=1)
        config = [dict(phase=0, index=3, word=0x100007)]
        common = dict(v=1, slot=12292, complete=1, reads=3, retired=1, af=0,
                      left=0x8f004001, target=0x8f005001, right=0x8f006001)
        points = [dict(common, phase=i, begin_us=100 + 100*i, end_us=150 + 100*i,
                       command=0x100402 if i == 1 else 0x100007) for i in range(3)]
        self.assertTrue(candidate_result(points, config, hold, display, native=True)["preserved"])
        changed = [points[0], dict(points[1], target=0), points[2]]
        self.assertFalse(candidate_result(changed, config, hold, display, native=True)["preserved"])
        for bad in (points[:2], [points[0], dict(points[1], command=0x100007), points[2]],
                    [points[0], dict(points[1], slot=12291), points[2]],
                    [points[0], dict(points[1], retired=0), points[2]],
                    [points[0], dict(points[1], af=0x100), points[2]],
                    [points[0], dict(points[1], begin_us=50), points[2]]):
            with self.assertRaises(ValueError):
                candidate_result(bad, config, hold, display, native=True)
        with self.assertRaises(ValueError):
            candidate_result(points, config, hold, display, native=False)
        refused = dict(display, stage=0, complete=0)
        self.assertIsNone(candidate_result([], [], dict(hold, v=4), refused, native=False))
        self.assertFalse(candidate_result([], [], dict(hold, samples=0, complete=0), refused, native=False)["preserved"])

    def test_dark_rc6_receipt(self):
        hold = dict(v=4, samples=5, writes=4, elapsed_us=204000, settle_us=250)
        first = dict(seq=0, begin_us=260, end_us=270, core=0, residency=0xffffffff, wake_ack=0)
        second = dict(first, seq=1, begin_us=2270, end_us=2280, residency=2)
        pair = [first, second]
        self.assertEqual(dark_rc6_result(pair, hold)["counter_delta_ticks_mod32"], 3)
        for points, summary in ((pair, dict(hold, v=3)), (pair[:1], hold),
                                (pair, dict(hold, samples=1)), (pair[::-1], hold),
                                ([first, dict(second, begin_us=2269)], hold),
                                ([first, dict(second, core=1 << 32)], hold)):
            with self.assertRaises(ValueError):
                dark_rc6_result(points, summary)
        partial = dark_rc6_result(pair[:1], dict(hold, samples=2, writes=1))
        self.assertEqual(partial, dict(samples=[first]))

    def test_pci_reopen_receipt_tracks_first_failure(self):
        def row(seq, check, at, ok, read=0, got=0, want=0):
            return (f"intel-edp-reopen: v=1 seq={seq} check={check} at={at} "
                    f"ok={ok} read={read} got={got} want={want}\n")

        owner = row(0, 1, 0, 1)
        mismatch = row(1, 2, 0x45400, 0, 1, 0xffffffff, 0xc0000000)
        summary = dict(v=2, samples=4, backing=0)
        failure = pci_reopen_result(rows_of(owner + mismatch), summary)
        self.assertEqual((failure["name"], failure["at"], failure["got"]),
                         ("platform register", 0x45400, 0xffffffff))
        self.assertIsNone(pci_reopen_result(rows_of(""), dict(v=2, samples=3, backing=0)))
        for bad in ("", owner, owner + mismatch + row(2, 2, 0x45408, 1),
                    owner + mismatch.replace("seq=1", "seq=8")):
            with self.assertRaises(ValueError):
                pci_reopen_result(rows_of(bad), summary)

        ordered = [(1, 0)]
        ordered.extend((2, reg) for reg in (0x45400, 0x45408, 0x4540c, 0x130040, 0x46200, 0xc2020,
                                           0xc2004, 0xc7208, 0xc720c, 0xc7210, 0x45404, 0x48250, 0xc8250, 0xc8254))
        ordered.extend((k, 0) for k in range(3, 9))
        fences = [(9, 0x100000), (10, 0), (11, 0x100000)]
        ordered.extend(fences)
        for i, page in enumerate((0x800000, 0x801000, 0x802000)):
            ordered.extend(((12, page), (13, i * 512), (14, page)))
        ordered.extend((*fences, (6, 1), (7, 1), (8, 1), (15, 0), (16, 0)))
        prefix = "".join(row(i, check, at, 1) for i, (check, at) in enumerate(ordered))
        failed_pixel = prefix + row(len(ordered), 19, 1382399, 0)
        self.assertEqual(pci_reopen_result(rows_of(failed_pixel), summary)["name"], "framebuffer contents")
        for bad in (failed_pixel.replace("at=1382399", "at=1382400"),
                    failed_pixel.replace("check=19", "check=18")):
            with self.assertRaises(ValueError):
                pci_reopen_result(rows_of(bad), summary)
        suffix = ((17, 1350), (18, 1350), (19, 1382400), (20, 42))
        complete = prefix + "".join(row(i, check, 0, 1, 1, value, value)
                                    for i, (check, value) in enumerate(suffix, len(ordered)))
        self.assertIsNone(pci_reopen_result(rows_of(complete), dict(v=2, samples=5, backing=1)))

        # New source requires both well transitions before it can read the frame.
        v3 = dict(v=3, samples=5, backing=1)
        lines = complete.splitlines(keepends=True)
        start = next(i for i, line in enumerate(lines) if " check=3 " in line)
        transitions = [row(start, 21, 0, 1, 1, 0x40000000, 0x40000000),
                       row(start + 1, 22, 4, 1, 1, 0xc0000000, 0xc0000000)]
        tail = [line.replace(f"seq={i} ", f"seq={i+2} ") for i, line in enumerate(lines[start:], start)]
        prefix = "".join(lines[:start])
        joined = prefix + "".join(transitions + tail)
        self.assertIsNone(pci_reopen_result(rows_of(joined), v3))
        for invalid in (complete, joined.replace("check=22 at=4", "check=22 at=2"),
                        joined.replace("got=3221225472", "got=1073741824")):
            with self.assertRaises(ValueError):
                pci_reopen_result(rows_of(invalid), v3)
        refused = prefix + transitions[0] + row(start+1, 22, 2, 0, 1, 0xc0000000, 0xc0000000)
        self.assertEqual(pci_reopen_result(rows_of(refused), dict(v=3, samples=4, backing=0))["at"], 2)

    def test_stage_binds_exact_source_and_separate_image(self):
        boot = importlib.import_module("hardware_boot")
        with tempfile.TemporaryDirectory(prefix="edp-stage-") as tmp:
            root = Path(tmp)
            stage = root / "stage"
            for name in ("limine-binary/BOOTX64.EFI", "intel-edp-baseline/srvros.elf", "initramfs.tar"):
                path = root / "build" / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(name)
            for mode in ("intel_edp_baseline", "intel_edp_cycle", "intel_edp_flr_preflight",
                         "intel_edp_dark_well", "intel_edp_pci_hold", "intel_edp_dark_pwm", "intel_edp_dark_pps"):
                with patch.object(boot, "ROOT", root), patch.object(boot, "STAGE", stage), \
                        patch.object(boot.subprocess, "check_output", side_effect=["a" * 40, ""]):
                    release = boot.stage(laptop_probe_volume=VOLUME, **{mode: True})
                payload = stage / release
                self.assertTrue(boot.verify(payload)["source_bound"])
                self.assertEqual((payload / "srvros.elf").read_text(), "intel-edp-baseline/srvros.elf")
                path = payload / "manifest.json"
                manifest = json.loads(path.read_text())
                del manifest["source_bound"]
                path.write_text(json.dumps(manifest))
                with self.assertRaises(ValueError):
                    boot.verify(payload)

    def test_exact_recipe_and_vm_refusal(self):
        version = PREFIX + "laptop-probe-" + VOLUME + "-auto-return"
        self.assertTrue(intel_probe_profile(version))
        self.assertEqual(intel_probe_cpu_args(version), ["-cpu", "Broadwell"])
        cmd = cmdline()
        self.assertEqual(recipe(cmd), dict(release=RELEASE, volume=VOLUME))
        with self.assertRaises(ValueError):
            recipe(cmd + " srvros.audio=hda")
        text = packet(False)
        pre = "\n".join(text.splitlines()[:2]) + "\n"
        self.assertTrue(checkpoint(pre, text))
        self.assertFalse(result(text, native=False)["complete"])
        with self.assertRaises(ValueError):
            result(text.replace("events=0 writes=0", "events=0 writes=1", 1), native=False)
        with self.assertRaises(ValueError):
            checkpoint(pre.replace("preflight", "other"), text)

    def test_complete_native_requires_fresh_stable_and_changed_frames(self):
        text = packet(True)
        b = dict(observation_complete=True, summary=dict(count=1356, offset=0, tiled=1, fence_matches=1))
        with patch("intel_edp_baseline.backing_result", return_value=b):
            self.assertTrue(result(text)["diagnostic_complete"])
            older = "\n".join(line for line in text.splitlines() if not line.startswith("intel-edp-poll:"))
            self.assertTrue(result(older)["diagnostic_complete"])
            for old, new in [
                ("sample=1 event=3 frame=3 value=4661", "sample=1 event=3 frame=2 value=4661"),
                ("sample=1 entry=5 low=1 high=2", "sample=1 entry=5 low=1 high=3"),
                ("complete=1 same=1", "complete=0 same=1"),
                ("pte_writes=0", "pte_writes=1"),
                ("sample=1 iir=268435456", "sample=1 iir=805306368"),
            ]:
                with self.subTest(old=old), self.assertRaises(ValueError):
                    result(text.replace(old, new))
        with patch("intel_edp_baseline.backing_result", return_value=None), self.assertRaises(ValueError):
            result(text)

    def test_fatal_sanitizers(self):
        with tempfile.TemporaryDirectory(prefix="edp-baseline-") as tmp:
            for fixture, sources in [
                ("baseline", ["intel_bdw_edp_baseline", "intel_bdw_edp_platform"]),
                ("restore", ["intel_bdw_edp_baseline", "intel_bdw_edp", "intel_bdw_edp_source", "intel_bdw_edp_platform",
                             "intel_bdw_flr", "intel_vtd_drain", "intel_bdw_pci_hold",
                             "intel_bdw_display_takeover", "intel_bdw_boot_scanout", "intel_bdw_ggtt"]),
            ]:
                with self.subTest(fixture=fixture):
                    binary = str(Path(tmp) / fixture)
                    test_source = "tools/test_intel_bdw_edp" + ("_baseline" if fixture == "baseline" else "") + ".c"
                    paths = [f"kernel/diagnostics/{source}.c" for source in sources]
                    if fixture == "restore":
                        paths.extend(f"kernel/src/{source}.c" for source in ("scanout_space", "scanout_publish", "display_access"))
                    subprocess.run([os.environ.get("HOST_CC", "cc"), "-std=c11", "-O1", "-g", "-UNDEBUG",
                                    "-Wall", "-Wextra", "-Werror", "-I", "kernel/include",
                                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-no-pie",
                                    test_source, *paths,
                                    "-o", binary], cwd=ROOT, check=True, timeout=60)
                    subprocess.run([binary], cwd=ROOT, env={**os.environ, "ASAN_OPTIONS": "halt_on_error=1"},
                                   check=True, timeout=45)
                    if fixture == "restore":
                        for mode in ("takeover-roundtrip", "takeover-range-protected", "takeover-bad-crc",
                                     "takeover-reset-declined", "takeover-traffic-open", "takeover-stale-rollback",
                                     "takeover-drain-io", "takeover-drain-sequence", "takeover-drain-completion",
                                     "takeover-drain-result-owner", "takeover-rmrr-scratch", "takeover-rmrr-private"):
                            subprocess.run([binary, mode], cwd=ROOT, env={**os.environ, "ASAN_OPTIONS": "halt_on_error=1"},
                                           check=True, timeout=45)

    def test_pattern_replay_must_be_fresh_and_match(self):
        text = packet(True, patterned=True)
        b = dict(observation_complete=True, summary=dict(count=1356, offset=0, tiled=1, fence_matches=1))
        with patch("intel_edp_baseline.backing_result", return_value=b):
            self.assertTrue(result(text)["pattern_repeated"])
            for old, new in [
                ("sample=2 event=0 frame=20", "sample=2 event=0 frame=12"),
                ("sample=2 event=2 frame=22 value=4660", "sample=2 event=2 frame=22 value=4662"),
                ("intel-edp-pattern: v=1 repeat=1", "intel-edp-pattern: v=1 repeat=0"),
            ]:
                with self.subTest(old=old), self.assertRaises(ValueError):
                    result(text.replace(old, new))
            wrong = (text.replace("frame=22 value=4660", "frame=22 value=4662")
                     .replace("frame=23 value=4660", "frame=23 value=4662")
                     .replace("repeat=1", "repeat=0")
                     .replace("complete=1 same=1 changed=1", "complete=0 same=1 changed=1")
                     .replace("completed=yes", "completed=NO"))
            self.assertFalse(result(wrong)["diagnostic_complete"])
        self.assertFalse(result(packet(False, patterned=True), native=False)["diagnostic_complete"])

    def test_well_request_must_restore_only_driver_bit(self):
        rows = [
            "intel-edp-platform: v=1 samples=4 writes=2 reason=0 acquired=1 released=1 retained=0 begin=1000 end=103001",
        ]
        for phase in range(4):
            driver = 0xc0000000 if phase in (1, 2) else 0x40000000
            rows.append(f"intel-edp-platform-word: v=1 phase={phase} bios=0xc0000000 driver={driver} "
                        "kvmr=0x40000000 debug=0x40000000 fuse=0 pll=0x4c000000 cdclk=674 gate=4096 "
                        "chicken=0 pps_status=0xc0000008 pps_ctl=7 pps_on=1 pps_off=1 pps_div=1 "
                        "cpu_pwm=0 cpu_duty=0 pch_pwm=0x80000000 pch_duty=0x0ad90184")
        text = packet(True, patterned=True) + "\n".join(rows) + "\n"
        backing = dict(observation_complete=True, summary=dict(count=1356, offset=0, tiled=1, fence_matches=1))
        with patch("intel_edp_baseline.backing_result", return_value=backing):
            self.assertTrue(result(text)["well_cycle"])
            for changed in (
                text.replace("phase=2 bios=0xc0000000", "phase=2 bios=0x40000000"),
                text.replace("phase=2 bios=0xc0000000 driver=3221225472", "phase=2 bios=0xc0000000 driver=1073741824"),
                text.replace("acquired=1 released=1 retained=0", "acquired=1 released=0 retained=1"),
                text.replace("end=103001", "end=601000"),
            ):
                with self.assertRaises(ValueError):
                    result(changed)
        with self.assertRaises(ValueError):
            result(text, native=False)

        refused = ("intel-edp-platform: v=1 samples=0 writes=0 reason=0 acquired=0 "
                   "released=0 retained=0 begin=0 end=0\n")
        self.assertFalse(result(packet(False, patterned=True) + refused, native=False)["well_cycle"])

        preflight = ("intel-edp-cycle-preflight: v=1 complete=1 hash=1 blockers=65 "
                     "imr=0x80000000 ier=0 iir=0 aux=0 fitter=0 audio=0 "
                     "ddi_b=0 ddi_c=0 ddi_d=0 ddi_e=0x80000000 wm1=0 wm2=0 wm3=0\n")
        with patch("intel_edp_baseline.backing_result", return_value=backing):
            data = result(text + preflight)
            self.assertTrue(data["diagnostic_complete"])
            self.assertEqual(data["cycle_preflight"]["blockers"], 65)
            with self.assertRaises(ValueError):
                result(text + preflight.replace("blockers=65", "blockers=0"))
            with self.assertRaises(ValueError):
                result(text + preflight.replace("hash=1", "hash=2"))
            cycle_data = (
                "intel-edp-cycle: v=1 attempted=1 complete=1 stage=8 shutdown_error=0 restore_error=0 "
                "restore_result=0 offset=0xc7204 word=7 panel=6 aux=0 dark_hash=1 crc_checks=2 "
                "crc0=4660 crc1=4660 mask_before=0xffffffff mask_live=0x7fffffff mask_after=0xffffffff "
                "mask_writes=2 mask_restored=1 shutdown_writes=9 restore_writes=52 well_released=1 retired=1 retained=0\n"
            )
            native = (text.replace(cmdline(), cmdline(True)) +
                      preflight.replace("blockers=65", "blockers=1").replace("ddi_e=0x80000000", "ddi_e=128")
                      .replace("imr=0x80000000", "imr=0xffffffff") + cycle_data)
            self.assertTrue(result(native, cycle=True)["diagnostic_complete"])
            self.assertEqual(recipe(cmdline(True), cycle=True), dict(release=RELEASE, volume=VOLUME))
            for wrong in (native.replace("crc1=4660", "crc1=4661"),
                          native.replace("mask_after=0xffffffff", "mask_after=0x7fffffff"),
                          native.replace("mask_live=0x7fffffff", "mask_live=0x7ffffffe"),
                          native.replace("dark_hash=1", "dark_hash=0"),
                          native.replace("retired=1 retained=0", "retired=0 retained=1")):
                with self.assertRaises(ValueError):
                    result(wrong, cycle=True)
            with self.assertRaises(ValueError):
                result(native)
            refused_cycle = native.replace("completed=yes", "completed=NO").replace(
                "attempted=1 complete=1 stage=8", "attempted=1 complete=0 stage=7").replace(
                "well_released=1 retired=1 retained=0", "well_released=0 retired=0 retained=1")
            self.assertTrue(result(refused_cycle, cycle=True)["retained"])
            self.assertFalse(result(refused_cycle, cycle=True)["diagnostic_complete"])
            off = ("intel-edp-panel-off: v=1 outcome=3 samples=3000 first=0xa000000b "
                   "last=0x08000001 elapsed_us=324000 mask_error=0\n")
            self.assertEqual(result(native + off, cycle=True)["panel_off"]["elapsed_us"], 324000)
            for wrong in (off.replace("last=0x08000001", "last=0xa000000b"),
                          off.replace("elapsed_us=324000", "elapsed_us=350000"),
                          off.replace("mask_error=0", "mask_error=7"), off + off):
                with self.assertRaises(ValueError):
                    result(native + wrong, cycle=True)
            timeout = (refused_cycle.replace("stage=7 shutdown_error=0", "stage=5 shutdown_error=4")
                       .replace("shutdown_writes=9", "shutdown_writes=8") +
                       off.replace("outcome=3", "outcome=2").replace("last=0x08000001", "last=0xa000000b")
                       .replace("elapsed_us=324000", "elapsed_us=350152"))
            failed = result(timeout, cycle=True)
            self.assertTrue(failed["retained"])
            self.assertEqual(failed["panel_off"]["last"], 0xa000000b)
            flr = native.replace(cmdline(True), cmdline(cycle=True, flr=True)) + off
            self.assertEqual(recipe(cmdline(cycle=True, flr=True), cycle=True, flr=True),
                             dict(release=RELEASE, volume=VOLUME))
            flr += "intel-edp-flr-preflight: v=1 complete=1 pixels=3 configs=3 bytes=5529600 released=1 reset=0\n"
            config = [0x16268086, 0x03000009, 0x011b106b, 0x100007, 0xc0000004, 0, 0xb000000c,
                      0, 1, 0x90, 0xd005, 0x2a401, 0, 0x03060013, 0, 0x8ad03190]
            for phase in range(3):
                flr += f"intel-edp-flr-pixels: v=1 phase={phase} digest=17 elapsed_us=10000\n"
                for index, word in enumerate(config):
                    flr += f"intel-edp-flr-config: v=1 phase={phase} index={index} word={word}\n"
            self.assertTrue(result(flr, cycle=True, flr=True)["diagnostic_complete"])
            pwm = flr.replace(cmdline(flr=True), cmdline(dark_pwm=True))
            pwm = pwm.replace("pch_pwm=0x80000000", "pch_pwm=0xc0000000")
            pwm += ("intel-edp-pwm: v=1 complete=1 retained=0 writes=5 elapsed_us=120 "
                    "before_ctl=0xc0000000 before_data=0x0ad90184 after_ctl=0xc0000000 after_data=0x0ad90184\n")
            self.assertEqual(recipe(cmdline(dark_pwm=True), dark_pwm=True),
                             dict(release=RELEASE, volume=VOLUME))
            self.assertTrue(result(pwm, dark_pwm=True)["pwm"]["complete"])
            for wrong in (pwm.replace("after_data=0x0ad90184", "after_data=0x0ad90000"),
                          pwm.replace("writes=5 elapsed_us=120", "writes=4 elapsed_us=120"),
                          pwm.replace("elapsed_us=120 before_ctl", "elapsed_us=25000 before_ctl"),
                          pwm.replace("pch_pwm=0xc0000000", "pch_pwm=0x80000000")):
                with self.assertRaises(ValueError):
                    result(wrong, dark_pwm=True)
            pps = flr.replace(cmdline(flr=True), cmdline(dark_pps=True))
            pps = pps.replace("pps_on=1 pps_off=1 pps_div=1",
                              "pps_on=0x007d0834 pps_off=0x02e409c4 pps_div=0x0004af06")
            timing = ("intel-edp-pps: v=1 complete=1 retained=0 samples=3 writes=5 wait_us=301241 "
                      "program_us=126 initial_status=0x08000001 idle_status=0\n")
            for phase in range(3):
                on, off = ("0x007d0834", "0x02e409c4") if phase != 1 else ("0", "0")
                timing += f"intel-edp-pps-word: v=1 phase={phase} on={on} off={off} div=0x0004af06\n"
            self.assertEqual(recipe(cmdline(dark_pps=True), dark_pps=True),
                             dict(release=RELEASE, volume=VOLUME))
            self.assertTrue(result(pps + timing, dark_pps=True)["pps"]["complete"])
            for wrong in (timing.replace("idle_status=0", "idle_status=0x08000001"),
                          timing.replace("wait_us=301241", "wait_us=525000"),
                          timing.replace("program_us=126", "program_us=25000"),
                          timing.replace("phase=1 on=0 off=0", "phase=1 on=1 off=0"),
                          timing.replace("phase=2 on=0x007d0834", "phase=2 on=0x007d0835")):
                with self.assertRaises(ValueError):
                    result(pps + wrong, dark_pps=True)
            with self.assertRaises(ValueError):
                result(pps + timing, flr=True)
            for changed in (flr.replace("phase=1 digest=17", "phase=1 digest=18"),
                            flr.replace(f"phase=1 index=4 word={config[4]}", "phase=1 index=4 word=4"),
                            flr.replace("bytes=5529600 released=1 reset=0", "bytes=5529600 released=1 reset=1")):
                with self.assertRaises(ValueError):
                    result(changed, cycle=True, flr=True)
            well = flr.replace(cmdline(flr=True), cmdline(dark_well=True))
            well += "intel-edp-dark-well: v=1 complete=1 samples=3 writes=2 elapsed_us=100\n"
            for phase, driver in enumerate((0xc0000000, 0x40000000, 0xc0000000)):
                well += f"intel-edp-dark-well-word: v=1 phase={phase} bios=0xc0000000 driver={driver} pps=0x08000001\n"
            self.assertEqual(recipe(cmdline(dark_well=True), dark_well=True),
                             dict(release=RELEASE, volume=VOLUME))
            self.assertTrue(result(well, dark_well=True)["dark_well"]["complete"])
            with self.assertRaises(ValueError):
                result(well, flr=True)
            for wrong in (well.replace("bios=0xc0000000", "bios=0x40000000"),
                          well.replace("pps=0x08000001", "pps=0x80000008"),
                          well.replace("writes=2 elapsed_us=100", "writes=1 elapsed_us=100"),
                          well.replace("elapsed_us=100\n", "elapsed_us=100000\n"),
                          well.replace("samples=3 writes=2", "samples=1000000000000 writes=2")):
                with self.assertRaises(ValueError):
                    result(wrong, dark_well=True)
            hold = flr.replace(cmdline(flr=True), cmdline(pci_hold=True))
            hold += ("intel-edp-pci-hold: v=1 complete=1 samples=5 writes=4 polls=2 "
                     "elapsed_us=200000 settle_us=224 first_clear_us=112 cpu_copy=1 digest=17 backing=1 retained=0\n")
            for phase, command in enumerate((7, 0x403, 0x400, 0x402, 7)):
                hold += f"intel-edp-pci-hold-word: v=1 phase={phase} command={config[3] & 0xffff0000 | command} af=0\n"
            self.assertEqual(recipe(cmdline(pci_hold=True), pci_hold=True), dict(release=RELEASE, volume=VOLUME))
            self.assertTrue(result(hold, pci_hold=True)["pci_hold"]["complete"])
            with self.assertRaises(ValueError):
                result(hold, flr=True)
            for wrong in (hold.replace(f"phase=2 command={config[3] & 0xffff0000 | 0x400}",
                                       f"phase=2 command={config[3] & 0xffff0000 | 0x402}"),
                          hold.replace("settle_us=224", "settle_us=123"),
                          hold.replace("first_clear_us=112", "first_clear_us=210"),
                          hold.replace("cpu_copy=1 digest=17", "cpu_copy=1 digest=18"),
                          hold.replace("backing=1 retained=0", "backing=0 retained=0"),
                          hold.replace("elapsed_us=200000", "elapsed_us=2000000"),
                          hold.replace(" af=0\n", " af=0x100\n")):
                with self.assertRaises(ValueError):
                    result(wrong, pci_hold=True)
