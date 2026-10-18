#!/usr/bin/env python3
"""Exercise native Calculator keypad, history, compact layout and result clipboard in a disposable VM."""
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
    out=root/"build/review/desktop-calculator"
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-calculator-") as directory:
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
                click(590,750);wait(b"desktools: starting role=calculator");wait(b"deskd: commit")
                type_text("2 + 3 * (4 - 1)");qmp.key("ret");wait(b"calculator result=11 history=1")
                # The second window's keypad uses shared centered buttons.
                click(335,418);click(183,474);click(543,164);wait(b"calculator result=22 history=2")
                click(335,530);click(335,306);click(543,164);wait(b"calculator result=44 history=3")
                click(541,250);click(259,530);click(543,164);wait(b"calculator result=-44 history=4")
                shot("calculator-keypad")
                click(639,250);wait(b"clipboard role=3 action=8 bytes=3")
                click(490,750);shortcut("ctrl-v");wait(b"clipboard role=2 action=32 bytes=3")
                shortcut("ctrl-l");type_text("/fat/calculator-proof.txt");shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                click(590,750);click(470,354);qmp.key("ret");wait(b"calculator recalled=3")
                shortcut("ctrl-a");type_text("1e21474836479999");qmp.key("ret");wait(b"calculator error=The number is outside the supported range.")
                shortcut("ctrl-a");type_text("9/0");qmp.key("ret");wait(b"calculator error=Division by zero.")
                shortcut("ctrl-a");type_text("4.5/1.5");qmp.key("ret");wait(b"calculator result=3 history=5")
                # Compact mode retains expression entry and menu actions.
                qmp.mouse(693-pointer[0],648-pointer[1]);pointer[:]=[693,648]
                qmp.mouse(0,0,1);qmp.mouse(-360,-280,1);qmp.mouse(0,0,0);pointer[0]-=360;pointer[1]-=280;time.sleep(.5)
                type_text("+2");qmp.key("ret");wait(b"calculator result=5 history=6");shot("calculator-compact")
                shortcut("f10");shortcut("right");shortcut("right");shortcut("right")
                shortcut("down");shortcut("down");shortcut("down");shortcut("ret");wait(b"clipboard role=3 action=8 bytes=1")
                click(490,750);shortcut("ctrl-end");type_text(" ");shortcut("ctrl-v");wait(b"clipboard role=2 action=32 bytes=1")
                shortcut("ctrl-s");wait(b"file-result role=text op=2 status=0")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                serial.sendall(b"cat /fat/calculator-proof.txt\n");wait(b"-44 5");wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
    print(f"desktop-calculator: PASS native keypad, chained results, answer/negation, history recall, bounded errors, compact layout, global result-copy menu and exact cross-app readback. Evidence: {out}")


if __name__=="__main__": main()
