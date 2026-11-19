from pathlib import Path
import importlib
import json
import os
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from hardware_boot import limine_config
from hardware_boot_smoke import intel_probe_cpu_args, intel_probe_profile
from intel_edp_baseline import PREFIX, checkpoint, recipe, result

ROOT = Path(__file__).resolve().parent.parent
RELEASE = "1" * 16
VOLUME = "01234567"
BOOT = "laptop-capture: boot options="


def cmdline():
    return next(line.partition("cmdline:")[2].strip() for line in
                limine_config(RELEASE, PREFIX + "laptop-probe-" + VOLUME + "-auto-return").splitlines()
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
    def test_stage_binds_exact_source_and_separate_image(self):
        boot = importlib.import_module("hardware_boot")
        with tempfile.TemporaryDirectory(prefix="edp-stage-") as tmp:
            root = Path(tmp)
            stage = root / "stage"
            for name in ("limine-binary/BOOTX64.EFI", "intel-edp-baseline/srvros.elf", "initramfs.tar"):
                path = root / "build" / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(name)
            with patch.object(boot, "ROOT", root), patch.object(boot, "STAGE", stage), \
                    patch.object(boot.subprocess, "check_output", side_effect=["a" * 40, ""]):
                release = boot.stage(intel_edp_baseline=True, laptop_probe_volume=VOLUME)
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
                ("restore", ["intel_bdw_edp_baseline", "intel_bdw_edp", "intel_bdw_edp_source"]),
            ]:
                with self.subTest(fixture=fixture):
                    binary = str(Path(tmp) / fixture)
                    test_source = "tools/test_intel_bdw_edp" + ("_baseline" if fixture == "baseline" else "") + ".c"
                    subprocess.run([os.environ.get("HOST_CC", "cc"), "-std=c11", "-O1", "-g", "-UNDEBUG",
                                    "-Wall", "-Wextra", "-Werror", "-I", "kernel/include",
                                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-no-pie",
                                    test_source, *(f"kernel/diagnostics/{s}.c" for s in sources),
                                    "-o", binary], cwd=ROOT, check=True, timeout=60)
                    subprocess.run([binary], cwd=ROOT, env={**os.environ, "ASAN_OPTIONS": "halt_on_error=1"},
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
