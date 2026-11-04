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
    parser.add_argument("--retire-slots", action="store_true", help="disable each boot slot before terminal stop/replug")
    parser.add_argument("--reclaim", action="store_true", help="retire references and reclaim DMA after terminal halt")
    parser.add_argument("--usb-storage", action="store_true", help="attach a disposable second disk copy as USB storage")
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
        if args.usb_storage:
            stick=tmp/"usb.exfat";shutil.copyfile(args.disk,stick)
            command += ["-drive",f"if=none,id=stick,file={stick},format=raw",
                        "-device","usb-storage,id=storage,drive=stick,bus=usbtest.0,port=3"]
        with (out / "qemu.log").open("w") as errors:
            process = subprocess.Popen(command, stdout=errors, stderr=errors)
            serial = qmp = None
            try:
                serial = connect_serial(port, 15)
                serial.settimeout(.2)
                qmp = QMP(tmp / "qmp")
                boot = read_until(serial, b"srv> ", 60)
                output.extend(boot)
                count=3 if args.usb_storage else 2
                expected=f"configured={count} hid_keyboards=1 hid_mice=1".encode()
                assert b"srv> " in boot and expected in boot

                def status(configured):
                    serial.sendall(b"xhci\n")
                    part = read_until(serial, b"srv> ", 8)
                    output.extend(part)
                    devices = re.findall(rb"xhci: device slot=(\d+) [^\r\n]*configured=(\d+)", part)
                    assert sorted(devices) == [(str(i).encode(),configured) for i in range(1,count+1)],part
                    rings = re.search(rb"cmd_events=(\d+).*mismatches=(\d+)", part)
                    assert rings and rings[2] == b"0", part
                    return rings[1]

                initial_commands = status(b"1")
                if args.retire_slots:
                    for slot in range(count,0,-1):
                        for _ in range(2):
                            serial.sendall(f"xhcislot {slot}\n".encode())
                            part=read_until(serial,b"srv> ",10);output.extend(part)
                            assert b"xhci-slot: disabled-retained" in part,part
                    after=status(b"0")
                    assert int(after)==int(initial_commands)+count
                    initial_commands=after
                if args.terminal_stop or args.reclaim:
                    for _ in range(2):
                        serial.sendall(b"xhcistop\n")
                        part = read_until(serial,b"srv> ",10)
                        output.extend(part)
                        assert b"xhci-stop: stopped-retained" in part, part
                    assert status(b"0") == initial_commands
                    assert b"lifecycle=stopped-retained owners=0" in output
                if args.reclaim:
                    for _ in range(256):
                        serial.sendall(b"xhcireclaim\n")
                        part=read_until(serial,b"srv> ",10);output.extend(part)
                        if b"xhci-reclaim: reclaimed-tombstones" in part:break
                        assert b"xhci-reclaim: pending-or-unproven" in part,part
                    else:raise AssertionError("xHCI references or hardware did not retire")
                    serial.sendall(b"xhcireclaim\n")
                    part=read_until(serial,b"srv> ",10);output.extend(part)
                    assert b"xhci-reclaim: reclaimed-tombstones" in part,part
                    serial.sendall(b"xhci\n")
                    part=read_until(serial,b"srv> ",10);output.extend(part)
                    assert b"lifecycle=reclaimed-tombstones" in part and b"worker_retired=1" in part,part
                    match=re.search(rb"reclaim frames=(\d+)",part)
                    assert match and int(match[1])>0,part
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
    print("usb-removal: PASS two devices removed/replaced, retained slots quarantined, no new commands, sysabi" + (", terminal halt" if args.terminal_stop or args.reclaim else "") + (", per-slot disable" if args.retire_slots else "") + (", DMA reclaimed" if args.reclaim else ""))


if __name__ == "__main__":
    main()
