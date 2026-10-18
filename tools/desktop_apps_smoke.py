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
    out=root/"build/review/desktop-apps"
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-apps-") as directory:
        tmp=Path(directory)
        shutil.copyfile(root/"build/srvros.exfat",tmp/"disk.exfat")
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy())
        command=["qemu-system-x86_64","-M","q35","-m","512M","-cdrom",str(iso),"-boot","d",
            "-drive",f"if=none,id=disk,file={tmp/'disk.exfat'},format=raw",
            "-device","ich9-ahci,id=ahci","-device","ide-hd,drive=disk,bus=ahci.0",
            "-device","qemu-xhci,id=xhci","-device","usb-kbd,bus=xhci.0",
            "-serial",f"tcp:127.0.0.1:{port},server=on,wait=off",
            "-qmp",f"unix:{tmp/'qmp'},server=on,wait=off","-display","none","-monitor","none","-no-reboot"]
        with (out/"qemu.log").open("wb") as errors:
            process=subprocess.Popen(command,stdout=errors,stderr=errors)
            try:
                serial=connect_serial(port,20);serial.settimeout(.2);qmp=QMP(tmp/"qmp")
                def wait(marker,timeout=40):
                    nonlocal output,consumed
                    end=time.monotonic()+timeout
                    while marker not in output[consumed:]:
                        if time.monotonic()>end or process.poll() is not None:
                            qmp.call("screendump",{"filename":str(out/"failure.ppm")})
                            raise RuntimeError(f"Missing {marker!r}; see {out}")
                        try:
                            chunk=serial.recv(16384)
                            if not chunk: raise RuntimeError("serial disconnected")
                            output+=chunk
                        except socket.timeout: pass
                    consumed=output.index(marker,consumed)+len(marker)
                def shortcut(keys):
                    qmp.call("human-monitor-command",{"command-line":f"sendkey {keys}"});time.sleep(.3)
                def type_text(text):
                    mapping={" ":("spc",False),"/":("slash",False),".":("dot",False),
                        "-":("minus",False),"_":("minus",True),"\n":("ret",False),
                        ",":("comma",False),"!":("1",True),"+":("equal",True),
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
                type_text("Hello, SrvrOS!\nNative desktop text.")
                qmp.key("left");qmp.key("right")
                shortcut("ctrl-l");shortcut("ctrl-a");type_text("/fat/desktop-smoke.txt")
                # Global File menu uses keyboard navigation and app commands.
                shortcut("f10");shortcut("right");shortcut("down");shortcut("down");shot("file-menu")
                shortcut("ret");wait(b"picker ready mode=2");shot("save-picker");qmp.key("ret")
                wait(b"file-result role=text op=2 status=0")
                shot("text-saved")
                shortcut("ctrl-o");wait(b"picker ready mode=1");shot("open-picker")
                type_text("desktop-smoke");qmp.key("ret");wait(b"file-result role=text op=1 status=0");shortcut("ctrl-end")
                # Close with unsaved changes, then Escape must preserve the window.
                type_text(" More.");shortcut("ctrl-w");shot("unsaved-dialog")
                qmp.key("esc");type_text(" Kept.");shortcut("ctrl-s")
                wait(b"file-result role=text op=2 status=0")
                # Files lists asynchronously; open a saved file using its location field.
                click(390,750);wait(b"desktools: starting role=files")
                wait(b"file-result role=files op=3 status=0");shot("files")
                click(180,550);shortcut("ctrl-a");type_text("GUI folder");click(700,553)
                wait(b"file-result role=files op=4 status=0");wait(b"file-result role=files op=3 status=0")
                shortcut("ctrl-l");type_text("/fat/GUI folder");qmp.key("ret")
                wait(b"file-result role=files op=3 status=0");shot("new-folder")
                shortcut("alt-up");wait(b"file-result role=files op=3 status=0")
                type_text("desktop-smoke");qmp.key("ret")
                wait(b"desktools: starting role=text");wait(b"file-result role=text op=1 status=0")
                shot("opened-from-files");shortcut("ctrl-w");wait(b"desktools: exited role=text failure=0")
                # Calculator is a separate process and receives raw editing/navigation keys.
                click(590,750);wait(b"desktools: starting role=calculator");wait(b"deskd: commit")
                type_text("2 + 3 * (4 - 1)");qmp.key("ret");shot("calculator")
                shortcut("ctrl-a");click(178,22);shot("edit-menu");click(200,162);wait(b"clipboard role=3 action=8")
                click(690,750);wait(b"desktools: starting role=system");shot("system")
                click(790,750);wait(b"desktools: starting role=settings");shot("settings")
                click(210,240);time.sleep(.3);click(490,750);shot("larger-text")
                # Minimize and restore through the dock, then zoom and restore.
                click(666,86);shot("text-minimized");click(490,750)
                click(700,86);shot("text-zoomed");click(1078,74);shot("text-restored")
                shortcut("ctrl-n");shortcut("ctrl-l");shortcut("ctrl-a");type_text("/fat/desktop-empty.txt")
                shortcut("ctrl-shift-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                shortcut("ctrl-n");shortcut("ctrl-v");wait(b"clipboard role=2 action=32")
                shortcut("ctrl-a");shortcut("ctrl-x");wait(b"clipboard role=2 action=16")
                shortcut("ctrl-z");shot("shared-clipboard")
                shortcut("ctrl-l");shortcut("ctrl-a");type_text("/fat/desktop-clipboard.txt")
                shortcut("ctrl-shift-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                # Save As must ask before replacing another file, and Cancel keeps edits.
                shortcut("ctrl-n");type_text("Original replacement target")
                shortcut("ctrl-shift-s");wait(b"picker ready mode=2");shortcut("ctrl-a");type_text("desktop-replace.txt");qmp.key("ret")
                wait(b"file-result role=text op=2 status=0")
                shortcut("ctrl-n");type_text("Updated replacement target")
                shortcut("ctrl-shift-s");wait(b"picker ready mode=2");shortcut("ctrl-a");type_text("desktop-replace.txt");qmp.key("ret")
                wait(b"file-result role=text op=2 status=2");shot("replace-confirmation");qmp.key("esc")
                shortcut("ctrl-shift-s");wait(b"picker ready mode=2");qmp.key("ret")
                wait(b"file-result role=text op=2 status=2");shortcut("shift-tab");qmp.key("ret")
                wait(b"file-result role=text op=2 status=0");shot("replacement-saved")
                # Browse inside the Open picker, then cancel without touching the document.
                shortcut("ctrl-o");wait(b"picker ready mode=1");type_text("GUI folder");qmp.key("ret")
                wait(b"picker ready mode=1");shot("picker-folder");shortcut("alt-up");wait(b"picker ready mode=1");qmp.key("esc")
                # Return through the global menu; saved documents close normally.
                shortcut("f10");shortcut("end");shortcut("ret")
                wait(b"deskd: exited",60);wait(b"srv> ")
                serial.sendall(b"cat /fat/desktop-smoke.txt\n")
                wait(b"Hello, SrvrOS!\r\nNative desktop text. More. Kept.")
                wait(b"srv> ")
                serial.sendall(b"cat /fat/desktop-clipboard.txt\n")
                wait(b"2 + 3 * (4 - 1)");wait(b"srv> ")
                serial.sendall(b"cat /fat/desktop-replace.txt\n");wait(b"Updated replacement target");wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
    print(f"desktop-apps: PASS native tools, Open/Save/Save As, replacement cancel/verified overwrite, folder navigation, unsaved cancel, shared clipboard/cut undo, text-size broadcast, minimize/zoom restore, empty save and global menus. Evidence: {out}")


if __name__=="__main__": main()
