"""Qualify the held Broadwell VGA, FBC and PSR observation."""

import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import hardware_boot as boot
from fixture_compile import compile_fixture, recipe
from intel_gt_bootstrap import bootstrap_result
from test_intel_gt_primary import run
from test_intel_gt_range import RECIPE, ROOT, TESTS, matching_lines
from workflow.runner import trial_parsers

TARGET = "build/tests/intel-gt-aux-adapter"


def observe(mode, position=0, *, entry=None, binary=ROOT / TARGET):
    return run(mode, position, entry=entry, binary=binary)


class Auxiliary(unittest.TestCase):
    def test_both_original_paths_produce_inactive_observations_without_authority(self):
        for entry in (None, "disabled"):
            result = bootstrap_result(observe("pass", entry=entry))
            self.assertEqual(result["version"], 11)
            self.assertTrue(result["complete"])
            self.assertFalse(result["hardware_ownership"])
            aux = result["aux"]
            self.assertEqual(aux["summary"], {
                "v": 1, "samples": 2, "reads": 14, "maps": 8, "retired": 8, "live": 0,
                "stable": 1, "inactive": 1,
            })
            self.assertEqual([packet["blockers"] for packet in aux["packets"]], [0, 0])

    def test_active_status_or_drift_is_reported_without_writing_control_registers(self):
        modes = {
            "aux-vga-active": 1, "aux-fbc-active": 2, "aux-fbc-segment": 4,
            "aux-psr-enable": 8, "aux-psr-state": 16, "aux-route": 32, "aux-drift": 0,
        }
        for mode, mask in modes.items():
            with self.subTest(mode=mode):
                result = bootstrap_result(observe(mode))
                self.assertTrue(result["complete"])
                self.assertFalse(result["hardware_ownership"])
                self.assertEqual([packet["blockers"] for packet in result["aux"]["packets"]], [mask, mask])
                self.assertEqual(result["aux"]["summary"]["inactive"], 0)
                self.assertEqual(result["aux"]["summary"]["stable"], int(mode != "aux-drift"))

    def test_read_alias_and_clock_faults_never_return_the_parent(self):
        for mode, limit in (("aux-read", 14), ("aux-float", 14), ("aux-clock", 14),
                            ("aux-map", 8), ("aux-map-retained", 8), ("aux-alias", 8), ("aux-unmap", 8)):
            for pos in range(1, limit + 1):
                with self.subTest(mode=mode, word=pos):
                    result = bootstrap_result(observe(mode, pos))
                    self.assertFalse(result["complete"])
                    self.assertEqual(result["summary"]["commands"], 1)
                    live = result["aux"]["summary"]["live"]
                    self.assertEqual(bool(live), mode in ("aux-map-retained", "aux-unmap", "aux-clock"))

    def test_receipt_rejects_fabricated_state_and_out_of_scope_claims(self):
        text = observe("pass")
        sample = next(line for line in text.splitlines() if line.startswith("intel-aux-sample:") and "n=1 " in line)
        packet = bootstrap_result(text)["aux"]["packets"][1]
        for old, new in ((sample, ""), (sample, sample + "\n" + sample),
                         ("authority=none\nintel-aux-sample", "authority=write\nintel-aux-sample"),
                         ("inactive=1", "inactive=0"),
                         (sample, sample.replace("blockers=0", "blockers=2")),
                         (sample, sample.replace("vga=0x80000000", "vga=0x100000000")),
                         (sample, sample.replace(f"begin={packet['begin']}", "begin=1")),
                         (sample, sample.replace("ddi=0x82200000", "ddi=0")),
                         ("intel-bootstrap-summary: v=11 ", "intel-bootstrap-summary: v=10 ")):
            with self.subTest(fault=new), self.assertRaises(ValueError):
                bootstrap_result(text.replace(old, new))

    def test_exact_source_bound_stage_and_boot_recipe(self):
        for build in ("aux", "primary"):
            for token in ("aux", "primary"):
                status = subprocess.run(
                    [TESTS / f"intel-gt-{build}-boot", "gt-bootstrap", RECIPE + token + "-v1"], timeout=5,
                ).returncode
                self.assertEqual(status, 0 if build == token else 1)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            build = root / "build"
            for name in ("limine-binary/BOOTX64.EFI", "intel-gt-aux/srvros.elf", "initramfs.tar"):
                path = build / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(name)
            with (patch.object(boot, "ROOT", root), patch.object(boot, "STAGE", build / "hardware-boot"),
                  patch.object(boot.subprocess, "check_output", side_effect=["d" * 40, ""])):
                release = boot.stage(laptop_probe_volume="12345678", intel_gt_aux=True)
            payload = build / "hardware-boot" / release
            self.assertTrue(boot.verify(payload, release)["source_bound"])
            self.assertEqual((payload / "srvros.elf").read_text(), "intel-gt-aux/srvros.elf")
            cmdline = next(line.split("cmdline:", 1)[1].strip()
                           for line in (payload / "limine.conf").read_text().splitlines() if "cmdline:" in line)
            self.assertEqual(trial_parsers("intel-gt-aux-v58")[0](cmdline), dict(release=release, volume="12345678"))
            with self.assertRaises(ValueError):
                trial_parsers("intel-gt-primary-v57")[0](cmdline)
        with self.assertRaises(ValueError):
            boot.stage(laptop_probe_volume="12345678", intel_gt_aux=True, intel_gt_primary=True)

    def test_fatal_sanitizers(self):
        cc, flags, sources = recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix="gt-aux-san-") as temp:
            binary = Path(temp) / "aux"
            compile_fixture(TARGET, output=binary, declaration=(
                cc, [*flags, "-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"], sources,
            ))
            for mode, pos in (("pass", 0), ("aux-clock", 14), ("aux-unmap", 4),
                              ("aux-psr-state", 0), ("aux-float", 8)):
                observe(mode, pos, binary=binary)


if __name__ == "__main__":
    unittest.main()
