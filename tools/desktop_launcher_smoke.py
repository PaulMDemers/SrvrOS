#!/usr/bin/env python3
"""Exercise the global launcher and focus isolation in a disposable VM."""
import os
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
    out=root/"build/review/desktop-launcher"
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-launcher-") as directory:
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
                wait(b"desktools: starting role=text");wait(b"deskd: commit")
                type_text("Kept.")
                shortcut("meta_l-spc");wait(b"launcher opened");type_text("Calculator");shot("launcher-search");qmp.key("esc");wait(b"launcher closed")
                type_text(" Focus returned.")
                shortcut("meta_l-spc");wait(b"launcher opened");type_text("Calculator");qmp.key("ret")
                wait(b"launcher application=3");wait(b"desktools: starting role=calculator");wait(b"deskd: commit")
                shortcut("meta_l-spc");wait(b"launcher opened");type_text("Text window");qmp.key("ret");wait(b"launcher window=")
                type_text(" Existing.")
                # Search field uses the compositor-owned clipboard, in both directions.
                shortcut("meta_l-spc");wait(b"launcher opened");type_text("Files");shortcut("ctrl-a");shortcut("ctrl-c")
                type_text("missing");shortcut("ctrl-a");shortcut("ctrl-v");qmp.key("ret")
                wait(b"launcher application=1");wait(b"desktools: starting role=files");wait(b"file-result role=files op=3 status=0")
                shortcut("meta_l-spc");wait(b"launcher opened");shot("launcher-windows")
                # Clicking the Files close box outside the overlay only cancels it.
                click(772,124);wait(b"launcher closed")
                shortcut("meta_l-spc");wait(b"launcher opened");type_text("Files window");qmp.key("ret");wait(b"launcher window=")
                if b"exited role=files" in output: raise RuntimeError("Launcher outside click leaked to window close")
                shortcut("meta_l-spc");wait(b"launcher opened");type_text("Calculator window");qmp.key("ret");wait(b"launcher window=")
                shortcut("ctrl-v");wait(b"clipboard role=3 action=32")
                shortcut("ctrl-a");type_text("System");shortcut("ctrl-a");shortcut("ctrl-c");wait(b"clipboard role=3 action=8")
                shortcut("meta_l-spc");wait(b"launcher opened");shortcut("ctrl-v");qmp.key("ret")
                wait(b"launcher application=4");wait(b"desktools: starting role=system");wait(b"system-ready")
                # Global menu access opens the same panel.
                shortcut("f10");shortcut("down");shortcut("down");shortcut("ret");wait(b"launcher opened")
                type_text("Text window");qmp.key("ret");wait(b"launcher window=")
                shortcut("ctrl-l");shortcut("ctrl-a");type_text("/fat/launcher-proof.txt");shortcut("ctrl-s")
                wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                shortcut("meta_l-spc");wait(b"launcher opened");type_text("new Text");qmp.key("ret")
                wait(b"launcher application=2");wait(b"desktools: starting role=text");wait(b"deskd: commit")
                shortcut("meta_l-spc");wait(b"launcher opened");shortcut("meta_l-spc");wait(b"launcher closed")
                shortcut("ctrl-w");wait(b"desktools: exited role=text failure=0")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                serial.sendall(b"cat /fat/launcher-proof.txt\n");wait(b"Kept. Focus returned. Existing.");wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
    print(f"desktop-launcher: PASS application/window search, keyboard/menu entry, focus restoration, click-through prevention, two-way clipboard and explicit new windows. Evidence: {out}")


if __name__=="__main__": main()
