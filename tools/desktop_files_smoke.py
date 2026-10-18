#!/usr/bin/env python3
"""Exercise the Files mutations and shared prompts in a disposable VM."""
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
    out=root/"build/review/desktop-files"
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-files-") as directory:
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
                type_text("Files action sample.")
                shortcut("ctrl-l");shortcut("ctrl-a");type_text("/fat/file-actions.txt");shortcut("ctrl-s")
                wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                click(390,750);wait(b"desktools: starting role=files");wait(b"file-result role=files op=3 status=0")
                type_text("file-actions");shortcut("f2");wait(b"files-prompt operation=5");shot("rename-prompt")
                type_text("renamed-actions.txt");qmp.key("ret");wait(b"file-result role=files op=5 status=0");wait(b"file-result role=files op=3 status=0")
                click(460,162);wait(b"files-prompt operation=7");shot("duplicate-prompt");qmp.key("ret")
                wait(b"file-result role=files op=7 status=0");wait(b"file-result role=files op=3 status=0");shot("copy-verified")
                click(565,162);wait(b"files-delete-confirm");shot("delete-confirm");qmp.key("ret");wait(b"files-delete-canceled")
                click(565,162);wait(b"files-delete-confirm");click(580,445)
                wait(b"file-result role=files op=6 status=0");wait(b"file-result role=files op=3 status=0")
                type_text("renamed-actions");shortcut("f2");wait(b"files-prompt operation=5");type_text("home");qmp.key("ret")
                wait(b"file-result role=files op=5 status=2");shot("collision-kept")
                shortcut("ctrl-r");wait(b"file-result role=files op=3 status=0")
                # Exercise directory creation and guarded empty-directory rename.
                click(180,550);shortcut("ctrl-a");type_text("Action folder");click(700,553)
                wait(b"file-result role=files op=4 status=0");wait(b"file-result role=files op=3 status=0")
                type_text("Action folder");shortcut("f2");wait(b"files-prompt operation=5");type_text("Renamed folder");qmp.key("ret")
                wait(b"file-result role=files op=5 status=0");wait(b"file-result role=files op=3 status=0")
                # Sort via the shared heading and inspect details.
                click(670,247);wait(b"files-sort column=3 descending=0");shot("files-details")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                serial.sendall(b"cat /fat/renamed-actions.txt\n");wait(b"Files action sample.");wait(b"srv> ")
                serial.sendall(b"ls /fat\n");wait(b"Renamed folder");wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
    print(f"desktop-files: PASS sorted metadata, shared rename/duplicate prompts, verified copy, safe delete cancel/confirm, collision preservation and empty-directory rename. Evidence: {out}")


if __name__=="__main__": main()
