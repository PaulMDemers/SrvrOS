#!/usr/bin/env python3
"""Exercise native recursive file-name search, result actions and bounded snapshots in a disposable VM."""
import os
import argparse
import json
import hashlib
import io
import contextlib
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
from test_exfat_generator import verify_image
from serial_records import desktop_progress_end


def main():
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output",type=Path)
    args=parser.parse_args()
    out=(args.output or root/"build/review/desktop-find").resolve()
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    (out/'result.json').unlink(missing_ok=True)
    source={'source':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
        'source_status':subprocess.check_output(['git','status','--porcelain'],cwd=root,text=True).splitlines(),
        'submodules':subprocess.check_output(['git','submodule','status','--recursive'],cwd=root,text=True).splitlines(),
        'payload_sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
            (root/'build/kernel/srvros.elf',root/'build/initramfs.tar',root/'build/srvros.exfat')}}
    (out/'source.json').write_text(json.dumps(source,indent=2)+'\n')
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
        def check_image(label):
            log=io.StringIO()
            with contextlib.redirect_stdout(log): verify_image(tmp/'disk.exfat')
            (out/(label+'-generator.txt')).write_text(log.getvalue())
            check=subprocess.run([str(root/'build/tests/exfat-driver'),str(tmp/'disk.exfat'),'allocation-check'],capture_output=True,text=True)
            (out/(label+'-allocation.txt')).write_text(check.stdout+check.stderr)
            if check.returncode: raise RuntimeError('Allocation check failed: '+label)
        check_image('before')
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
                    while desktop_progress_end(output,marker,consumed) is None:
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
                    consumed=desktop_progress_end(output,marker,consumed)
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
                def focus_window(query):
                    shortcut("meta_l-spc");wait(b"launcher opened");type_text(query);qmp.key("ret");wait(b"launcher window=")
                def focus_find():
                    focus_window("Find Files window")
                def save_text(name):
                    shortcut("ctrl-l");type_text("/fat/"+name);shortcut("ctrl-s");wait(b"picker ready mode=2");shortcut("ctrl-l");type_text("/fat/home/../");qmp.key("ret");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                click(390,750);wait(b"desktools: starting role=files");wait(b"file-result role=files op=3 status=0")
                shortcut("ctrl-l");type_text("/fat/find-fixture");qmp.key("ret");wait(b"files-location path=/fat/find-fixture")
                shortcut("ctrl-f");wait_all([b"deskfind: starting",b"size=760x500"])
                shortcut("ctrl-l");type_text("/fat/find-fixture/../find-fixture//");shortcut("ctrl-f");type_text("REPORT");qmp.key("ret");wait(b"deskfind: started root=/fat/find-fixture query=REPORT kind=0");wait(b"count=2 matched=2 skipped=0");shot("find-results")
                # Result selection has keyboard focus after completion.
                shortcut("ctrl-c");wait(b"deskfind: copy path=/fat/find-fixture/Report-one.txt");time.sleep(.5)
                focus_window("Untitled window");shortcut("ctrl-v");wait(b"clipboard role=2 action=32");save_text("find-path.txt")
                focus_find();shortcut("ret");wait(b"deskfind: open application=2 path=/fat/find-fixture/Report-one.txt");wait(b"file-result role=text op=1 status=0");shot("find-open-text")
                shortcut("ctrl-a");shortcut("ctrl-c");wait(b"clipboard role=2 action=8");shortcut("ctrl-w");wait(b"text-runtime: closed window=")
                # Save the opened bytes through the original Text window.
                focus_window("find-path.txt window");shortcut("ctrl-n");shortcut("ctrl-v");wait(b"clipboard role=2 action=32");save_text("find-content.txt")
                focus_find();shortcut("ctrl-r");wait(b"deskfind: open application=1 path=/fat/find-fixture");wait(b"files-location path=/fat/find-fixture");shot("find-reveal");shortcut("ctrl-w");wait(b"desktools: exited role=files failure=0")
                focus_find();shortcut("ctrl-f");type_text("bulk-");shortcut("ctrl-2");shortcut("ctrl-ret");wait(b"query=bulk- kind=1");wait(b"count=256 matched=300 skipped=0");shot("find-limited")
                shortcut("end");shortcut("ctrl-c");wait(b"deskfind: copy path=/fat/find-fixture/bulk-255.txt");time.sleep(.5)
                focus_window("find-content.txt window");shortcut("ctrl-n");shortcut("ctrl-v");wait(b"clipboard role=2 action=32");save_text("find-last-path.txt")
                focus_find();shortcut("ctrl-l");type_text("/fat");shortcut("ctrl-f");type_text("find-fixture");shortcut("ctrl-3");shortcut("ctrl-ret");wait(b"query=find-fixture kind=2");wait(b"count=2 matched=2 skipped=0");shot("find-folders")
                # A failed scan clears actionable results, but keeps editable fields.
                shortcut("ctrl-l");type_text("/fat/absent-folder");shortcut("ctrl-ret");wait(b"deskfind: finished result=1 exit=1 count=-1");shot("find-error")
                shortcut("ctrl-l");type_text("/fat/find-fixture");shortcut("ctrl-f");type_text("nothing-matches");shortcut("ctrl-1");shortcut("ctrl-ret");wait(b"count=0 matched=0 skipped=0")
                shortcut("ctrl-f");type_text("report");shortcut("ctrl-ret");wait(b"count=2 matched=2 skipped=0")
                shortcut("shift-f10");shot("find-context");shortcut("esc")
                # Calibrate against a clamped screen origin, then use the
                # compositor's actual frame rather than a stale cursor offset.
                frames=re.findall(rb'deskd: geometry pid=(\d+) window=(\d+) frame=(\d+),(\d+),(\d+),(\d+)',output)
                pid,window,x,y,w,h=next(tuple(map(int,row)) for row in frames if int(row[4])==762 and int(row[5])==545)
                qmp.mouse(-1600,-1000);pointer[:]=[0,0]
                qmp.mouse(x+w-2,y+h-2);pointer[:]=[x+w-2,y+h-2]
                qmp.mouse(0,0,1);qmp.mouse(-480,-260,1);qmp.mouse(0,0,0);pointer[0]-=480;pointer[1]-=260
                wait_all([f'deskd: commit pid={pid}'.encode(),b'size=280x240']);shot("find-compact")
                click(x+1+24,y+44+146+24+14)
                shortcut("down");shortcut("ctrl-c");wait(b"deskfind: copy path=/fat/find-fixture/report-two.txt")
                shortcut("ctrl-shift-2");shortcut("ctrl-f");type_text("first report");shortcut("ctrl-ret")
                wait(b"deskfind: started root=/fat/find-fixture query=first report kind=1 mode=1")
                wait(b"count=1 matched=1 skipped=0 mode=1 flags=0");shot("find-compact-contents")
                shortcut("ctrl-shift-1");shortcut("ctrl-f");type_text("report");shortcut("ctrl-ret")
                wait(b"count=2 matched=2 skipped=0 mode=0 flags=0")
                shortcut("f1");wait_all([b"deskhelp: page=find",b"deskhelp: starting"]);shot("find-help");shortcut("ctrl-w");wait(b"deskhelp: exited")
                focus_find();shortcut("ctrl-w");wait(b"deskfind: exited failure=0")
                # The global shortcut starts a new search from the home folder.
                shortcut("meta_l-shift-f");wait(b"deskfind: starting");type_text("report");shortcut("ctrl-ret");wait(b"deskfind: started root=/fat/home query=report kind=0");wait(b"count=0 matched=0 skipped=0")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.sendall(b"run /fat/bin/sync\n");wait(b"run: pid=");wait(b" exited status=0");wait(b"srv> ")
                serial.sendall(b"fsck /fat\n");wait(b"errors=0 ok");wait(b"srv> ")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
                shutil.copyfile(tmp/'disk.exfat',out/'last-workspace.img')
            fixture=ExfatFixture((tmp/"disk.exfat").read_bytes())
            assert fixture.read("find-path.txt")==b"/fat/find-fixture/Report-one.txt"
            assert fixture.read("find-content.txt")==source
            assert fixture.read("find-last-path.txt")==b"/fat/find-fixture/bulk-255.txt"
            for name,data in fixtures.items(): assert fixture.read("find-fixture/"+name)==data
            assert fixture.read("find-fixture-extra/Report-outside.txt")==b"outside"
            check_image('after')
            (out/'result.json').write_text(json.dumps({'status':'PASS','source':source,'guest_fsck':'PASS','offline_allocation':'PASS','filename_search':'unchanged','open_reveal_copy_exact_bytes':'PASS'},indent=2)+'\n')
    print(f"desktop-find: PASS Files/global launch, case-insensitive literal names, root boundaries, result open/reveal/copy, 256 of 300 cap, kind filtering, errors/empty results, compact/context/Help and exact saved proofs. Evidence: {out}")


if __name__=="__main__": main()
