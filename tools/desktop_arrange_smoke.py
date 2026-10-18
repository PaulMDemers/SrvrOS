#!/usr/bin/env python3
"""Exercise side-by-side window arrangement, restoration and input after resize in a disposable VM."""
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
    parser=argparse.ArgumentParser();parser.add_argument("--small",action="store_true");args=parser.parse_args()
    width,height=(800,600) if args.small else (1280,800)
    root=Path(__file__).resolve().parents[1]
    out=root/("build/review/desktop-arrange-small" if args.small else "build/review/desktop-arrange")
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
                wait(b"desktools: starting role=text");wait(b"deskd: commit")
                def save_text(name):
                    shortcut("ctrl-l");type_text("/fat/"+name);shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                initial_left=(32,64,min(720,width-40)+2,min(480,height-189)+45)
                initial_right=(56,88,min(720,width-64)+2,min(480,height-213)+45)
                tile_w=min((width-24)//2,962);tile_h=min(height-132,685)
                def tiled(side):
                    x=8 if side==1 else width-tile_w-8
                    return f"mode={side} frame={x},52,{tile_w},{tile_h}".encode()
                type_text("Left document.");shortcut("meta_l-alt-left");wait(tiled(1))
                shortcut("meta_l-spc");wait(b"launcher opened");type_text("Text new app");qmp.key("ret");wait(b"launcher application=2")
                wait_all([b"desktools: starting role=text",b"text history=multi-step",b"deskd: commit"])
                type_text("Right document.");shortcut("meta_l-alt-right");wait(tiled(2));shot("arranged-pair")
                type_text(" Still typing.");save_text("arranged-right.txt")
                click(100,73);type_text(" Still typing.");save_text("arranged-left.txt")
                # Global Arrange is immediately before Window, independent of
                # how many headings the focused application publishes.
                shortcut("f10");shortcut("left");shortcut("left");shot("arrange-menu");shortcut("home");shortcut("down");shortcut("ret");wait(tiled(2))
                shortcut("meta_l-alt-left");wait(tiled(1))
                # Switching sides preserves the one original restore frame.
                shortcut("meta_l-alt-down");wait(("maximized=0 frame="+",".join(map(str,initial_left))).encode());shot("arrange-restored")
                shortcut("meta_l-alt-left");wait(tiled(1));shortcut("meta_l-alt-up")
                centered=( (width-initial_left[2])//2,52+(height-132-initial_left[3])//2,initial_left[2],initial_left[3])
                wait(("mode=3 frame="+",".join(map(str,centered))).encode());shot("arrange-centered")
                shortcut("meta_l-alt-down");wait(b"maximized=1 frame=");shortcut("meta_l-alt-down");wait(("maximized=0 frame="+",".join(map(str,centered))).encode())
                # Repeated configuration generations must retain document state.
                for i in range(12):
                    side=1+i%2;shortcut("meta_l-alt-"+("left" if side==1 else "right"));wait(tiled(side))
                shortcut("meta_l-alt-left");wait(tiled(1))
                click(width-tile_w+40,73);shortcut("meta_l-alt-down");wait(("maximized=0 frame="+",".join(map(str,initial_right))).encode())
                shortcut("meta_l-alt-right");wait(tiled(2));shot("arranged-final")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
            from exfat_test_image import ExfatFixture
            fixture=ExfatFixture((tmp/"disk.exfat").read_bytes())
            assert fixture.read("arranged-left.txt")==b"Left document. Still typing."
            assert fixture.read("arranged-right.txt")==b"Right document. Still typing."
            positions=re.findall(rb"arranged pid=(\d+) mode=([12]) frame=(\d+),(\d+),(\d+),(\d+)",output)
            assert len(positions)>=18
            for _,side,x,y,w,h in positions:
                assert int(x)==(8 if side==b"1" else width-tile_w-8) and int(y)==52 and int(w)==tile_w and int(h)==tile_h
    print(f"desktop-arrange: PASS {width}x{height} side-by-side input/save, menu and shortcuts, original-frame restore, centering, zoom/restore, repeated generations and exact documents. Evidence: {out}")


if __name__=="__main__": main()
