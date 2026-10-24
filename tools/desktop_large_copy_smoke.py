#!/usr/bin/env python3
"""Exercise large guarded file duplication with responsive input and exact on-disk verification in a disposable VM."""
import os
import argparse
import re
from pathlib import Path
import socket
import subprocess
import tempfile
import time
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from desktop_foundation_smoke import QMP


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--usb',action='store_true',help='Use real USB BOT storage for the throttled copy')
    args=parser.parse_args()
    volume='/usb' if args.usb else '/fat'
    root=Path(__file__).resolve().parents[1]
    out=root/("build/review/desktop-large-copy-usb" if args.usb else "build/review/desktop-large-copy")
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-large-copy-") as directory:
        tmp=Path(directory)
        from exfat_test_image import ExfatFixture
        pattern=bytes((i*29+i//71)&255 for i in range(4096))
        payload=pattern*1024+b"tail-of-copy-test!"
        fixture=ExfatFixture((root/"build/srvros.exfat").read_bytes())
        fixture.add_directory_files("copy-fixture",{"large.dat":payload,"collision.dat":b"preserve me"})
        (tmp/"disk.exfat").write_bytes(fixture.data)
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy(),cmdline='srvros.storage=ram srvros.usb-storage=rw' if args.usb else None)
        command=["qemu-system-x86_64","-M","q35","-m","512M","-cdrom",str(iso),"-boot","d",
            "-drive",f"if=none,id=disk,file={tmp/'disk.exfat'},format=raw,bps_wr={131072 if args.usb else 1048576}",
            "-device","ich9-ahci,id=ahci","-device","ide-hd,drive=disk,bus=ahci.0",
            "-device","qemu-xhci,id=xhci","-device","usb-kbd,bus=xhci.0",
            "-serial",f"tcp:127.0.0.1:{port},server=on,wait=off",
            "-qmp",f"unix:{tmp/'qmp'},server=on,wait=off","-display","none","-monitor","none","-no-reboot","-rtc","base=2026-10-18T12:34:00,clock=vm"]
        if args.usb:
            at=command.index('ich9-ahci,id=ahci')-1
            del command[at:at+4]
            command+=['-device','usb-storage,drive=disk,bus=xhci.0']
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
                def right_click(x,y):
                    qmp.mouse(x-pointer[0],y-pointer[1]);pointer[:]=[x,y]
                    qmp.mouse(0,0,2);qmp.mouse(0,0,0)
                def control_click(x,y):
                    qmp.call("input-send-event",{"events":[{"type":"key","data":{"down":True,"key":{"type":"qcode","data":"ctrl"}}}]});time.sleep(.1)
                    click(x,y)
                    qmp.call("input-send-event",{"events":[{"type":"key","data":{"down":False,"key":{"type":"qcode","data":"ctrl"}}}]});time.sleep(.1)
                def shot(name):
                    time.sleep(.5);qmp.call("screendump",{"filename":str(out/f"{name}.ppm")})
                wait(b"srv> ",100)
                if args.usb:
                    serial.sendall(b"mount usb0 /usb\n");wait(b"exfat: mounted /usb");wait(b"srv> ")
                serial.sendall(b"run /fat/bin/deskd --apps-trace\n")
                wait(b"desktools: starting role=text");wait_all([b"deskd: commit",b"text history=multi-step"])
                click(390,750);wait(b"desktools: starting role=files");wait(b"file-result role=files op=3 status=0")
                shortcut("ctrl-l");type_text(volume+"/copy-fixture");qmp.key("ret");wait(b"file-result role=files op=3 status=0")
                type_text("large");shortcut("shift-f10");wait(b"context opened role=1 target=9");qmp.key("d");qmp.key("ret");wait(b"files-prompt operation=7")
                type_text("large-copy.dat");qmp.key("ret")
                shortcut("alt-tab");type_text("A")
                # Deliberate disk throttling keeps this copy in flight. Observe
                # the GUI before the worker can report success, then verify the
                # complete document on disk after the run.
                while True:
                    try: output+=serial.recv(16384)
                    except socket.timeout: break
                assert b"file-result role=files op=7" not in output[consumed:],"Copy finished before input overlap was exercised"
                shot("input-during-copy");type_text(" live document during a large copy.")
                wait(b"file-result role=files op=7 status=0",90);wait(b"file-result role=files op=3 status=0")
                shortcut("ctrl-l");type_text(volume+"/copy-input.txt");shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                click(390,750);shot("large-copy-complete")
                shortcut("shift-f10");wait(b"context opened role=1 target=9");qmp.key("d");qmp.key("ret");wait(b"files-prompt operation=7")
                type_text("collision.dat");qmp.key("ret");wait(b"file-result role=files op=7 status=2");shot("collision-preserved")
                # Rename the verified copy using the same identity guards.
                shortcut("shift-f10");wait(b"context opened role=1 target=9");qmp.key("r");qmp.key("ret");wait(b"files-prompt operation=5")
                type_text("verified.dat");qmp.key("ret");wait(b"file-result role=files op=5 status=0");wait(b"file-result role=files op=3 status=0")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                if args.usb:
                    serial.sendall(b"unmount /usb\n");wait(b"exfat: unmounted /usb");wait(b"srv> ")
                    start=len(output);serial.sendall(b"xhci\n");wait(b"srv> ")
                    timing=re.findall(rb"xhci: storage .*errors=0 offline=0 wait_calls=(\d+) wait_us=(\d+)",output[start:])
                    assert timing and any(int(c)>0 and int(us)>10000 for c,us in timing),'USB completion waits were not exercised'
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
            from exfat_test_image import ExfatFixture
            fixture=ExfatFixture((tmp/"disk.exfat").read_bytes())
            assert fixture.read("copy-fixture/large.dat")==payload
            assert fixture.read("copy-fixture/verified.dat")==payload
            assert fixture.read("copy-fixture/collision.dat")==b"preserve me"
            assert fixture.read("copy-input.txt")==b"A live document during a large copy."
            names=[name for name,_ in fixture.entries(fixture.read("copy-fixture"))]
            assert set(names)=={"large.dat","verified.dat","collision.dat"},names
    print(f"desktop-large-copy: PASS {'USB BOT' if args.usb else 'AHCI'} 4 MiB+17 byte guarded duplicate, input during throttled copy, exact document and binary readback, collision preservation, rename and clean exit. Evidence: {out}")




if __name__=="__main__": main()
