#!/usr/bin/env python3
import argparse
import json
import os
import random
import re
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


def read_until_any(sock, markers, seconds):
    data = b""
    deadline = time.time() + seconds
    while not any(marker in data for marker in markers) and time.time() < deadline:
        data += read_for(sock, 0.5)
    return data


def connect_serial(port, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            return socket.create_connection(("127.0.0.1", port), timeout=1)
        except OSError:
            time.sleep(0.2)
    raise RuntimeError("serial connection failed")


def connect_qmp(port, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            sock = socket.create_connection(("127.0.0.1", port), timeout=1)
            sock.settimeout(1)
            sock.recv(4096)
            qmp_command(sock, {"execute": "qmp_capabilities"})
            return sock
        except OSError:
            time.sleep(0.2)
    raise RuntimeError("qmp connection failed")


def qmp_command(sock, command):
    sock.sendall(json.dumps(command).encode("ascii") + b"\r\n")
    data = b""
    while b"\r\n" not in data:
        data += sock.recv(4096)
    return data


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


def send_qmp_mouse_move(sock, dx, dy):
    qmp_command(sock, {
        "execute": "human-monitor-command",
        "arguments": {
            "command-line": f"mouse_move {dx} {dy}",
        },
    })


def has_fatal_exception(text):
    for line in text.splitlines():
        if "exception:" in line and "breakpoint" not in line:
            return True
    return False


def device_line_matches(text, required, min_intr=None, min_reports=None, min_mouse_reports=None):
    for line in text.splitlines():
        if not line.startswith("xhci: device "):
            continue
        if any(token not in line for token in required):
            continue
        if min_intr is not None:
            match = re.search(r"\bintr=(\d+)", line)
            if match is None or int(match.group(1)) < min_intr:
                continue
        if min_reports is not None:
            match = re.search(r"\breports=(\d+)", line)
            if match is None or int(match.group(1)) < min_reports:
                continue
        if min_mouse_reports is not None:
            match = re.search(r"\bmouse_reports=(\d+)", line)
            if match is None or int(match.group(1)) < min_mouse_reports:
                continue
        return True
    return False


def numeric_marker_at_least(text, name, minimum):
    for match in re.finditer(rf"\b{re.escape(name)}=(\d+)", text):
        if int(match.group(1)) >= minimum:
            return True
    return False


def marker_occurrences_at_least(text, name, value, minimum):
    pattern = rf"\b{re.escape(name)}={re.escape(str(value))}\b"
    return len(re.findall(pattern, text)) >= minimum


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


def main():
    parser = argparse.ArgumentParser(description="Boot the srvros GPT/FAT32 USB image through OVMF.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--image", default="build/srvros-usb.img")
    parser.add_argument("--ovmf", default=default_ovmf_path())
    parser.add_argument("--boot-wait", type=float, default=90)
    parser.add_argument("--memory", default="2G")
    parser.add_argument("--no-xhci", action="store_true",
        help="Do not add QEMU's xHCI PCI controller to the smoke boot.")
    parser.add_argument("--no-usb-kbd", action="store_true",
        help="Do not attach a QEMU USB keyboard to the xHCI controller.")
    parser.add_argument("--no-usb-mouse", action="store_true",
        help="Do not attach a QEMU USB mouse to the xHCI controller.")
    parser.add_argument("--usb-hub", action="store_true",
        help="Attach the USB keyboard and mouse behind a QEMU USB hub.")
    parser.add_argument("--usb-type-text", default="",
        help="Type this monitor command through QEMU's keyboard event path, then press Enter.")
    parser.add_argument("--usb-type-expect", default="",
        help="Text expected in serial output after --usb-type-text is submitted.")
    parser.add_argument("--usb-mouse-move", default="",
        help="Send a QEMU mouse_move 'dx,dy' event through the USB mouse path.")
    parser.add_argument("--usb-key-delay", type=float, default=0.05)
    parser.add_argument("--usb-input-settle", type=float, default=1.5,
        help="Seconds to keep reading after QMP USB input before serial monitor commands.")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    source_image = args.image if os.path.isabs(args.image) else os.path.join(root, args.image)
    serial_port = random.randint(30000, 39000)
    qmp_port = random.randint(39001, 45000)

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")

    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-uefi-usb-") as temp_dir:
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
            "-serial", f"tcp:127.0.0.1:{serial_port},server,nowait",
            "-monitor", "none",
            "-qmp", f"tcp:127.0.0.1:{qmp_port},server,nowait",
            "-display", "none",
            "-no-reboot",
        ]
        if not args.no_xhci:
            command.extend(["-device", "qemu-xhci,id=xhci"])
            kbd_bus = "xhci.0"
            kbd_port = None
            mouse_bus = "xhci.0"
            mouse_port = None
            if args.usb_hub:
                command.extend(["-device", "usb-hub,bus=xhci.0,port=1"])
                kbd_port = "1.1"
                mouse_port = "1.2"
            if not args.no_usb_kbd:
                device = f"usb-kbd,bus={kbd_bus}"
                if kbd_port is not None:
                    device += f",port={kbd_port}"
                command.extend(["-device", device])
            if not args.no_usb_mouse:
                device = f"usb-mouse,bus={mouse_bus}"
                if mouse_port is not None:
                    device += f",port={mouse_port}"
                command.extend(["-device", device])
        process = subprocess.Popen(command, cwd=root, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        try:
            sock = connect_serial(serial_port, 20)
            sock.settimeout(0.3)
            qmp = connect_qmp(qmp_port, 20)
            output += read_until_any(sock, [b"srv> ", b" $ "], args.boot_wait)
            if b"srv> " in output:
                if args.usb_type_text and not args.no_xhci and not args.no_usb_kbd:
                    send_qmp_text(qmp, args.usb_type_text + "\n", args.usb_key_delay)
                    output += read_for(sock, args.usb_input_settle)
                    output += read_until_any(sock, [b"srv> "], 10)
                if args.usb_mouse_move and not args.no_xhci and not args.no_usb_mouse:
                    parts = args.usb_mouse_move.replace("x", ",").split(",", 1)
                    dx = int(parts[0])
                    dy = int(parts[1]) if len(parts) > 1 else 0
                    send_qmp_mouse_move(qmp, dx, dy)
                    output += read_for(sock, 1)
                sock.sendall(b"bootinfo\n")
                output += read_until_any(sock, [b"srv> "], 10)
                sock.sendall(b"dmesg 512\n")
                output += read_until_any(sock, [b"srv> "], 10)
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)

    missing = []
    if "exfat: mounted /fat" not in text:
        missing.append("exfat mount")
    if "srv>" not in text and " $ " not in text:
        missing.append("shell or monitor prompt")
    if "pci: devices=" not in text:
        missing.append("PCI inventory")
    if "config=ecam" not in text:
        missing.append("ECAM PCI config")
    if not args.no_xhci:
        expected_hid_devices = int(not args.no_usb_kbd) + int(not args.no_usb_mouse)
        expected_addressed = expected_hid_devices + int(args.usb_hub and expected_hid_devices > 0)
        expected_root_devices = 1 if args.usb_hub and expected_hid_devices > 0 else expected_hid_devices
        if "xhci: vendor=" not in text or "op=yes" not in text:
            missing.append("xHCI inventory")
        if not numeric_marker_at_least(text, "enable_slot", max(1, expected_addressed)):
            missing.append("xHCI command completion")
        if expected_root_devices > 0 and not marker_occurrences_at_least(text, "connected", 1, expected_root_devices):
            missing.append("xHCI connected port")
        if expected_root_devices > 0 and not numeric_marker_at_least(text, "ports_enabled", expected_root_devices):
            missing.append("xHCI port reset")
        if args.usb_hub and expected_hid_devices > 0 and not numeric_marker_at_least(text, "hubs", 1):
            missing.append("USB hub enumeration")
        if not args.no_usb_kbd and not numeric_marker_at_least(text, "hid_keyboards", 1):
            missing.append("USB HID keyboard enumeration")
        if not args.no_usb_kbd and not device_line_matches(text, ["addressed=1", "configured=1", "keyboard=1"]):
            missing.append("USB HID keyboard device state")
        if not args.no_usb_mouse and not numeric_marker_at_least(text, "hid_mice", 1):
            missing.append("USB HID mouse enumeration")
        if not args.no_usb_mouse and not device_line_matches(text, ["addressed=1", "configured=1", "mouse=1"]):
            missing.append("USB HID mouse device state")
        if args.usb_type_text:
            if not device_line_matches(text, ["keyboard=1"], min_intr=1, min_reports=1):
                missing.append("USB HID keyboard input reports")
            if args.usb_type_expect and args.usb_type_expect not in text:
                missing.append("USB HID typed command output")
        if args.usb_mouse_move and not device_line_matches(text, ["mouse=1"], min_intr=1, min_mouse_reports=1):
            missing.append("USB HID mouse input reports")
    if "dmesg 512" not in text:
        missing.append("dmesg output")
    if has_fatal_exception(text):
        print("uefi-usb-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("uefi-usb-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3

    print("uefi-usb-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
