#!/usr/bin/env python3
"""Repeat every native GUI application and desktop lifetime; check cleanup and memory plateau in a disposable VM."""
import argparse
import os
import re
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import time
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from desktop_foundation_smoke import QMP


def main():
    parser=argparse.ArgumentParser();parser.add_argument("--rounds",type=int,default=6);parser.add_argument("--output",type=Path);args=parser.parse_args()
    if not 2<=args.rounds<=30: parser.error("rounds must be 2..30")
    root=Path(__file__).resolve().parents[1]
    out=(args.output or root/"build/review/desktop-lifecycle").resolve()
    out.mkdir(parents=True,exist_ok=True)
    output=b"";consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix="srvros-lifecycle-") as directory:
        tmp=Path(directory)
        from exfat_test_image import ExfatFixture
        from image_fixtures import image_pattern
        fixture=ExfatFixture((root/"build/srvros.exfat").read_bytes())
        fixture.add_directory_files("lifecycle",{"Pattern.png":image_pattern(1024,768)})
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
                samples=[]
                applications=[("Files",1),("Text",2),("Calculator",3),("System",4),
                    ("Settings",5),("Terminal",6),("Preview",7),("Help",8),("Clock",9),
                    ("Sketch",10),("Network Utility",11),("Find Files",12)]
                baseline_processes=None
                def memory_sample(round):
                    nonlocal baseline_processes
                    # Init reaps orphaned workers on its one-second cadence.
                    # Wait for cleanup, but fail rather than mask a persistent PID.
                    cleanup_started=time.monotonic()
                    while True:
                        serial.sendall(b"ps\n");wait(b"pid name status");wait(b"srv> ")
                        process_lines=output.split(b"pid name status")[-1].split(b"srv> ")[0]
                        processes=set(re.findall(rb"^(\d+) (\S+) (background|exited|foreground)",process_lines,re.M))
                        if not processes: raise RuntimeError("Missing process snapshot")
                        if baseline_processes is None: baseline_processes=processes
                        if processes==baseline_processes: break
                        if time.monotonic()-cleanup_started>10:
                            raise RuntimeError("Process set did not return to baseline within 10 seconds: "+process_lines.decode(errors="replace"))
                        time.sleep(.5)
                    start=len(output);serial.sendall(b"memstat\n");wait(b"heap: total=");wait(b"srv> ")
                    heap=re.findall(rb"heap: total=(\d+) free=(\d+) used=(\d+) bytes",output[start:])
                    if not heap: raise RuntimeError("Missing heap snapshot")
                    frames=re.findall(rb"pmm: total=(\d+) free=(\d+) used=(\d+) frames",output[start:])
                    if not frames: raise RuntimeError("Missing physical memory snapshot")
                    tags=dict((name.decode(),int(value)) for name,value in re.findall(rb"pmm: ([a-z_-]+)=(\d+)",output[start:]))
                    samples.append({"round":round,"heap_total":int(heap[-1][0]),"heap_free":int(heap[-1][1]),"heap_used":int(heap[-1][2]),"frames_used":int(frames[-1][2]),"frame_tags":tags,"cleanup_ms":int((time.monotonic()-cleanup_started)*1000),"processes":process_lines.decode(errors="replace")})
                    (out/"samples.json").write_text(json.dumps(samples,indent=2))
                memory_sample(-1)
                for round in range(args.rounds):
                    serial.sendall(b"run /fat/bin/deskd --apps-trace\n")
                    wait(b"deskd: commit")
                    # Keep the initial Text document as a stable focus fallback.
                    for name,number in applications:
                        shortcut("meta_l-spc");wait(b"launcher opened");type_text(name+" new app");qmp.key("ret");wait(f"launcher application={number}".encode())
                        start=consumed;wait(b"deskd: commit")
                        registered=re.findall(rb"deskd: opened pid=(\d+) window=",output[start:consumed])
                        if not registered: raise RuntimeError("Missing application registration")
                        pid=registered[-1]
                        shortcut("meta_l-alt-left");wait(b"arranged pid="+pid+b" mode=1")
                        shortcut("meta_l-alt-right");wait(b"arranged pid="+pid+b" mode=2")
                        shortcut("meta_l-alt-down");wait(b"zoom pid="+pid+b" maximized=0")
                        if name=="Preview":
                            shortcut("ctrl-o");wait(b"deskview: picker ready")
                            shortcut("ctrl-l");type_text("/fat/lifecycle");qmp.key("ret");wait(b"deskview: picker ready")
                            shortcut("tab");type_text("Pattern.png");qmp.key("ret");wait(b"loaded path=/fat/lifecycle/Pattern.png width=1024 height=768")
                        if name=="Find Files":
                            type_text("txt");shortcut("ctrl-ret");wait(b"deskfind: finished result=1 exit=0")
                        if name=="Terminal":
                            type_text("exit\n");wait(b"deskterm: shell exited status=0")
                        # Client and compositor logs can interleave within a line.
                        # Retirement is keyed to this PID; the session's ps snapshot
                        # independently verifies that clients and workers exited.
                        shortcut("meta_l-w");wait(b"deskd: retired pid="+pid)
                    shot(f"round-{round:02}-complete")
                    shortcut("f10");shortcut("end");shortcut("ret");wait(b"deskd: exited",60);wait(b"srv> ")
                    time.sleep(.5);memory_sample(round)
                    print(f"round {round+1}/{args.rounds}: all 12 apps opened/arranged/closed; heap used={samples[-1]['heap_used']}",flush=True)
                # First pass warms caches. Later completed sessions should not
                # accumulate application-owned buffers, surfaces, PTYs or PIDs.
                steady=[sample["heap_used"] for sample in samples[1:]]
                frame_counts=[sample["frames_used"] for sample in samples[1:]]
                if max(frame_counts)-min(frame_counts)>128:
                    raise RuntimeError(f"Physical memory did not plateau within 128 frames: {frame_counts}")
                if max(steady)-min(steady)>65536:
                    raise RuntimeError(f"Heap did not plateau within 64 KiB: {steady}")
                if b"failure=1" in output or b"deskd: FAIL" in output or b"exception:" in output:
                    raise RuntimeError("Guest failure; see serial.log")
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/"serial.log").write_bytes(output)
    print(f"desktop-lifecycle: PASS {args.rounds} desktop sessions, {args.rounds*12} application lifetimes, repeated tiling/restoration, terminal exit, no surviving GUI workers and bounded heap plateau. Evidence: {out}")


if __name__=="__main__": main()
