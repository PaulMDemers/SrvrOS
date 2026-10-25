#!/usr/bin/env python3
"""Exercise native Preview image loading, view controls and failure recovery in a disposable VM."""
import os
import argparse
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time
import struct
from image_fixtures import image_pattern
from exfat_test_image import ExfatFixture
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from desktop_foundation_smoke import QMP


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--stress",action="store_true",help="also exercise maximum-size images and cancellation")
    parser.add_argument("--resolution",choices=["1280x800","1440x900"],default="1280x800")
    args=parser.parse_args()
    width,height=map(int,args.resolution.split("x"))
    root=Path(__file__).resolve().parents[1]
    out=root/("build/review/desktop-preview"+("-1440x900" if width==1440 else ""))
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-preview-") as directory:
        tmp=Path(directory)
        fixture=ExfatFixture((root/"build/srvros.exfat").read_bytes())
        png=image_pattern()
        files={"Landscape.png":png,"Bad.png":b"not an image", "Oversize.png":png[:16]+struct.pack(">II",4096,4096)+png[24:]}
        if args.stress: files["Maximum.png"]=image_pattern(4096,1024)
        files.update({"Sample."+extension:(root/f"tools/fixtures/images/pattern.{extension}").read_bytes() for extension in ("png","jpg","bmp")})
        fixture.add_directory_files("pictures",files)
        for name,data in files.items(): assert fixture.read("pictures/"+name)==data
        (tmp/"disk.exfat").write_bytes(fixture.data)
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
                serial.sendall(b"run /fat/bin/desktools text --chooser-capture\n")
                wait(b"chooser-capture: PASS",80);wait(b"run: pid=");wait(b" exited status=0");wait(b"srv> ")
                assert b"mode=legacy canceled-submitted=yes reopened=yes stale-rejected=yes worker-retired=yes" in output
                assert b"mode=pooled canceled-submitted=yes reopened=yes stale-rejected=yes worker-retired=yes" in output
                serial.sendall(b"run /fat/bin/deskd --apps-trace\n")
                wait(b"desktools: starting role=text");wait_all([b"deskd: commit",b"text history=multi-step"])
                shortcut("meta_l-spc");wait(b"launcher opened");type_text("Preview");qmp.key("ret")
                wait(b"launcher application=7");wait(b"deskview: starting");wait(b"deskd: commit")
                shot("preview-empty")
                def open_name(name,first=False):
                    shortcut("ctrl-o");wait(b"deskview: picker ready")
                    if first:
                        shortcut("ctrl-l");type_text("/fat/pictures");qmp.key("ret");wait(b"deskview: picker ready")
                    shortcut("tab");shortcut("ctrl-a");type_text(name);qmp.key("ret")
                open_name("Landscape.png",True);wait(b"deskview: loaded path=/fat/pictures/Landscape.png width=1600 height=1000")
                shot("preview-fit")
                # Exercise the titlebar control itself, then wait for the large
                # client commit so a frame-only resize cannot pass this check.
                click(766,109)
                wait(f"maximized=1 frame=8,52,{width-16},{height-132}".encode())
                wait(f"size={width-18}x{height-177}".encode())
                shot("preview-maximized")
                click(width-60,73)
                wait(b"maximized=0 frame=56,88,762,565")
                wait(b"size=760x520")
                shot("preview-restored")
                qmp.key("1");wait(b"view scale=65536 fit=0")
                qmp.key("right");wait(b"view scale=65536 fit=0")
                qmp.key("down");wait(b"view scale=65536 fit=0")
                qmp.mouse(500-pointer[0],380-pointer[1]);pointer[:]=[500,380]
                qmp.mouse(0,0,1);qmp.mouse(-140,-90,1);qmp.mouse(0,0,0);pointer[0]-=140;pointer[1]-=90
                wait(b"deskview: view scale=65536 fit=0")
                qmp.key("equal",True);wait(b"view scale=131072 fit=0");shot("preview-zoom")
                qmp.key("0");wait(b"fit=1")
                if args.stress:
                    open_name("Maximum.png");wait(b"deskview: loading path=/fat/pictures/Maximum.png");qmp.key("esc")
                    wait(b"deskview: action=2");shot("preview-canceled")
                    open_name("Maximum.png");wait(b"deskview: loading path=/fat/pictures/Maximum.png")
                    click(width//2-150,height-50);type_text("Preview remained responsive.")
                    wait(b"loaded path=/fat/pictures/Maximum.png width=4096 height=1024",90)
                    shortcut("ctrl-l");type_text("/fat/preview-input-proof.txt");shortcut("ctrl-s")
                    wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                    shortcut("meta_l-spc");wait(b"launcher opened");type_text("Maximum.png");qmp.key("ret");wait(b"launcher window=")
                    shot("preview-maximum")
                    open_name("Landscape.png");wait(b"loaded path=/fat/pictures/Landscape.png width=1600 height=1000")
                open_name("Bad.png");wait(b"image-result transport=1 status=3 retained=1");shot("preview-invalid")
                open_name("Oversize.png");wait(b"image-result transport=1 status=4 retained=1")
                open_name("Sample.jpg");wait(b"loaded path=/fat/pictures/Sample.jpg width=19 height=13")
                open_name("Sample.bmp");wait(b"loaded path=/fat/pictures/Sample.bmp width=19 height=13")
                open_name("Sample.png");wait(b"loaded path=/fat/pictures/Sample.png width=19 height=13")
                shortcut("ctrl-o");wait(b"deskview: picker ready");shortcut("esc")
                wait(b"deskview: picker canceled pending=0 retained=1");shot("preview-picker-canceled")
                # The picker uses the shared asynchronous clipboard path.
                shortcut("ctrl-o");wait(b"deskview: picker ready");shortcut("tab");type_text("Landscape.png")
                shortcut("ctrl-a");shortcut("ctrl-c");time.sleep(.5);type_text("wrong");shortcut("ctrl-a");shortcut("ctrl-v");time.sleep(.5);qmp.key("ret")
                wait(b"loaded path=/fat/pictures/Landscape.png width=1600 height=1000")
                # Global View menu and compact client surface.
                shortcut("f10");shortcut("right");shortcut("right");shortcut("down");shortcut("ret")
                wait(b"deskview: action=4 scale=65536")
                qmp.mouse(813-pointer[0],648-pointer[1]);pointer[:]=[813,648]
                qmp.mouse(0,0,1);qmp.mouse(-470,-270,1);qmp.mouse(0,0,0);pointer[0]-=470;pointer[1]-=270;time.sleep(.5)
                qmp.key("0");wait(b"fit=1");shot("preview-compact")
                shortcut("ctrl-w");wait(b"deskview: exited failure=0")
                # Files associates the image extension with Preview.
                click(width//2-250,height-50);wait(b"desktools: starting role=files");wait(b"file-result role=files op=3 status=0")
                shortcut("ctrl-l");type_text("/fat/pictures");qmp.key("ret");wait(b"file-result role=files op=3 status=0")
                qmp.key("home");qmp.key("down");qmp.key("ret")
                wait(b"deskview: starting");wait(b"loaded path=/fat/pictures/Landscape.png width=1600 height=1000");shot("preview-files-launch")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                if args.stress:
                    serial.sendall(b"cat /fat/preview-input-proof.txt\n");wait(b"Preview remained responsive.");wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"chooser-capture: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
    print(f"desktop-preview: PASS launcher, PNG/JPEG/BMP, fit/actual/zoom/pan, invalid/oversized input retention, picker clipboard, global View menu, compact layout, Files association and clean exit. Maximum-image/cancel stress: {args.stress}. Evidence: {out}")


if __name__=="__main__": main()
