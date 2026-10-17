#!/usr/bin/env python3
"""Exercise the native rendering study on a disposable QEMU disk."""
import argparse
import json
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


class QMP:
    def __init__(self, path):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        deadline = time.monotonic()+20
        while True:
            try:
                self.sock.connect(str(path))
                break
            except OSError:
                if time.monotonic()>deadline:
                    raise
                time.sleep(.1)
        self.sock.settimeout(10)
        self.stream = self.sock.makefile("rwb", buffering=0)
        self.stream.readline()
        self.call("qmp_capabilities")

    def call(self, execute, arguments=None):
        self.stream.write(json.dumps({"execute": execute, "arguments": arguments or {}}).encode()+b"\n")
        while True:
            response = json.loads(self.stream.readline())
            if "error" in response:
                raise RuntimeError(response["error"])
            if "return" in response:
                return response["return"]

    def key(self, key, shift=False):
        for down in (True, False):
            events = [{"type":"key", "data":{"down":down,"key":{"type":"qcode","data":key}}}]
            if shift:
                events.insert(0,{"type":"key","data":{"down":down,"key":{"type":"qcode","data":"shift"}}})
            self.call("input-send-event", {"events":events})
            time.sleep(.04)

    def mouse(self, dx, dy, button=None):
        if button is not None:
            self.call("human-monitor-command",{"command-line":f"mouse_button {button}"})
            # QEMU's PS/2 device may combine an immediate motion with the
            # button transition; let the press reach its intended hit target.
            time.sleep(.2)
        while dx or dy:
            x, y = max(-40,min(40,dx)), max(-40,min(40,dy))
            self.call("human-monitor-command",{"command-line":f"mouse_move {x} {y}"})
            dx-=x; dy-=y
            time.sleep(.06)
        time.sleep(.2)


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
        with (out/"desktop-foundation-qemu.log").open("wb") as errors:
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
                            raise RuntimeError(f"Missing {marker!r}; see desktop-foundation-serial.log")
                        try:
                            chunk=serial.recv(8192)
                            if not chunk: raise RuntimeError("serial disconnected")
                            output+=chunk
                        except socket.timeout:
                            pass
                    consumed=output.index(marker,consumed)+len(marker)

                wait(b"srv> ",100)
                serial.sendall(b"run /fat/bin/deskstudy --trace\n")
                wait(b"deskstudy: ready;")
                time.sleep(.5)
                qmp.call("screendump",{"filename":str(out/"desktop-native-initial.ppm")})
                with (out/"desktop-native-initial.ppm").open("rb") as image:
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
                    wait(f"deskstudy: pointer x={pointer_x} y={pointer_y} buttons={buttons}".encode())
                for key,shift in [("a",False),("a",True),("z",False),("z",True),("1",False),("2",False),("3",False)]:
                    qmp.key(key,shift)
                qmp.key("tab")
                # Initial study cursor is (24,56). Drag the title by (60,48).
                move(76,36); move(0,0,1); move(60,48); move(0,0,0)
                # Clamp expected window movement/size to this framebuffer.
                w,h=min(640,width-96),min(420,height-136)
                x,y=min(108,width-w-6),min(124,height-h-40)
                move(x+w-10-160,y+h-10-140)
                move(0,0,1); move(70,40); move(0,0,0)
                expected=(x,y,min(w+61,width-x-6),min(h+31,height-y-40))
                qmp.call("screendump",{"filename":str(out/"desktop-native-input.ppm")})
                qmp.key("esc"); wait(b"deskstudy: exited"); wait(b"srv> ")
                text=output.decode(errors="replace")
                if "large=1 input=aAzZ123" not in text:
                    raise RuntimeError("Native text/size input did not survive")
                state=re.search(r"state window=(\d+),(\d+),(\d+),(\d+)",text)
                if not state or tuple(map(int,state.groups()))!=expected:
                    raise RuntimeError(f"Drag/resize mismatch: {state.group(0) if state else 'missing'}")
                # A second launch verifies owner release; idle must not repaint.
                serial.sendall(b"run /fat/bin/deskstudy --smoke\n")
                wait(b"deskstudy: ready;"); wait(b"deskstudy: exited",45); wait(b"srv> ")
                text=output.decode(errors="replace")
                metrics=re.findall(r"deskstudy: frames=(\d+) writes=(\d+) fb_bytes=(\d+)",text)
                if len(metrics)!=2 or int(metrics[-1][0])!=1:
                    raise RuntimeError(f"Idle repaint or relaunch failure: {metrics}")
                if any("exception:" in line and "breakpoint" not in line for line in text.splitlines()):
                    raise RuntimeError("Guest exception")
                serial.close(); qmp.stream.close(); qmp.sock.close()
            finally:
                stop_process(process)
                (out/"desktop-foundation-serial.log").write_bytes(output)
    print("desktop-foundation: PASS USB mixed-case input, text size, drag, resize, exit/relaunch, one idle present")
    print(f"Evidence: {out}")


if __name__=="__main__":
    main()
