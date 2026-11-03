#!/usr/bin/env python3
"""Exercise the opt-in VirtIO GPU probe alongside the inherited VGA console."""
import argparse
import hashlib
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


CASES = (
    ("enabled-device", True, True, b"virtio-gpu-probe: PASS"),
    ("enabled-absent", True, False, b"virtio-gpu-probe: SKIP"),
    ("disabled-device", False, True, None),
)


def validate_probe(text, expected):
    markers = re.findall(rb"virtio-gpu-probe: (?:PASS|FAIL|SKIP)", text)
    if markers != ([expected] if expected else []):
        raise AssertionError(f"unexpected probe markers {markers!r}")
    if expected != b"virtio-gpu-probe: PASS":
        return None
    match = re.search(rb"virtio-gpu-probe: PASS status=(\S+) width=(\d+) height=(\d+) fence=(\d+) reset=(\d+) quarantined=(\d+)(?:\r?\n|$)", text)
    if not match:
        raise AssertionError("PASS is missing the complete probe diagnostics")
    status = match.group(1).decode("ascii")
    width, height, fence, reset, quarantined = map(int, match.groups()[1:])
    if not (1 <= width <= 16384 and 1 <= height <= 16384 and fence == 1 and reset == 1 and quarantined == 0):
        raise AssertionError(f"probe diagnostics failed: {match.group(0)!r}")
    return {"status": status, "width": width, "height": height,
            "fence": fence, "reset": reset, "quarantined": quarantined}


def provenance(root):
    def digest(path):
        value = hashlib.sha256()
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                value.update(chunk)
        return value.hexdigest()
    def command(*args):
        return subprocess.check_output(args, cwd=root, text=True).strip()
    return {"source_commit": command("git", "rev-parse", "HEAD"),
            "source_dirty": bool(command("git", "status", "--porcelain")),
            "kernel_sha256": digest(root / "build/iso_root/boot/srvros.elf"),
            "exfat_sha256": digest(root / "build/srvros.exfat"),
            "qemu_version": command("qemu-system-x86_64", "--version").splitlines()[0]}


def run_case(root, base, name, enabled, device, expected, boot_wait, metadata):
    out = base / name
    out.mkdir(parents=True, exist_ok=True)
    for artifact in ("result.json", "console.ppm", "failure.ppm"):
        (out / artifact).unlink(missing_ok=True)
    output = bytearray()
    with tempfile.TemporaryDirectory(prefix="srvros-virtio-gpu-") as directory:
        tmp = Path(directory)
        disk = tmp / "disk.exfat"
        shutil.copyfile(root / "build/srvros.exfat", disk)
        iso = build_iso(str(root), str(tmp), 1280, 800, os.environ.copy(),
                        cmdline="srvros.virtio-gpu=probe" if enabled else None)
        port = choose_port(30001, 38000)
        command = ["qemu-system-x86_64", "-M", "q35", "-m", "512M",
                   "-cdrom", str(iso), "-boot", "d", "-vga", "std",
                   "-drive", f"if=none,id=disk,file={disk},format=raw",
                   "-device", "ich9-ahci,id=ahci", "-device", "ide-hd,drive=disk,bus=ahci.0",
                   "-device", "qemu-xhci,id=xhci", "-device", "usb-kbd,bus=xhci.0",
                   "-serial", f"tcp:127.0.0.1:{port},server=on,wait=off",
                   "-qmp", f"unix:{tmp / 'qmp'},server=on,wait=off",
                   "-display", "none", "-monitor", "none", "-no-reboot"]
        if device:
            command += ["-device", "virtio-gpu-pci,disable-legacy=on"]
        (out / "command.json").write_text(json.dumps(command, indent=2) + "\n")
        serial = qmp = process = None
        with (out / "qemu.log").open("wb") as errors:
            try:
                process = subprocess.Popen(command, stdout=errors, stderr=errors)
                serial = connect_serial(port, 20)
                serial.settimeout(.2)
                qmp = QMP(tmp / "qmp")
                start = time.monotonic()

                def wait_for(marker, start_at, timeout, boot=False):
                    deadline = time.monotonic() + timeout
                    while marker not in output[start_at:]:
                        if process.poll() is not None or time.monotonic() >= deadline:
                            raise RuntimeError(f"{name}: missing {marker!r}; see {out}")
                        if boot:
                            qmp.boot_if_menu_stalled(output, time.monotonic() - start)
                        try:
                            chunk = serial.recv(16384)
                            if not chunk:
                                raise RuntimeError(f"{name}: serial disconnected")
                            output.extend(chunk)
                        except socket.timeout:
                            pass
                    return output.index(marker, start_at) + len(marker)

                wait_for(b"srv> ", 0, boot_wait, boot=True)
                # Requiring a new prompt and a standalone response proves the
                # shell executed input after the probe, not just echoed bytes.
                at = len(output)
                serial.sendall(b"echo virtio-probe-console-ok\n")
                reply = wait_for(b"\nvirtio-probe-console-ok\r\n", at, 15)
                end = wait_for(b"srv> ", reply, 15)
                response = bytes(output[at:end]).replace(b"\r\n", b"\n")
                if b"\nvirtio-probe-console-ok\n" not in response:
                    raise AssertionError(f"{name}: shell response missing: {response!r}")
                if name == "enabled-absent":
                    at = len(output)
                    serial.sendall(b"run /fat/bin/deskd --smoke\n")
                    wait_for(b"deskd: ready;", at, 30)
                    finished = wait_for(b"deskd: exited frames=", at, 45)
                    wait_for(b"srv> ", finished, 15)
                    if b"gfx-present begin" in output[at:]:
                        raise AssertionError("standard VGA unexpectedly acquired VirtIO presentation")
                    frames = re.search(rb"deskd: exited frames=(\d+)", output[at:])
                    if not frames or int(frames.group(1)) < 1:
                        raise AssertionError("standard VGA desktop did not render")
                qmp.call("screendump", {"filename": str(out / "console.ppm")})
                screenshot = (out / "console.ppm").read_bytes()
                header = re.match(rb"P6\s+(\d+)\s+(\d+)\s+255\s", screenshot)
                if not header:
                    raise AssertionError(f"{name}: invalid framebuffer screenshot")
                width, height = map(int, header.groups()); pixels = screenshot[header.end():]
                if width < 280 or height < 240 or len(pixels) != width*height*3 or len({pixels[i:i+3] for i in range(0,len(pixels),3)}) < 2:
                    raise AssertionError(f"{name}: blank or truncated framebuffer screenshot")
                text = bytes(output)
                details = validate_probe(text, expected)
                decoded = text.decode(errors="replace")
                if has_fatal_exception(decoded) or re.search(r"\b(?:FAIL|PANIC)\b", decoded):
                    raise AssertionError(f"{name}: guest failure; see serial.log")
                (out / "result.json").write_text(json.dumps({
                    "case": name, "status": "PASS", "probe": expected.decode() if expected else "disabled",
                    "console": "command and prompt verified", "framebuffer": "console.ppm",
                    "probe_details": details, "provenance": metadata,
                    "standard_desktop": "rendered and exited" if name == "enabled-absent" else "not exercised",
                }, indent=2) + "\n")
            except BaseException:
                if qmp and process and process.poll() is None:
                    try:
                        qmp.call("screendump", {"filename": str(out / "failure.ppm")})
                    except Exception:
                        pass
                raise
            finally:
                (out / "serial.log").write_bytes(output)
                if serial:
                    serial.close()
                if qmp:
                    qmp.stream.close()
                    qmp.sock.close()
                if process:
                    stop_process(process)
    print(f"virtio-gpu-probe: {name} PASS; evidence: {out}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--boot-wait", type=int, default=100)
    args = parser.parse_args()
    if not 10 <= args.boot_wait <= 180:
        parser.error("--boot-wait must be between 10 and 180 seconds")
    root = Path(__file__).resolve().parents[1]
    base = root / "build/review/virtio-gpu-probe"
    metadata = provenance(root)
    for case in CASES:
        run_case(root, base, *case, args.boot_wait, metadata)
    print("virtio-gpu-probe: PASS all opt-in, absent-device and disabled cases")


if __name__ == "__main__":
    main()
