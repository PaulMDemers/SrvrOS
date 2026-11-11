#!/usr/bin/env python3
"""Complete populated screenshot save with actual process/VFS/exFAT code.

Independent CPU0 turns, advancing time and block callbacks are host models.
The same 65 writes, foreign barriers, marker updates and full proofs run in
both padding implementations. Source I/O excludes extra test-only readback.
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
import struct
import subprocess

from exfat_test_image import ExfatFixture, check_clean_image
from test_storage_leases import empty_image

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ["tools/test_populated_save_driver.c", "tools/test_populated_save_process.c",
           "kernel/src/fs/exfat.c", "kernel/src/vfs.c", "kernel/src/vfs_namespace.c",
           "kernel/src/autotest_storage.c", "kernel/src/file_verify.c"]


def populated_seed():
    fixture = empty_image(32 * 1024 * 1024, cluster_shift=8)
    files = {"other.txt": b"", "anchor.txt": b"unchanged"}
    files.update({f"home-fixture-{index:04d}-" + "x" * 48 + ".txt": b""
                  for index in range(829)})
    fixture.add_directory_files("work", files)
    metadata = "SRVROSMETA1\n" + "".join(
        f"/vol/work/{name}\t{10000 + index}\t33188\t1\t0\t0\t"
        "1792326896\t1792326896\t1792326896\n"
        for index, name in enumerate(files))
    fixture.add_directory_files(".srvros", {"meta": metadata.encode()})
    # Freed allocation slack can contain old bytes on real media.
    root = fixture.chain(fixture.root)
    bitmap_cluster = next(struct.unpack_from("<I", root, offset + 20)[0]
                          for offset in range(0, len(root), 32) if root[offset] == 0x81)
    bitmap = fixture.offset(bitmap_cluster)
    for cluster in range(2, fixture.count + 2):
        bit = cluster - 2
        if not fixture.data[bitmap + bit // 8] & (1 << (bit % 8)):
            offset = fixture.offset(cluster)
            fixture.data[offset:offset + fixture.cluster] = b"\xa5" * fixture.cluster
    return bytes(fixture.data)


def logical_files(raw):
    fixture = ExfatFixture(raw)
    files = {}

    def walk(directory, prefix):
        for name, entry in fixture.entries(directory):
            path = prefix + name
            first = struct.unpack_from("<I", entry, 52)[0]
            size = struct.unpack_from("<Q", entry, 56)[0]
            data = fixture.chain(first, size, bool(entry[33] & 2))
            if struct.unpack_from("<H", entry, 4)[0] & 0x10:
                walk(data, path + "/")
            else:
                files[path] = dict(size=size, sha256=hashlib.sha256(data).hexdigest())

    walk(fixture.chain(fixture.root), "")
    return files


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "build/review/populated-save")
    parser.add_argument("--limine-include", type=Path, default=ROOT / "build/limine-protocol/include")
    parser.add_argument("--compare-nofat", action="store_true", help="Compare lazy invalid FAT against the exact eager FAT work")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    dependencies = SOURCES + ["tools/test_populated_save.py", "tools/test_reserved_append_process.c",
        "tools/test_reserved_append_driver.c", "tools/test_reserved_append_io.h",
        "tools/test_sync_content_bridge.inc", "tools/exfat_host_driver.c",
        "kernel/src/process.c", "kernel/src/sync_timing.c"]
    source_hashes = {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest()
                     for path in dependencies}
    result = dict(status="RUNNING", scope=__doc__, commands=[], cases={})
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")

    def run(command, name):
        process = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
        (output / (name + ".log")).write_text(process.stdout + process.stderr)
        assert process.returncode == 0, (name, process.returncode, process.stderr)
        return process.stdout

    drivers = {}
    for implementation in ("baseline", "candidate"):
        command = shlex.split(os.environ.get("CC", "cc"))
        if platform.system() == "Darwin":
            command += ["-arch", "x86_64"]
        command += ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                    "-ffunction-sections", "-fdata-sections",
                    "-idirafter", str(ROOT / "userspace/lib/include"),
                    "-I", str(ROOT / "shared/include"), "-I", str(ROOT / "kernel/include"),
                    "-I", str(args.limine_include.resolve()), "-DSRVROS_EXFAT_HOST_TEST",
                    "-DSRVROS_EXFAT_COST_TEST", "-DSRVROS_SYNC_CONTENT_HOST_TEST"]
        if implementation == "baseline":
            command += ["-DSRVROS_EXFAT_EAGER_NOFAT" if args.compare_nofat else "-DSRVROS_EXFAT_METADATA_TAIL_OFF"]
        binary = output / implementation
        command += [str(ROOT / source) for source in SOURCES]
        command += ["-Wl,-dead_strip" if platform.system() == "Darwin" else "-Wl,--gc-sections",
                    "-o", str(binary)]
        run(command, implementation + "-build")
        result["commands"].append(command)
        drivers[implementation] = binary

    seed = populated_seed()
    expected = bytes((index * 37 + 11) & 255 for index in range(1366 * 768 * 4 + 54))
    result["seed_sha256"] = hashlib.sha256(seed).hexdigest()
    for mode in ("baseline", "sequential", "overlap"):
        pair = {}
        for implementation, binary in drivers.items():
            name = implementation + "-" + mode
            image = output / (name + ".exfat")
            image.write_bytes(seed)
            text = run([str(binary), str(image), mode], name)
            summary, = [line for line in text.splitlines() if line.startswith("reserved-result:")]
            fields = {key: int(value) for key, value in re.findall(r"(\w+)=(\d+)", summary)}
            assert fields["writes"] == fields["foreign_syncs"] == 65
            assert fields["bytes"] == len(expected) and fields["fresh_verifications"] == 2
            assert fields["allocation_clean"] == 1
            profiles = [{key: int(value) for key, value in re.findall(r"(\w+)=(\d+)", line)}
                        for line in text.splitlines() if line.startswith("save-profile:")]
            assert [(row["phase"], row["role"]) for row in profiles] == [
                (phase, role) for phase in range(2) for role in range(7)]
            validation, = [line for line in text.splitlines() if line.startswith("save-validation:")]
            assert int(re.search(r"read_bytes=(\d+)", validation)[1]) > 0
            raw = image.read_bytes()
            check_clean_image(image)
            media = ExfatFixture(raw)
            assert media.read("work/capture.bmp") == expected
            assert media.read("work/anchor.txt") == b"unchanged"
            assert media.read("work/other.txt") == b"A" * 17
            files = logical_files(raw)
            for path, info in logical_files(seed).items():
                if path not in (".srvros/meta", "work/other.txt"):
                    assert files[path] == info, path
            archive = output / (name + ".exfat.gz")
            archive.write_bytes(gzip.compress(raw, mtime=0))
            assert gzip.decompress(archive.read_bytes()) == raw
            pair[implementation] = dict(summary=fields, roles=profiles, files=files,
                raw_sha256=hashlib.sha256(raw).hexdigest(),
                archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest())
            image.unlink()
        old, new = pair["baseline"], pair["candidate"]
        assert old["summary"] == new["summary"] and old["files"] == new["files"]
        for before, after in zip(old["roles"], new["roles"]):
            assert before.keys() == after.keys()
            for key in before:
                if args.compare_nofat and before["role"] in (0, 1) and key in ("callbacks", "writes", "write_bytes"):
                    # Required terminal creation moves from allocation into
                    # append preparation. Compare total write work below;
                    # every read/proof/flush and non-FAT role stays identical.
                    if before["role"] == 1:
                        assert 0 <= after[key] <= before[key], (mode, key, before, after)
                    if key == "callbacks":
                        assert before[key]-after[key] == before["writes"]-after["writes"]
                elif not args.compare_nofat and before["role"] == 4 and key == "write_bytes":
                    assert 0 < after[key] < before[key]
                else:
                    assert before[key] == after[key], (mode, key, before, after)
        pair["whole_save_write_bytes_saved"] = sum(
            before["write_bytes"] - after["write_bytes"]
            for before, after in zip(old["roles"], new["roles"]) if before["phase"] == 0)
        assert pair["whole_save_write_bytes_saved"] > 0
        pair["whole_save_write_callbacks_saved"] = sum(
            before["writes"] - after["writes"]
            for before, after in zip(old["roles"], new["roles"]) if before["phase"] == 0)
        if args.compare_nofat:
            assert pair["whole_save_write_callbacks_saved"] > 0
        result["cases"][mode] = pair
        print("populated-save:", mode, "PASS bytes saved", pair["whole_save_write_bytes_saved"], flush=True)

    assert source_hashes == {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest()
                             for path in dependencies}, "Source changed during populated replay"
    result.update(status="PASS", source_sha256=source_hashes)
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("populated-save: PASS complete65-write/65-barrier controls, exact logical media, "
          "unchanged prefix/read/barrier work and lower FAT callbacks" if args.compare_nofat else
          "unchanged prefix/read/write-count/barrier work and lower metadata write bytes")


if __name__ == "__main__":
    main()
