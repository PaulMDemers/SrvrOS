"""Stage a single private Demers2.4 PMK with its measured station and BSSID."""
import io
import os
from pathlib import Path
import stat
import tarfile

from wifi_wpa5_credentials import derive

NAME = "cfg/rtl8188cu-wpa2.bin"
MAGIC = b"SRCUWPA\x01"
NETGEAR_NAME = "cfg/rtl8822bu-wpa2.bin"
NETGEAR_MAGIC = b"SRBUWPA\x01"


def validate(blob, *, netgear=False):
    if len(blob) != 52 or blob[:8] != (NETGEAR_MAGIC if netgear else MAGIC):
        raise ValueError("Invalid Realtek station record")
    station, bssid = blob[8:14], blob[14:20]
    if not all(any(address) and not address[0] & 1 for address in (station, bssid)) or station == bssid:
        raise ValueError("Invalid Realtek station binding")
    return blob


def provision(path, credentials, station, bssid, *, netgear=False):
    pmk = derive(credentials, "Demers2.4")[41:]
    magic = NETGEAR_MAGIC if netgear else MAGIC
    data = validate(magic + bytes.fromhex(station.replace(":", "")) +
                    bytes.fromhex(bssid.replace(":", "")) + pmk, netgear=netgear)
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    with os.fdopen(fd, "wb") as output:
        output.write(data)


def verify_initramfs(archive, required, *, netgear=False):
    name = NETGEAR_NAME if netgear else NAME
    with tarfile.open(archive, "r:") as bundle:
        entries = [item for item in bundle.getmembers() if item.name.lstrip("./") == name]
        if not required:
            if entries:
                raise ValueError("Unexpected private Realtek credential")
            return
        if len(entries) != 1:
            raise ValueError("Missing or duplicate Realtek credential")
        entry = entries[0]
        if entry.name != name or not entry.isfile() or entry.mode != 0o600 or entry.size != 52:
            raise ValueError("Invalid Realtek credential tar entry")
        validate(bundle.extractfile(entry).read(53), netgear=netgear)


def add_to_initramfs(record, archive, *, netgear=False):
    fd = os.open(Path(record), os.O_RDONLY | os.O_NOFOLLOW)
    with os.fdopen(fd, "rb") as source:
        state = os.fstat(source.fileno())
        if not stat.S_ISREG(state.st_mode) or stat.S_IMODE(state.st_mode) != 0o600 or state.st_size != 52:
            raise ValueError("Realtek station record must be a regular 0600 file")
        data = validate(source.read(53), netgear=netgear)
    verify_initramfs(archive, False, netgear=netgear)
    with tarfile.open(archive, "a", format=tarfile.USTAR_FORMAT) as bundle:
        entry = tarfile.TarInfo(NETGEAR_NAME if netgear else NAME)
        entry.size = len(data)
        entry.mode = 0o600
        bundle.addfile(entry, io.BytesIO(data))
    os.chmod(archive, 0o600)
