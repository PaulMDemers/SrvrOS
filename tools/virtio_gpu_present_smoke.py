#!/usr/bin/env python3
"""Exercise primary VirtIO VGA presentation and desktop-owner recovery in QEMU."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
import time

from desktop_foundation_smoke import QMP
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso, has_fatal_exception
from virtio_gpu_probe_smoke import provenance


def ppm(path, min_colors=16):
    data = path.read_bytes()
    header = re.match(rb"P6\s+(\d+)\s+(\d+)\s+255\s", data)
    if not header:
        raise AssertionError(f"invalid framebuffer screenshot: {path}")
    width, height = map(int, header.groups()); pixels = data[header.end():]
    if width < 640 or height < 480 or len(pixels) != width*height*3:
        raise AssertionError(f"invalid framebuffer dimensions: {path}")
    if len({pixels[i:i+3] for i in range(0, len(pixels), 3)}) < min_colors:
        raise AssertionError(f"blank framebuffer screenshot: {path}")
    return width, height, pixels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--boot-wait", type=int, default=100)
    args = parser.parse_args()
    if not 10 <= args.boot_wait <= 180:
        parser.error("--boot-wait must be between 10 and 180")
    root = Path(__file__).resolve().parents[1]
    out = root / "build/review/virtio-gpu-present"
    out.mkdir(parents=True, exist_ok=True)
    (out / "result.json").unlink(missing_ok=True)
    metadata = provenance(root)
    output = bytearray(); consumed = 0
    with tempfile.TemporaryDirectory(prefix="srvros-virtio-present-") as directory:
        tmp = Path(directory); disk = tmp / "disk.exfat"
        shutil.copyfile(root / "build/srvros.exfat", disk)
        iso = build_iso(str(root), str(tmp), 1280, 800, os.environ.copy(),
                        cmdline="srvros.virtio-gpu=present")
        port = choose_port(30001, 38000)
        command = ["qemu-system-x86_64", "-M", "q35", "-m", "512M", "-cdrom", iso, "-boot", "d",
                   "-vga", "none", "-device", "virtio-vga", "-drive", f"if=none,id=disk,file={disk},format=raw",
                   "-device", "ich9-ahci,id=ahci", "-device", "ide-hd,drive=disk,bus=ahci.0",
                   "-device", "qemu-xhci,id=xhci", "-device", "usb-kbd,bus=xhci.0",
                   "-serial", f"tcp:127.0.0.1:{port},server=on,wait=off",
                   "-qmp", f"unix:{tmp / 'qmp'},server=on,wait=off", "-display", "none", "-monitor", "none", "-no-reboot"]
        (out / "command.json").write_text(json.dumps(command, indent=2)+"\n")
        serial = qmp = process = None
        with (out / "qemu.log").open("wb") as errors:
            try:
                process = subprocess.Popen(command, stdout=errors, stderr=errors)
                serial = connect_serial(port, 20); serial.settimeout(.2); qmp = QMP(tmp / "qmp")
                started = time.monotonic()

                def wait(marker, timeout=40, boot=False):
                    nonlocal consumed
                    deadline = time.monotonic()+timeout
                    while marker not in output[consumed:]:
                        if process.poll() is not None or time.monotonic()>deadline:
                            raise RuntimeError(f"missing {marker!r}; see {out / 'serial.log'}")
                        if boot: qmp.boot_if_menu_stalled(output, time.monotonic()-started)
                        try:
                            chunk = serial.recv(16384)
                            if not chunk: raise RuntimeError("serial disconnected")
                            output.extend(chunk)
                        except socket.timeout: pass
                    consumed = output.index(marker, consumed)+len(marker)

                def wait_since(marker, start_at, timeout=40):
                    deadline = time.monotonic()+timeout
                    while marker not in output[start_at:]:
                        if process.poll() is not None or time.monotonic()>deadline:
                            raise RuntimeError(f"missing {marker!r} after session start; see {out / 'serial.log'}")
                        try:
                            chunk = serial.recv(16384)
                            if not chunk: raise RuntimeError("serial disconnected")
                            output.extend(chunk)
                        except socket.timeout: pass

                def shortcut(keys):
                    qmp.call("human-monitor-command", {"command-line": f"sendkey {keys}"});time.sleep(.3)

                def shot(name, min_colors=16, delay=.5):
                    time.sleep(delay); path = out / f"{name}.ppm"
                    qmp.call("screendump", {"filename": str(path)})
                    return ppm(path, min_colors)

                wait(b"virtio-gpu-present: ready", args.boot_wait, boot=True)
                wait(b"srv> ", args.boot_wait, boot=True)
                first_start=len(output)
                serial.sendall(b"run /fat/bin/deskd --apps-trace\n")
                wait(b"deskd: ready;");wait(b"deskd: commit")
                wait_since(b"deskd: gfx-present begin", first_start)
                w, h, before = shot("desktop-before-drag")
                # The first Text title bar begins at (32,64); drag it with capture.
                qmp.mouse(180-24, 80-56);qmp.mouse(0,0,1);qmp.mouse(120,70,1);qmp.mouse(0,0,0)
                wait(b"deskd: moved identity=")
                w2, h2, after = shot("desktop-after-drag")
                if (w,h)!=(w2,h2) or sum(a!=b for a,b in zip(before,after))<3000:
                    raise AssertionError("drag did not visibly update the presented framebuffer")
                shortcut("f10");shortcut("end");shortcut("ret")
                wait(b"deskd: gfx-present end");wait(b"deskd: exited");wait(b"srv> ")
                serial.sendall(b"run /fat/bin/guifail --console-muted\n")
                wait(b"guifail: console-muted=0");wait(b"srv> ");shot("console-after-exit", min_colors=2)
                # The existing study supplies deterministic scrolling and exits itself.
                scroll_start=len(output)
                serial.sendall(b"run /fat/bin/deskd --scroll-smoke\n")
                wait(b"deskd: ready;");wait(b"deskclient: scroll=120");wait(b"deskclient: scroll=96")
                wait_since(b"deskd: gfx-present begin", scroll_start)
                if b"deskd: gfx-present end" in output[scroll_start:]:
                    raise AssertionError("scroll session exited before its screenshot")
                shot("desktop-after-scroll", delay=.1)
                wait(b"deskd: gfx-present end");wait(b"deskd: exited");wait(b"srv> ")
                # Start a separate background owner and terminate it through the
                # monitor, then prove ownership and console are recoverable.
                crash_start=len(output)
                serial.sendall(b"bg /fat/bin/deskd\n")
                wait(b"bg: started pid=");wait(b" ")
                match = re.findall(rb"bg: started pid=(\d+) /fat/bin/deskd", bytes(output))
                if not match:
                    wait(b"/fat/bin/deskd");match = re.findall(rb"bg: started pid=(\d+) /fat/bin/deskd", bytes(output))
                if not match: raise AssertionError("background desktop PID missing")
                pid = match[-1];wait(b"deskd: ready;");wait_since(b"deskd: gfx-present begin", crash_start);time.sleep(1)
                serial.sendall(b"kill "+pid+b"\n");wait(b"kill: requested process stop");time.sleep(2)
                serial.sendall(b"run /fat/bin/guifail --console-muted\n")
                wait(b"guifail: console-muted=0");wait(b"srv> ")
                serial.sendall(b"echo virtio-present-console-ok\n")
                wait(b"\nvirtio-present-console-ok\r\n");wait(b"srv> ")
                shot("console-after-kill", min_colors=2)
                restart_start=len(output)
                serial.sendall(b"run /fat/bin/deskd --smoke\n")
                wait(b"deskd: ready;");wait_since(b"deskd: gfx-present begin", restart_start)
                wait(b"deskd: gfx-present end");wait(b"deskd: exited",60);wait(b"srv> ")
                restarted=bytes(output[restart_start:])
                metrics=re.search(rb"deskd: exited frames=(\d+)", restarted)
                if not metrics or int(metrics.group(1))<1:
                    raise AssertionError("restarted desktop did not present a frame")
                decoded = bytes(output).decode(errors="replace")
                if has_fatal_exception(decoded) or re.search(r"\b(?:FAIL|PANIC)\b", decoded) or "gfx-present frame failed" in decoded:
                    raise AssertionError("guest failure; see serial.log")
                ends = re.findall(r"deskd: exited frames=(\d+)", decoded)
                if bytes(output).count(b"deskd: gfx-present begin") != 4:
                    raise AssertionError("expected four separate presenter acquisitions")
                if len(ends)<3 or int(ends[0])<2:
                    raise AssertionError(f"missing repeated presented frames: {ends}")
                (out / "result.json").write_text(json.dumps({"status":"PASS", "frame_counts":ends,
                    "checks":["glass desktop", "drag updates", "scroll", "normal console restore", "abrupt owner termination", "reacquisition"],
                    "provenance":metadata}, indent=2)+"\n")
            except BaseException:
                if qmp and process and process.poll() is None:
                    try: qmp.call("screendump", {"filename": str(out / "failure.ppm")})
                    except Exception: pass
                raise
            finally:
                (out / "serial.log").write_bytes(output)
                if serial: serial.close()
                if qmp: qmp.stream.close();qmp.sock.close()
                if process: stop_process(process)
    print(f"virtio-gpu-present: PASS primary scanout, desktop and recovery; evidence: {out}")


if __name__ == "__main__":
    main()
