#!/usr/bin/env python3
"""Exercise immutable desktop capture, background file saving and notification-to-Preview navigation in a disposable VM."""
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
    out=root/"build/review/desktop-capture"
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-capture-") as directory:
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
                type_text("Screenshot proof.")
                # Remove the blinking caret before the pixel-reference snapshot.
                shortcut("shift-tab");shortcut("shift-tab");click(1100,500);shot("reference")
                shortcut("meta_l-shift-3");wait(b"deskd: screenshot captured path=");wait(b"height=800")
                captured=re.findall(rb"deskd: screenshot captured path=([^\s]+) width=1280 height=800",output)
                assert len(captured)==1,output[-4000:]
                first_path=captured[0].decode()
                # Continue typing while the snapshot streams to the worker.
                click(300,250);shortcut("ctrl-end");type_text(" Input during saving.")
                wait(b"deskd: screenshot saved status=0 path=",90);shot("saved-notification")
                shortcut("ctrl-l");type_text("/fat/capture-input.txt");shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                # History's Open folder uses the server-owned destination path.
                shortcut("meta_l-shift-n");wait(b"deskd: notifications opened count=1")
                click(760,522);wait(b"desktools: starting role=files");wait(b"file-result role=files op=3 status=0")
                shot("screenshots-folder");type_text("Screen-");qmp.key("ret");wait(b"deskview: loaded path=",60);shot("preview-capture")
                # Menu path captures again and leaves Return to console as End.
                shortcut("f10");qmp.key("end");qmp.key("up");qmp.key("ret")
                wait(b"deskd: screenshot captured path=");wait(b"height=800");wait(b"deskd: screenshot saved status=0 path=",90)
                captured=re.findall(rb"deskd: screenshot captured path=([^\s]+) width=1280 height=800",output)
                assert len(captured)==2 and captured[0]!=captured[1]
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
            from exfat_test_image import ExfatFixture
            fixture=ExfatFixture((tmp/"disk.exfat").read_bytes())
            assert fixture.read("capture-input.txt")==b"Screenshot proof. Input during saving."
            from PIL import Image, ImageChops
            import io
            first=Image.open(io.BytesIO(fixture.read(first_path.removeprefix("/fat/")))).convert("RGB")
            reference=Image.open(out/"reference.ppm").convert("RGB")
            assert first.size==reference.size==(1280,800)
            # A capture command is consumed globally and does not alter app text.
            difference=ImageChops.difference(first,reference)
            assert difference.getbbox() is None,difference.getbbox()
            first.save(out/"saved-capture.png")
            second=Image.open(io.BytesIO(fixture.read(captured[1].decode().removeprefix("/fat/")))).convert("RGB")
            assert second.size==first.size and second.tobytes()!=first.tobytes()
            entries=[name for name,_ in fixture.entries(fixture.read("home/Screenshots"))]
            assert len(entries)==2 and all(name.endswith(".bmp") for name in entries),entries
    print(f"desktop-capture: PASS exact display pixels, immutable snapshot during typing, durable BMP readback, notification folder, Preview, menu/shortcut repeat and clean exit. Evidence: {out}")


if __name__=="__main__": main()
