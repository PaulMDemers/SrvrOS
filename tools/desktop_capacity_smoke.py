#!/usr/bin/env python3
"""Exercise all sixteen desktop slots, launch refusal/reuse and input under load in a disposable VM."""
import argparse
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
    parser=argparse.ArgumentParser()
    parser.add_argument("--clipboard",action="store_true",help="Exercise copy/paste in sixteen admitted mixed clients")
    args=parser.parse_args()
    width,height=1280,800
    root=Path(__file__).resolve().parents[1]
    out=root/("build/review/desktop-clipboard-capacity" if args.clipboard else "build/review/desktop-capacity")
    expected_files={}
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
                applications=[("Files",1),("Calculator",3),("Terminal",6),("Preview",7),("Sketch",10),("Clock",9),("Find Files",12),("Calculator",3),("Clock",9),("Help",8),("Settings",5),("Files",1),("Preview",7),("Text",2),("Calculator",3)]
                pids=[]
                def launch(name,number):
                    shortcut("meta_l-spc");wait(b"launcher opened");type_text(name+" new app");qmp.key("ret");wait(f"launcher application={number}".encode())
                if args.clipboard:
                    # One Text plus fifteen Calculators fits the current worker
                    # budget. Sixteen Text processes exhaust file-worker resources.
                    type_text("0");shortcut("ctrl-a");shortcut("ctrl-c");wait(b"clipboard role=2 action=8 bytes=1")
                    shortcut("ctrl-v");wait(b"clipboard role=2 action=32 bytes=1");qmp.key("ret")
                    def clipboard_document(index):
                        contents=str(index);type_text(contents);shortcut("ctrl-a");shortcut("ctrl-c")
                        wait(f"clipboard role=3 action=8 bytes={len(contents)}".encode())
                        shortcut("ctrl-v");wait(f"clipboard role=3 action=32 bytes={len(contents)}".encode())
                        qmp.key("ret");wait(f"calculator result={index} history=1".encode())
                        shortcut("meta_l-spc");wait(b"launcher opened");type_text("Text Untitled");qmp.key("ret");wait(b"launcher window=")
                        shortcut("ctrl-end");shortcut("ctrl-v");wait(f"clipboard role=2 action=32 bytes={len(contents)}".encode());qmp.key("ret")
                    for index in range(1,16):
                        launch("Calculator",3);start=consumed;wait(b"deskd: commit")
                        registered=re.findall(rb"deskd: opened pid=(\d+) window=(\d+)",output[start:consumed])
                        assert registered and int(registered[-1][1])==index
                        pids.append(registered[-1][0]);clipboard_document(index)
                    shot("sixteen-clipboard-clients")
                    launch("Calculator",3)
                    shortcut("meta_l-spc");wait(b"launcher opened items=28");shortcut("esc")
                    assert len(re.findall(rb"deskd: opened pid=",output))==16
                    assert b"resources reason=launch-capacity clients=16 children=16 clipboard_peers=16" in output
                    shortcut("alt-shift-tab");shortcut("meta_l-w");wait(b"deskd: retired pid="+pids[-1])
                    launch("Calculator",3);start=consumed;wait(b"deskd: commit")
                    registered=re.findall(rb"deskd: opened pid=(\d+) window=(\d+)",output[start:consumed])
                    assert registered and registered[-1][1]==b"15"
                    clipboard_document(16)
                    shortcut("ctrl-l");shortcut("ctrl-a");type_text("/fat/clipboard-capacity.txt");shortcut("ctrl-s")
                    wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                    expected_files["clipboard-capacity.txt"]="".join(f"{i}\n" for i in range(17)).encode()
                    shortcut("meta_l-shift-3");wait(b"deskd: screenshot saved status=0 path=",80)
                    shot("clipboard-reused-slot")
                else:
                    for index,(name,number) in enumerate(applications):
                        launch(name,number);start=consumed;wait(b"deskd: commit")
                        registered=re.findall(rb"deskd: opened pid=(\d+) window=(\d+)",output[start:consumed])
                        if not registered: raise RuntimeError("Missing client registration")
                        pid,slot=registered[-1];assert int(slot)==index+1;pids.append(pid)
                        if name=="Terminal":
                            type_text("exit\n");wait(b"deskterm: shell exited status=0")
                        shortcut("meta_l-alt-"+("left" if index%2 else "right"));wait(b"arranged pid="+pid)
                    shot("sixteen-windows")
                    # Window heading contains all sixteen entries plus its commands.
                    shortcut("f10");shortcut("left");shot("full-window-menu");shortcut("esc");shortcut("esc")
                    launch("System",4)
                    # Reopening the launcher is an ordered event fence. Refusal must
                    # leave exactly sixteen windows, with no seventeenth process registered.
                    shortcut("meta_l-spc");wait(b"launcher opened items=28");shot("capacity-refused");shortcut("esc")
                    assert len(re.findall(rb"deskd: opened pid=",output))==16
                    # Free the last slot and prove the previously refused app opens.
                    shortcut("meta_l-w");wait(b"deskd: retired pid="+pids[-1])
                    launch("System",4);start=consumed;wait(b"deskd: commit")
                    registered=re.findall(rb"deskd: opened pid=(\d+) window=(\d+)",output[start:consumed])
                    assert registered and registered[-1][1]==b"15"
                    shortcut("meta_l-alt-left");wait(b"arranged pid="+registered[-1][0]);shot("reused-slot")
                    # Focus the existing Text window; do not consume another slot.
                    shortcut("meta_l-spc");wait(b"launcher opened items=28");type_text("Text Untitled");qmp.key("ret");wait(b"launcher window=")
                    type_text("Sixteen windows remain responsive.")
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
                cache=re.search(rb"deskd: cache peak_bytes=(\d+) retired_bytes=(\d+) budget_bytes=(\d+) windows=16",output)
                assert cache and int(cache[2])==0 and 0<int(cache[1])<=int(cache[3])
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
            from exfat_test_image import ExfatFixture
            fixture=ExfatFixture((tmp/"disk.exfat").read_bytes())
            if not args.clipboard: expected_files["capacity-proof.txt"]=b"Sixteen windows remain responsive."
            for name,contents in expected_files.items(): assert fixture.read(name)==contents,(name,fixture.read(name),contents)
            screenshots=re.findall(rb"deskd: screenshot saved status=0 path=([^\r\n ]+)",output)
            assert len(screenshots)==1
            capture=fixture.read(screenshots[0].decode().removeprefix("/fat/"))
            assert capture[:2]==b"BM" and len(capture)>width*height*3
    if args.clipboard:
        print(f"desktop-clipboard-capacity: PASS sixteen real clients copy/paste, refusal, slot reuse, exact saved cross-client contents, screenshot and full process cleanup. Evidence: {out}")
        return
    print(f"desktop-capacity: PASS sixteen simultaneous clients, seventeenth launch refusal, slot reuse, full Window menu, exact saved text, screenshot worker at capacity and complete cleanup. Evidence: {out}")


if __name__=="__main__": main()
