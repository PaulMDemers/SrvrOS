"""Stage the pinned RTL8822BU firmware and its redistribution license."""

import rtl8188cu_firmware as package

FILES = {
    "rtw8822b_fw.bin": (161240, "a72da690597bfa99d8eb6fc2ab090d18d8ad92ac2befd35db1c9e3662d8d8418"),
    "LICENCE.rtlwifi_firmware.txt": package.FILES["LICENCE.rtlwifi_firmware.txt"],
}


def add_to_initramfs(directory, archive):
    package.add_to_initramfs(directory, archive, files=FILES)


def verify_initramfs(archive):
    package.verify_initramfs(archive, files=FILES)
