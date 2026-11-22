#!/usr/bin/env python3
"""Replay the Air6,1 captured-prefix boundary with real process/VFS/exFAT code.

The independent CPU0 turn and callback clock are host models. No USB controller,
packet timing, actual scheduler wait or native speedup is modeled by this test.
"""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess

from exfat_test_image import ExfatFixture
from test_storage_leases import empty_image

ROOT = Path(__file__).resolve().parents[1]
SOURCES = [
    "tools/test_reserved_append_driver.c", "tools/test_sync_commit_boundary.c",
    "kernel/src/fs/exfat.c", "kernel/src/vfs.c", "kernel/src/vfs_namespace.c",
    "kernel/src/autotest_storage.c", "kernel/src/file_verify.c",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "build/review/sync-commit-boundary")
    parser.add_argument("--limine-include", type=Path, default=ROOT / "build/limine-protocol/include")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    fsck = shutil.which("fsck.exfat")
    if not fsck:
        parser.error("exfatprogs fsck.exfat is required")

    def run(command, name):
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
        (output / (name + ".log")).write_text(result.stdout + result.stderr)
        assert result.returncode == 0, (name, result.returncode, result.stderr)
        return result.stdout

    command = shlex.split(os.environ.get("CC", "cc"))
    if platform.system() == "Darwin":
        command += ["-arch", "x86_64"]
    command += [
        "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
        "-ffunction-sections", "-fdata-sections",
        "-idirafter", str(ROOT / "userspace/lib/include"),
        "-I", str(ROOT / "shared/include"), "-I", str(ROOT / "kernel/include"),
        "-I", str(args.limine_include.resolve()),
        "-DSRVROS_EXFAT_HOST_TEST", "-DSRVROS_EXFAT_COST_TEST",
        "-DSRVROS_SYNC_CONTENT_HOST_TEST",
    ]
    binary = output / "sync-commit-boundary"
    command += [str(ROOT / source) for source in SOURCES]
    command += ["-Wl,-dead_strip" if platform.system() == "Darwin" else "-Wl,--gc-sections",
                "-o", str(binary)]
    run(command, "build")

    fixture = empty_image(32 * 1024 * 1024, cluster_shift=8)
    fixture.add_directory_files("work", {"other.txt": b"", "anchor.txt": b"unchanged"})
    seed = bytes(fixture.data)
    expected = bytes((index * 37 + 11) & 255 for index in range(1366 * 768 * 4 + 54))
    cases = {}
    for mode in ("control", "delay"):
        image = output / (mode + ".exfat")
        image.write_bytes(seed)
        text = run([str(binary), str(image), mode], mode)
        line, = [line for line in text.splitlines() if line.startswith("commit-boundary:")]
        row = {key: int(value) for key, value in re.findall(r"(\w+)=(\d+)", line)}
        assert row["prefix"] == 2555904 and row["captured"] == 3145728
        assert row["live"] == 3407872 and row["appended"] == 262144
        assert row["fast"] == 4 and row["refused_fifth"] == row["dirty"] == 1
        assert row["flushes"] == 8
        trace, = [line for line in text.splitlines() if line.startswith("reserved-io:")]
        raw = image.read_bytes()
        media = ExfatFixture(raw)
        assert media.read("work/capture.bmp") == expected
        assert media.read("work/anchor.txt") == b"unchanged"
        assert media.read("work/other.txt") == b""
        assert ": clean." in run([fsck, "-n", str(image)], mode + "-fsck")
        archive = output / (mode + ".exfat.gz")
        archive.write_bytes(gzip.compress(raw, mtime=0))
        assert gzip.decompress(archive.read_bytes()) == raw
        row.update(io_trace=trace, raw_sha256=hashlib.sha256(raw).hexdigest(),
                   archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),
                   full_media_verified=True)
        cases[mode] = row
        image.unlink()

    control, delayed = cases["control"], cases["delay"]
    assert control["model_delay_us"] == 0 and delayed["model_delay_us"] == 359848
    assert control["raw_sha256"] == delayed["raw_sha256"]
    assert control["io_trace"] == delayed["io_trace"]
    assert delayed["content_us"] - control["content_us"] == 359848
    assert delayed["data_us"] - control["data_us"] == 359848
    assert delayed["prefix_us"] == control["prefix_us"]
    dependencies = SOURCES + [
        "tools/test_sync_commit_boundary.py", "tools/test_reserved_append_io.h",
        "tools/test_reserved_append_process.c", "tools/test_sync_content_bridge.inc",
        "tools/exfat_host_driver.c", "kernel/src/process.c", "kernel/src/sync_timing.c",
    ]
    result = dict(status="PASS", scope=__doc__, cases=cases, compile_command=command,
        source_sha256={path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest()
                       for path in dependencies})
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("sync-commit-boundary: PASS captured-prefix/dirty-survivor and fifth-copy refusal; "
          "identical control/delayed media and ordered I/O, eight barriers, two fresh proofs, "
          "full allocation and independent fsck")


if __name__ == "__main__":
    main()
