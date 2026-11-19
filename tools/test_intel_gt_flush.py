"""Exercise the Broadwell hardware acknowledgement capture and its receipts."""

import subprocess
import re
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import hardware_boot as boot
from fixture_compile import compile_fixture, recipe
from hardware_desktop import session_log_limit
from hardware_boot_smoke import shared_receipt
from intel_gt_bootstrap import bootstrap_result
from intel_gt_preflight import gt_result
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
                        "v": 3, "samples": 3, "reads": 24, "maps": 3, "retired": 3, "live": 0,
                    })
                    idle = mode != "flush-quiet"
                    self.assertEqual(flush["gam"], [
                        dict(allowed=0x5f, idle=False), dict(allowed=0 if idle else 0x5f, idle=idle),
                        dict(allowed=0x5f, idle=False),
                    ])
                    self.assertEqual(flush["samples"][1]["gam_go"], 0x20 if idle else 0x7f)
                    self.assertEqual(flush["samples"][1]["gam_eco"], 0x201 if idle else 0x200)
                    self.assertEqual(flush["smi_guard"], {
                        "v": 1, "reads": result["summary"]["native_writes"] + 7,
                        "point": 6, "fault": 0, "first": 13, "last": 13,
                    })

    def test_smi_change_stops_each_store_and_both_retirement_checkpoints(self):
        healthy = bootstrap_result(observe("pass"))
        expected = [2, *([3] * healthy["summary"]["native_writes"]), 4, 4, 2, 5, 6]
        for read, point in enumerate(expected, start=2):
            with self.subTest(read=read, point=point):
                result = bootstrap_result(observe("smi-count", read))
                self.assertFalse(result["complete"])
                self.assertFalse(result["hardware_ownership"])
                self.assertEqual(result["summary"]["returned"], point == 6)
                self.assertEqual(result["flush"]["smi_guard"], {
                    "v": 1, "reads": read, "point": point, "fault": 2, "first": 13, "last": 14,
                })
                if read == 2:
                    self.assertEqual(result["summary"]["commands"], 0)
                elif point in (4, 5, 6):
                    self.assertEqual(result["summary"]["commands"], 2 if point in (5, 6) else 1)
        wrapped = bootstrap_result(observe("smi-wrap", 2))
        self.assertFalse(wrapped["complete"])
        self.assertEqual(wrapped["flush"]["smi_guard"]["first"], 0xffffffff)
        self.assertEqual(wrapped["flush"]["smi_guard"]["last"], 0)
        failure = observe("smi-count", 3)
        with self.assertRaisesRegex(ValueError, "GT SMI checkpoint accounting"):
            bootstrap_result(failure.replace("intel-smi-guard: v=1 reads=3 ", "intel-smi-guard: v=1 reads=2 "))

    def test_smi_msr_is_never_accessed_on_an_unsupported_cpu_profile(self):
        for mode in ("smi-no-leaf", "smi-vendor", "smi-model", "smi-hypervisor", "smi-no-msr"):
            with self.subTest(mode=mode):
                result = bootstrap_result(observe(mode, 1))
                self.assertFalse(result["complete"])
                self.assertEqual(result["summary"]["commands"], 0)
                self.assertEqual(result["summary"]["native_writes"], 0)
                self.assertEqual(result["flush"]["smi_guard"], {
                    "v": 1, "reads": 0, "point": 1, "fault": 1, "first": 0, "last": 0,
                })

    def test_access_failures_retain_the_parent_and_report_the_attempt(self):
        for mode, limit in (("flush-read", 24), ("flush-float", 24), ("flush-clock", 24), ("flush-window", 24),
                            ("flush-map", 3), ("flush-map-retained", 3), ("flush-alias", 3), ("flush-unmap", 3)):
            for pos in range(1, limit + 1):
                with self.subTest(mode=mode, position=pos):
                    result = bootstrap_result(observe(mode, pos))
                    self.assertFalse(result["complete"])
                    self.assertEqual(result["summary"]["commands"], 1)
                    self.assertFalse(result["summary"]["returned"])
                    summary = result["flush"]["summary"]
                    self.assertEqual(summary["samples"], (pos - 1) // (8 if limit == 24 else 1))
                    self.assertEqual(len(result["flush"]["gam"]), summary["samples"])
                    self.assertEqual(bool(summary["live"]), mode in ("flush-clock", "flush-map-retained", "flush-unmap"))

    def test_strict_version_authority_and_bracket_validation(self):
        text = observe("pass")
        flush = bootstrap_result(text)["flush"]
        self.assertTrue(gt_result(text)["qualification_complete"])
        self.assertEqual(session_log_limit(text.encode()), 98304)
        row = next(line for line in text.splitlines(keepends=True) if line.startswith("intel-smi-guard:"))
        for bad in ("", row * 2, row.replace("fault=0", "fault=2"), row.replace("last=13", "last=14"),
                    row.replace("point=6", "point=5"), row.replace("reads=", "other="),
                    row.replace("authority=none", "authority=write")):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                bootstrap_result(text.replace(row, bad))
        previous = text.replace("intel-flush-summary: v=3 samples=3 reads=24 ",
                                "intel-flush-summary: v=2 samples=3 reads=18 ")
        previous = re.sub(r" gam_go=\S+ gam_eco=\S+", "", previous)
        previous = previous.replace("intel-flush-sample: v=2 ", "intel-flush-sample: v=1 ")
        self.assertNotIn("gam", bootstrap_result(previous)["flush"])
        old = previous.replace(row, "").replace("intel-flush-summary: v=2 ", "intel-flush-summary: v=1 ")
        self.assertNotIn("smi_guard", bootstrap_result(old)["flush"])
        for n, packet in enumerate(flush["samples"]):
            sample = next(line for line in text.splitlines() if line.startswith(f"intel-flush-sample: v=2 n={n} "))
            for bad in ("", sample + "\n" + sample, sample.replace(f"begin={packet['begin']}", "begin=1"),
                        sample.replace(f"end={packet['end']}", f"end={packet['begin'] + 10001}"),
                        sample.replace(" l3=", " unknown="),
                        sample.replace(" gam_go=", " unknown_go="),
                        sample.rsplit(" ", 1)[0],
                        sample.rsplit("=", 1)[0] + "=0xffffffff",
                        sample.rsplit("=", 1)[0] + "=0x100000000"):
                with self.subTest(bad=bad), self.assertRaises(ValueError):
                    bootstrap_result(text.replace(sample, bad))
        for before, after in (("intel-bootstrap-summary: v=12 ", "intel-bootstrap-summary: v=11 "),
                              ("authority=none\nintel-flush-sample", "authority=write\nintel-flush-sample"),
                              ("samples=3 reads=24 maps=3 retired=3", "samples=3 reads=24 maps=3 retired=2"),
                              ("samples=3 reads=24 maps=3 retired=3", "samples=3 reads=18 maps=3 retired=3"),
                              ("intel-flush-summary: v=3 ", "intel-flush-summary: v=2 ")):
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
            options = "laptop-capture: boot options=" + cmdline
            self.assertIn("srvros.intel-ring=entry-v1", shared_receipt(options))
            self.assertNotIn("gt-bootstrap-flush-v1", shared_receipt(options))
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
            for mode, pos in (("pass", 0), ("flush-clock", 12), ("flush-unmap", 2), ("flush-float", 24),
                              ("smi-count", 2), ("smi-wrap", 2), ("smi-hypervisor", 1)):
                observe(mode, pos, binary=binary)
