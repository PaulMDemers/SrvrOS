#!/usr/bin/env python3
"""Metadata-tail publication and fault comparison against whole-cluster padding."""
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
import struct
import subprocess

from exfat_test_image import ExfatFixture, check_clean_image
from test_populated_save import populated_seed, logical_files
from test_storage_leases import empty_image

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "build/review/exfat-metadata-tail")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    fsck = shutil.which("fsck.exfat")
    if not fsck:
        parser.error("exfatprogs fsck.exfat is required")
    sources = ["tools/exfat_host_driver.c", "kernel/src/fs/exfat.c",
               "kernel/src/vfs.c", "kernel/src/autotest_storage.c"]
    dependencies = sources + ["tools/test_exfat_metadata_tail.c", "tools/test_exfat_metadata_tail.py",
        "tools/test_populated_save.py", "kernel/src/fs/exfat_stream.inc"]
    source_hashes = {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest()
                     for path in dependencies}
    record = dict(status="RUNNING", cases=[], faults=[], commands=[])

    def save():
        (output / "result.json").write_text(json.dumps(record, indent=2) + "\n")

    def run(command, name, allowed=(0,), env=None):
        process = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                                 timeout=120, env={**os.environ, **(env or {})})
        (output / (name + ".log")).write_text(process.stdout + process.stderr)
        assert process.returncode in allowed, (name, process.returncode, process.stderr)
        return process

    def compile_binary(name, sources, flags=()):
        command = shlex.split(os.environ.get("CC", "cc"))
        if platform.system() == "Darwin":
            command += ["-arch", "x86_64"]
        command += ["-std=c11", "-O2", "-g", "-Wall", "-Wextra", "-Werror",
                    "-ffunction-sections", "-fdata-sections", "-I", str(ROOT / "shared/include"),
                    "-I", str(ROOT / "kernel/include"), *flags]
        binary = output / name
        command += [str(ROOT / source) for source in sources]
        command += ["-Wl,-dead_strip" if platform.system() == "Darwin" else "-Wl,--gc-sections",
                    "-o", str(binary)]
        record["commands"].append(command)
        run(command, name + "-build")
        return binary

    unit = compile_binary("unit", ["tools/test_exfat_metadata_tail.c"])
    run([str(unit)], "unit")
    drivers = {name: compile_binary(name, sources,
        ["-DSRVROS_EXFAT_HOST_TEST", *(["-DSRVROS_EXFAT_METADATA_TAIL_OFF"] if name == "baseline" else [])])
        for name in ("baseline", "candidate")}

    def archive(name, raw):
        path = output / (name + ".exfat.gz")
        path.write_bytes(gzip.compress(raw, mtime=0))
        assert gzip.decompress(path.read_bytes()) == raw
        return hashlib.sha256(raw).hexdigest()

    def events(text):
        return [line for line in text.splitlines()
                if line.startswith(("write-trace:", "flush-trace:"))]

    geometries = [("small", bytes(empty_image(16 * 1024 * 1024, cluster_shift=3).data), 512),
                  ("4k-sector", bytes(empty_image(16 * 1024 * 1024, sector_shift=12, cluster_shift=5).data), 4096),
                  ("populated", populated_seed(), 512)]
    for label, initial, block_size in geometries:
        image = output / "work.exfat"
        image.write_bytes(initial)
        environment = {"SRVROS_TEST_BLOCK_BYTES": str(block_size), "SRVROS_TRACE_WRITES": "1"}
        run([str(drivers["baseline"]), str(image), "sidecar-seed"], label + "-seed", env=environment)
        seed = image.read_bytes()
        original = logical_files(seed)
        success = {}
        for implementation, driver in drivers.items():
            image.write_bytes(seed)
            process = run([str(driver), str(image), "sidecar-changed"],
                          label + "-" + implementation, env=environment)
            assert "operation=PASS" in process.stdout
            check_clean_image(image)
            run([str(driver), str(image), "allocation-check"], label + "-" + implementation + "-allocation")
            verification = run([str(driver), str(image), "sidecar-verify"], label + "-" + implementation + "-verify")
            assert "mode=416" in verification.stdout
            counts = re.search(r"writes=(\d+) flushes=(\d+) write_bytes=(\d+)", process.stdout)
            assert counts
            raw = image.read_bytes()
            success[implementation] = dict(writes=int(counts[1]), flushes=int(counts[2]),
                write_bytes=int(counts[3]), events=events(process.stdout), files=logical_files(raw),
                raw_sha256=archive(label + "-" + implementation, raw))
        before, after = success["baseline"], success["candidate"]
        assert before["files"] == after["files"]
        assert before["writes"] == after["writes"] and before["flushes"] == after["flushes"]
        assert len(before["events"]) == len(after["events"])
        shortened = []
        for old, new in zip(before["events"], after["events"]):
            if old.startswith("flush-trace:"):
                assert old == new
                continue
            old_fields = {key: int(value) for key, value in re.findall(r"(\w+)=(\d+)", old)}
            new_fields = {key: int(value) for key, value in re.findall(r"(\w+)=(\d+)", new)}
            assert old_fields["call"] == new_fields["call"] and old_fields["offset"] == new_fields["offset"]
            assert 0 < new_fields["bytes"] <= old_fields["bytes"]
            if new_fields["bytes"] < old_fields["bytes"]:
                assert new_fields["bytes"] % block_size == 0
                shortened.append(dict(before=old_fields, after=new_fields))
        assert shortened and after["write_bytes"] < before["write_bytes"]
        record["cases"].append(dict(geometry=label, result=success, shortened=shortened))
        save()

        cuts = [(kind, index, count) for kind, index, count in (
            ("write-fail", 0, before["writes"]), ("write-cut", 1, before["writes"]),
            ("flush-fail", 2, before["flushes"]), ("flush-cut", 3, before["flushes"]),
            ("tear", 4, before["writes"]))]
        for kind, index, count in cuts:
            for number in range(1, count + 1):
                pair = {}
                for implementation, driver in drivers.items():
                    name = f"{label}-{kind}-{number}-{implementation}"
                    image.write_bytes(seed)
                    numbers = [0] * 5
                    numbers[index] = number
                    process = run([str(driver), str(image), "sidecar-changed", *map(str, numbers)],
                                  name, allowed=(0, 77, 78, 79), env=environment)
                    assert process.returncode or "operation=FAIL" in process.stdout
                    raw = image.read_bytes()
                    files = logical_files(raw)
                    for path, value in original.items():
                        if not path.startswith(".srvros/"):
                            assert files[path] == value, (name, path)
                    allocation = run([str(driver), str(image), "allocation-check"],
                                     name + "-allocation", allowed=(0, 1, 2))
                    raw_hash = archive(name, raw)
                    repair = run([fsck, "-y", str(image)], name + "-repair", allowed=(0, 1))
                    repaired = image.read_bytes()
                    repaired_files = logical_files(repaired)
                    for path, value in original.items():
                        if not path.startswith(".srvros/"):
                            assert repaired_files[path] == value, (name, "repair", path)
                    after_allocation = run([str(driver), str(image), "allocation-check"],
                        name + "-after-allocation", allowed=(0, 1, 2))
                    pair[implementation] = dict(exit=process.returncode, files=files,
                        allocation_exit=allocation.returncode, allocation=allocation.stdout,
                        raw_sha256=raw_hash, repair_exit=repair.returncode,
                        repaired_files=repaired_files,
                        after_allocation_exit=after_allocation.returncode,
                        after_allocation=after_allocation.stdout,
                        repaired_sha256=archive(name + "-repaired", repaired))
                old, new = pair["baseline"], pair["candidate"]
                for key in ("exit", "files", "allocation_exit", "allocation", "repair_exit",
                            "repaired_files", "after_allocation_exit", "after_allocation"):
                    assert old[key] == new[key], (label, kind, number, key)
                for value in pair.values():
                    for key in ("files", "repaired_files"):
                        namespace = value.pop(key)
                        value[key + "_sha256"] = hashlib.sha256(
                            json.dumps(namespace, sort_keys=True).encode()).hexdigest()
                record["faults"].append(dict(geometry=label, kind=kind, number=number, results=pair))
                save()
        print("metadata-tail:", label, "PASS publication/fault/repair equivalence", flush=True)
    assert source_hashes == {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest()
                             for path in dependencies}, "Source changed during metadata-tail comparison"
    record.update(status="PASS", source_sha256=source_hashes)
    save()
    print("metadata-tail: PASS geometry, exact payload, publication/barrier sequence, "
          "all write/failure/flush/tear cuts, protected files and baseline-matched allocation/repair outcomes")


if __name__ == "__main__":
    main()
