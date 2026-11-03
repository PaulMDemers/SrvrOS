#!/usr/bin/env python3
"""Exercise no-write installed-root exFAT admission on disposable images."""
import argparse
from pathlib import Path
import subprocess
import struct

from exfat_test_image import ExfatFixture
from test_storage_leases import empty_image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--driver", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    image = (args.output / "installed-root-clean-check.exfat").resolve()
    driver = args.driver.resolve()

    def run(mode, *arguments):
        subprocess.run([str(driver), str(image), mode, *arguments], check=True)

    def check(expected):
        before = image.read_bytes()
        run("clean-check", expected)
        assert image.read_bytes() == before

    image.write_bytes(empty_image(4 * 1024 * 1024).data)
    check("accept")
    run("seed")
    check("accept")
    clean = image.read_bytes()
    run("seed-temp")
    fs = ExfatFixture(image.read_bytes())
    assert fs.read(".srvros/meta.tmp")  # A valid pending sidecar, not just a name.
    check("refuse")
    fs = ExfatFixture(clean)
    meta = dict(fs.entries(fs.read(".srvros")))["meta"]
    cluster = struct.unpack_from("<I", meta, 52)[0]
    malformed = bytearray(clean)
    malformed[fs.offset(cluster)] ^= 0xff
    image.write_bytes(malformed)
    check("refuse")
    damaged = bytearray(clean)
    damaged[12 * fs.sector + 3] ^= 1  # Invalid backup boot region.
    image.write_bytes(damaged)
    check("refuse")
    print("installed-root exfat: clean, pending, malformed and backup admission PASS")


if __name__ == "__main__":
    main()
