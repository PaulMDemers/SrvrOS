#!/usr/bin/env python3
"""Exercise native text scrollbars, selection preservation and exact saved content in a disposable VM."""
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from desktop_foundation_smoke import QMP


def main():
    root=Path(__file__).resolve().parents[1]
    out=root/"build/review/desktop-edit-scroll"
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-edit-scroll-") as directory:
        tmp=Path(directory)
        from exfat_test_image import ExfatFixture
        payload="".join(f"Line {i:03d}: "+"The quick brown fox jumps over the lazy dog. "*3+f" TAIL-{i:03d}\n" for i in range(200)).encode()
        fixture=ExfatFixture((root/"build/srvros.exfat").read_bytes())
        fixture.add_directory_files("scroll-fixture",{"long.txt":payload})
        (tmp/"disk.exfat").write_bytes(fixture.data)
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
                def shot(name):
                    time.sleep(.5);qmp.call("screendump",{"filename":str(out/f"{name}.ppm")})
                wait(b"srv> ",100)
                serial.sendall(b"run /fat/bin/deskd --apps-trace\n")
                wait(b"desktools: starting role=text");wait(b"deskd: commit")
                shortcut("ctrl-l");type_text("/fat/scroll-fixture/long.txt");qmp.key("ret");wait(b"file-result role=text op=1 status=0");time.sleep(.5);shot("long-document")
                def drag(x,y,dx,dy):
                    qmp.mouse(x-pointer[0],y-pointer[1]);pointer[:]=[x,y]
                    qmp.mouse(0,0,1);qmp.mouse(dx,dy,1);qmp.mouse(0,0,0);pointer[0]+=dx;pointer[1]+=dy;time.sleep(.4)
                drag(727,220,0,300);shot("vertical-end")
                drag(56,536,660,0);shot("horizontal-end")
                # The original caret is still at byte zero despite both thumb
                # drags. Typing must reveal it and prepend, never edit the tail.
                type_text("Prefix ");shot("caret-revealed")
                shortcut("ctrl-l");type_text("/fat/scroll-proof.txt");shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                shortcut("ctrl-end");type_text("Suffix");shortcut("ctrl-s");wait(b"file-result role=text op=2 status=0")
                shortcut("ctrl-a");shortcut("ctrl-c");wait(b"clipboard role=2 action=8")
                shortcut("ctrl-n");shortcut("ctrl-v");wait(b"clipboard role=2 action=32")
                shortcut("ctrl-l");type_text("/fat/scroll-clipboard.txt");shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                qmp.mouse(749-pointer[0],584-pointer[1]);pointer[:]=[749,584]
                qmp.mouse(0,0,1);qmp.mouse(-440,-240,1);qmp.mouse(0,0,0);pointer[0]-=440;pointer[1]-=240;time.sleep(.5)
                shortcut("ctrl-end");shot("scrollbars-compact")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
            fixture=ExfatFixture((tmp/"disk.exfat").read_bytes())
            assert fixture.read("scroll-fixture/long.txt")==payload
            assert fixture.read("scroll-proof.txt")==b"Prefix "+payload+b"Suffix"
            assert fixture.read("scroll-clipboard.txt")==b"Prefix "+payload+b"Suffix"
    print(f"desktop-edit-scroll: PASS both thumb axes, caret preserved while scrolling and revealed by typing, 200-line document clipboard, compact resize, exact saved bytes and clean exit. Evidence: {out}")


if __name__=="__main__": main()
