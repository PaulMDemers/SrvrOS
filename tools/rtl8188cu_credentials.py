"""Stage a single private Demers2.4 PMK with its measured station and BSSID."""
import io
import os
from pathlib import Path
import stat
import tarfile

from wifi_wpa5_credentials import derive

NAME = "cfg/rtl8188cu-wpa2.bin"
MAGIC = b"SRCUWPA\x01"


def validate(blob):
    if len(blob) != 52 or blob[:8] != MAGIC:
        raise ValueError("Invalid Realtek station record")
    station, bssid = blob[8:14], blob[14:20]
    if not all(any(address) and not address[0] & 1 for address in (station, bssid)) or station == bssid:
        raise ValueError("Invalid Realtek station binding")
    return blob


def provision(path, credentials, station, bssid):
    pmk = derive(credentials, "Demers2.4")[41:]
    data = validate(MAGIC + bytes.fromhex(station.replace(":", "")) +
                    bytes.fromhex(bssid.replace(":", "")) + pmk)
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    with os.fdopen(fd, "wb") as output:
        output.write(data)


def verify_initramfs(archive, required):
    with tarfile.open(archive, "r:") as bundle:
        entries = [item for item in bundle.getmembers() if item.name.lstrip("./") == NAME]
        if not required:
            if entries:
                raise ValueError("Unexpected private Realtek credential")
            return
        if len(entries) != 1:
            raise ValueError("Missing or duplicate Realtek credential")
        entry = entries[0]
        if entry.name != NAME or not entry.isfile() or entry.mode != 0o600 or entry.size != 52:
            raise ValueError("Invalid Realtek credential tar entry")
        validate(bundle.extractfile(entry).read(53))


def add_to_initramfs(record, archive):
    fd = os.open(Path(record), os.O_RDONLY | os.O_NOFOLLOW)
    with os.fdopen(fd, "rb") as source:
        state = os.fstat(source.fileno())
        if not stat.S_ISREG(state.st_mode) or stat.S_IMODE(state.st_mode) != 0o600 or state.st_size != 52:
            raise ValueError("Realtek station record must be a regular 0600 file")
        data = validate(source.read(53))
    verify_initramfs(archive, False)
    with tarfile.open(archive, "a", format=tarfile.USTAR_FORMAT) as bundle:
        entry = tarfile.TarInfo(NAME)
        entry.size = len(data)
        entry.mode = 0o600
        bundle.addfile(entry, io.BytesIO(data))
    os.chmod(archive, 0o600)
