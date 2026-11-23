"""Stage the pinned RTL8188CUS TSMC image and its linux-firmware license."""

import hashlib
import io
import tarfile


FILES = {
    "rtl8192cufw_TMSC.bin": (16126, "c083f2db36ef33d0ffffc979aba1d40f55fa9dc38dce50f26e0108a21c080286"),
    "LICENCE.rtlwifi_firmware.txt": (2115, "a61351665b4f264f6c631364f85b907d8f8f41f8b369533ef4021765f9f3b62e"),
}


def _checked(name, stream):
    length, digest = FILES[name]
    data = stream.read(length + 1)
    if len(data) != length or hashlib.sha256(data).hexdigest() != digest:
        raise ValueError(f"RTL8188CUS firmware package mismatch: {name}")
    return data


def _firmware_members(archive):
    return [member for member in archive.getmembers() if member.name.lstrip("./").split("/")[0] == "firmware"]


def add_to_initramfs(directory, archive):
    items = {}
    for name in FILES:
        with (directory / name).open("rb") as stream:
            items[name] = _checked(name, stream)
    with tarfile.open(archive, "a", format=tarfile.USTAR_FORMAT) as bundle:
        if _firmware_members(bundle):
            raise ValueError("Initramfs already contains firmware")
        for name, content in items.items():
            info = tarfile.TarInfo("firmware/" + name)
            info.size = len(content)
            info.mode = 0o444
            bundle.addfile(info, io.BytesIO(content))


def verify_initramfs(archive):
    with tarfile.open(archive, "r:") as bundle:
        members = _firmware_members(bundle)
        if sorted(m.name for m in members) != sorted("firmware/" + name for name in FILES):
            raise ValueError("Unexpected or duplicate RTL8188CUS firmware members")
        for member in members:
            name = member.name.removeprefix("firmware/")
            if not member.isfile() or member.size != FILES[name][0]:
                raise ValueError(f"Unexpected RTL8188CUS firmware member type or size: {name}")
            with bundle.extractfile(member) as stream:
                _checked(name, stream)
