#!/usr/bin/env python3
"""Exercise the calendar panel, clock offset and focus isolation in a disposable VM."""
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
    out=root/"build/review/desktop-calendar"
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-calendar-") as directory:
        tmp=Path(directory)
        shutil.copyfile(root/"build/srvros.exfat",tmp/"disk.exfat")
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy())
        command=["qemu-system-x86_64","-M","q35","-m","512M","-cdrom",str(iso),"-boot","d",
            "-drive",f"if=none,id=disk,file={tmp/'disk.exfat'},format=raw",
            "-device","ich9-ahci,id=ahci","-device","ide-hd,drive=disk,bus=ahci.0",
            "-device","qemu-xhci,id=xhci","-device","usb-kbd,bus=xhci.0",
            "-serial",f"tcp:127.0.0.1:{port},server=on,wait=off",
            "-qmp",f"unix:{tmp/'qmp'},server=on,wait=off","-display","none","-monitor","none","-no-reboot","-rtc","base=2026-10-18T23:34:00,clock=vm"]
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
                type_text("Calendar kept.")
                click(1180,20);wait(b"calendar selected=2026-10-18");wait(b"calendar opened")
                shortcut("pgdn");wait(b"calendar selected=2026-11-18")
                shortcut("shift-pgup");wait(b"calendar selected=2025-11-18")
                shortcut("home");wait(b"calendar selected=2025-11-01")
                shortcut("end");wait(b"calendar selected=2025-11-30")
                shortcut("ctrl-home");wait(b"calendar selected=2026-10-18")
                shortcut("right");wait(b"calendar selected=2026-10-19")
                shortcut("down");wait(b"calendar selected=2026-10-26");shot("calendar-browse")
                shortcut("tab");qmp.key("ret");wait(b"calendar selected=2026-10-18")
                shortcut("tab");shortcut("tab");qmp.key("ret");wait(b"calendar closed")
                type_text(" Focus returned.")
                shortcut("f10");shortcut("down");shortcut("down");shortcut("down");qmp.key("ret");wait(b"calendar opened")
                click(731,85);wait(b"calendar closed")
                if b"desktools: exited role=text" in output: raise RuntimeError("Calendar outside click leaked to client close")
                click(1180,20);wait(b"calendar opened");shortcut("meta_l-spc");wait(b"calendar closed");wait(b"launcher opened")
                type_text("Settings");qmp.key("ret");wait(b"desktools: starting role=settings");wait(b"deskd: commit")
                click(140,496);shortcut("ctrl-a");type_text("+02:00");qmp.key("ret")
                wait(b"status=1 text=16 offset=120 flags=3 wallpaper=0")
                click(1180,20);wait(b"calendar selected=2026-10-19");wait(b"calendar opened");shot("calendar-offset")
                click(1088,292);wait(b"calendar selected=2026-10-21")
                click(1235,117);wait(b"calendar selected=2026-11-21")
                click(974,454);wait(b"calendar selected=2026-10-19")
                shortcut("meta_l-spc");wait(b"calendar closed");wait(b"launcher opened");type_text("Text window");qmp.key("ret");wait(b"launcher window=")
                type_text(" After calendar.")
                shortcut("ctrl-l");type_text("/fat/calendar-proof.txt");shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                serial.sendall(b"cat /fat/calendar-proof.txt\n");wait(b"Calendar kept. Focus returned. After calendar.");wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
    print(f"desktop-calendar: PASS clock/menu entry, day/month/year navigation, Today, click activation, offset day rollover, outside-click isolation, launcher switching and focus readback. Evidence: {out}")


if __name__=="__main__": main()
