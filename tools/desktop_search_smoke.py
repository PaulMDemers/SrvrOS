#!/usr/bin/env python3
"""Exercise the Text search controls and focus isolation in a disposable VM."""
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
    out=root/"build/review/desktop-search"
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-search-") as directory:
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
                type_text("One two ONE.\nA second line with one.")
                shortcut("ctrl-home");shortcut("ctrl-f");wait(b"search opened");type_text("one");wait(b"search match=0..3 wrapped=0")
                qmp.key("ret");wait(b"search match=8..11 wrapped=0")
                shortcut("shift-ret");wait(b"search match=0..3 wrapped=0")
                # Match case is a shared checkbox reached by Tab.
                shortcut("tab");shortcut("tab");shortcut("tab");shortcut("spc");wait(b"search match=32..35 wrapped=0")
                shortcut("ctrl-f");wait(b"search opened");type_text("ONE");wait(b"search match=8..11 wrapped=0")
                shortcut("ctrl-a");shortcut("ctrl-c");wait(b"clipboard role=2 action=8 bytes=3")
                shortcut("esc");wait(b"search closed");shortcut("ctrl-g");wait(b"search match=8..11 wrapped=1")
                type_text("THREE")
                shortcut("ctrl-f");wait(b"search opened");shortcut("ctrl-a");shortcut("ctrl-v");wait(b"clipboard role=2 action=32 bytes=3");wait(b"search no-match")
                shortcut("ctrl-a");type_text("one");wait(b"search match=34..37 wrapped=0")
                click(215,260);wait(b"search match=34..37 wrapped=0")
                shortcut("ctrl-g");wait(b"search match=0..3 wrapped=1");shot("search-bar")
                # Clicking into the document restores typing while search stays visible.
                click(500,302);type_text(" Kept.")
                shortcut("ctrl-f");wait(b"search opened");shortcut("ctrl-o");wait(b"search closed");wait(b"picker ready mode=1")
                shortcut("esc");shortcut("ctrl-f");wait(b"search opened");shortcut("esc");wait(b"search closed")
                shortcut("f10");shortcut("right");shortcut("right");shortcut("right");shortcut("ret");wait(b"search opened")
                shortcut("esc");wait(b"search closed")
                shortcut("ctrl-l");type_text("/fat/search-proof.txt");shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                shot("search-edited")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                serial.sendall(b"cat /fat/search-proof.txt\n");wait(b"One two THREE. Kept.");wait(b"A second line with one.");wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
    print(f"desktop-search: PASS incremental/next/previous/wrapped/case search, query clipboard, hidden-bar repeat, document focus, picker isolation and saved readback. Evidence: {out}")


if __name__=="__main__": main()
