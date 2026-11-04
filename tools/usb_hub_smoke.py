#!/usr/bin/env python3
"""Prove downstream hub removal containment and explicit EP0 STALL recovery.

Copies the source disk and stops its disposable Q35 guest on every exit.
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
    parser.add_argument("--nested", action="store_true", help="place keyboard behind two full-speed hubs")
    parser.add_argument("--interrupt-recovery", action="store_true", help="stop a root HID endpoint and verify fresh dequeue/input")
    parser.add_argument("--output", default="build/review/usb-hub")
    args = parser.parse_args()
    out = Path(args.output).resolve()
    out.mkdir(parents=True, exist_ok=True)
    output = bytearray()
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0))
        port = reserve.getsockname()[1]
    with tempfile.TemporaryDirectory(prefix="srvros-usb-hub-") as directory:
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
                   "-device", "usb-hub,id=hub,bus=usbtest.0,port=1",
                   *(["-device","usb-hub,id=inner,bus=usbtest.0,port=1.1"] if args.nested else []),
                   "-device", f"usb-kbd,id=keyboard,bus=usbtest.0,port={'1.1.1' if args.nested else '1.1'}",
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
                count=4 if args.nested else 3
                assert b"srv> " in boot and f"configured={count} hid_keyboards=1 hid_mice=1".encode() in boot

                def command(text):
                    serial.sendall(text.encode()+b"\n")
                    part=read_until(serial,b"srv> ",10);output.extend(part)
                    assert b"srv> " in part,part
                    return part

                def status():
                    part=command("xhci")
                    records={}
                    for line in part.splitlines():
                        if line.startswith(b"xhci: device slot="):
                            record={key.decode():int(value) for key,value in
                                    re.findall(rb"(slot|parent|hub_port|configured|keyboard|mouse|mouse_reports)=(\d+)",line)}
                            records[record["slot"]]=record
                    assert len(records)==count,part
                    rings=re.search(rb"cmd_events=(\d+).*mismatches=(\d+)",part)
                    assert rings and rings[2]==b"0",part
                    return records,int(rings[1])

                records,before=status()
                keyboard=next(v["slot"] for v in records.values() if v["keyboard"])
                tablet=next(v["slot"] for v in records.values() if v["mouse"])
                hub=records[keyboard]["parent"]
                assert hub and records[keyboard]["hub_port"]==1 and not records[tablet]["parent"]
                assert b"xhci-ep0-check: ok" in command(f"xhciep0check {tablet}")
                records,after=status();assert after==before+2 and all(v["configured"] for v in records.values())
                if args.nested:assert records[hub]["parent"] and not records[records[hub]["parent"]]["parent"]
                if args.interrupt_recovery:
                    reports=records[tablet]["mouse_reports"]
                    assert b"xhci-intr-check: ok" in command(f"xhciintrcheck {tablet}")
                    assert b"xhci-endpoint: refused" in command(f"xhcireset {tablet} 3")
                    assert b"usage: xhcireset" in command("xhcireset 0 3")
                    qmp.call("input-send-event", {"events":[
                        {"type":"abs","data":{"axis":"x","value":16000}},
                        {"type":"abs","data":{"axis":"y","value":20000}},
                        {"type":"btn","data":{"down":True,"button":"left"}}]})
                    qmp.call("input-send-event", {"events":[{"type":"btn","data":{"down":False,"button":"left"}}]})
                    output.extend(read_for(serial,1))
                    records,current=status()
                    assert current==after+2 and records[tablet]["mouse_reports"]>reports
                    after=current
                qmp.call("device_del", {"id":"keyboard"})
                output.extend(read_for(serial,2))
                records,current=status()
                assert current==after and not records[keyboard]["configured"]
                assert records[hub]["configured"] and records[tablet]["configured"]
                qmp.call("device_add", {"driver":"usb-kbd","id":"replacement_keyboard",
                                        "bus":"usbtest.0","port":"1.1.1" if args.nested else "1.1"})
                output.extend(read_for(serial,1))
                records,current=status()
                assert current==after and not records[keyboard]["configured"]
                assert records[hub]["configured"] and records[tablet]["configured"]
                # Parent request makes one child-first command of progress per call.
                first=command(f"xhcislot {hub}")
                assert b"xhci-slot: pending-or-unproven" in first,first
                assert b"xhci-slot: disabled-retained" in command(f"xhcislot {hub}")
                records,current=status()
                assert current==after+2 and records[tablet]["configured"]
                assert not records[hub]["configured"] and not records[keyboard]["configured"]
                assert b"xhci-stop: stopped-retained" in command("xhcistop")
                for _ in range(256):
                    if b"xhci-reclaim: reclaimed-tombstones" in command("xhcireclaim"):break
                else:raise AssertionError("xHCI references or hardware did not retire")
                assert b"xhci-reclaim: reclaimed-tombstones" in command("xhcireclaim")
                final=command("xhci")
                assert b"lifecycle=reclaimed-tombstones" in final and b"worker_retired=1" in final
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
    print("usb-hub: PASS downstream removal/replacement containment, sibling survival, EP0 fresh-dequeue recovery, child-first disable, terminal reclaim and sysabi" + (", nested hubs" if args.nested else "") + (", interrupt stopped/dequeue recovery and input" if args.interrupt_recovery else ""))


if __name__ == "__main__":
    main()
