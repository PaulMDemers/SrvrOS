from pathlib import Path
import os
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from hardware_boot import limine_config
from intel_edp_baseline import PREFIX, checkpoint, recipe, result

ROOT = Path(__file__).resolve().parent.parent
RELEASE = "1" * 16
VOLUME = "01234567"
BOOT = "laptop-capture: boot options="


def cmdline():
    return next(line.partition("cmdline:")[2].strip() for line in
                limine_config(RELEASE, PREFIX + "laptop-probe-" + VOLUME + "-auto-return").splitlines()
                if "cmdline:" in line)


def packet(positive):
    text = [BOOT + cmdline(), "intel-display-observe: preflight"]
    for sample in range(2):
        status, reason, events, writes, restored = (1, 0, 4, 8, 1) if positive else (0, 1-sample, 0, 0, 0)
        text.append(f"intel-edp-window: v=1 sample={sample} status={status} reason={reason} "
                    f"events={events} writes={writes} restored={restored} retained=0 "
                    f"hash={int(positive)} port={0x20000000 * positive} buf=0 tp=0 pll=0 cdclk=0")
        for n in range(4):
            text.append(f"intel-edp-crc: v=1 sample={sample} event={n} "
                        f"frame={n*positive} value={(0x1234 + sample)*positive}")
        for n in range(9):
            text.append(f"intel-edp-phy: v=1 sample={sample} entry={n} low={1*positive} high={2*positive}")
    value = int(positive)
    text.append(f"intel-edp-baseline: v=1 complete={value} same={value} changed={value} retained=0 pte_writes=0 reset=0")
    text.append("laptop-capture: completed=" + ("yes" if positive else "NO"))
    return "\n".join(text) + "\n"


class EdpBaseline(unittest.TestCase):
    def test_exact_recipe_and_vm_refusal(self):
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
            for old, new in [
                ("sample=1 event=3 frame=3 value=4661", "sample=1 event=3 frame=2 value=4661"),
                ("sample=1 entry=5 low=1 high=2", "sample=1 entry=5 low=1 high=3"),
                ("complete=1 same=1", "complete=0 same=1"),
                ("pte_writes=0", "pte_writes=1"),
            ]:
                with self.subTest(old=old), self.assertRaises(ValueError):
                    result(text.replace(old, new))
        with patch("intel_edp_baseline.backing_result", return_value=None), self.assertRaises(ValueError):
            result(text)

    def test_fatal_sanitizers(self):
        with tempfile.TemporaryDirectory(prefix="edp-baseline-") as tmp:
            binary = str(Path(tmp) / "test")
            subprocess.run([os.environ.get("HOST_CC", "cc"), "-std=c11", "-O1", "-g", "-UNDEBUG",
                            "-Wall", "-Wextra", "-Werror", "-I", "kernel/include",
                            "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-no-pie",
                            "tools/test_intel_bdw_edp_baseline.c", "kernel/diagnostics/intel_bdw_edp_baseline.c",
                            "-o", binary], cwd=ROOT, check=True, timeout=60)
            subprocess.run([binary], cwd=ROOT, env={**os.environ, "ASAN_OPTIONS": "halt_on_error=1"},
                           check=True, timeout=20)
