#!/usr/bin/env python3
"""Verify boot-enumerated USB identities stay quarantined across QMP replug.

Uses a disposable disk copy; accepts an already built ISO without staging it.
This checks removal containment, not successful runtime re-enumeration.
"""
import argparse
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile

from desktop_foundation_smoke import QMP
from sysabi_smoke import connect_serial, has_fatal_exception, read_for, read_until


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iso", required=True)
    parser.add_argument("--disk", required=True)
    parser.add_argument("--qemu", default="qemu-system-x86_64")
    parser.add_argument("--terminal-stop", action="store_true", help="verify terminal xHCI halt after replug")
    parser.add_argument("--output", default="build/review/usb-removal")
    args = parser.parse_args()
    out = Path(args.output).resolve()
    out.mkdir(parents=True, exist_ok=True)
    output = bytearray()
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0))
        port = reserve.getsockname()[1]
    with tempfile.TemporaryDirectory(prefix="srvros-usb-removal-") as directory:
        tmp = Path(directory)
        disk = tmp / "work.exfat"
        shutil.copyfile(args.disk, disk)
        command = [args.qemu, "-M", "q35", "-m", "512M", "-cdrom",
                   str(Path(args.iso).resolve()), "-boot", "d",
                   "-serial", f"tcp:127.0.0.1:{port},server=on,wait=off",
                   "-qmp", f"unix:{tmp}/qmp,server=on,wait=off",
                   "-drive", f"if=none,id=work,file={disk},format=raw",
                   "-device", "ich9-ahci,id=ahci", "-device", "ide-hd,drive=work,bus=ahci.0",
                   "-device", "qemu-xhci,id=usbtest",
                   "-device", "usb-kbd,id=keyboard,bus=usbtest.0,port=1",
                   "-device", "usb-tablet,id=tablet,bus=usbtest.0,port=2",
                   "-display", "none", "-monitor", "none", "-no-reboot"]
        with (out / "qemu.log").open("w") as errors:
            process = subprocess.Popen(command, stdout=errors, stderr=errors)
            serial = qmp = None
            try:
                serial = connect_serial(port, 15)
                serial.settimeout(.2)
                qmp = QMP(tmp / "qmp")
                boot = read_until(serial, b"srv> ", 60)
                output.extend(boot)
                assert b"srv> " in boot and b"configured=2 hid_keyboards=1 hid_mice=1" in boot

                def status(configured):
                    serial.sendall(b"xhci\n")
                    part = read_until(serial, b"srv> ", 8)
                    output.extend(part)
                    devices = re.findall(rb"xhci: device slot=(\d+) [^\r\n]*configured=(\d+)", part)
                    assert sorted(devices) == [(b"1", configured), (b"2", configured)], part
                    rings = re.search(rb"cmd_events=(\d+).*mismatches=(\d+)", part)
                    assert rings and rings[2] == b"0", part
                    return rings[1]

                initial_commands = status(b"1")
                if args.terminal_stop:
                    for _ in range(2):
                        serial.sendall(b"xhcistop\n")
                        part = read_until(serial,b"srv> ",10)
                        output.extend(part)
                        assert b"xhci-stop: stopped-retained" in part, part
                    assert status(b"0") == initial_commands
                    assert b"lifecycle=stopped-retained owners=0" in output
                qmp.call("device_del", {"id": "keyboard"})
                qmp.call("device_del", {"id": "tablet"})
                output.extend(read_for(serial, 1))
                assert status(b"0") == initial_commands
                qmp.call("device_add", {"driver": "usb-kbd", "id": "replacement_keyboard",
                                        "bus": "usbtest.0", "port": "1"})
                qmp.call("device_add", {"driver": "usb-tablet", "id": "replacement_tablet",
                                        "bus": "usbtest.0", "port": "2"})
                output.extend(read_for(serial, 1))
                assert status(b"0") == initial_commands
                serial.sendall(b"run /fat/bin/sh\n")
                output.extend(read_until(serial, b" $ ", 8))
                serial.sendall(b"sysabi\n")
                result = read_until(serial, b"sysabi: ok", 30)
                output.extend(result)
                assert b"sysabi: ok" in result and b"sysabi: fail" not in result
                assert not has_fatal_exception(output.decode("utf-8", "replace"))
            finally:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
                if serial:
                    serial.close()
                if qmp:
                    qmp.stream.close()
                    qmp.sock.close()
                (out / "serial.log").write_bytes(output)
    print("usb-removal: PASS two devices removed/replaced, retained slots quarantined, no new commands, sysabi" + (", terminal halt" if args.terminal_stop else ""))


if __name__ == "__main__":
    main()
