#!/usr/bin/env python3
"""Exercise Clock, nonintrusive notifications, lap export and compact layouts in a disposable VM."""
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
    out=root/"build/review/desktop-clock"
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-clock-") as directory:
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
                def shot(name):
                    time.sleep(.5);qmp.call("screendump",{"filename":str(out/f"{name}.ppm")})
                wait(b"srv> ",100)
                serial.sendall(b"run /fat/bin/deskd --apps-trace\n")
                wait(b"desktools: starting role=text");wait_all([b"deskd: commit",b"text history=multi-step"])
                shortcut("meta_l-spc");wait(b"launcher opened");type_text("Clock new app");qmp.key("ret");wait(b"launcher application=9")
                wait_all([b"deskclock: starting",b"size=560x440"])
                shortcut("ctrl-a");type_text("0:05");shortcut("ctrl-ret");wait(b"deskclock: timer running=1")
                shortcut("ctrl-w");wait(b"deskclock: close confirmation");shortcut("esc");wait(b"deskclock: close canceled")
                click(533,112);time.sleep(.3)
                type_text("Clock kept focus.")
                wait_all([b"deskclock: timer finished",b"deskclock: notification result=1"]);shot("clock-toast")
                click(1244,78);type_text(" Still typing.")
                shortcut("ctrl-l");type_text("/fat/clock-input-proof.txt");shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                shortcut("meta_l-shift-n");wait(b"notifications opened count=1");shot("clock-notifications")
                shortcut("tab");shortcut("ret");wait(b"notification opened owner=")
                shortcut("ctrl-1");shortcut("tab");shortcut("ctrl-a");type_text("2");shortcut("ctrl-ret");wait(b"deskclock: timer running=1")
                shortcut("ctrl-ret");wait(b"deskclock: timer running=0");shortcut("ctrl-shift-c");wait(b"deskclock: copy kind=time")
                time.sleep(.5);click(490,750);shortcut("ctrl-n");shortcut("ctrl-v");wait(b"clipboard role=2 action=32")
                shortcut("ctrl-l");type_text("/fat/clock-time.txt");shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                shortcut("meta_l-spc");wait(b"launcher opened");type_text("Clock Timer");qmp.key("ret");wait(b"launcher window=")
                shortcut("ctrl-2");wait(b"deskclock: page=stopwatch");shortcut("ctrl-ret");wait(b"deskclock: stopwatch running=1")
                time.sleep(.4);shortcut("ctrl-l");wait(b"deskclock: lap=1 ");time.sleep(.3);shortcut("ctrl-l");wait(b"deskclock: lap=2 ")
                shortcut("ctrl-ret");wait(b"deskclock: stopwatch running=0");shot("clock-stopwatch")
                shortcut("ctrl-shift-l");wait(b"deskclock: copy kind=laps");time.sleep(.5)
                click(490,750);shortcut("ctrl-n");shortcut("ctrl-v");wait(b"clipboard role=2 action=32")
                shortcut("ctrl-l");type_text("/fat/clock-laps.txt");shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                shortcut("meta_l-spc");wait(b"launcher opened");type_text("Clock Stopwatch");qmp.key("ret");wait(b"launcher window=")
                qmp.mouse(613-pointer[0],568-pointer[1]);pointer[:]=[613,568]
                qmp.mouse(0,0,1);qmp.mouse(-280,-200,1);qmp.mouse(0,0,0);pointer[0]-=280;pointer[1]-=200;time.sleep(.5);shot("clock-compact-stopwatch")
                shortcut("ctrl-1");wait(b"deskclock: page=timer");shortcut("tab");shortcut("ctrl-a");type_text("bad");shortcut("ctrl-ret");wait(b"deskclock: invalid duration");shot("clock-compact-timer")
                shortcut("ctrl-a");type_text("1");shortcut("ctrl-ret");wait(b"deskclock: timer running=1")
                wait_all([b"deskclock: timer finished",b"deskclock: notification result=1"])
                shortcut("meta_l-shift-n");wait(b"notifications opened count=2");click(960,522);wait(b"notification action=4 count=0");shot("clock-notifications-empty");shortcut("esc")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
            from exfat_test_image import ExfatFixture
            fixture=ExfatFixture((tmp/"disk.exfat").read_bytes())
            assert fixture.read("clock-input-proof.txt")==b"Clock kept focus. Still typing."
            assert fixture.read("clock-time.txt")==b"00:00:02"
            def format_lap(value):
                cs=int(value)//10000
                return f"{cs//6000:02}:{cs//100%60:02}.{cs%100:02}"
            laps=re.findall(rb"deskclock: lap=(\d+) total_us=(\d+) split_us=(\d+)",output)
            assert len(laps)==2
            expected="Lap\tElapsed\tSplit\n"+"".join(f"{int(n)}\t{format_lap(total)}\t{format_lap(split)}\n" for n,total,split in laps)
            assert fixture.read("clock-laps.txt")==expected.encode()
    print(f"desktop-clock: PASS timer, close cancellation, background completion, nonintrusive toast, notification history, exact clipboard time/laps, compact layouts, validation and clean exit. Evidence: {out}")



if __name__=="__main__": main()
