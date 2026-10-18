#!/usr/bin/env python3
"""Exercise all eight desktop slots, launch refusal/reuse and input under load in a disposable VM."""
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
    width,height=1280,800
    root=Path(__file__).resolve().parents[1]
    out=root/"build/review/desktop-capacity"
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-capacity-") as directory:
        tmp=Path(directory)
        shutil.copyfile(root/"build/srvros.exfat",tmp/"disk.exfat")
        iso=build_iso(str(root),str(tmp),width,height,os.environ.copy())
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
                def shot(name):
                    time.sleep(.5);qmp.call("screendump",{"filename":str(out/f"{name}.ppm")})
                wait(b"srv> ",100)
                serial.sendall(b"ps\n");wait(b"pid name status");wait(b"srv> ")
                baseline_processes=set(re.findall(rb"^(\d+) (\S+) (background|exited|foreground)",output.split(b"pid name status")[-1].split(b"srv> ")[0],re.M))
                if not baseline_processes: raise RuntimeError("Missing baseline process snapshot")
                serial.sendall(b"run /fat/bin/deskd --apps-trace\n")
                wait(b"deskd: commit")
                applications=[("Files",1),("Calculator",3),("Terminal",6),("Preview",7),("Sketch",10),("Clock",9),("Find Files",12)]
                pids=[]
                def launch(name,number):
                    shortcut("meta_l-spc");wait(b"launcher opened");type_text(name+" new app");qmp.key("ret");wait(f"launcher application={number}".encode())
                for index,(name,number) in enumerate(applications):
                    launch(name,number);start=consumed;wait(b"deskd: commit")
                    registered=re.findall(rb"deskd: opened pid=(\d+) window=(\d+)",output[start:consumed])
                    if not registered: raise RuntimeError("Missing client registration")
                    pid,slot=registered[-1];assert int(slot)==index+1;pids.append(pid)
                    if name=="Terminal":
                        type_text("exit\n");wait(b"deskterm: shell exited status=0")
                    shortcut("meta_l-alt-"+("left" if index%2 else "right"));wait(b"arranged pid="+pid)
                shot("eight-windows")
                # Window heading contains all eight entries plus its commands.
                shortcut("f10");shortcut("left");shot("full-window-menu");shortcut("esc");shortcut("esc")
                launch("System",4)
                # Reopening the launcher is an ordered event fence. Refusal must
                # leave exactly eight windows, with no ninth process registered.
                shortcut("meta_l-spc");wait(b"launcher opened items=20");shot("capacity-refused");shortcut("esc")
                assert len(re.findall(rb"deskd: opened pid=",output))==8
                # Free the last slot and prove the previously refused app opens.
                shortcut("meta_l-w");wait(b"deskd: retired pid="+pids[-1])
                launch("System",4);start=consumed;wait(b"deskd: commit")
                registered=re.findall(rb"deskd: opened pid=(\d+) window=(\d+)",output[start:consumed])
                assert registered and registered[-1][1]==b"7"
                shortcut("meta_l-alt-left");wait(b"arranged pid="+registered[-1][0]);shot("reused-slot")
                # Focus the existing Text window; do not consume another slot.
                shortcut("meta_l-spc");wait(b"launcher opened items=20");type_text("Text Untitled");qmp.key("ret");wait(b"launcher window=")
                type_text("Eight windows remain responsive.")
                shortcut("ctrl-l");type_text("/fat/capacity-proof.txt");shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                shortcut("meta_l-shift-3");wait(b"deskd: screenshot saved status=0 path=",80);shot("capacity-capture")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",90);wait(b"srv> ")
                cleanup_deadline=time.monotonic()+10
                while True:
                    serial.sendall(b"ps\n");wait(b"pid name status");wait(b"srv> ")
                    process_lines=output.split(b"pid name status")[-1].split(b"srv> ")[0]
                    processes=set(re.findall(rb"^(\d+) (\S+) (background|exited|foreground)",process_lines,re.M))
                    if processes==baseline_processes: break
                    if time.monotonic()>cleanup_deadline: raise RuntimeError("Process set did not return to baseline after full-session cleanup")
                    time.sleep(.5)
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
            from exfat_test_image import ExfatFixture
            fixture=ExfatFixture((tmp/"disk.exfat").read_bytes())
            assert fixture.read("capacity-proof.txt")==b"Eight windows remain responsive."
            screenshots=re.findall(rb"deskd: screenshot saved status=0 path=([^\r\n ]+)",output)
            assert len(screenshots)==1
            capture=fixture.read(screenshots[0].decode().removeprefix("/fat/"))
            assert capture[:2]==b"BM" and len(capture)>width*height*3
    print(f"desktop-capacity: PASS eight simultaneous clients, ninth launch refusal, slot reuse, full Window menu, exact saved text, screenshot worker at capacity and complete cleanup. Evidence: {out}")


if __name__=="__main__": main()
