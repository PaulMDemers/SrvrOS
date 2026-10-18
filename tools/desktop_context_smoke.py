#!/usr/bin/env python3
"""Exercise native contextual menus, selection/clipboard isolation and Files actions in a disposable VM."""
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
    out=root/"build/review/desktop-context"
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-context-") as directory:
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
                type_text("Context proof.");shortcut("ctrl-a");right_click(100,230);wait(b"context opened role=2 target=5");shot("text-context")
                qmp.key("c");qmp.key("c");qmp.key("ret");wait(b"clipboard role=2 action=8")
                qmp.key("right");control_click(150,230);wait(b"context opened role=2 target=5");qmp.key("p");qmp.key("ret");wait(b"clipboard role=2 action=32")
                shortcut("shift-f10");wait(b"context opened role=2 target=5");shortcut("end");shortcut("ret");qmp.key("right");type_text(" Done.")
                shortcut("ctrl-l");type_text("/fat/context-proof.txt");shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                click(250,230);shortcut("ctrl-end");right_click(150,230);wait(b"context opened role=2 target=5")
                click(80,138);type_text(" Kept.");shortcut("ctrl-s");wait(b"file-result role=text op=2 status=0")
                # The outside press on New must only dismiss the popup.
                qmp.mouse(749-pointer[0],584-pointer[1]);pointer[:]=[749,584]
                qmp.mouse(0,0,1);qmp.mouse(-440,-240,1);qmp.mouse(0,0,0);pointer[0]-=440;pointer[1]-=240;time.sleep(.5)
                shortcut("shift-f10");wait(b"context opened role=2 target=5");shot("text-context-compact");shortcut("esc")
                right_click(1000,600);wait(b"desktop context opened");shot("desktop-context");shortcut("home");shortcut("ret")
                wait(b"desktools: starting role=files");wait(b"file-result role=files op=3 status=0")
                type_text("context-proof");shortcut("shift-f10");wait(b"context opened role=1 target=9");shot("files-context");qmp.key("r");qmp.key("ret");wait(b"files-prompt operation=5")
                type_text("context-renamed.txt");qmp.key("ret");wait(b"file-result role=files op=5 status=0");wait(b"file-result role=files op=3 status=0")
                shortcut("shift-f10");wait(b"context opened role=1 target=9");qmp.key("d");qmp.key("ret");wait(b"files-prompt operation=7");shortcut("esc")
                shortcut("shift-f10");wait(b"context opened role=1 target=9");qmp.key("d");qmp.key("d");qmp.key("ret");wait(b"files-delete-confirm");shortcut("esc");wait(b"files-delete-canceled")
                right_click(1000,600);wait(b"desktop context opened");qmp.key("s");qmp.key("ret");wait(b"desktools: starting role=settings");wait(b"deskd: commit");shot("settings-from-context")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
            from exfat_test_image import ExfatFixture
            fixture=ExfatFixture((tmp/"disk.exfat").read_bytes())
            assert fixture.read("context-renamed.txt")==b"Context proof.Context proof. Done. Kept."
    print(f"desktop-context: PASS right-click/Control-click/Shift-F10, selected-text copy/paste, outside-press isolation, compact menus, desktop launch, Files rename and safe duplicate/delete cancellation. Evidence: {out}")



if __name__=="__main__": main()
