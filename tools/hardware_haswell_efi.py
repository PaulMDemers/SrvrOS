"""Decode a seeded pre-ExitBootServices Haswell graphics inventory."""

import argparse
import json
from pathlib import Path
import re
import zlib

PREFIX = "SRVROS-HSW-EFI-V1"
ESP_NAME = "HSW-EFI.LOG"
CAPACITY = 4096
PCI_IO = "4cf5b200-68b8-4ca5-9eec-b23e3f50029a"
GOP = "9042a9de-23dc-4a38-96fb-7aded080516a"
GUID = r"[0-9a-f]{8}-(?:[0-9a-f]{4}-){3}[0-9a-f]{12}"


def seed(release, nonce):
    if not re.fullmatch(r"[0-9a-f]{16}", release) or not re.fullmatch(r"[0-9a-f]{32}", nonce):
        raise ValueError("Invalid EFI inventory release or nonce")
    return f"{PREFIX} {release} {nonce}\n".encode().ljust(CAPACITY, b"\0")


def decode(raw, release, nonce):
    expected = seed(release, nonce)
    header = expected.rstrip(b"\0")
    if len(raw) != CAPACITY or not raw.startswith(header):
        raise ValueError("Invalid EFI inventory size or trial identity")
    payload = raw[len(header):].rstrip(b"\0")
    body, marker, checksum = payload.rpartition(b"end ")
    if not marker or not re.fullmatch(rb"[0-9a-f]{8}\n", checksum) or zlib.crc32(body) != int(checksum, 16):
        raise ValueError("Incomplete EFI inventory or invalid checksum")
    rows = body.decode("ascii").splitlines()
    result = {"release": release, "nonce": nonce, "firmware_exclusion": "unverified"}
    if rows == ["absent"]:
        return {**result, "status": "absent"}
    if len(rows) == 1 and (row := re.fullmatch(r"unavailable ([A-Za-z][A-Za-z0-9]*)", rows[0])):
        return {**result, "status": "unavailable", "reason": row[1]}

    first = re.fullmatch(r"igd ([1-9a-f][0-9a-f]{0,15})", rows[0]) if rows else None
    last = re.fullmatch(r"complete (\d{1,2}) (\d{1,2})", rows[-1]) if len(rows) >= 2 else None
    if not first or not last:
        raise ValueError("Missing EFI inventory begin or completion")
    aperture, count, gops = int(first[1], 16), int(last[1]), int(last[2])
    if aperture & 0xfffffff or not 1 <= count <= 24 or gops > 16:
        raise ValueError("Out-of-range EFI inventory summary")

    protocols = {index: [] for index in range(1, count + 1)}
    opens, modes, images = [], {}, {}
    for row in rows[1:-1]:
        if item := re.fullmatch(rf"protocol (\d{{1,2}}) ({GUID})", row):
            index, guid = int(item[1]), item[2]
            if index not in protocols or guid in protocols[index] or len(protocols[index]) >= 32:
                raise ValueError("Unexpected handle protocol")
            protocols[index].append(guid)
        elif item := re.fullmatch(rf"open ({GUID}) (\d{{1,2}}) (\d{{1,2}}) (8|10|30) (\d{{1,10}})", row):
            guid, agent, child = item[1], int(item[2]), int(item[3])
            attributes, open_count = int(item[4], 16), int(item[5])
            if agent not in protocols or child not in protocols or not 1 <= open_count <= 0xffffffff:
                raise ValueError("Unexpected EFI driver relationship")
            opens.append(dict(protocol=guid, agent=agent, controller=child,
                              attributes=attributes, count=open_count))
        elif item := re.fullmatch(r"gop (\d{1,2}) ([0-9a-f]{1,16}) ([0-9a-f]{1,8}) (\d{4}) ([01])", row):
            index, address, length, stride, console = int(item[1]), int(item[2], 16), int(item[3], 16), int(item[4]), int(item[5])
            if (index not in protocols or index in modes or not 1366 <= stride <= 8192 or
                    not 0 <= address - aperture < 0x10000000 or
                    not 0 < length <= 0x10000000 - (address - aperture)):
                raise ValueError("Unexpected EFI panel mode")
            modes[index] = dict(base=address, size=length, stride=stride, console=bool(console))
        elif item := re.fullmatch(rf"image (\d{{1,2}}) ({GUID})", row):
            index = int(item[1])
            if index == 1 or index not in protocols or index in images:
                raise ValueError("Unexpected EFI driver image")
            images[index] = item[2]
        else:
            raise ValueError("Unknown EFI inventory row")

    open_keys = [(record["protocol"], record["agent"], record["controller"], record["attributes"]) for record in opens]
    if (not all(protocols.values()) or PCI_IO not in protocols[1] or len(modes) != gops or
            any(GOP not in protocols[index] for index in modes) or
            sum(mode["console"] for mode in modes.values()) > 1 or
            len(set(open_keys)) != len(open_keys) or
            any(record["protocol"] not in protocols[1] or
                sum(other["protocol"] == record["protocol"] for other in opens) > 16
                for record in opens)):
        raise ValueError("Inconsistent EFI inventory")
    return {**result, "status": "observed", "aperture": aperture, "protocols": protocols,
            "modes": modes, "opens": opens, "images": images}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("receipt", type=Path)
    parser.add_argument("release")
    parser.add_argument("nonce")
    args = parser.parse_args()
    print(json.dumps(decode(args.receipt.read_bytes(), args.release, args.nonce), indent=2))
