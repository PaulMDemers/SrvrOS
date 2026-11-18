"""Layout and lifetime checks against the actual full-GT range adapter."""
import base64
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import hardware_boot as boot
from hardware_boot_smoke import shared_receipt
from intel_gt_bootstrap import bootstrap_recipe, bootstrap_result
from workflow.media import raw_log_limit
from workflow.runner import trial_parsers

ROOT = Path(__file__).resolve().parents[1]
TESTS = ROOT / "build/tests"
RECIPE = (
    "srvros.storage=ram srvros.workspace-volume=12345678 "
    "srvros.desktop-release=0123456789abcdef srvros.capture=intel-display "
    "srvros.intel-backing=read-only srvros.intel-cache=read-only srvros.intel-window=read-only "
    "srvros.intel-handoff=qualify srvros.intel-gt-preflight=read-only srvros.intel-firmware=read-only "
    "srvros.intel-wake=trial srvros.intel-engines=read-only srvros.intel-completion=read-only "
    "srvros.capture-failure=reboot srvros.intel-ring=gt-bootstrap-"
)


def matching_lines(output, tag):
    return [
        dict(token.split("=", 1) for token in line.split()[1:])
        for line in output.splitlines() if line.startswith(tag + ": ")
    ]


def transcript(mode, position=0, entry=None):
    args = [str(TESTS / "intel-gt-range-adapter"), mode, str(position)]
    if entry:
        args.append(entry)
    result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True, timeout=10)
    if result.returncode:
        raise AssertionError((mode, position, result.returncode, result.stderr[-1000:]))
    return result.stdout


def fixture(mode, position=0, entry=None):
    output = transcript(mode, position, entry)
    [report] = matching_lines(output, "intel-range-summary")
    return report, matching_lines(output, "intel-range-vector")


class Range(unittest.TestCase):
    def test_stage_selects_only_the_range_image(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for name in ('limine-binary/BOOTX64.EFI', 'intel-gt-range/srvros.elf',
                         'intel-gt-memory/srvros.elf', 'initramfs.tar'):
                artifact = root / 'build' / name
                artifact.parent.mkdir(parents=True, exist_ok=True)
                artifact.write_text(name)
            stage = root / 'build/hardware-boot'
            with (
                patch.object(boot, 'ROOT', root),
                patch.object(boot, 'STAGE', stage),
                patch.object(boot.subprocess, 'check_output', side_effect=['source', '']),
            ):
                release = boot.stage(laptop_probe_volume='12345678', intel_gt_range=True)
            payload = stage / release
            self.assertEqual(boot.verify(payload, release)['config_version'],
                             'ram-usb-v1-intel-gt-bootstrap-range-laptop-probe-12345678-auto-return')
            self.assertEqual((payload / 'srvros.elf').read_text(), 'intel-gt-range/srvros.elf')
            command = next(line.split('cmdline:', 1)[1].strip()
                           for line in (payload / 'limine.conf').read_text().splitlines() if 'cmdline:' in line)
            recipe, _, _ = trial_parsers('intel-gt-range-v56')
            self.assertEqual(recipe(command), dict(release=release, volume='12345678'))
            self.assertIn('srvros.intel-ring=entry-v1', shared_receipt('laptop-capture: boot options=' + command))
            self.assertEqual(raw_log_limit('intel-gt-range-v56'), 98304)
        for others in (dict(intel_gt_memory=True), dict(intel_gt_arena=True), dict(hold_on_failure=True)):
            with self.assertRaises(ValueError):
                boot.stage(laptop_probe_volume='12345678', intel_gt_range=True, **others)

    def test_distinct_recipe(self):
        for build in ("range", "memory"):
            for recipe in ("range", "memory", "arena"):
                with self.subTest(build=build, recipe=recipe):
                    status = subprocess.run(
                        [TESTS / f"intel-gt-{build}-boot", "gt-bootstrap", RECIPE + recipe + "-v1"],
                        timeout=5,
                    ).returncode
                    self.assertEqual(status, 0 if build == recipe else 1)
        bootstrap_recipe(RECIPE + "range-v1", practical=True, memory=True, range_check=True)
        with self.assertRaises(ValueError):
            bootstrap_recipe(RECIPE + "memory-v1", practical=True, memory=True, range_check=True)

    def test_exact_words_under_both_original_entry_routes(self):
        expected = tuple(0x200000001 + slot * 4096 for slot in (*range(1356, 1484), 8704))
        for entry in (None, "disabled"):
            report, rows = fixture("pass", entry=entry)
            self.assertEqual(
                [report[k] for k in ("complete", "phase", "maps", "retired", "live", "reads", "authority")],
                ["1", "2", "4", "4", "0", "774", "none"],
            )
            for i, row in enumerate(rows):
                self.assertEqual((row["sample"], row["count"]), (str(i), "129"))
                self.assertEqual(
                    struct.unpack("<129Q", base64.b64decode(row["data"], validate=True)), expected,
                )
            self.assertEqual(len(rows), 2)
            self.assertLess(int(report["end0"]), int(report["begin1"]))

    def test_every_post_reset_entry_drift_refuses(self):
        for i in range(129):
            with self.subTest(entry=i):
                report, rows = fixture("range-drift", 389 + i * 3)
                self.assertEqual((report["complete"], report["phase"], report["live"]), ("0", "1", "0"))
                self.assertEqual(rows[1]["count"], str(i + 1))

    def test_alias_read_and_clock_faults(self):
        for mode, positions in {
            "range-map": range(1, 5),
            "range-map-retained": range(1, 5),
            "range-alias": range(1, 5),
            "range-unmap": range(1, 5),
            "range-read": (1, 2, 3, 384, 385, 386, 387, 388, 771, 772, 773, 774),
            "range-high": (1, 3, 385, 387, 388, 390, 772, 774),
            "range-clock": (1, 385, 388, 772),
        }.items():
            for position in positions:
                with self.subTest(mode=mode, position=position):
                    report, _ = fixture(mode, position)
                    self.assertEqual(report["complete"], "0")
                    retained = mode in ("range-map-retained", "range-unmap", "range-clock")
                    self.assertEqual(report["live"] != "0", retained)

    def test_strict_bootstrap_range_receipt(self):
        raw = transcript("pass")
        result = bootstrap_result(raw)
        self.assertTrue(result["complete"])
        self.assertFalse(result["hardware_ownership"])
        [before, after] = result["range"]["vectors"]
        self.assertEqual(before, after)
        lines = raw.splitlines()
        vectors = [line for line in lines if line.startswith("intel-range-vector:")]
        summary = next(line for line in lines if line.startswith("intel-range-summary:"))
        changed = base64.b64encode(struct.pack("<129Q", *after[:-1], after[-1] ^ (1 << 44))).decode("ascii")
        faults = (
            raw.replace(vectors[1], vectors[0]),
            raw.replace(vectors[1], ""),
            raw.replace(vectors[1], vectors[1] + "\n" + vectors[1]),
            raw.replace(vectors[1], vectors[1].replace("count=129", "count=128")),
            raw.replace(vectors[1], vectors[1].replace("data=", "data=X")),
            raw.replace(vectors[1], vectors[1].split("data=", 1)[0] + "data=" + changed),
            raw.replace(summary, summary.replace("authority=none", "authority=driver")),
            raw.replace(summary, summary.replace("retired=4", "retired=3")),
            raw.replace(summary, summary.replace(f"begin1={result['range']['summary']['begin1']}", "begin1=1")),
            raw.replace("intel-bootstrap-summary: v=9 ", "intel-bootstrap-summary: v=6 "),
        )
        for bad in faults:
            with self.assertRaises(ValueError):
                bootstrap_result(bad)
        for case, pos in (("range-drift", 389), ("range-clock", 388), ("range-read", 1)):
            with self.subTest(case=case):
                value = bootstrap_result(transcript(case, pos))
                self.assertFalse(value["complete"])
                self.assertFalse(value["diagnostic_complete"])
                self.assertEqual(value["range"]["summary"]["complete"], 0)
