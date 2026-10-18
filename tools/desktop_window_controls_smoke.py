#!/usr/bin/env python3
"""Click the standard maximize and restore titlebar controls in every desktop application."""
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
    parser=argparse.ArgumentParser();modes=parser.add_mutually_exclusive_group()
    modes.add_argument("--small",action="store_true",help="800x600 compatibility mode")
    modes.add_argument("--resolution",choices=["800x600","1280x800","1440x900","1920x1080"],default="1440x900")
    args=parser.parse_args()
    width,height=(800,600) if args.small else tuple(map(int,args.resolution.split("x")))
    root=Path(__file__).resolve().parents[1]
    suffix="-small" if (width,height)==(800,600) else "" if (width,height)==(1280,800) else f"-{width}x{height}"
    out=root/("build/review/desktop-window-controls"+suffix)
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-arrange-") as directory:
        tmp=Path(directory)
        shutil.copyfile(root/"build/srvros.exfat",tmp/"disk.exfat")
        iso=build_iso(str(root),str(tmp),width,height,os.environ.copy())
        command=["qemu-system-x86_64","-M","q35","-m","512M","-cdrom",str(iso),"-boot","d",
            "-drive",f"if=none,id=disk,file={tmp/'disk.exfat'},format=raw",
            "-device","ich9-ahci,id=ahci","-device","ide-hd,drive=disk,bus=ahci.0",
            "-device","qemu-xhci,id=xhci","-device","usb-kbd,bus=xhci.0",
            "-serial",f"tcp:127.0.0.1:{port},server=on,wait=off",
            "-qmp",f"unix:{tmp/'qmp'},server=on,wait=off","-display","none","-monitor","none","-no-reboot","-rtc","base=2026-10-18T12:34:00,clock=vm"]
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
                            output+=chunk;(out/"serial.log").write_bytes(output)
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
                serial.sendall(b"run /fat/bin/deskd --apps-trace\n")
                wait(b"deskd: commit");wait(b"\n")
                applications=[("Text",2),("Files",1),("Calculator",3),("System",4),
                    ("Settings",5),("Terminal",6),("Preview",7),("Help",8),
                    ("Clock",9),("Sketch",10),("Network Utility",11),("Find Files",12)]
                for name,number in applications:
                    if number!=2:
                        shortcut("meta_l-spc");wait(b"launcher opened")
                        type_text(name+" new app");qmp.key("ret")
                        wait(f"launcher application={number}".encode());wait(b"deskd: commit");wait(b"\n")
                    pid,slot=re.findall(rb"deskd: opened pid=(\d+) window=(\d+)",output)[-1]
                    sizes=re.findall(rb"deskd: commit pid="+pid+rb" generation=1 size=(\d+)x(\d+)",output)
                    assert sizes, (name,pid)
                    cw,ch=map(int,sizes[-1]);fx=32+24*(int(slot)%4);fy=64+24*(int(slot)%4)
                    if number==6:
                        type_text("exit\n");wait(b"deskterm: shell exited status=0")
                    shot(f"{number:02d}-normal")
                    click(fx+cw+2-56,fy+22)
                    zoom_w=min(width-18,1440)+2;zoom_h=min(height-177,900)+45
                    wait(b"zoom pid="+pid+f" maximized=1 frame={(width-zoom_w)//2},52,{zoom_w},{zoom_h}".encode())
                    wait(b"commit pid="+pid+b" generation=2 size="+f"{zoom_w-2}x{zoom_h-45}".encode())
                    shot(f"{number:02d}-maximized")
                    click((width-zoom_w)//2+zoom_w-56,74)
                    wait(b"zoom pid="+pid+f" maximized=0 frame={fx},{fy},{cw+2},{ch+45}".encode())
                    wait(b"commit pid="+pid+b" generation=3 size="+f"{cw}x{ch}".encode())
                    click(fx+cw+2-24,fy+22);wait(b"deskd: retired pid="+pid+b"\r\n")
                    print(f"window-controls: {name} titlebar maximize/restore/close passed",flush=True)
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
    print(f"window-controls: PASS all 12 applications, titlebar clicks, full-size client commits, original-frame restoration and clean close. Evidence: {out}")


if __name__=="__main__": main()
