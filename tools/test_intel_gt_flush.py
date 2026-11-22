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
from intel_gt_flush import MEDIA_WORDS
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
            for mode, expected in (("pass", [0, 31, 0, 0]), ("flush-quiet", [0, 0, 0, 0]), ("flush-stale", [31, 31, 31, 31])):
                with self.subTest(entry=entry, mode=mode):
                    result = bootstrap_result(observe(mode, entry=entry))
                    self.assertEqual(result["version"], 12)
                    self.assertEqual(result["range"]["slot"]["returned"], 1)
                    self.assertTrue(result["complete"])
                    self.assertFalse(result["hardware_ownership"])
                    self.assertEqual(result["summary"]["obligations"], 255)
                    self.assertEqual(result["summary"]["pte_writes"], 0)
                    flush = result["flush"]
                    self.assertEqual(flush["engine_ack"], expected)
                    self.assertEqual(flush["engine_ack_rise"], 31 if mode == "pass" else 0)
                    self.assertEqual(flush["l3_flush_ack"], list(map(bool, expected)))
                    self.assertEqual(flush["summary"], {
                        "v": 9, "samples": 4, "reads": 100, "maps": 28, "retired": 28, "live": 0,
                    })
                    self.assertEqual(flush["media_outstanding"], [dict.fromkeys(MEDIA_WORDS, 0)] * 4)
                    power = flush["power_controls"]
                    self.assertEqual([p["requested_clients"] for p in power], [0x5f, 0x5f if mode == "flush-quiet" else 0, 0x5f, 0x5f])
                    self.assertEqual([p["sw_target"] for p in power], [0, 0, 4, 4])
                    self.assertEqual([p["rc6_enabled"] for p in power], [True, True, False, False])
                    self.assertTrue(all(p["threshold_ticks"] == 625 for p in power))
                    idle = mode != "flush-quiet"
                    self.assertEqual(flush["gam"], [
                        dict(allowed=0x5f, idle=False), dict(allowed=0 if idle else 0x5f, idle=idle),
                        dict(allowed=0x5f, idle=False), dict(allowed=0x5f, idle=False),
                    ])
                    self.assertEqual(flush["samples"][1]["gam_go"], 0x20 if idle else 0x7f)
                    self.assertEqual(flush["samples"][1]["gam_eco"], 0x201 if idle else 0x200)
                    self.assertEqual(flush["smi_guard"], {
                        "v": 1, "reads": result["summary"]["native_writes"] + 7 + flush["l3_banks"]["summary"]["writes"],
                        "point": 6, "fault": 0, "first": 13, "last": 13,
                    })

    def test_bank_steering_and_restoration(self):
        text = observe("pass")
        bank = bootstrap_result(text)["flush"]["l3_banks"]
        self.assertEqual(bank["summary"]["original"], bank["summary"]["final"])
        self.assertEqual([b["sqc"] for b in bank["banks"]], [0x40400000 | i for i in range(8)])
        self.assertEqual([b["scratch"] for b in bank["banks"]], [1]*7 + [17])
        for mode, limit in (("bank-float", 8), ("bank-power", 8), ("bank-foreign", 1), ("bank-map", 4), ("bank-post", 8), ("bank-alias", 10)):
            for i in range(1, limit + 1):
                with self.subTest(mode=mode, i=i):
                    failed = bootstrap_result(observe(mode, i))
                    self.assertFalse(failed["complete"])
                    self.assertEqual(failed["reset"]["reset_attempts"], 0)
                    if mode in ("bank-float", "bank-power"):
                        self.assertTrue(failed["flush"]["l3_banks"]["summary"]["restored"])
                    if mode == "bank-foreign":
                        self.assertFalse(failed["flush"]["l3_banks"]["summary"]["restored"])
        row = next(l for l in text.splitlines() if l.startswith("intel-l3-bank: v=1 n=7 "))
        for bad in ("", row.replace("n=7", "n=0"), re.sub(r"selector=\S+", "selector=0", row),
                    re.sub(r"scratch=\S+", "scratch=0xffffffff", row)):
            with self.assertRaises(ValueError):
                bootstrap_result(text.replace(row, bad))

    def test_live_slice_power(self):
        text = observe("pass")
        self.assertEqual(bootstrap_result(text)["flush"]["slice_power"],
                         [dict(available=3, before=m, after=m) for m in (0, 3, 0, 0)])
        row = next(line for line in text.splitlines() if line.startswith("intel-l3-power:"))
        for bad in ("", row + "\n" + row, row.replace("n=0", "n=2"),
                    re.sub(r"after=\S+", "after=0x4", row)):
            with self.assertRaises(ValueError):
                bootstrap_result(text.replace(row, bad))
        self.assertFalse(bootstrap_result(observe("slice-read", 1))["complete"])

    def test_smi_change_stops_each_store_and_both_retirement_checkpoints(self):
        healthy = bootstrap_result(observe("pass"))
        expected = [2, *([3] * (healthy["summary"]["native_writes"] + healthy["flush"]["l3_banks"]["summary"]["writes"])), 4, 4, 2, 5, 6]
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
        for mode, limit in (("flush-read", 100), ("flush-float", 100), ("flush-clock", 100), ("flush-window", 100),
                            ("flush-map", 28), ("flush-map-retained", 28), ("flush-alias", 28), ("flush-unmap", 28)):
            for pos in range(1, limit + 1):
                with self.subTest(mode=mode, position=pos):
                    result = bootstrap_result(observe(mode, pos))
                    self.assertFalse(result["complete"])
                    self.assertEqual(result["range"]["slot"]["reserved"], 1)
                    self.assertEqual(result["range"]["slot"]["returned"], 0)
                    self.assertEqual(result["summary"]["commands"], 1)
                    self.assertFalse(result["summary"]["returned"])
                    summary = result["flush"]["summary"]
                    self.assertEqual(summary["samples"], (pos - 1) // (25 if limit == 100 else 7))
                    self.assertEqual(len(result["flush"]["gam"]), summary["samples"])
                    self.assertEqual(bool(summary["live"]), mode in ("flush-clock", "flush-map-retained", "flush-unmap"))

    def test_each_outstanding_counter_stops_the_current_phase(self):
        for pos in range(16):
            n, index = divmod(pos, 4)
            result = bootstrap_result(observe("flush-busy", pos + 1))
            self.assertFalse(result["complete"])
            self.assertFalse(result["hardware_ownership"])
            queues = result["flush"]["media_outstanding"]
            self.assertEqual(len(queues), n + 1)
            expected = dict.fromkeys(MEDIA_WORDS, 0)
            expected[MEDIA_WORDS[index]] = index + 1
            self.assertEqual(queues[-1], expected)
            self.assertFalse(result["flush"]["summary"]["live"])

    def test_strict_version_authority_and_bracket_validation(self):
        text = observe("pass")
        flush = bootstrap_result(text)["flush"]
        self.assertTrue(gt_result(text)["qualification_complete"])
        self.assertEqual(session_log_limit(text.encode()), 98304)
        for key in MEDIA_WORDS:
            with self.assertRaisesRegex(ValueError, "advanced past outstanding"):
                bootstrap_result(re.sub(rf" {key}=\S+", f" {key}=0x40014321", text, count=1))
        row = next(line for line in text.splitlines(keepends=True) if line.startswith("intel-smi-guard:"))
        for bad in ("", row * 2, row.replace("fault=0", "fault=2"), row.replace("last=13", "last=14"),
                    row.replace("point=6", "point=5"), row.replace("reads=", "other="),
                    row.replace("authority=none", "authority=write")):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                bootstrap_result(text.replace(row, bad))
        old_power = re.sub(r"^intel-l3-(?:power|steer|bank):.*\n|^intel-gam-.*\n|^intel-flush-sample: v=5 n=2 .*\n", "", text, flags=re.M)
        legacy_smi = row.replace(f"reads={flush['smi_guard']['reads']}",
                                 f"reads={flush['smi_guard']['reads'] - flush['l3_banks']['summary']['writes']}")
        old_power = old_power.replace(row, legacy_smi)
        old_power = re.sub(r" l3_close=.*", "", old_power)
        old_power = old_power.replace("v=5 n=3 ", "v=5 n=2 ").replace("intel-flush-sample: v=5", "intel-flush-sample: v=4")
        old_power = re.sub(r"^intel-slot-summary:.*\n", "", old_power, flags=re.M)
        old_power = old_power.replace("intel-range-summary: v=2 ", "intel-range-summary: v=1 ")
        old_power = old_power.replace("v=9 samples=4 reads=100 maps=28 retired=28", "v=5 samples=3 reads=48 maps=15 retired=15")
        self.assertNotIn("request_trace", bootstrap_result(old_power)["flush"])
        media = old_power.replace("intel-flush-summary: v=5 samples=3 reads=48 maps=15 retired=15",
                             "intel-flush-summary: v=4 samples=3 reads=36 maps=9 retired=9")
        media = re.sub(r" gam_req=\S+ rc_control=\S+ rc_target=\S+ rc6_threshold=\S+", "", media)
        media = media.replace("intel-flush-sample: v=4 ", "intel-flush-sample: v=3 ")
        self.assertNotIn("power_controls", bootstrap_result(media)["flush"])
        legacy = media.replace("intel-flush-summary: v=4 samples=3 reads=36 maps=9 retired=9",
                              "intel-flush-summary: v=3 samples=3 reads=24 maps=3 retired=3")
        legacy = re.sub(r" vcs0_ref=\S+ vcs0_prefetch=\S+ vcs1_ref=\S+ vcs1_prefetch=\S+", "", legacy)
        legacy = legacy.replace("intel-flush-sample: v=3 ", "intel-flush-sample: v=2 ")
        self.assertNotIn("media_outstanding", bootstrap_result(legacy)["flush"])
        previous = legacy.replace("intel-flush-summary: v=3 samples=3 reads=24 ",
                                "intel-flush-summary: v=2 samples=3 reads=18 ")
        previous = re.sub(r" gam_go=\S+ gam_eco=\S+", "", previous)
        previous = previous.replace("intel-flush-sample: v=2 ", "intel-flush-sample: v=1 ")
        self.assertNotIn("gam", bootstrap_result(previous)["flush"])
        old = previous.replace(legacy_smi, "").replace("intel-flush-summary: v=2 ", "intel-flush-summary: v=1 ")
        self.assertNotIn("smi_guard", bootstrap_result(old)["flush"])
        for n, packet in enumerate(flush["samples"]):
            sample = next(line for line in text.splitlines() if line.startswith(f"intel-flush-sample: v=5 n={n} "))
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
                              ("authority=none\nintel-l3-power", "authority=write\nintel-l3-power"),
                              ("samples=4 reads=100 maps=28 retired=28", "samples=4 reads=100 maps=28 retired=27"),
                              ("samples=4 reads=100 maps=28 retired=28", "samples=4 reads=99 maps=28 retired=28"),
                              ("intel-flush-summary: v=9 ", "intel-flush-summary: v=6 ")):
            self.assertIn(before, text)
            with self.assertRaises(ValueError):
                bootstrap_result(text.replace(before, after))

    def test_reason_transition_bounds_and_blocked_flush_evidence(self):
        text = observe("pass")
        trace = bootstrap_result(text)["flush"]["request_trace"]
        self.assertGreater(trace["summary"]["max_gap_us"], 0)
        self.assertEqual([(c["asserted"], c["cleared"]) for c in trace["reason_changes"]], [(127, 0), (0, 127)])
        self.assertEqual(trace["last_observed_reasons"], 0)
        self.assertTrue(all(c["earliest_us"] < c["latest_us"] for c in trace["reason_changes"]))
        self.assertEqual([w["sampled_block_and_flush"] for w in trace["windows"]], [False, True, False, False])
        self.assertEqual(trace["maintenance"]["observed_open_clients"], 0x5f)
        self.assertFalse(trace["maintenance"]["observed_block"])
        sample = next(l for l in text.splitlines() if l.startswith("intel-flush-sample: v=5 n=1 "))
        for old, new in (("req_close=0", "req_close=0x1"), ("go_close=0x20", "go_close=0x21"),
                         ("rcs_close=0x20", "rcs_close=0x10"), ("eco_close=0x201", "eco_close=0x200"),
                         ("l3_close=0x1", "l3_close=0x3")):
            changed = text.replace(sample, sample.replace(old, new))
            self.assertFalse(bootstrap_result(changed)["flush"]["request_trace"]["windows"][1]["sampled_block_and_flush"])
        stale = bootstrap_result(observe("flush-stale"))["flush"]["request_trace"]
        self.assertFalse(any(w["sampled_block_and_flush"] for w in stale["windows"]))
        latched = bootstrap_result(observe("flush-latched"))["flush"]["request_trace"]
        self.assertEqual(latched["last_observed_reasons"], 0x7f)
        self.assertFalse(any(c["cleared"] for c in latched["reason_changes"]))
        chatter = bootstrap_result(observe("flush-chatter"))["flush"]["request_trace"]
        self.assertEqual(chatter["summary"]["spans"], 32)
        self.assertGreater(chatter["summary"]["omitted"], 0)
        self.assertIsNone(chatter["last_observed_reasons"])
        self.assertFalse(any(w["observed_block"] for w in chatter["windows"]))
        span = next(l for l in text.splitlines() if l.startswith("intel-gam-span: v=1 n=1 "))
        for bad in ("", span + "\n" + span, span.replace("pulse=0", "pulse=2"),
                    re.sub(r"first_end=\d+", "first_end=1", span)):
            with self.assertRaises(ValueError):
                bootstrap_result(text.replace(span, bad))
        with self.assertRaises(ValueError):
            bootstrap_result(text.replace("omitted=0 max_gap_us=", "omitted=1 max_gap_us="))

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
            for mode, pos in (("pass", 0), ("flush-clock", 12), ("flush-unmap", 2), ("flush-float", 100),
                              ("smi-count", 2), ("smi-wrap", 2), ("smi-hypervisor", 1)):
                observe(mode, pos, binary=binary)
