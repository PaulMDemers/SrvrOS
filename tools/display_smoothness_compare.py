#!/usr/bin/env python3
"""Compare desktop software timing with standard VGA and primary VirtIO VGA.

This records compositor-side intervals and host injection timing. It does not
measure display scanout, vsync, physical input latency, or visible tearing.
"""
import argparse
import json
import statistics
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
import threading
import time

from desktop_foundation_smoke import QMP
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso, has_fatal_exception
from virtio_gpu_probe_smoke import provenance

TIMING = re.compile(rb"cursor-timing: (poll-gap|consecutive-motion-gap|consume-to-final-blit|draw-blit) count=(\d+) mean_us=(\d+) max_us=(\d+) p95_bound_us=(\d+) p99_bound_us=(\d+)")


class SerialLog:
    def __init__(self, sock):
        self.sock = sock; self.sock.settimeout(.2); self.data = bytearray()
        self.condition = threading.Condition(); self.closed = False; self.error = None
        self.thread = threading.Thread(target=self._drain, daemon=True); self.thread.start()

    def _drain(self):
        try:
            while True:
                try: chunk = self.sock.recv(16384)
                except socket.timeout:
                    with self.condition:
                        if self.closed: return
                    continue
                if not chunk: break
                with self.condition:
                    self.data.extend(chunk); self.condition.notify_all()
        except OSError as error:
            with self.condition:
                if not self.closed: self.error = str(error)
        finally:
            with self.condition:
                self.closed = True; self.condition.notify_all()

    def snapshot(self):
        with self.condition: return bytes(self.data)

    def wait(self, marker, start, timeout, process, boot_qmp=None):
        begun = time.monotonic(); deadline = begun+timeout
        with self.condition:
            while True:
                pos = self.data.find(marker, start)
                if pos >= 0: return pos+len(marker)
                if self.closed or process.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError(f"missing {marker!r} after offset {start}: {self.error}")
                if boot_qmp: boot_qmp.boot_if_menu_stalled(bytes(self.data), time.monotonic()-begun)
                self.condition.wait(.2)

    def close(self):
        with self.condition: self.closed = True; self.condition.notify_all()
        self.sock.close(); self.thread.join(timeout=2)


def parse_metrics(segment, kind):
    found = TIMING.findall(segment)
    names = ("poll-gap", "consecutive-motion-gap", "consume-to-final-blit", "draw-blit")
    if len(found) != 4 or {m[0].decode() for m in found} != set(names):
        raise AssertionError(f"missing or duplicate compositor timing metrics: {found!r}")
    result = {}
    for name, *values in found:
        count, mean, maximum, p95, p99 = map(int, values)
        if count < 1:
            if name != b"consecutive-motion-gap" or kind != "scroll":
                raise AssertionError(f"zero samples in {name.decode()}")
            result[name.decode()] = {"count":0,"mean_us":None,"max_us":None,
                                     "p95_bound_us":None,"p99_bound_us":None}
        else:
            result[name.decode()] = {"count": count, "mean_us": mean, "max_us": maximum,
                                     "p95_bound_us": p95, "p99_bound_us": p99}
    return result


def screenshot(path, resolution):
    raw=path.read_bytes();header=re.match(rb"P6\s+(\d+)\s+(\d+)\s+255\s",raw)
    if not header: raise AssertionError(f"invalid PPM: {path}")
    width,height=map(int,header.groups());pixels=raw[header.end():]
    if (width,height)!=resolution or len(pixels)!=width*height*3:
        raise AssertionError(f"unexpected framebuffer dimensions or truncated PPM: {path}")
    if len({pixels[i:i+3] for i in range(0,len(pixels),3)})<16:
        raise AssertionError(f"blank or invalid framebuffer: {path}")
    return width,pixels


def window_left(path, resolution):
    width,pixels=screenshot(path, resolution);edges=[]
    for y in (160,240,320,400,480):
        for x in range(0,1000):
            # Neutral, light window body against the fixed blue wallpaper.
            run=True
            for offset in range(10):
                rgb=pixels[((y*width+x+offset)*3):((y*width+x+offset)*3)+3]
                if min(rgb)<220 or max(rgb)-min(rgb)>5:run=False;break
            if run:edges.append(x);break
    if len(edges)<3:raise AssertionError(f"window edge not found in {path}: {edges}")
    return statistics.median(edges)


def call_hmp(qmp, text):
    response=qmp.call("human-monitor-command", {"command-line": text})
    if isinstance(response,str) and re.search(r"\b(error|invalid|unknown)\b",response,re.IGNORECASE):
        raise RuntimeError(f"HMP {text!r} failed: {response.strip()}")
    return response


def move_to(qmp, dx, dy):
    # Relative PS/2 reports are bounded; avoid truncating one large move.
    while dx or dy:
        x=max(-20,min(20,dx)); y=max(-20,min(20,dy))
        call_hmp(qmp, f"mouse_move {x} {y}"); dx-=x;dy-=y;time.sleep(.01)


def paced(qmp, commands, period):
    origin = time.monotonic_ns(); events = []
    for index, command in enumerate(commands):
        target = origin+int(index*period*1e9)
        delay = (target-time.monotonic_ns())/1e9
        if delay>0: time.sleep(delay)
        sent = time.monotonic_ns();call_hmp(qmp,command);done=time.monotonic_ns()
        events.append({"command":command,"scheduled_ns":target-origin,"sent_ns":sent-origin,
                       "completed_ns":done-origin})
    return {"clock":"host CLOCK_MONOTONIC", "origin_ns":origin, "events":events,"count":len(events),"elapsed_ns":time.monotonic_ns()-origin,
            "late_ns_max":max((e["sent_ns"]-e["scheduled_ns"] for e in events),default=0)}


def workload(qmp, kind):
    if kind == "drag":
        move_to(qmp,176,24) # cursor (24,56) -> Help titlebar (200,80)
        press_start=time.monotonic_ns();call_hmp(qmp,"mouse_button 1");press_done=time.monotonic_ns();time.sleep(.2)
        commands=[]
        for direction in (1,-1,1,-1,1):
            last=0
            for i in range(1,91):
                next_position=300*i//90;delta=(next_position-last)*direction;last=next_position
                commands.append(f"mouse_move {delta} 0")
        try:
            trace=paced(qmp,commands,1/60)
        finally:
            release_start=time.monotonic_ns();call_hmp(qmp,"mouse_button 0");release_done=time.monotonic_ns()
        trace["button_press_ns"]=[press_start,press_done]
        trace["button_release_ns"]=[release_start,release_done]
        return trace
    # Help at (32,64), 760x540: client (33,108), document (12,52),
    # vertical track at x=769..780 and initial thumb starts at y=160.
    move_to(qmp,751,112) # cursor (24,56) -> scrollbar thumb (775,168)
    press_start=time.monotonic_ns();call_hmp(qmp,"mouse_button 1");press_done=time.monotonic_ns();time.sleep(.2)
    commands=[]
    for direction in (1,-1,1,-1,1):
        last=0
        for i in range(1,91):
            next_position=120*i//90;delta=(next_position-last)*direction;last=next_position
            commands.append(f"mouse_move 0 {delta}")
    try:
        trace=paced(qmp,commands,1/60)
    finally:
        release_start=time.monotonic_ns();call_hmp(qmp,"mouse_button 0");release_done=time.monotonic_ns()
    trace["button_press_ns"]=[press_start,press_done]
    trace["button_release_ns"]=[release_start,release_done]
    return trace


def run_trial(root, target, iso, backend, repeat, metadata, display, resolution):
    target.mkdir(parents=True,exist_ok=True);(target/"result.json").unlink(missing_ok=True)
    result={"backend":backend,"repeat":repeat,"provenance":metadata,"sessions":{},
            "scope":"Software timings include compositor presentation; exclude device arrival, scanout, vsync and tearing."}
    with tempfile.TemporaryDirectory(prefix="srvros-smoothness-") as directory:
        tmp=Path(directory);disk=tmp/"disk.exfat";shutil.copyfile(root/"build/srvros.exfat",disk)
        port=choose_port(30001,38000)
        cmd=["qemu-system-x86_64","-M","q35","-accel","tcg,thread=single,tb-size=256","-cpu","qemu64","-smp","1","-m","1024M",
             "-cdrom",str(iso),"-boot","d",*( ["-vga","std"] if backend=="std" else ["-vga","none","-device","virtio-vga"] ),
             "-drive",f"if=none,id=disk,file={disk},format=raw","-device","ich9-ahci,id=ahci",
             "-device","ide-hd,drive=disk,bus=ahci.0","-device","qemu-xhci,id=xhci","-device","usb-kbd,bus=xhci.0",
             "-serial",f"tcp:127.0.0.1:{port},server=on,wait=off","-qmp",f"unix:{tmp/'qmp'},server=on,wait=off",
             "-display","none" if display=="none" else "cocoa,zoom-to-fit=on","-monitor","none","-no-reboot"]
        result["qemu_command"]=cmd
        serial=qmp=proc=None; log=None; wall_start=time.monotonic_ns()
        with (target/"qemu.log").open("wb") as qlog:
            try:
                proc=subprocess.Popen(cmd,stdout=qlog,stderr=qlog)
                serial=connect_serial(port,20); log=SerialLog(serial);qmp=QMP(tmp/"qmp")
                cursor=log.wait(b"srv> ",0,120,proc,qmp)
                boot=log.snapshot()[:cursor]
                if has_fatal_exception(boot.decode(errors="replace")) or re.search(rb"\b(?:FAIL|PANIC)\b",boot):
                    raise AssertionError("guest reported boot failure")
                if backend=="virtio" and b"virtio-gpu-present: ready" not in boot:
                    raise AssertionError("VirtIO presentation did not become ready")
                if backend=="std" and b"virtio-gpu-present: ready" in boot:
                    raise AssertionError("standard VGA unexpectedly activated VirtIO presentation")
                for kind in ("drag","scroll"):
                    session_start=len(log.snapshot()); session_wall=time.monotonic_ns()
                    serial.sendall(b"run /fat/bin/deskd --hardware-trial --scale=1\n")
                    log.wait(b"deskd: ready;",session_start,90,proc)
                    log.wait(b"deskhelp: page=laptop-trial",session_start,90,proc)
                    log.wait(b"deskhelp: starting",session_start,90,proc)
                    time.sleep(2) # identical startup settling, outside input trace
                    prior=log.snapshot()[session_start:]
                    if backend=="virtio" and b"deskd: gfx-present begin" not in prior:
                        raise AssertionError("VirtIO desktop did not acquire presenter")
                    if backend=="std" and b"deskd: gfx-present begin" in prior:
                        raise AssertionError("VGA desktop unexpectedly acquired presenter")
                    before_path=target/f"{kind}-before.ppm";after_path=target/f"{kind}-after.ppm"
                    qmp.call("screendump",{"filename":str(before_path)});screenshot(before_path,resolution)
                    trace=workload(qmp,kind);time.sleep(1)
                    qmp.call("screendump",{"filename":str(after_path)});screenshot(after_path,resolution)
                    displacement=None
                    if kind=="drag":
                        left_before=window_left(before_path,resolution);left_after=window_left(after_path,resolution)
                        displacement=left_after-left_before
                        if abs(displacement-300)>20:
                            raise AssertionError(f"drag did not move the visible Help window ~300px: {left_before} -> {left_after}")
                    for key in ("f10","end","ret"):
                        call_hmp(qmp,f"sendkey {key}");time.sleep(.3)
                    cursor=log.wait(b"deskd: exited",session_start,90,proc)
                    cursor=log.wait(b"srv> ",cursor,40,proc)
                    segment=log.snapshot()[session_start:cursor]
                    frames=re.search(rb"deskd: exited frames=(\d+) fb_bytes=(\d+)",segment)
                    if not frames or int(frames.group(1))<2:
                        raise AssertionError(f"{kind}: insufficient completed desktop frames")
                    if backend=="virtio" and b"deskd: gfx-present end" not in segment:
                        raise AssertionError(f"{kind}: presenter did not release")
                    if has_fatal_exception(segment.decode(errors="replace")) or re.search(rb"\b(?:FAIL|PANIC)\b",segment):
                        raise AssertionError(f"{kind}: guest reported failure")
                    offsets=[int(value) for value in re.findall(rb"deskhelp: scroll=(\d+)",segment)]
                    if kind=="scroll" and (not offsets or max(offsets)<=0 or not any(b<a for a,b in zip(offsets,offsets[1:]))):
                        raise AssertionError(f"scrollbar drag did not scroll Help down and back up: {offsets}")
                    result["sessions"][kind]={"timings":parse_metrics(segment,kind),"frames":int(frames.group(1)),
                        "window_displacement_px":displacement,"scroll_offsets":offsets,
                        "framebuffer_bytes":int(frames.group(2)),"input":trace,
                        "wall_ns":time.monotonic_ns()-session_wall,"settle_seconds":2}
                    (target/f"{kind}-serial.log").write_bytes(segment)
                result["wall_ns"]=time.monotonic_ns()-wall_start;result["status"]="PASS"
                (target/"result.json").write_text(json.dumps(result,indent=2)+"\n")
            except BaseException:
                if qmp and proc and proc.poll() is None:
                    try:qmp.call("screendump",{"filename":str(target/"failure.ppm")})
                    except Exception:pass
                raise
            finally:
                if log:
                    (target/"serial.log").write_bytes(log.snapshot());log.close()
                elif serial:serial.close()
                if qmp:qmp.stream.close();qmp.sock.close()
                if proc:stop_process(proc)
    print(f"{backend} repeat {repeat}: PASS; evidence {target}",flush=True)
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repeats",type=int,default=3)
    parser.add_argument("--display",choices=("none","cocoa"),default="none")
    parser.add_argument("--only",choices=("std","virtio"))
    parser.add_argument("--out",type=Path)
    parser.add_argument("--resolution",choices=("1440x900","1280x800"),default="1440x900")
    args=parser.parse_args()
    if not 1<=args.repeats<=20:parser.error("--repeats must be between 1 and 20")
    root=Path(__file__).resolve().parents[1]
    out=(args.out or root/"build/review/display-smoothness").expanduser().resolve();out.mkdir(parents=True,exist_ok=True)
    (out/"summary.json").unlink(missing_ok=True)
    metadata=provenance(root);results=[];resolution=tuple(map(int,args.resolution.split("x")))
    with tempfile.TemporaryDirectory(prefix="srvros-smoothness-iso-") as directory:
        iso={}
        for backend in ((args.only,) if args.only else ("std","virtio")):
            stage=Path(directory)/backend;stage.mkdir()
            iso[backend]=build_iso(str(root),str(stage),*resolution,os.environ.copy(),
                cmdline="srvros.virtio-gpu=present" if backend=="virtio" else None)
        for repeat in range(1,args.repeats+1):
            order=[args.only] if args.only else (["std","virtio"] if repeat%2 else ["virtio","std"])
            for backend in order:
                results.append(run_trial(root,out/f"{backend}-{repeat:02d}",iso[backend],backend,repeat,metadata,args.display,resolution))
    summary={"status":"PASS","provenance":metadata,"runs":results,
        "interpretation":"Software timing only; excludes device arrival, display scanout, vsync and visible tearing."}
    (out/"summary.json").write_text(json.dumps(summary,indent=2)+"\n")
    print(f"display-smoothness: completed {len(results)} paired-session runs; evidence {out}")


if __name__=="__main__":main()
