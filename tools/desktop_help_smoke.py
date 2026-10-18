#!/usr/bin/env python3
"""Exercise native Help, topic navigation, global integration and exact page clipboard in a disposable VM."""
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
    out=root/"build/review/desktop-help"
    out.mkdir(parents=True,exist_ok=True)
    subprocess.run(["make","build/tests/help-app-host"],cwd=root,check=True,stdout=subprocess.DEVNULL)
    expected=subprocess.check_output([str(root/"build/tests/help-app-host"),"--dump","keyboard"])
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-help-") as directory:
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
                wait(b"desktools: starting role=text");wait_all([b"deskd: commit",b"text history=multi-step"])
                # F1 opens the active application's guide directly.
                shortcut("f1");wait_all([b"deskhelp: page=text",b"deskhelp: starting"]);wait(b"deskd: commit")
                shot("help-text")
                shortcut("ctrl-l");wait(b"deskhelp: topics opened");type_text("keyboard");qmp.key("ret");wait(b"deskhelp: page=keyboard")
                qmp.key("pgdn");wait(b"deskhelp: scroll=");shot("help-keyboard")
                shortcut("alt-left");wait(b"deskhelp: history topic=text")
                shortcut("alt-right");wait(b"deskhelp: history topic=keyboard")
                shortcut("ctrl-c");wait(b"deskhelp: copy-page result=1")
                click(490,750);shortcut("ctrl-v");wait(b"clipboard role=2 action=32")
                shortcut("ctrl-l");type_text("/fat/help-proof.txt");shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                shortcut("meta_l-spc");wait(b"launcher opened");type_text("Help Keyboard");qmp.key("ret");wait(b"launcher window=")
                shortcut("ctrl-l");wait(b"deskhelp: topics opened");type_text("preview");qmp.key("ret");wait(b"deskhelp: page=preview")
                # The related-app action uses the normal desktop launch protocol.
                click(368,158);wait(b"deskview: starting");wait(b"deskd: commit");shortcut("ctrl-w");wait(b"deskview: exited failure=0")
                shortcut("meta_l-spc");wait(b"launcher opened");type_text("Help Preview");qmp.key("ret");wait(b"launcher window=")
                # Query copy/paste is separate from copying the displayed page.
                shortcut("ctrl-l");wait(b"deskhelp: topics opened");type_text("desktop");shortcut("ctrl-a");shortcut("ctrl-c");time.sleep(.5)
                type_text("wrong");shortcut("ctrl-a");shortcut("ctrl-v");time.sleep(.5);qmp.key("ret");wait(b"deskhelp: page=desktop")
                qmp.key("end");wait(b"deskhelp: scroll=");shot("help-desktop-bottom")
                # Shrink to the minimum width, keeping the toolbar and document usable.
                qmp.mouse(813-pointer[0],668-pointer[1]);pointer[:]=[813,668]
                qmp.mouse(0,0,1);qmp.mouse(-480,-300,1);qmp.mouse(0,0,0);pointer[0]-=480;pointer[1]-=300;time.sleep(.5)
                qmp.key("home");wait(b"deskhelp: scroll=0");shot("help-compact")
                shortcut("ctrl-l");wait(b"deskhelp: topics opened");type_text("about");shot("help-compact-topics");qmp.key("ret");wait(b"deskhelp: page=about")
                shortcut("ctrl-w");wait(b"deskhelp: exited failure=0")
                # About is now a real guide page, accessed through the global menu.
                shortcut("f10");shortcut("ret");wait(b"deskhelp: page=about");wait(b"deskhelp: starting");wait(b"deskd: commit");shot("help-about")
                shortcut("ctrl-w");wait(b"deskhelp: exited failure=0")
                shortcut("meta_l-spc");wait(b"launcher opened");type_text("Help new app");qmp.key("ret");wait(b"launcher application=8");wait(b"deskhelp: page=welcome");wait(b"deskd: commit");shot("help-welcome")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
            from exfat_test_image import ExfatFixture
            actual=ExfatFixture((tmp/"disk.exfat").read_bytes()).read("help-proof.txt")
            assert actual==expected, f"Copied page mismatch: {len(actual)} != {len(expected)}"
    print(f"desktop-help: PASS F1 context, topic search, history, scrolling, exact page clipboard, related-app launch, query clipboard, compact layout, About/global menu, launcher and clean exit. Evidence: {out}")


if __name__=="__main__": main()
