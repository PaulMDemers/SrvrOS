"""Exercise the actual post-maintenance Broadwell primary snapshot adapter."""

import base64
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import hardware_boot as boot
from fixture_compile import compile_fixture, recipe
from intel_display_backing import backing_result
from intel_gt_bootstrap import bootstrap_result, historical_view
from intel_gt_primary import verify_primary_backing
from test_intel_gt_range import RECIPE, ROOT, TESTS, matching_lines
from workflow.runner import trial_parsers

TARGET = "build/tests/intel-gt-primary-adapter"


def run(mode, position=0, entry=None, binary=ROOT / TARGET):
    args = [str(binary), mode, str(position)]
    if entry:
        args.append(entry)
    return subprocess.run(args, cwd=ROOT, text=True, capture_output=True, timeout=20, check=True).stdout


class Primary(unittest.TestCase):
    def test_stage_selects_a_source_bound_primary_image(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            build = root / "build"
            for name in ("limine-binary/BOOTX64.EFI", "intel-gt-primary/srvros.elf", "initramfs.tar"):
                path = build / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(name)
            with (patch.object(boot, "ROOT", root), patch.object(boot, "STAGE", build / "hardware-boot"),
                  patch.object(boot.subprocess, "check_output", side_effect=["c" * 40, ""])):
                release = boot.stage(laptop_probe_volume="12345678", intel_gt_primary=True)
            payload = build / "hardware-boot" / release
            self.assertTrue(boot.verify(payload, release)["source_bound"])
            self.assertEqual((payload / "srvros.elf").read_text(), "intel-gt-primary/srvros.elf")
            recipe, _, _ = trial_parsers("intel-gt-primary-v57")
            cmdline = next(line.split("cmdline:", 1)[1].strip()
                           for line in (payload / "limine.conf").read_text().splitlines() if "cmdline:" in line)
            self.assertEqual(recipe(cmdline), dict(release=release, volume="12345678"))
            with self.assertRaises(ValueError):
                trial_parsers("intel-gt-range-v56")[0](cmdline)
        with self.assertRaises(ValueError):
            boot.stage(laptop_probe_volume="12345678", intel_gt_primary=True, intel_gt_range=True)

    def test_distinct_recipe(self):
        for build in ("primary", "range"):
            for token in ("primary", "range"):
                status = subprocess.run(
                    [TESTS / f"intel-gt-{build}-boot", "gt-bootstrap", RECIPE + token + "-v1"], timeout=5,
                ).returncode
                self.assertEqual(status, 0 if build == token else 1)

    def test_two_fresh_samples_under_the_same_original_owner(self):
        for entry in (None, "disabled"):
            out = run("pass", entry=entry)
            parent, = matching_lines(out, "intel-bootstrap-summary")
            self.assertEqual((parent["v"], parent["complete"], parent["hardware_ownership"]), ("10", "1", "0"))
            summary, = matching_lines(out, "intel-primary-summary")
            self.assertEqual([summary[k] for k in ("complete", "samples", "reads", "maps", "retired", "authority")],
                             ["1", "2", "112", "10", "10", "none"])
            self.assertEqual([int(summary[k]) for k in ("image_end", "pte_end", "candidate")],
                             [5554176, 6078464, 0x2200000])
            rows = matching_lines(out, "intel-primary-vector")
            self.assertEqual([row["sample"] for row in rows], ["0", "1"])
            first, second = (struct.unpack("<77I", base64.b64decode(row["data"], validate=True)) for row in rows)
            self.assertEqual(first, second)
            self.assertEqual(first[:3], ((1439 << 16) | 899, 0, 0))
            self.assertEqual(first[69:], (0xc0000000,) * 8)

    def test_every_word_rejects_drift_and_read_failure(self):
        for mode in ("primary-drift", "primary-read"):
            for position in range(1, 113):
                with self.subTest(mode=mode, position=position):
                    report, = matching_lines(run(mode, position), "intel-primary-summary")
                    self.assertEqual(report["complete"], "0")

    def test_distinct_virtual_slot_with_aliased_physical_target_refuses(self):
        raw = run("pass")
        parsed = bootstrap_result(raw)
        source = backing_result(historical_view(raw, practical=True, memory=True, range_check=True, primary=True))
        verify_primary_backing(source, parsed["range"], parsed["primary"])
        for position in (0, 1355, 1356, 1483):
            with self.subTest(physical_page=position), self.assertRaisesRegex(ValueError, "aliases primary"):
                parsed["range"]["vectors"][1][-1] = 0x8c001001 + position * 4096
                verify_primary_backing(source, parsed["range"], parsed["primary"])
        fault = bootstrap_result(run("primary-physical-overlap", 1))
        self.assertFalse(fault["owner_complete"])
        self.assertEqual((fault["primary"]["summary"]["samples"], fault["primary"]["summary"]["reason"]), (2, 7))
        self.assertEqual(fault["summary"]["commands"], 1)
        self.assertEqual(fault["summary"]["pte_writes"], 0)

    def test_alias_and_clock_failure_retain_the_right_resources(self):
        for mode in ("primary-map", "primary-map-retained", "primary-alias", "primary-unmap"):
            for position in range(1, 11):
                with self.subTest(mode=mode, position=position):
                    report, = matching_lines(run(mode, position), "intel-primary-summary")
                    self.assertEqual(report["complete"], "0")
                    self.assertEqual(int(report["live"], 16) != 0, mode in ("primary-map-retained", "primary-unmap"))
        for position, mapped in ((1, True), (6, False), (56, True), (112, True)):
            report, = matching_lines(run("primary-clock", position), "intel-primary-summary")
            self.assertEqual((report["complete"], int(report["live"], 16) != 0), ("0", mapped))
        for position in (1, 6, 56, 112):
            report, = matching_lines(run("primary-float", position), "intel-primary-summary")
            self.assertEqual((report["complete"], int(report["live"], 16)), ("0", 0))

    def test_strict_receipt_requires_original_readers_geometry_and_clock_order(self):
        raw = run("pass")
        parsed = bootstrap_result(raw)
        self.assertTrue(parsed["complete"])
        self.assertTrue(parsed["diagnostic_primary"])
        self.assertFalse(parsed["hardware_ownership"])
        lines = raw.splitlines()
        _, second = [line for line in lines if line.startswith("intel-primary-vector:")]
        summary = next(line for line in lines if line.startswith("intel-primary-summary:"))
        words = parsed["primary"]["vectors"][1]
        for index in (0, 6, 30, 44, 76):
            changed = list(words)
            changed[index] ^= 1
            packet = base64.b64encode(struct.pack("<77I", *changed)).decode("ascii")
            with self.assertRaises(ValueError):
                bootstrap_result(raw.replace(second, second.split("data=")[0] + "data=" + packet))
        for old, new in ((second, ""), (second, second + "\n" + second),
                         (summary, summary.replace("maps=10", "maps=9")),
                         (summary, summary.replace("authority=none", "authority=driver")),
                         (summary, summary.replace(f"begin0={parsed['primary']['summary']['begin0']}", "begin0=1")),
                         ("intel-bootstrap-summary: v=10 ", "intel-bootstrap-summary: v=9 ")):
            with self.assertRaises(ValueError):
                bootstrap_result(raw.replace(old, new))
        for mode, position in (("primary-drift", 112), ("primary-clock", 1), ("primary-map-retained", 2)):
            self.assertFalse(bootstrap_result(run(mode, position))["complete"])

    def test_fatal_sanitizers(self):
        compiler, flags, sources = recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix="gt-primary-san-") as temp:
            binary = Path(temp) / "primary"
            compile_fixture(TARGET, output=binary, declaration=(
                compiler, [*flags, "-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"], sources,
            ))
            for mode, position in (("pass", 0), ("primary-drift", 6), ("primary-unmap", 10),
                                   ("primary-clock", 112), ("primary-float", 1), ("primary-physical-overlap", 1)):
                run(mode, position, binary=binary)


if __name__ == "__main__":
    unittest.main()
