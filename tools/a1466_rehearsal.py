#!/usr/bin/env python3
import argparse
import json
import os
import random
import shutil
import socket
import subprocess
import sys
import tempfile
import time


def read_for(sock, seconds):
    chunks = []
    deadline = time.time() + seconds
    while time.time() < deadline:
        try:
            chunk = sock.recv(8192)
            if not chunk:
                break
            chunks.append(chunk)
        except socket.timeout:
            pass
    return b"".join(chunks)


def read_until(sock, marker, seconds):
    data = b""
    deadline = time.time() + seconds
    while marker not in data and time.time() < deadline:
        data += read_for(sock, 0.5)
    return data


def connect_tcp(port, timeout, label):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            return socket.create_connection(("127.0.0.1", port), timeout=1)
        except OSError:
            time.sleep(0.2)
    raise RuntimeError(f"{label} connection failed")


def qmp_command(sock, command):
    sock.sendall(json.dumps(command).encode("ascii") + b"\r\n")
    data = b""
    while b"\r\n" not in data:
        data += sock.recv(4096)
    return data


def connect_qmp(port, timeout):
    sock = connect_tcp(port, timeout, "qmp")
    sock.settimeout(1)
    sock.recv(4096)
    qmp_command(sock, {"execute": "qmp_capabilities"})
    return sock


def send_qmp_key(sock, key):
    for down in (True, False):
        qmp_command(sock, {
            "execute": "input-send-event",
            "arguments": {
                "events": [{
                    "type": "key",
                    "data": {
                        "down": down,
                        "key": {
                            "type": "qcode",
                            "data": key,
                        },
                    },
                }],
            },
        })


def send_qmp_text(sock, text, key_delay):
    key_names = {
        " ": "spc",
        "\n": "ret",
        "\r": "ret",
        "-": "minus",
        "=": "equal",
        ".": "dot",
        "/": "slash",
        "\\": "backslash",
        "'": "apostrophe",
        ";": "semicolon",
        ",": "comma",
    }
    for char in text:
        key = key_names.get(char, char.lower())
        if len(key) != 1 and key not in key_names.values():
            continue
        send_qmp_key(sock, key)
        time.sleep(key_delay)


def has_fatal_exception(text):
    for line in text.splitlines():
        if "exception:" in line and "breakpoint" not in line:
            return True
    return False


def default_ovmf_path():
    env = os.environ.get("OVMF_CODE")
    if env:
        return env
    candidates = [
        r"C:\msys64\ucrt64\share\qemu\edk2-x86_64-code.fd",
        "/ucrt64/share/qemu/edk2-x86_64-code.fd",
    ]
    for candidate in candidates:
        if os.path.exists(candidate):
            return candidate
    return candidates[0]


def run_monitor_command(sock, command, timeout):
    sock.sendall(command.encode("ascii") + b"\n")
    return read_until(sock, b"srv> ", timeout)


def main():
    parser = argparse.ArgumentParser(
        description="Rehearse the MacBook Air A1466 first-boot checklist in hidden QEMU.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--image", default="build/srvros-usb.img")
    parser.add_argument("--ovmf", default=default_ovmf_path())
    parser.add_argument("--output", default="build/a1466-rehearsal.log")
    parser.add_argument("--memory", default="2G")
    parser.add_argument("--boot-wait", type=float, default=90)
    parser.add_argument("--command-wait", type=float, default=25)
    parser.add_argument("--gui-wait", type=float, default=45)
    parser.add_argument("--usb-key-delay", type=float, default=0.05)
    parser.add_argument("--skip-gui", action="store_true",
        help="Stop after monitor diagnostics instead of launching gui --smoke-autostart.")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    source_image = args.image if os.path.isabs(args.image) else os.path.join(root, args.image)
    output_path = args.output if os.path.isabs(args.output) else os.path.join(root, args.output)
    serial_port = random.randint(30000, 39000)
    qmp_port = random.randint(39001, 45000)

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")

    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-a1466-") as temp_dir:
        image = os.path.join(temp_dir, "srvros-usb.img")
        shutil.copyfile(source_image, image)
        command = [
            args.qemu,
            "-M", "q35",
            "-m", args.memory,
            "-drive", f"if=pflash,format=raw,readonly=on,file={args.ovmf}",
            "-drive", f"if=none,id=usb,file={image},format=raw",
            "-device", "ich9-ahci,id=ahci",
            "-device", "ide-hd,drive=usb,bus=ahci.0",
            "-device", "qemu-xhci,id=xhci",
            "-device", "usb-kbd,bus=xhci.0",
            "-device", "usb-tablet,bus=xhci.0",
            "-serial", f"tcp:127.0.0.1:{serial_port},server,nowait",
            "-monitor", "none",
            "-qmp", f"tcp:127.0.0.1:{qmp_port},server,nowait",
            "-display", "none",
            "-no-reboot",
        ]

        process = subprocess.Popen(command, cwd=root, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        try:
            serial = connect_tcp(serial_port, 20, "serial")
            serial.settimeout(0.3)
            qmp = connect_qmp(qmp_port, 20)
            output += read_until(serial, b"srv> ", args.boot_wait)

            send_qmp_text(qmp, "echo usb-input-ok\n", args.usb_key_delay)
            output += read_until(serial, b"srv> ", args.command_wait)

            for command_text in ("hwdiag", "dmesg 8192", "xhci", "pci", "block"):
                output += run_monitor_command(serial, command_text, args.command_wait)

            if not args.skip_gui:
                serial.sendall(b"run /fat/bin/sh\n")
                output += read_until(serial, b" $ ", args.command_wait)
                serial.sendall(b"gui --smoke-autostart\n")
                output += read_until(serial, b"displayd: exited", args.gui_wait)
                output += read_for(serial, 2)
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    text = output.decode("utf-8", "replace")
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
    sys.stdout.write(text)

    expected = [
        "srv>",
        "usb-input-ok",
        "hwdiag: begin",
        "hwdiag: end",
        "xhci: usb addressed=",
        "exfat: mounted /fat",
    ]
    if not args.skip_gui:
        expected.extend([
            "gui: starting displayd",
            "displayd: root backbuffer ready",
            "displayd: smoke ok",
            "displayd: exited",
        ])
    missing = [marker for marker in expected if marker not in text]
    if has_fatal_exception(text):
        print(f"a1466-rehearsal: fatal exception detected; log: {output_path}", file=sys.stderr)
        return 2
    if missing:
        print(f"a1466-rehearsal: missing markers; log: {output_path}", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3

    print(f"a1466-rehearsal: ok; log: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
