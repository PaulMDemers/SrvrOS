"""Extract and stage the measured Netgear A6150 RFE type 3 PHY table writes."""

import argparse
import hashlib
import io
from pathlib import Path
import struct
import tarfile

MEMBER = "rtl8822bu-phy.bin"
SIZE = 2891 * 7
SHA256 = "85c0c86ea01958a697b7c7ca162fe1b95e89013b67def1377b5fcaa94436224c"
TRACE_SHA256 = "327a6129dfd3c78eed1a4bf949811f33f27fc7e47f19b27f521ddd6302780786"


def checked(data):
    if len(data) != SIZE or hashlib.sha256(data).hexdigest() != SHA256:
        raise ValueError("RTL8822BU PHY profile identity mismatch")
    return data


def from_trace(path):
    with path.open("rb") as stream:
        raw = stream.read(4 * 1024 * 1024)
    if hashlib.sha256(raw).hexdigest() != TRACE_SHA256:
        raise ValueError("RTL8822BU PHY source trace mismatch")
    # Between the initial RXPSEL clear and the XTAL trim, inclusive of both
    # RF tables. USB echo transfers have already been removed by the decoder.
    rows = []
    for line in raw.splitlines()[3942:6833]:
        _, _, direction, request, address, width, status, payload = line.split()
        reg, index = address.split(b":")
        width = int(width)
        data = bytes.fromhex(payload.decode("ascii"))
        if (direction, request, index, status) != (b"W", b"40:05", b"0000", b"0") or len(data) != width:
            raise ValueError("Unexpected RTL8822BU control transfer in PHY tables")
        rows.append(struct.pack("<HBI", int(reg, 16), width, int.from_bytes(data, "little")))
    return checked(b"".join(rows))


def _members(bundle):
    return [item for item in bundle.getmembers() if item.name.removeprefix("./") == MEMBER]


def add_to_initramfs(profile, archive):
    with profile.open("rb") as stream:
        data = checked(stream.read(SIZE + 1))
    with tarfile.open(archive, "a", format=tarfile.USTAR_FORMAT) as bundle:
        if _members(bundle):
            raise ValueError("RTL8822BU PHY profile already exists")
        info = tarfile.TarInfo(MEMBER)
        info.mode = 0o444
        info.size = len(data)
        bundle.addfile(info, io.BytesIO(data))


def verify_initramfs(archive):
    with tarfile.open(archive) as bundle:
        members = _members(bundle)
        if len(members) != 1 or not members[0].isfile() or members[0].size != SIZE:
            raise ValueError("Expected one exact RTL8822BU PHY profile")
        with bundle.extractfile(members[0]) as stream:
            checked(stream.read(SIZE + 1))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    profile = from_trace(args.trace)
    with args.output.open("xb") as stream:
        stream.write(profile)
