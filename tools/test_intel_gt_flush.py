"""Exercise the Broadwell hardware acknowledgement capture and its receipts."""

import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import hardware_boot as boot
from fixture_compile import compile_fixture, recipe
from hardware_desktop import session_log_limit
from intel_gt_bootstrap import bootstrap_result
from test_intel_gt_primary import run
from test_intel_gt_range import RECIPE, ROOT, TESTS
from workflow.runner import trial_parsers

TARGET = "build/tests/intel-gt-flush-adapter"


def observe(mode, position=0, *, entry=None, binary=ROOT / TARGET):
    return run(mode, position, entry=entry, binary=binary)


class FlushAcknowledgements(unittest.TestCase):
    def test_samples_and_timing_on_both_original_paths(self):
        for entry in (None, "disabled"):
            for mode, expected in (("pass", [0, 31, 0]), ("flush-quiet", [0, 0, 0]), ("flush-stale", [31, 31, 31])):
                with self.subTest(entry=entry, mode=mode):
                    result = bootstrap_result(observe(mode, entry=entry))
                    self.assertEqual(result["version"], 12)
                    self.assertTrue(result["complete"])
                    self.assertFalse(result["hardware_ownership"])
                    self.assertEqual(result["summary"]["obligations"], 255)
                    self.assertEqual(result["summary"]["pte_writes"], 0)
                    flush = result["flush"]
                    self.assertEqual(flush["engine_ack"], expected)
                    self.assertEqual(flush["engine_ack_rise"], 31 if mode == "pass" else 0)
                    self.assertEqual(flush["l3_flush_ack"], list(map(bool, expected)))
                    self.assertEqual(flush["summary"], {
                        "v": 1, "samples": 3, "reads": 18, "maps": 3, "retired": 3, "live": 0,
                    })

    def test_access_failures_retain_the_parent_and_report_the_attempt(self):
        for mode, limit in (("flush-read", 18), ("flush-float", 18), ("flush-clock", 18), ("flush-window", 18),
                            ("flush-map", 3), ("flush-map-retained", 3), ("flush-alias", 3), ("flush-unmap", 3)):
            for pos in range(1, limit + 1):
                with self.subTest(mode=mode, position=pos):
                    result = bootstrap_result(observe(mode, pos))
                    self.assertFalse(result["complete"])
                    self.assertEqual(result["summary"]["commands"], 1)
                    self.assertFalse(result["summary"]["returned"])
                    summary = result["flush"]["summary"]
                    self.assertEqual(summary["samples"], (pos - 1) // (6 if limit == 18 else 1))
                    self.assertEqual(bool(summary["live"]), mode in ("flush-clock", "flush-map-retained", "flush-unmap"))

    def test_strict_version_authority_and_bracket_validation(self):
        text = observe("pass")
        flush = bootstrap_result(text)["flush"]
        self.assertEqual(session_log_limit(text.encode()), 98304)
        for n, packet in enumerate(flush["samples"]):
            sample = next(line for line in text.splitlines() if line.startswith(f"intel-gt-flush-sample: v=1 n={n} "))
            for bad in ("", sample + "\n" + sample, sample.replace(f"begin={packet['begin']}", "begin=1"),
                        sample.replace(f"end={packet['end']}", f"end={packet['begin'] + 10001}"),
                        sample.replace(" l3=", " unknown="),
                        sample.rsplit("=", 1)[0] + "=0x100000000"):
                with self.subTest(bad=bad), self.assertRaises(ValueError):
                    bootstrap_result(text.replace(sample, bad))
        for before, after in (("intel-bootstrap-summary: v=12 ", "intel-bootstrap-summary: v=11 "),
                              ("authority=none\nintel-gt-flush-sample", "authority=write\nintel-gt-flush-sample"),
                              ("samples=3 reads=18 maps=3 retired=3", "samples=3 reads=18 maps=3 retired=2")):
            self.assertIn(before, text)
            with self.assertRaises(ValueError):
                bootstrap_result(text.replace(before, after))

    def test_dedicated_stage_and_boot_recipe(self):
        for build in ("flush", "aux"):
            for token in ("flush", "aux"):
                status = subprocess.run(
                    [TESTS / f"intel-gt-{build}-boot", "gt-bootstrap", RECIPE + token + "-v1"], timeout=5,
                ).returncode
                self.assertEqual(status, 0 if build == token else 1)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            build = root / "build"
            for name in ("limine-binary/BOOTX64.EFI", "intel-gt-flush/srvros.elf", "initramfs.tar"):
                path = build / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(name)
            with (patch.object(boot, "ROOT", root), patch.object(boot, "STAGE", build / "hardware-boot"),
                  patch.object(boot.subprocess, "check_output", side_effect=["d" * 40, ""])):
                release = boot.stage(laptop_probe_volume="12345678", intel_gt_flush=True)
            payload = build / "hardware-boot" / release
            self.assertTrue(boot.verify(payload, release)["source_bound"])
            self.assertEqual((payload / "srvros.elf").read_text(), "intel-gt-flush/srvros.elf")
            cmdline = next(line.split("cmdline:", 1)[1].strip()
                           for line in (payload / "limine.conf").read_text().splitlines() if "cmdline:" in line)
            self.assertEqual(trial_parsers("intel-gt-flush-v59")[0](cmdline), dict(release=release, volume="12345678"))
            with self.assertRaises(ValueError):
                trial_parsers("intel-gt-aux-v58")[0](cmdline)
        with self.assertRaises(ValueError):
            boot.stage(laptop_probe_volume="12345678", intel_gt_flush=True, intel_gt_aux=True)

    def test_fatal_sanitizers(self):
        cc, flags, sources = recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix="gt-flush-san-") as temp:
            binary = Path(temp) / "flush"
            compile_fixture(TARGET, output=binary, declaration=(
                cc, [*flags, "-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"], sources,
            ))
            for mode, pos in (("pass", 0), ("flush-clock", 12), ("flush-unmap", 2), ("flush-float", 18)):
                observe(mode, pos, binary=binary)
