"""Build and stage the measured RTL8188CUS receive-only board profile."""

import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
import tarfile

MEMBER = "rtl8188cu-passive.bin"
SIZE = 2970
SHA256 = "b1e861a24ab18f03a0e643c5099bd11b756ad15da023caf688e1a9c7b096c3cb"
CAPTURE_SHA256 = "5f9db83576eddf05e3c2e3ec3fa127bedfaf1fe0f201a357ef0187198aed7904"


def checked(raw):
    if len(raw) != SIZE or hashlib.sha256(raw).hexdigest() != SHA256:
        raise ValueError("RTL8188CUS passive profile identity mismatch")
    return raw


def from_capture(path):
    with path.open("rb") as source:
        raw = source.read(2 * 1024 * 1024)
    if hashlib.sha256(raw).hexdigest() != CAPTURE_SHA256:
        raise ValueError("RTL8188CUS source capture mismatch")
    rows = json.loads(raw)["records"][2181:2692]
    settings = []
    for row in rows:
        if row["direction"] != "W" or not 0x800 <= row["register"] < 0x1000:
            continue
        if row["len"] != 4 or row["actual"] != 4 or row["status"] != 0:
            raise ValueError("Incomplete baseband control")
        settings.append(struct.pack("<HI", row["register"], row["value"]))
    return checked(b"".join(settings))


def _members(bundle):
    return [m for m in bundle.getmembers() if m.name.removeprefix("./") == MEMBER]


def add_to_initramfs(profile, archive):
    with profile.open("rb") as stream:
        raw = checked(stream.read(SIZE + 1))
    with tarfile.open(archive, "a", format=tarfile.USTAR_FORMAT) as bundle:
        if _members(bundle):
            raise ValueError("RTL8188CUS passive profile already exists")
        info = tarfile.TarInfo(MEMBER)
        info.size = len(raw)
        info.mode = 0o444
        bundle.addfile(info, io.BytesIO(raw))


def verify_initramfs(archive):
    with tarfile.open(archive, "r:") as bundle:
        members = _members(bundle)
        if len(members) != 1 or not members[0].isfile() or members[0].size != SIZE:
            raise ValueError("Expected one exact RTL8188CUS passive profile")
        with bundle.extractfile(members[0]) as stream:
            checked(stream.read(SIZE + 1))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = from_capture(args.capture)
    with args.output.open("xb") as output:
        output.write(data)
