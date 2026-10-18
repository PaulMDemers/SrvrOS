#!/usr/bin/env python3
"""Exercise emergency desktop exit with unsaved text, a live PTY and a pending diagnostic, then recover."""
import os
import http.client
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
    out=root/"build/review/desktop-recovery"
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000);http_port=choose_port(38001,45000)
    with tempfile.TemporaryDirectory(prefix="srvros-recovery-") as directory:
        tmp=Path(directory)
        shutil.copyfile(root/"build/srvros.exfat",tmp/"disk.exfat")
        iso=build_iso(str(root),str(tmp),width,height,os.environ.copy())
        command=["qemu-system-x86_64","-M","q35","-m","512M","-cdrom",str(iso),"-boot","d",
            "-drive",f"if=none,id=disk,file={tmp/'disk.exfat'},format=raw",
            "-device","ich9-ahci,id=ahci","-device","ide-hd,drive=disk,bus=ahci.0",
            "-netdev",f"user,id=net0,hostfwd=tcp:127.0.0.1:{http_port}-10.0.2.15:80","-device","e1000,netdev=net0,id=nic0",
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
                expected_http=b"srvros webd: static file serving from /fat/www is online.\n"
                def http_check():
                    deadline=time.monotonic()+15
                    while True:
                        connection=http.client.HTTPConnection("127.0.0.1",http_port,timeout=2)
                        try:
                            connection.request("GET","/status.txt",headers={"Connection":"close"})
                            response=connection.getresponse();body=response.read()
                            if response.status!=200 or body!=expected_http: raise RuntimeError("Unexpected background HTTP response")
                            return
                        except (OSError,http.client.HTTPException):
                            if time.monotonic()>deadline: raise
                            time.sleep(.2)
                        finally: connection.close()
                baseline=None
                def cleanup():
                    deadline=time.monotonic()+12
                    while True:
                        serial.sendall(b"ps\n");wait(b"pid name status");wait(b"srv> ")
                        lines=output.split(b"pid name status")[-1].split(b"srv> ")[0]
                        processes=set(re.findall(rb"^(\d+) (\S+) (background|exited|foreground)",lines,re.M))
                        if not processes: raise RuntimeError("Missing process snapshot")
                        extra=[name for _,name,_ in processes if name not in [b"/init",b"/fat/bin/svscan",b"/fat/bin/webd"]]
                        if not extra:
                            if baseline is not None and processes!=baseline: raise RuntimeError("Background service process changed across desktop recovery")
                            return processes
                        if time.monotonic()>deadline: raise RuntimeError("Processes survived emergency exit: "+lines.decode(errors="replace"))
                        time.sleep(.5)
                http_check();baseline=cleanup()
                for round in range(3):
                    serial.sendall(b"run /fat/bin/deskd --apps-trace\n");wait(b"deskd: commit")
                    type_text("Unsaved recovery fixture")
                    shortcut("meta_l-spc");wait(b"launcher opened");type_text("Terminal new app");qmp.key("ret");wait(b"launcher application=6");wait(b"deskd: commit")
                    type_text("cat\n")
                    shortcut("meta_l-spc");wait(b"launcher opened");type_text("Network Utility new app");qmp.key("ret");wait(b"launcher application=11");wait(b"deskd: commit")
                    shortcut("ctrl-l");type_text("10.0.2.99");shortcut("ctrl-ret");wait(b"desknet: started mode=ping target=10.0.2.99")
                    shot(f"emergency-{round}-pending");http_check()
                    shortcut("ctrl-alt-esc");wait(b"deskd: exited",30);wait(b"srv> ");cleanup();http_check()
                    # USB input must return to the console byte stream, rather
                    # than remain owned by a dead GUI event consumer.
                    type_text("memstat\n");wait(b"heap: total=");wait(b"srv> ")
                    print(f"recovery {round+1}/3: no app, PTY child or diagnostic survived; USB console input restored; same background web server serves exact HTTP bytes",flush=True)
                serial.sendall(b"run /fat/bin/deskd --apps-trace\n");wait(b"deskd: commit")
                type_text("Recovered desktop saved this document.")
                shortcut("ctrl-l");type_text("/fat/recovery-proof.txt");shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0");shot("recovered-document")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ");cleanup();http_check()
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
            from exfat_test_image import ExfatFixture
            fixture=ExfatFixture((tmp/"disk.exfat").read_bytes())
            assert fixture.read("recovery-proof.txt")==b"Recovered desktop saved this document."
    print(f"desktop-recovery: PASS three emergency exits with unsaved Text, a live shell/cat PTY and pending Ping; complete process cleanup, USB console input, relaunch, exact saved recovery document and uninterrupted background HTTP service. Evidence: {out}")


if __name__=="__main__": main()
