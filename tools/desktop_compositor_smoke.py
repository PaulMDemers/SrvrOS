#!/usr/bin/env python3
"""Exercise the separate compositor and application on a disposable QEMU disk."""
import argparse
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
import time

from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from desktop_foundation_smoke import QMP


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu",default="qemu-system-x86_64")
    parser.add_argument("--resolution",choices=["640x480","1280x800","1440x900"])
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    out=root/"build/review"
    if args.resolution:
        out=out/f"desktop-{args.resolution}"
    out.mkdir(parents=True,exist_ok=True)
    output=b""
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-desktop-") as tmp:
        tmp=Path(tmp)
        shutil.copyfile(root/"build/srvros.exfat",tmp/"disk.exfat")
        iso=root/"build/srvros-x86_64.iso"
        if args.resolution:
            width,height=map(int,args.resolution.split("x"))
            iso=build_iso(str(root),str(tmp),width,height,os.environ.copy())
        command=[args.qemu,"-M","q35","-m","512M","-cdrom",str(iso),
                 "-boot","d","-drive",f"if=none,id=disk,file={tmp/'disk.exfat'},format=raw",
                 "-device","ich9-ahci,id=ahci","-device","ide-hd,drive=disk,bus=ahci.0",
                 "-device","qemu-xhci,id=xhci","-device","usb-kbd,bus=xhci.0",
                 "-serial",f"tcp:127.0.0.1:{port},server=on,wait=off",
                 "-qmp",f"unix:{tmp/'qmp'},server=on,wait=off","-display","none","-monitor","none","-no-reboot"]
        with (out/"desktop-compositor-qemu.log").open("wb") as errors:
            process=subprocess.Popen(command,stdout=errors,stderr=errors)
            try:
                serial=connect_serial(port,20); serial.settimeout(.2)
                qmp=QMP(tmp/"qmp")

                consumed=0

                def wait(marker, timeout=40):
                    nonlocal output, consumed
                    deadline=time.monotonic()+timeout
                    while marker not in output[consumed:]:
                        if time.monotonic()>deadline or process.poll() is not None:
                            raise RuntimeError(f"Missing {marker!r}; see desktop-compositor-serial.log")
                        try:
                            chunk=serial.recv(8192)
                            if not chunk: raise RuntimeError("serial disconnected")
                            output+=chunk
                        except socket.timeout:
                            pass
                    consumed=output.index(marker,consumed)+len(marker)

                wait(b"srv> ",100)
                serial.sendall(b"run /fat/bin/guifail --keyboard-events\n")
                wait(b"guifail: keyboard-events ready")
                qmp.key("a",True)
                wait(b"guifail: key usage=4 text=65 mods=2 flags=1")
                wait(b"guifail: key usage=4 text=0")
                qmp.key("left")
                wait(b"guifail: key usage=80 text=0 mods=0 flags=1")
                qmp.call("human-monitor-command",{"command-line":"sendkey ctrl-c"})
                wait(b"guifail: key usage=6 text=99 mods=1 flags=1")
                qmp.key("esc")
                wait(b"guifail: keyboard-events abrupt exit"); wait(b"srv> ")
                serial.sendall(b"run /fat/bin/deskd --trace\n")
                wait(b"deskd: ready;")
                time.sleep(.5)
                qmp.call("screendump",{"filename":str(out/"desktop-compositor-initial.ppm")})
                with (out/"desktop-compositor-initial.ppm").open("rb") as image:
                    assert image.readline().strip()==b"P6"
                    width,height=map(int,image.readline().split())
                if args.resolution and f"{width}x{height}"!=args.resolution:
                    raise RuntimeError("Requested framebuffer mode was not selected")
                pointer_x,pointer_y,buttons=24,56,0

                def move(dx,dy,button=None):
                    nonlocal pointer_x,pointer_y,buttons
                    qmp.mouse(dx,dy,button)
                    pointer_x=max(0,min(width-1,pointer_x+dx))
                    pointer_y=max(0,min(height-1,pointer_y+dy))
                    if button is not None:
                        buttons=button
                    wait(f"deskd: pointer x={pointer_x} y={pointer_y} buttons={buttons}".encode())
                for key,shift in [("a",False),("a",True),("z",False),("z",True),("1",False),("2",False),("3",False)]:
                    qmp.key(key,shift)
                wait(b"deskclient: text=aAzZ123")
                # Emulated mouse -> compositor capture -> application thumb.
                client_w=min(600,width-40); client_h=min(320,height-149)
                # The study uses the shared library button; release activates it.
                move(33+client_w-80-pointer_x,108+client_h-24-pointer_y)
                move(0,0,1); move(0,0,0)
                wait(b"deskclient: text-size=20")
                move(0,0,1); move(0,0,0)
                wait(b"deskclient: text-size=16")
                bar_x=33+client_w-21; bar_y=108+94
                limit=48*24-(client_h-202)
                move(bar_x-pointer_x,bar_y+7-pointer_y); move(0,0,1)
                scroll_start=len(output)
                move(min(width-1,bar_x+80)-pointer_x,bar_y+client_h-202+30-pointer_y)
                end_marker=f"deskclient: scroll={limit}".encode()
                if end_marker not in output[scroll_start:]: wait(end_marker)
                move(0,0,0)
                move(bar_x-pointer_x,bar_y+2-pointer_y); move(0,0,1)
                wait(f"deskclient: scroll={limit-(client_h-202)}".encode())
                move(0,0,0)
                time.sleep(.2)
                qmp.call("screendump",{"filename":str(out/"desktop-scrollbar.ppm")})
                move(24-pointer_x,56-pointer_y)
                qmp.call("human-monitor-command",{"command-line":"sendkey ctrl-n"})
                wait(b"window=1"); wait(b"generation=1 size=")
                qmp.key("b"); qmp.key("b",True); wait(b"deskclient: text=bB")
                qmp.key("tab"); qmp.key("1"); wait(b"deskclient: text=aAzZ1231")
                move(76,24); move(0,0,1); move(60,48); move(0,0,0)
                w,h=min(600,width-40)+2,min(320,height-149)+45
                x,y=min(92,width-w-6),min(112,height-h-36)
                move(x+w-10-pointer_x,y+h-10-pointer_y)
                move(0,0,1); move(-70,-40); move(0,0,0)
                resized=(w-81,h-94)
                marker=f"size={resized[0]}x{resized[1]}".encode()
                if marker not in output:
                    wait(marker)
                qmp.call("screendump",{"filename":str(out/"desktop-compositor-input.ppm")})
                move(x+resized[0]+2-20-pointer_x,y+20-pointer_y)
                move(0,0,1); move(0,0,0)
                if b"input=aAzZ1231" not in output:
                    wait(b"input=aAzZ1231")
                qmp.key("z",True); wait(b"deskclient: text=bBZ")
                qmp.key("esc"); wait(b"deskd: exited"); wait(b"srv> ")
                serial.sendall(b"run /fat/bin/deskd --scroll-smoke\n")
                wait(b"deskd: ready;"); wait(b"deskclient: scroll=120"); wait(b"deskclient: scroll=96")
                time.sleep(.2) # Client logs input before its sealed frame reaches scanout.
                qmp.call("screendump",{"filename":str(out/"desktop-scroll.ppm")})
                wait(b"deskd: exited"); wait(b"srv> ")
                # Relaunch checks kernel server cleanup, bounded idle rendering.
                serial.sendall(b"run /fat/bin/deskd --smoke\n")
                wait(b"deskd: ready;"); wait(b"deskd: exited",45); wait(b"srv> ")
                text=output.decode(errors="replace")
                metrics=re.findall(r"deskd: exited frames=(\d+) fb_bytes=(\d+)",text)
                if len(metrics)!=3 or not 1<=int(metrics[-1][0])<=3:
                    raise RuntimeError(f"Idle repaint or relaunch failure: {metrics}")
                if "deskclient: FAIL" in text or "deskd: FAIL" in text:
                    raise RuntimeError("Client/compositor failure")
                serial.sendall(b"run /fat/bin/deskd --lifecycle-smoke\n")
                wait(b"deskd: lifecycle killing client")
                wait(b"deskd: lifecycle replacing client")
                wait(b"deskd: lifecycle abrupt server exit")
                wait(b"deskclient: exited")
                # The kernel must restore the console after abrupt owner exit.
                serial.sendall(b"run /fat/bin/guifail --console-muted\n")
                wait(b"guifail: console-muted=0"); wait(b"srv> ")
                serial.sendall(b"run /fat/bin/deskd --smoke\n")
                wait(b"deskd: ready;"); wait(b"deskd: exited",45); wait(b"srv> ")
                text=output.decode(errors="replace")
                if "deskclient: FAIL" in text or "deskd: FAIL" in text:
                    raise RuntimeError("Lifecycle failure")
                if any("exception:" in line and "breakpoint" not in line for line in text.splitlines()):
                    raise RuntimeError("Guest exception")
                serial.close(); qmp.stream.close(); qmp.sock.close()
            finally:
                stop_process(process)
                (out/"desktop-compositor-serial.log").write_bytes(output)
    print("desktop-compositor: PASS separate processes, mixed case, shared button, scrollbar capture/page clicks, two windows, focus, drag, resize, close, idle, client death/replacement, abrupt server exit, console return/relaunch")
    print(f"Evidence: {out}")


if __name__=="__main__":
    main()
