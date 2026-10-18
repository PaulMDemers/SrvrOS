#!/usr/bin/env python3
"""Exercise native recursive file-name search, result actions and bounded snapshots in a disposable VM."""
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
    out=root/"build/review/desktop-find"
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-find-") as directory:
        tmp=Path(directory)
        from exfat_test_image import ExfatFixture
        fixture=ExfatFixture((root/"build/srvros.exfat").read_bytes())
        source=b"Found the first report.\n"
        fixtures={"Report-one.txt":source,"report-two.txt":b"Second report.\n","unrelated.txt":b"keep"}
        fixtures.update({f"bulk-{i:03}.txt":b"" for i in range(300)})
        fixture.add_directory_files("find-fixture",fixtures)
        fixture.add_directory_files("find-fixture-extra",{"Report-outside.txt":b"outside"})
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
                def focus_find():
                    shortcut("meta_l-spc");wait(b"launcher opened");type_text("Find Files");qmp.key("ret");wait(b"launcher window=")
                def save_text(name):
                    shortcut("ctrl-l");type_text("/fat/"+name);shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                click(390,750);wait(b"desktools: starting role=files");wait(b"file-result role=files op=3 status=0")
                shortcut("ctrl-l");type_text("/fat/find-fixture");qmp.key("ret");wait(b"files-location path=/fat/find-fixture")
                shortcut("ctrl-f");wait_all([b"deskfind: starting",b"size=760x500"])
                type_text("REPORT");qmp.key("ret");wait(b"deskfind: started root=/fat/find-fixture query=REPORT kind=0");wait(b"count=2 matched=2 skipped=0");shot("find-results")
                # Result selection has keyboard focus after completion.
                shortcut("ctrl-c");wait(b"deskfind: copy path=/fat/find-fixture/Report-one.txt");time.sleep(.5)
                click(490,750);shortcut("ctrl-v");wait(b"clipboard role=2 action=32");save_text("find-path.txt")
                focus_find();shortcut("ret");wait(b"deskfind: open application=2 path=/fat/find-fixture/Report-one.txt");wait(b"file-result role=text op=1 status=0");shot("find-open-text")
                shortcut("ctrl-a");shortcut("ctrl-c");wait(b"clipboard role=2 action=8");shortcut("ctrl-w");wait(b"desktools: exited role=text failure=0")
                # Save the opened bytes through the original Text window.
                click(490,750);shortcut("ctrl-n");shortcut("ctrl-v");wait(b"clipboard role=2 action=32");save_text("find-content.txt")
                focus_find();shortcut("ctrl-r");wait(b"deskfind: open application=1 path=/fat/find-fixture");wait(b"files-location path=/fat/find-fixture");shot("find-reveal");shortcut("ctrl-w");wait(b"desktools: exited role=files failure=0")
                focus_find();shortcut("ctrl-f");type_text("bulk-");shortcut("ctrl-2");shortcut("ctrl-ret");wait(b"query=bulk- kind=1");wait(b"count=256 matched=300 skipped=0");shot("find-limited")
                shortcut("end");shortcut("ctrl-c");wait(b"deskfind: copy path=/fat/find-fixture/bulk-255.txt");time.sleep(.5)
                click(490,750);shortcut("ctrl-n");shortcut("ctrl-v");wait(b"clipboard role=2 action=32");save_text("find-last-path.txt")
                focus_find();shortcut("ctrl-l");type_text("/fat");shortcut("ctrl-f");type_text("find-fixture");shortcut("ctrl-3");shortcut("ctrl-ret");wait(b"query=find-fixture kind=2");wait(b"count=2 matched=2 skipped=0");shot("find-folders")
                # A failed scan clears actionable results, but keeps editable fields.
                shortcut("ctrl-l");type_text("/fat/absent-folder");shortcut("ctrl-ret");wait(b"deskfind: finished result=1 exit=1 count=-1");shot("find-error")
                shortcut("ctrl-l");type_text("/fat/find-fixture");shortcut("ctrl-f");type_text("nothing-matches");shortcut("ctrl-1");shortcut("ctrl-ret");wait(b"count=0 matched=0 skipped=0")
                shortcut("ctrl-f");type_text("report");shortcut("ctrl-ret");wait(b"count=2 matched=2 skipped=0")
                shortcut("shift-f10");shot("find-context");shortcut("esc")
                # Third window: content origin (81,156), 760 by 500.
                qmp.mouse(837-pointer[0],652-pointer[1]);pointer[:]=[837,652]
                qmp.mouse(0,0,1);qmp.mouse(-480,-260,1);qmp.mouse(0,0,0);pointer[0]-=480;pointer[1]-=260;time.sleep(.5);shot("find-compact")
                shortcut("down");shortcut("ctrl-c");wait(b"deskfind: copy path=/fat/find-fixture/report-two.txt")
                shortcut("f1");wait_all([b"deskhelp: page=find",b"deskhelp: starting"]);shot("find-help");shortcut("ctrl-w");wait(b"deskhelp: exited")
                focus_find();shortcut("ctrl-w");wait(b"deskfind: exited failure=0")
                # The global shortcut starts a new search from the home folder.
                shortcut("meta_l-shift-f");wait(b"deskfind: starting");type_text("report");shortcut("ctrl-ret");wait(b"deskfind: started root=/fat/home query=report kind=0");wait(b"count=0 matched=0 skipped=0")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
            fixture=ExfatFixture((tmp/"disk.exfat").read_bytes())
            assert fixture.read("find-path.txt")==b"/fat/find-fixture/Report-one.txt"
            assert fixture.read("find-content.txt")==source
            assert fixture.read("find-last-path.txt")==b"/fat/find-fixture/bulk-255.txt"
            for name,data in fixtures.items(): assert fixture.read("find-fixture/"+name)==data
            assert fixture.read("find-fixture-extra/Report-outside.txt")==b"outside"
    print(f"desktop-find: PASS Files/global launch, case-insensitive literal names, root boundaries, result open/reveal/copy, 256 of 300 cap, kind filtering, errors/empty results, compact/context/Help and exact saved proofs. Evidence: {out}")


if __name__=="__main__": main()
