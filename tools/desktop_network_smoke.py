#!/usr/bin/env python3
"""Exercise native network diagnostics, link changes and responsive background jobs in a disposable VM."""
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
    out=root/"build/review/desktop-network"
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-network-") as directory:
        tmp=Path(directory)
        shutil.copyfile(root/"build/srvros.exfat",tmp/"disk.exfat")
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy())
        command=["qemu-system-x86_64","-M","q35","-m","512M","-cdrom",str(iso),"-boot","d",
            "-drive",f"if=none,id=disk,file={tmp/'disk.exfat'},format=raw",
            "-device","ich9-ahci,id=ahci","-device","ide-hd,drive=disk,bus=ahci.0",
            "-device","qemu-xhci,id=xhci","-device","usb-kbd,bus=xhci.0",
            "-netdev","user,id=net0","-device","e1000,netdev=net0,id=nic0",
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
                serial.sendall(b"run /fat/bin/sh\n");wait(b" $ ")
                serial.sendall(b"dhcp\n");wait(b"dhcp: 10.0.2.15");wait(b" $ ")
                serial.sendall(b"exit\n");wait(b"srv> ")
                serial.sendall(b"run /fat/bin/deskd --apps-trace\n")
                wait(b"desktools: starting role=text");wait(b"deskd: commit")
                def launch_network():
                    shortcut("meta_l-spc");wait(b"launcher opened");type_text("Network Utility");qmp.key("ret")
                def save_text(name):
                    shortcut("ctrl-l");type_text("/fat/"+name);shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                launch_network();wait_all([b"desknet: starting",b"size=680x460"])
                shortcut("ctrl-ret");wait(b"desknet: started mode=ping target=10.0.2.2");wait(b"desknet: finished result=1 exit=0");shot("network-ping")
                shortcut("ctrl-shift-c");wait(b"desknet: copy-report");time.sleep(.5)
                click(490,750);shortcut("ctrl-v");wait(b"clipboard role=2 action=32");save_text("network-ping.txt")
                launch_network();wait(b"launcher window=")
                shortcut("ctrl-2");shortcut("ctrl-l");type_text("localhost");shortcut("ctrl-ret")
                wait(b"desknet: started mode=lookup target=localhost");wait(b"desknet: finished result=1");shot("network-lookup")
                shortcut("ctrl-shift-c");wait(b"desknet: copy-report");time.sleep(.5)
                click(490,750);shortcut("ctrl-n");shortcut("ctrl-v");wait(b"clipboard role=2 action=32");save_text("network-lookup.txt")
                launch_network();wait(b"launcher window=")
                shortcut("ctrl-1");shortcut("ctrl-l");type_text("10.0.2.99");shortcut("ctrl-ret");wait(b"desknet: started mode=ping target=10.0.2.99")
                click(490,750);shortcut("ctrl-n");type_text("A");shot("network-typing-during-job")
                launch_network();wait(b"launcher window=");shortcut("esc");wait(b"desknet: canceled")
                click(490,750);save_text("network-input-proof.txt")
                # A retained DHCP address is not a connected cable. System reads
                # the device's current STATUS, and Network Utility refuses Run.
                click(690,750);wait(b"desktools: starting role=system");wait(b"desktools: system-ready")
                click(634,228);wait(b"system-page=network");time.sleep(.5)
                qmp.call("set_link",{"name":"nic0","up":False});shortcut("f5")
                wait(b"link=1 speed=0");shot("network-disconnected-system")
                launch_network();wait(b"launcher window=");shortcut("ctrl-ret");wait(b"desknet: disconnected");shot("network-disconnected")
                qmp.call("set_link",{"name":"nic0","up":True})
                shortcut("ctrl-l");type_text("10.0.2.2");shortcut("ctrl-ret");wait(b"desknet: started mode=ping target=10.0.2.2");wait(b"desknet: finished result=1 exit=0")
                # Compact layout, shared context menu, and global Help topic.
                qmp.mouse(733-pointer[0],588-pointer[1]);pointer[:]=[733,588]
                qmp.mouse(0,0,1);qmp.mouse(-400,-220,1);qmp.mouse(0,0,0);pointer[0]-=400;pointer[1]-=220;time.sleep(.5);shot("network-compact")
                shortcut("ctrl-l");shortcut("shift-f10");shot("network-target-context");shortcut("esc")
                shortcut("f1");wait(b"deskhelp: starting");shot("network-help");shortcut("ctrl-w");wait(b"deskhelp: exited")
                # Close during a pending read-only command; no child survives.
                launch_network();wait(b"launcher window=");shortcut("ctrl-l");type_text("10.0.2.99");shortcut("ctrl-ret");wait(b"desknet: started mode=ping target=10.0.2.99")
                shortcut("ctrl-w");wait(b"desknet: exited failure=0")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
            from exfat_test_image import ExfatFixture
            fixture=ExfatFixture((tmp/"disk.exfat").read_bytes())
            ping=fixture.read("network-ping.txt")
            assert ping.startswith(b"Ping\nTarget: 10.0.2.2\nStatus: Diagnostic completed.\n\nPING 10.0.2.2 (10.0.2.2)\n"),ping
            assert ping.endswith(b"ping: sent=4 received=4 lost=0\n"),ping
            assert sum(b"reply from 10.0.2.2: seq=" in line for line in ping.splitlines())==4
            lookup=fixture.read("network-lookup.txt")
            assert lookup.startswith(b"DNS lookup\nTarget: localhost\nStatus: Diagnostic completed.\n\n"),lookup
            assert lookup.endswith(b"localhost has address 127.0.0.1\n"),lookup
            assert fixture.read("network-input-proof.txt")==b"A"
    print(f"desktop-network: PASS Ping/DNS, clipboard report exports, Text typing during pending job, cancellation, live link down/up, compact/context/Help, close during work and clean exit. Evidence: {out}")


if __name__=="__main__": main()
