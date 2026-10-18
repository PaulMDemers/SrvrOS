#!/usr/bin/env python3
"""Exercise the shared table and live System app in a disposable VM."""
import argparse
import os
import re
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from desktop_foundation_smoke import QMP


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--network",action="store_true",help="Exercise e1000 DHCP and live address/counter display")
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    out=root/("build/review/desktop-system-network" if args.network else "build/review/desktop-system")
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-system-") as directory:
        tmp=Path(directory)
        shutil.copyfile(root/"build/srvros.exfat",tmp/"disk.exfat")
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy())
        command=["qemu-system-x86_64","-M","q35","-m","512M","-cdrom",str(iso),"-boot","d",
            "-drive",f"if=none,id=disk,file={tmp/'disk.exfat'},format=raw",
            "-device","ich9-ahci,id=ahci","-device","ide-hd,drive=disk,bus=ahci.0",
            "-device","qemu-xhci,id=xhci","-device","usb-kbd,bus=xhci.0",
            "-serial",f"tcp:127.0.0.1:{port},server=on,wait=off",
            "-qmp",f"unix:{tmp/'qmp'},server=on,wait=off","-display","none","-monitor","none","-no-reboot","-rtc","base=2026-10-18T12:34:00,clock=vm"]
        command += ["-netdev","user,id=net0","-device","e1000,netdev=net0"] if args.network else ["-nic","none"]
        with (out/"qemu.log").open("wb") as errors:
            process=subprocess.Popen(command,stdout=errors,stderr=errors)
            try:
                serial=connect_serial(port,20);serial.settimeout(.2);qmp=QMP(tmp/"qmp")
                def wait(marker,timeout=40):
                    nonlocal output,consumed
                    end=time.monotonic()+timeout
                    while marker not in output[consumed:]:
                        if marker == b"srv> " and not consumed:
                            qmp.boot_if_menu_stalled(output, time.monotonic() - (end - timeout))
                        if time.monotonic()>end or process.poll() is not None:
                            qmp.call("screendump",{"filename":str(out/"failure.ppm")})
                            raise RuntimeError(f"Missing {marker!r}; see {out}")
                        try:
                            chunk=serial.recv(16384)
                            if not chunk: raise RuntimeError("serial disconnected")
                            output+=chunk
                        except socket.timeout: pass
                    consumed=output.index(marker,consumed)+len(marker)
                def wait_all(markers, timeout=40):
                    nonlocal consumed
                    start=consumed;end=start
                    for marker in markers:
                        consumed=start;wait(marker,timeout);end=max(end,consumed)
                    consumed=end
                def shortcut(keys):
                    qmp.call("human-monitor-command",{"command-line":f"sendkey {keys}"});time.sleep(.3)
                def type_text(text):
                    mapping={" ":("spc",False),"/":("slash",False),".":("dot",False),
                        "-":("minus",False),"_":("minus",True),"\n":("ret",False),
                        ":":("semicolon",True),",":("comma",False),"!":("1",True),"+":("equal",True),
                        "*":("8",True),"(":("9",True),")":("0",True)}
                    for char in text:
                        key,shift=mapping.get(char,(char.lower(),char.isupper()))
                        qmp.key(key,shift)
                pointer=[24,56]
                def click(x,y):
                    qmp.mouse(x-pointer[0],y-pointer[1]);pointer[:]=[x,y]
                    qmp.mouse(0,0,1);qmp.mouse(0,0,0)
                def shot(name):
                    time.sleep(.5);qmp.call("screendump",{"filename":str(out/f"{name}.ppm")})
                wait(b"srv> ",100)
                if args.network:
                    serial.sendall(b"run /fat/bin/sh\n");wait(b" $ ")
                    serial.sendall(b"dhcp\n");wait(b"dhcp: 10.0.2.15");wait(b" $ ")
                    serial.sendall(b"exit\n");wait(b"srv> ")
                serial.sendall(b"run /fat/bin/deskd --apps-trace\n")
                wait(b"desktools: starting role=text");wait(b"deskd: commit")
                click(690,750);wait(b"desktools: starting role=system");wait(b"desktools: system-ready");wait(b"deskd: commit");shot("system-default")
                # Second window content starts at (57,132), 760 by 500.
                click(720,305);wait(b"system-sort column=4 descending=0")
                click(720,305);wait(b"system-sort column=4 descending=1")
                shortcut("left");shortcut("ret");wait(b"system-sort column=3 descending=0")
                shortcut("f6");shortcut("end");shot("system-sorted")
                click(660,163);wait(b"system-paused=1")
                shortcut("f5");wait(b"system-refresh processes=");shot("system-paused")
                click(610,204);wait(b"system-page=network");shot("system-network")
                if args.network:
                    readings=re.findall(rb"network-refresh valid=1 initialized=1 ip=167772687 rx=(\d+) tx=(\d+)",output)
                    if not readings or not all(int(value)>0 for value in readings[-1]):
                        raise RuntimeError("Missing configured address or DHCP traffic counters")
                elif b"network-refresh valid=1 initialized=0 ip=" not in output:
                    raise RuntimeError("Missing no-adapter state")
                shortcut("left");wait(b"system-page=processes")
                # Resize while paused; rows remain keyboard/scroll accessible.
                qmp.mouse(813-pointer[0],628-pointer[1]);pointer[:]=[813,628]
                qmp.mouse(0,0,1);qmp.mouse(-160,-120,1);qmp.mouse(0,0,0);pointer[0]-=160;pointer[1]-=120;time.sleep(.5)
                click(170,320);shortcut("end");shot("system-narrow")
                click(500,163);wait(b"system-paused=0")
                # Global View menu uses the same pause/refresh commands.
                shortcut("f10");shortcut("right");shortcut("right");shortcut("ret")
                wait(b"system-paused=1")
                shortcut("f10");shortcut("home");shortcut("end");shortcut("ret")
                wait(b"deskd: exited",60);wait(b"srv> ")
                serial.sendall(b"run /fat/bin/libcprobe\n");wait(b"libcprobe: process details and legacy-size ABI ok");wait(b"libcprobe: ok");wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
    print(f"desktop-system: PASS shared table sorting/keyboard/resize, pause/refresh/menu controls, network tabs/address/traffic or no-adapter state, RAM and process ABI compatibility. Evidence: {out}")


if __name__=="__main__": main()
