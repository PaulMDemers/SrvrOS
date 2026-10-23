#!/usr/bin/env python3
"""Exercise native desktop applications and persisted files in a disposable VM."""
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time
from PIL import Image
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from desktop_foundation_smoke import QMP
from exfat_test_image import ExfatFixture


def main():
    root=Path(__file__).resolve().parents[1]
    out=root/"build/review/desktop-terminal"
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-apps-") as directory:
        tmp=Path(directory)
        shutil.copyfile(root/"build/srvros.exfat",tmp/"disk.exfat")
        fixture=ExfatFixture((tmp/"disk.exfat").read_bytes())
        fixture.add_directory_files("read-fixture",{
            "long.txt":b"x"*300+b"\nkeep-next\n",
            "check.sh":b"read OVER\necho overflow-status=$?\nread NEXT\necho next=$NEXT\n",
            "tap-diag.txt":b"vm-gestures-skipped\n\n",
            "tap.txt":b"vm-timing-skipped\nvm-input-skipped\nvm-link-down-skipped\nvm-link-up-skipped\n\n",
            "trackpad.txt":b"vm-press-skipped tap-skipped\nvm-drag-skipped cancel-skipped\n\n"})
        (tmp/"disk.exfat").write_bytes(fixture.data)
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
                        ";":("semicolon",False),"-":("minus",False),"_":("minus",True),"\n":("ret",False),
                        ",":("comma",False),"!":("1",True),"+":("equal",True),
                        "*":("8",True),">":("dot",True),"$":("4",True),"?":("slash",True),"(":("9",True),")":("0",True)}
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
                type_text("echo from-clipboard > /fat/terminal-paste.txt")
                shortcut("ctrl-a");shortcut("ctrl-c");wait(b"clipboard role=2 action=8")
                click(890,750);wait(b"deskterm: ready");time.sleep(1);shot("terminal-prompt")
                # Reproduce the physical stray-slash report. Verify pixels,
                # not merely the command buffer (which was already correct).
                type_text("abc");qmp.key("backspace");type_text("/")
                for _ in range(3): qmp.key("backspace")
                shot("backspace-cleared")
                # Fixed initial terminal geometry and /fat/home $ prompt:
                # exclude its blinking caret at x=195, compare the remaining
                # input row (the stale slash appears around x=225).
                with Image.open(out/"terminal-prompt.ppm") as before, Image.open(out/"backspace-cleared.ppm") as after:
                    assert before.size==after.size==(1280,800)
                    area=(200,210,830,232)
                    assert before.crop(area).getextrema()==((249,249),(247,247),(241,241)), "Unexpected initial input-row geometry"
                    assert before.crop(area).tobytes()==after.crop(area).tobytes(), "Backspace left stale terminal pixels"
                shortcut("ctrl-shift-v");wait(b"deskterm: clipboard pasted");qmp.key("ret");time.sleep(1)
                type_text("echo terminal-one > /fat/terminal-one.txt\n");time.sleep(1)
                type_text("echo ac");qmp.key("left");type_text("b");qmp.key("end")
                type_text(" > /fat/terminal-edited.txt\n");time.sleep(1)
                # A copied report must stay in one read and require physical Enter.
                click(490,750);shortcut("ctrl-a");type_text("report first\nreport second\n")
                shortcut("ctrl-a");shortcut("ctrl-c");wait(b"clipboard role=2 action=8")
                click(890,750)
                type_text("read FIRST; read SECOND; echo $FIRST > /fat/terminal-multiline.txt; echo $SECOND > /fat/terminal-followup.txt\n")
                time.sleep(.5);shortcut("ctrl-shift-v");wait(b"deskterm: clipboard pasted")
                time.sleep(.5);shot("multiline-awaits-enter")
                qmp.key("ret");time.sleep(.5);type_text("fresh-followup\n");time.sleep(1)
                type_text("seq 1 80\n");time.sleep(1);shot("scrollback-bottom")
                shortcut("shift-pgup");shot("scrollback-up")
                shortcut("ctrl-shift-n");wait(b"deskterm: ready");time.sleep(1)
                type_text("echo terminal-two > /fat/terminal-two.txt\n");time.sleep(1)
                type_text("sleep 30\n");wait(b"/fat/bin/sleep entry=")
                shortcut("ctrl-c");time.sleep(1)
                type_text("echo $? > /fat/terminal-interrupted.txt\n");time.sleep(1)
                type_text("echo survived > /fat/terminal-survived.txt\n");time.sleep(1);shot("second-session")
                type_text("clear\n");time.sleep(.5);shot("cleared-terminal")
                qmp.mouse(899-pointer[0],653-pointer[1]);pointer[:]=[899,653]
                qmp.mouse(0,0,1);qmp.mouse(180,50,1);qmp.mouse(0,0,0);pointer[:]=[1079,703];time.sleep(1)
                type_text("stty size > /fat/terminal-size.txt\n");time.sleep(1);shot("resized-terminal")
                type_text("exit\n");wait(b"deskterm: shell exited status=0");shot("session-ended")
                shortcut("ctrl-shift-w");wait(b"deskterm: exited failure=0")
                click(890,750);type_text("echo first-alive > /fat/terminal-first-alive.txt\n");time.sleep(1)
                shortcut("ctrl-shift-w");shot("close-confirmation");qmp.key("esc")
                type_text("exit\n");wait(b"deskterm: shell exited status=0")
                shortcut("ctrl-shift-w");wait(b"deskterm: exited failure=0")
                click(490,750);shortcut("ctrl-l");shortcut("ctrl-a");type_text("/fat/terminal-clipboard-source.txt")
                shortcut("ctrl-s");wait(b"picker ready mode=2");qmp.key("ret");wait(b"file-result role=text op=2 status=0")
                shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                serial.sendall(b"run /fat/bin/sh\n");wait(b" $ ")
                serial.sendall(b"sh /fat/read-fixture/check.sh < /fat/read-fixture/long.txt\n")
                wait(b"overflow-status=1");wait(b"next=keep-next");wait(b" $ ")
                serial.sendall(b"HOME=/fat/read-fixture sh /fat/share/examples/trackpad-checks.sh < /fat/read-fixture/trackpad.txt\n")
                wait(b"Checks recorded. Completion is NOT a pass");wait(b" $ ")
                serial.sendall(b"cat /fat/read-fixture/trackpad-checks/observations.txt\n")
                wait(b"secondary-input: vm-press-skipped tap-skipped");wait(b"drag-cancel: vm-drag-skipped cancel-skipped");wait(b" $ ")
                serial.sendall(b"HOME=/fat/read-fixture sh /fat/share/examples/tap-checks.sh < /fat/read-fixture/tap.txt\n")
                wait(b"Checks recorded. Completion is NOT a pass");wait(b" $ ")
                serial.sendall(b"cat /fat/read-fixture/tap-checks/observations.txt\n")
                wait(b"tap-timing: vm-timing-skipped");wait(b"input-regressions: vm-input-skipped")
                wait(b"network-down-ui: vm-link-down-skipped");wait(b"network-up-ui: vm-link-up-skipped");wait(b" $ ")
                serial.sendall(b"cat /fat/read-fixture/tap-checks/sequence.txt\n")
                wait(b"started\r\ncompleted");wait(b" $ ")
                serial.sendall(b"HOME=/fat/read-fixture sh /fat/share/examples/tap-diagnostics.sh < /fat/read-fixture/tap-diag.txt\n")
                wait(b"Sample recorded; completion is NOT acceptance");wait(b" $ ")
                serial.sendall(b"cat /fat/read-fixture/tap-diagnostics/observations.txt\n")
                wait(b"two-finger-sample: vm-gestures-skipped");wait(b" $ ")
                serial.sendall(b"cat /fat/read-fixture/tap-diagnostics/sequence.txt\n")
                wait(b"started\r\ncompleted");wait(b" $ ")
                serial.sendall(b"exit\n");wait(b"srv> ")
                # 820x500 client + 180x50 drag, minus terminal insets 54x98,
                # with current 10x22 cells: 94 columns and 20 rows.
                for name,contents in [("multiline","report first report second"),("followup","fresh-followup"),("paste","from-clipboard"),("one","terminal-one"),("two","terminal-two"),
                                      ("survived","survived"),("first-alive","first-alive"),("interrupted","130"),("size","20 94"),("edited","abc")]:
                    serial.sendall(f"cat /fat/terminal-{name}.txt\n".encode())
                    wait((contents+"\r\n").encode());wait(b"srv> ")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
    print(f"desktop-terminal: PASS backspace pixel clearing, real shells, multiline paste stays in one read, clipboard paste, scrollback, two sessions, Ctrl-C isolation, close cancel, exit and persisted command output. Evidence: {out}")

if __name__=="__main__": main()
