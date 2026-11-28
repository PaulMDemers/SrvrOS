#!/usr/bin/env python3
"""Prepare a private BCM4331 boot record with a PMK and fresh host entropy."""

import argparse
import os
from pathlib import Path
import stat

from wifi_wpa5_credentials import derive, validate


SSID = "Demers2.4"
ARCHIVE_NAME = "cfg/bcm4331-station.key"
SIZE = 105


def validate_station_key(record):
    if len(record) != SIZE or not any(record[73:]):
        raise ValueError("Invalid BCM4331 station key record")
    validate(record[:73])
    if record[9:9 + record[8]] != SSID.encode("ascii"):
        raise ValueError("BCM4331 station key is for a different network")
    return record


def read_station_key(path):
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    with os.fdopen(fd, "rb") as stream:
        st = os.fstat(stream.fileno())
        if not stat.S_ISREG(st.st_mode) or stat.S_IMODE(st.st_mode) != 0o600 or st.st_size != SIZE:
            raise ValueError("BCM4331 station key requires a regular 0600 record")
        return validate_station_key(stream.read(SIZE + 1))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("credentials", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        record = validate_station_key(derive(args.credentials, SSID) + os.getrandom(32))
        fd = os.open(args.output, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
        with os.fdopen(fd, "wb") as stream:
            stream.write(record)
    except (OSError, ValueError):
        parser.exit(1, "BCM4331 station key provisioning failed\n")


if __name__ == "__main__":
    main()
