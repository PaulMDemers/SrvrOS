#!/usr/bin/env python3
"""Launch a persistent, visible x86_64 SrvrOS VM on an Apple Silicon Mac."""
import argparse
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import socket
import subprocess
import sys
import tempfile
import time

from build_manifest import REPO, image_hashes

sys.path.insert(0, str(REPO / "tools"))
from displayd_resolution_smoke import build_iso


def verified_build(repo):
    try:
        record = json.loads((repo / "build/macos-build.json").read_text())
        if record["schema"] != 1 or record["hashes"] != image_hashes(repo):
            raise ValueError("image hashes differ from the recorded build")
        if not record["source"]:
            raise ValueError("missing source identity")
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise RuntimeError(f"Run sh tools/macos/build.sh -j2 all first: {error}") from error
    return record


def disk_identity(hashes):
    return hashlib.sha256(json.dumps(hashes, sort_keys=True).encode()).hexdigest()[:16]


def prepare_disk(state, repo, hashes):
    disk = state / f"disk-{disk_identity(hashes)}.exfat"
    if not disk.exists():
        # A failed copy must not leave a truncated disk that is reused next time.
        with tempfile.NamedTemporaryFile(prefix="disk-", suffix=".partial", dir=state, delete=False) as out:
            partial = Path(out.name)
            try:
                with (repo / "build/srvros.exfat").open("rb") as source:
                    shutil.copyfileobj(source, out)
                out.flush()
                os.fsync(out.fileno())
            except BaseException:
                partial.unlink(missing_ok=True)
                raise
        try:
            partial.replace(disk)
        finally:
            partial.unlink(missing_ok=True)
    return disk


def refuse_running(pidfile):
    if not pidfile.exists():
        return
    try:
        pid = int(pidfile.read_text().strip())
        if pid <= 0:
            raise ValueError("PID must be positive")
    except ValueError as error:
        raise RuntimeError(f"Invalid PID file; inspect {pidfile} before removing it") from error
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return
    except PermissionError:
        pass
    raise RuntimeError(f"PID {pid} still exists. Close the previous QEMU window first; no process was stopped.")


def option_path(path):
    # QEMU key/value options escape a literal comma by doubling it.
    return str(path).replace(",", ",,")


def qmp_call(path, command, arguments=None):
    with socket.socket(socket.AF_UNIX) as sock:
        sock.settimeout(10)
        sock.connect(str(path))
        stream = sock.makefile("rb")
        stream.readline()

        def request(name, args):
            sock.sendall((json.dumps({"execute": name, "arguments": args}) + "\n").encode())
            while True:
                line = stream.readline()
                if not line:
                    raise RuntimeError("QEMU monitor disconnected")
                reply = json.loads(line)
                if "error" in reply:
                    raise RuntimeError(str(reply["error"]))
                if "return" in reply:
                    return reply["return"]

        request("qmp_capabilities", {})
        return request(command, arguments or {})


def launch(args, state):
    refuse_running(state / "qemu.pid")
    build = verified_build(REPO)
    disk = prepare_disk(state, REPO, build["hashes"])
    session = state / datetime.datetime.now(datetime.timezone.utc).strftime("session-%Y%m%dT%H%M%S.%fZ")
    session.mkdir()
    iso = build_iso(str(REPO), str(session), 1440, 900, os.environ.copy(),
                    kernel_path=str(REPO / "build/kernel/srvros.elf"),
                    initramfs_path=str(REPO / "build/initramfs.tar"),
                    cmdline="srvros.log=quiet srvros.audio=hda")
    # macOS AF_UNIX paths are short. Never put sockets under a long checkout or
    # TMPDIR path; mkdtemp gives this instance a private, unique short directory.
    sockets = Path(tempfile.mkdtemp(prefix="srvros-", dir="/tmp"))
    serial, qmp = sockets / "serial", sockets / "qmp"
    pidfile = state / "qemu.pid"
    pidfile.unlink(missing_ok=True)
    serial_log = session / "serial.log"
    command = [args.qemu, "-name", "SrvrOS - x86_64 on Apple Silicon",
        "-machine", "q35", "-accel", "tcg,thread=single,tb-size=256",
        "-cpu", "qemu64", "-smp", "1", "-m", "1024M",
        "-cdrom", iso, "-boot", "d",
        "-drive", f"if=none,id=disk,file={option_path(disk)},format=raw",
        "-device", "ich9-ahci,id=ahci", "-device", "ide-hd,drive=disk,bus=ahci.0",
        "-device", "qemu-xhci,id=xhci", "-device", "usb-kbd,bus=xhci.0",
        "-device", "usb-mouse,bus=xhci.0",
        "-netdev", "user,id=net0", "-device", "e1000,netdev=net0",
        "-audiodev", ("none" if args.headless else "coreaudio") + ",id=audio0",
        "-device", "ich9-intel-hda", "-device", "hda-output,audiodev=audio0",
        "-chardev", f"socket,id=serial0,path={serial},server=on,wait=off,logfile={option_path(serial_log)}",
        "-serial", "chardev:serial0", "-qmp", f"unix:{qmp},server=on,wait=off",
        "-monitor", "none", "-display", "none" if args.headless else "cocoa,zoom-to-fit=on",
        "-pidfile", str(pidfile), "-no-reboot"]
    record = {"build": build, "disk": str(disk), "session": str(session),
              "sockets": str(sockets), "command": command}

    def save_record():
        data = json.dumps(record, indent=2) + "\n"
        (state / "current.json").write_text(data)
        (session / "manifest.json").write_text(data)

    with (session / "qemu.log").open("wb") as log:
        try:
            process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=log,
                                       stderr=log, start_new_session=True, cwd=REPO)
        except OSError:
            shutil.rmtree(sockets)
            raise
    record["pid"] = process.pid
    save_record()
    started = time.monotonic()

    def wait_for(marker, timeout):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError(f"QEMU exited; see {session / 'qemu.log'}")
            if serial_log.exists() and marker in serial_log.read_bytes():
                return
            time.sleep(0.25)
        raise RuntimeError(f"No {marker!r} after {timeout}s; VM left running; see {serial_log}")

    print(f"QEMU PID {process.pid}; booting with TCG. Logs: {session}", flush=True)
    wait_for(b"srv> ", 180)
    record["console_ready_seconds"] = round(time.monotonic() - started, 2)
    with socket.socket(socket.AF_UNIX) as sock:
        sock.connect(str(serial))
        sock.sendall(b"run /fat/bin/deskd --apps-trace\n")
    wait_for(b"deskd: ready;", 120)
    record["desktop_ready_seconds"] = round(time.monotonic() - started, 2)
    wait_for(b"deskd: commit", 120)
    record["first_app_frame_seconds"] = round(time.monotonic() - started, 2)
    qmp_call(qmp, "screendump", {"filename": str(session / "desktop.ppm")})
    record["status"] = qmp_call(qmp, "query-status")
    save_record()
    print(f"Desktop ready after {record['desktop_ready_seconds']}s; first app frame after "
          f"{record['first_app_frame_seconds']}s. QEMU is left running.", flush=True)
    print(f"Persistent guest disk: {disk}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--state-dir", type=Path, default=REPO / ".local/macos-vm",
                        help="persistent disks/logs; default: .local/macos-vm in the repo")
    parser.add_argument("--qemu", default="qemu-system-x86_64")
    parser.add_argument("--headless", action="store_true", help="no window or host audio; still leaves VM running")
    args = parser.parse_args()
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        parser.error("this launcher requires Apple Silicon macOS")
    for command in (args.qemu, "xorriso"):
        if shutil.which(command) is None:
            parser.error(f"missing {command}; see docs/macos-development.md")
    state = args.state_dir.expanduser().resolve()
    state.mkdir(parents=True, exist_ok=True)
    with (state / "launch.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            parser.error("another launcher is preparing this VM state directory")
        try:
            launch(args, state)
        except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
            parser.exit(1, f"macOS VM: {error}\n")


if __name__ == "__main__":
    main()
