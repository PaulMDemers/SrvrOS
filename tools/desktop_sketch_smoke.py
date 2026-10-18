#!/usr/bin/env python3
"""Exercise Sketch drawing, shared sliders/colors, undo, immutable BMP saves, import and compact layout in a disposable VM."""
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
    root=Path(__file__).resolve().parents[1]
    out=root/"build/review/desktop-sketch"
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-sketch-") as directory:
        tmp=Path(directory)
        shutil.copyfile(root/"build/srvros.exfat",tmp/"disk.exfat")
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy())
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
                def right_click(x,y):
                    qmp.mouse(x-pointer[0],y-pointer[1]);pointer[:]=[x,y]
                    qmp.mouse(0,0,2);qmp.mouse(0,0,0)
                def control_click(x,y):
                    qmp.call("input-send-event",{"events":[{"type":"key","data":{"down":True,"key":{"type":"qcode","data":"ctrl"}}}]});time.sleep(.1)
                    click(x,y)
                    qmp.call("input-send-event",{"events":[{"type":"key","data":{"down":False,"key":{"type":"qcode","data":"ctrl"}}}]});time.sleep(.1)
                def shot(name):
                    time.sleep(.5);qmp.call("screendump",{"filename":str(out/f"{name}.ppm")})
                wait(b"srv> ",100)
                serial.sendall(b"run /fat/bin/deskd --apps-trace\n")
                wait(b"desktools: starting role=text");wait_all([b"deskd: commit",b"text history=multi-step"])
                text_pid=re.search(rb"deskd: opened pid=(\d+) window=0",output).group(1)
                shortcut("meta_l-spc");type_text("Sketch new app");qmp.key("ret");wait(b"desksketch: starting");wait(b"deskd: commit");time.sleep(.5)
                shortcut("ctrl-k");shortcut("end");shortcut("tab");shortcut("home");shortcut("tab");shortcut("home");shot("color-panel")
                click(329,496);time.sleep(.4);click(585,496);wait(b"desksketch: color=ff0000")
                # Canvas is centered at (137,269), native 640 x 360, in window 1.
                shortcut("shift-tab");shortcut("home");shortcut("tab")
                def stroke(x0,y0,x1,y1,marker):
                    qmp.mouse(x0-pointer[0],y0-pointer[1]);pointer[:]=[x0,y0]
                    qmp.mouse(0,0,1);qmp.mouse(x1-x0,y1-y0,1);qmp.mouse(0,0,0);pointer[:]=[x1,y1]
                    wait(marker)
                stroke(157,289,237,289,b"desksketch: stroke revision=2 tool=0")
                qmp.key("l");wait(b"desksketch: action=31")
                stroke(157,309,237,389,b"desksketch: stroke revision=3 tool=1")
                shortcut("ctrl-z");wait(b"desksketch: action=5 revision=2")
                shortcut("ctrl-shift-z");wait(b"desksketch: action=6 revision=3")
                qmp.key("r");wait(b"desksketch: action=32")
                stroke(277,289,337,349,b"desksketch: stroke revision=4 tool=2");shot("drawing")
                qmp.key("b");wait(b"desksketch: action=30")
                shortcut("ctrl-s");wait(b"desksketch: picker ready mode=2");type_text("sketch-proof.bmp");qmp.key("ret");wait(b"desksketch: saving revision=4")
                click(337,369);wait_all([b"desksketch: stroke revision=5 tool=0",b"desksketch: saved success=1 status=0 revision=4 current=5"],90)
                shortcut("ctrl-w");wait(b"desksketch: confirm operation=11");shot("unsaved-drawing");shortcut("esc")
                # Refuse replacement of the first copy, then save the newer edit.
                shortcut("ctrl-s");wait(b"desksketch: picker ready mode=2");type_text("sketch-proof.bmp");qmp.key("ret");wait(b"desksketch: saved success=0 status=4",90)
                shortcut("ctrl-s");wait(b"desksketch: picker ready mode=2");type_text("sketch-latest.bmp");qmp.key("ret");wait(b"desksketch: saved success=1 status=0 revision=5 current=5",90)
                shortcut("ctrl-w");wait(b"desksketch: exited failure=0")
                click(490,750);shortcut("ctrl-v");wait(b"clipboard role=2 action=32")
                shortcut("ctrl-l");type_text("/fat/sketch-color.txt");shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                shortcut("ctrl-w");wait(b"deskd: retired pid="+text_pid+b"\r\n")
                click(390,750);wait(b"desktools: starting role=files");wait(b"file-result role=files op=3 status=0")
                shortcut("ctrl-l");type_text("/fat/home");qmp.key("ret");wait(b"file-result role=files op=3 status=0")
                type_text("sketch-proof");qmp.key("ret");wait(b"deskview: loaded path=/fat/home/sketch-proof.bmp",60)
                shortcut("ctrl-e");wait(b"desksketch: loaded path=/fat/home/sketch-proof.bmp width=640 height=360",60);shot("reopened-drawing")
                # Imported drawing is window 2, at (80,112), 802 x 605 overall.
                qmp.mouse(878-pointer[0],713-pointer[1]);pointer[:]=[878,713]
                qmp.mouse(0,0,1);qmp.mouse(-520,-320,1);qmp.mouse(0,0,0);pointer[0]-=520;pointer[1]-=320;time.sleep(.5)
                shot("drawing-compact");shortcut("ctrl-k");shot("color-panel-compact");shortcut("esc")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
            from exfat_test_image import ExfatFixture
            fixture=ExfatFixture((tmp/"disk.exfat").read_bytes())
            from PIL import Image, ImageDraw, ImageChops
            import io
            assert fixture.read("sketch-color.txt")==b"#FF0000"
            first=Image.open(io.BytesIO(fixture.read("home/sketch-proof.bmp"))).convert("RGB")
            expected=Image.new("RGB",(640,360),"white");pen=ImageDraw.Draw(expected)
            pen.line((20,20,100,20),fill="red",width=1)
            pen.line((20,40,100,120),fill="red",width=1)
            pen.rectangle((140,20,200,80),outline="red",width=1)
            diff=ImageChops.difference(first,expected)
            first.save(out/"saved-drawing.png");expected.save(out/"expected-drawing.png")
            assert diff.getbbox() is None,diff.getbbox()
            latest=Image.open(io.BytesIO(fixture.read("home/sketch-latest.bmp"))).convert("RGB")
            expected.putpixel((200,100),(255,0,0));diff=ImageChops.difference(latest,expected)
            assert diff.getbbox() is None,diff.getbbox()
    print(f"desktop-sketch: PASS shared RGB/size controls, pen/line/box exact pixels, undo/redo, editing during verified saves, existing-file preservation, close prompt, hex clipboard, Preview-to-Sketch import and compact layouts. Evidence: {out}")


if __name__=="__main__": main()
