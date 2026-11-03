#!/usr/bin/env python3
"""Exercise native desktop applications and persisted files in a disposable VM."""
import os
from collections import Counter
import re
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso, read_for
from desktop_foundation_smoke import QMP


def selection_color(path, x, y):
    """Dominant colored pixel in a selected Text row (not desktop chrome)."""
    data=path.read_bytes()
    match=re.match(rb"P6\s+(\d+)\s+(\d+)\s+255\s",data)
    if not match:
        raise AssertionError(f"Unexpected QEMU PPM: {path}")
    width,height=map(int,match.groups());pixels=data[match.end():]
    assert len(pixels)==width*height*3
    colors=Counter()
    for py in range(max(0,y),min(height,y+44)):
        for px in range(max(0,x),min(width,x+300)):
            offset=(py*width+px)*3;rgb=tuple(pixels[offset:offset+3])
            if min(rgb)>100 and max(rgb)-min(rgb)>12:
                colors[rgb]+=1
    assert colors, f"No selection color in {path}"
    color,count=colors.most_common(1)[0]
    assert count>=40, f"No substantial selection fill in {path}: {color} ({count})"
    return color


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
                        if marker == b"srv> " and not consumed:
                            qmp.boot_if_menu_stalled(output, time.monotonic() - (end - timeout))
                        if time.monotonic()>end or process.poll() is not None:
                            qmp.call("screendump",{"filename":str(out/"failure.ppm")})
                            qmp.call("stop")
                            shutil.copyfile(tmp/"disk.exfat",out/"failure.exfat")
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
                original=re.findall(rb"deskd: opened pid=(\d+) window=\d+ id=(\d+) parent=0",output)
                assert len(original)==1, "Expected the original Text window"
                text_pid,text_window=original[0]
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
                # Files starts in Home. Select the fixture root explicitly;
                # subsequent open/save assertions use absolute /fat paths.
                shortcut("ctrl-l");type_text("/fat");qmp.key("ret")
                wait(b"files-location path=/fat history=")
                # The Places sidebar moved the name field right of x=221.
                # x=180 hits outside it and leaves the default "New folder" name.
                click(300,550);shortcut("ctrl-a");type_text("GUI folder");click(700,553)
                wait(b"file-result role=files op=4 status=0");wait(b"file-result role=files op=3 status=0")
                shortcut("ctrl-l");type_text("/fat/GUI folder");qmp.key("ret")
                wait(b"file-result role=files op=3 status=0");shot("new-folder")
                shortcut("alt-up");wait(b"file-result role=files op=3 status=0")
                type_text("desktop-smoke");qmp.key("ret")
                wait(b"deskd: document-route reply pid="+text_pid+b" accepted=1")
                wait(b"text-runtime: admitted window=");wait(b"\n")
                routed=re.findall(rb"text-runtime: admitted window=(\d+)",output)[-1]
                assert routed!=text_window, "Routed document must have its own window"
                assert re.search(rb"deskd: opened pid="+text_pid+rb" window=\d+ id="+routed+rb" parent=0(?:\r?\n)",output), "Routed window must belong to the existing Text process"
                wait(b"file-result role=text op=1 status=0")
                shot("opened-from-files");output+=read_for(serial,.1)
                shortcut("ctrl-w");wait(b"text-runtime: closed window="+routed+b" ")
                assert b"deskd: child-exit pid="+text_pid+b" " not in output, "Closing the routed window must preserve the original Text process"
                assert b"text-runtime: closed window="+text_window+b" " not in output, "The original Text window must remain open"
                # Calculator is a separate process and receives raw editing/navigation keys.
                click(590,750);wait(b"desktools: starting role=calculator");wait(b"deskd: commit")
                type_text("2 + 3 * (4 - 1)");qmp.key("ret");shot("calculator")
                shortcut("ctrl-a");click(178,22);shot("edit-menu");click(200,162);wait(b"clipboard role=3 action=8")
                click(690,750);wait(b"desktools: starting role=system");shot("system")
                click(790,750);wait(b"desktools: starting role=settings");wait(b"deskd: geometry pid=");wait(b"\n");shot("settings")
                geometry=re.findall(rb"deskd: geometry pid=(\d+) window=(\d+) frame=(\d+),(\d+),(\d+),(\d+)",output)
                assert geometry, "Settings geometry missing from trace"
                _,_,sx,sy,sw,sh=map(int,geometry[-1]);client_w=sw-2;client_h=sh-45
                click(210,240);time.sleep(.3);click(490,750);shot("larger-text")
                # Select existing Text content: its selection fill is widget-owned,
                # so a changed pixel proves the already-running client repainted.
                click(90,225);shortcut("ctrl-a");shot("theme-before")
                before=selection_color(out/"theme-before.ppm",49,64+44+102)
                assert before==(0xc9,0xe1,0xf6), f"Expected Blue selection, got {before}"
                click(790,750)
                # Drag the Settings scrollbar to the bottom; coordinates derive
                # from the traced frame and the widget's documented viewport.
                track_x=sx+1+client_w-22;track_y=sy+44+64;view_h=client_h-108
                qmp.mouse(track_x-pointer[0],track_y+20-pointer[1]);pointer[:]=[track_x,track_y+20]
                qmp.mouse(0,0,1);qmp.mouse(0,view_h+30,1);qmp.mouse(0,0,0);pointer[1]+=view_h+30
                # Appearance rows are 36px high; choose Teal and Clear.
                offset=1514-view_h
                teal_y=sy+44+64-offset+460+760+36+72+16
                clear_y=sy+44+64-offset+460+760+36+180+28+16
                click(sx+45,teal_y);wait(b"theme_accent=2 glass_level=1")
                click(sx+45,clear_y);wait(b"theme_accent=2 glass_level=2")
                wait(b"status=1 ");shot("theme-settings-saved")
                click(490,750);shot("theme-existing-window")
                teal=selection_color(out/"theme-existing-window.ppm",49,64+44+102)
                assert teal==(0xc6,0xe7,0xdf), f"Existing Text selection did not adopt Teal: {teal}"
                shortcut("ctrl-shift-n");wait(b"text-runtime: admitted window=");wait(b"\n")
                new_window=re.findall(rb"text-runtime: admitted window=(\d+)",output)[-1]
                type_text("Theme sample");shortcut("ctrl-a");shot("theme-new-window")
                # Find the new window's screen origin from its server trace.
                frames=re.findall(rb"deskd: geometry pid="+text_pid+rb" window=\d+ frame=(\d+),(\d+),(\d+),(\d+)",output)
                assert frames, "New Text window geometry missing"
                nx,ny,_,_=map(int,frames[-1])
                assert selection_color(out/"theme-new-window.ppm",nx+17,ny+44+102)==teal, "New Text window did not inherit Teal"
                qmp.key("backspace");shortcut("ctrl-w");wait(b"text-runtime: closed window="+new_window+b" ")
                click(490,750)
                # Minimize and restore through the dock, then zoom and restore.
                click(666,86);shot("text-minimized");click(490,750)
                click(700,86);wait(b"maximized=1");shot("text-zoomed")
                click(1216,74);wait(b"maximized=0");shot("text-restored")
                shortcut("ctrl-n");shortcut("ctrl-l");shortcut("ctrl-a");type_text("/fat/desktop-empty.txt")
                shortcut("ctrl-shift-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                shortcut("ctrl-n");shortcut("ctrl-v");wait(b"clipboard role=2 action=32")
                shortcut("ctrl-a");shortcut("ctrl-x");wait(b"clipboard role=2 action=16")
                shortcut("ctrl-z");shot("shared-clipboard")
                shortcut("ctrl-l");shortcut("ctrl-a");type_text("/fat/desktop-clipboard.txt")
                shortcut("ctrl-shift-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                # Save As must ask before replacing another file, and Cancel keeps edits.
                shortcut("ctrl-n");type_text("Original replacement target")
                shortcut("ctrl-l");shortcut("ctrl-a");type_text("/fat/desktop-replace.txt")
                shortcut("ctrl-shift-s");wait(b"picker ready mode=2");shortcut("ctrl-a");type_text("desktop-replace.txt");qmp.key("ret")
                wait(b"file-result role=text op=2 status=0")
                shortcut("ctrl-n");type_text("Updated replacement target")
                shortcut("ctrl-l");shortcut("ctrl-a");type_text("/fat/desktop-replace.txt")
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
                serial.sendall(b"cat /fat/home/desktop.conf\n")
                wait(b"theme_accent=2\r\nglass_level=2");wait(b"srv> ")
                serial.sendall(b"cat /fat/desktop-smoke.txt\n")
                wait(b"Hello, SrvrOS!\r\nNative desktop text. More. Kept.")
                wait(b"srv> ")
                serial.sendall(b"cat /fat/desktop-clipboard.txt\n")
                wait(b"2 + 3 * (4 - 1)");wait(b"srv> ")
                serial.sendall(b"cat /fat/desktop-replace.txt\n");wait(b"Updated replacement target");wait(b"srv> ")
                # A new desktop process must reload the saved theme from disk.
                serial.sendall(b"run /fat/bin/deskd --apps-trace\n")
                wait(b"status=1 text=20");wait(b"theme_accent=2 glass_level=2")
                time.sleep(1);shortcut("f10");shortcut("end");shortcut("ret")
                wait(b"deskd: exited",60);wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
    print(f"desktop-apps: PASS native tools, Open/Save/Save As, replacement cancel/verified overwrite, folder navigation, unsaved cancel, shared clipboard/cut undo, text-size broadcast, live and persisted theme, minimize/zoom restore, empty save and global menus. Evidence: {out}")


if __name__=="__main__": main()
