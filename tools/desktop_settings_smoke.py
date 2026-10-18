#!/usr/bin/env python3
"""Exercise native desktop applications and persisted files in a disposable VM."""
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
    out=root/"build/review/desktop-settings"
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-settings-") as directory:
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
                click(790,750);wait(b"desktools: starting role=settings");wait(b"deskd: commit");shot("settings-default")
                # Second window content starts at (57,132); shared radio labels are clickable.
                click(210,260);wait(b"text=20 offset=0 flags=3 wallpaper=0")
                wait(b"status=1 text=20 offset=0 flags=3 wallpaper=0")
                click(110,382);wait(b"text=20 offset=0 flags=2 wallpaper=0")
                click(110,418);wait(b"text=20 offset=0 flags=0 wallpaper=0")
                click(140,496);shortcut("ctrl-a");type_text("-04:00");qmp.key("ret")
                wait(b"text=20 offset=-240 flags=0 wallpaper=0")
                wait(b"status=1 text=20 offset=-240 flags=0 wallpaper=0");shot("clock-offset")
                # Resize shorter, then Page Down scrolls the shared viewport; pick Sand.
                qmp.mouse(613-pointer[0],688-pointer[1]);pointer[:]=[613,688]
                qmp.mouse(0,0,1);qmp.mouse(0,-80,1);qmp.mouse(0,0,0);pointer[1]-=80;time.sleep(.5)
                shortcut("pgdn");shot("settings-scrolled");click(225,535)
                wait(b"text=20 offset=-240 flags=0 wallpaper=1")
                wait(b"status=1 text=20 offset=-240 flags=0 wallpaper=1");shot("settings-saved")
                shortcut("f10");shortcut("end");shortcut("ret")
                wait(b"deskd: exited",60);wait(b"srv> ")
                serial.sendall(b"cat /fat/home/desktop.conf\n")
                wait(b"srvros-desktop-v1\r\ntext_size=20\r\nutc_offset_minutes=-240\r\nclock_flags=0\r\nwallpaper=1")
                wait(b"srv> ")
                # A fresh desktop process reloads disk, not inherited process state.
                pointer[:]=[24,56]
                serial.sendall(b"run /fat/bin/deskd --apps-trace\n")
                wait_all([b"status=1 text=20 offset=-240 flags=0 wallpaper=1", b"deskd: commit"])
                click(790,750);wait(b"desktools: starting role=settings");shot("settings-restored")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
    print(f"desktop-settings: PASS shared controls, clock format/offset, live appearance, verified persistence and fresh-session reload. Evidence: {out}")


if __name__=="__main__": main()
