#!/usr/bin/env python3
"""Boot SrvrOS against the custom A1466 LPSS QEMU device model."""

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
        except OSError:
            break
    return b"".join(chunks)


def read_until(sock, marker, seconds):
    data = b""
    deadline = time.time() + seconds
    while marker not in data and time.time() < deadline:
        data += read_for(sock, 0.5)
    return data


def connect_tcp(port, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            return socket.create_connection(("127.0.0.1", port), timeout=1)
        except OSError:
            time.sleep(0.2)
    raise RuntimeError("serial connection failed")


def qmp_read_message(sock, timeout=5):
    sock.settimeout(timeout)
    data = b""
    while b"\n" not in data:
        data += sock.recv(4096)
    line, _, remainder = data.partition(b"\n")
    return json.loads(line.decode("utf-8")), remainder


def qmp_command(sock, execute, arguments=None):
    request = {"execute": execute}
    if arguments is not None:
        request["arguments"] = arguments
    sock.sendall(json.dumps(request).encode("utf-8") + b"\n")
    while True:
        response, _ = qmp_read_message(sock)
        if "return" in response:
            return response["return"]
        if "error" in response:
            raise RuntimeError(f"QMP {execute} failed: {response['error']}")


def qmp_type(sock, text):
    punctuation = {
        " ": "spc",
        "-": "minus",
        "\n": "ret",
    }
    events = []
    for char in text:
        qcode = punctuation.get(char, char.lower())
        if not (char.isalnum() or char in punctuation):
            raise RuntimeError(f"unsupported smoke-test character: {char!r}")
        for down in (True, False):
            events.append({
                "type": "key",
                "data": {
                    "down": down,
                    "key": {"type": "qcode", "data": qcode},
                },
            })
    qmp_command(sock, "input-send-event", {"events": events})


def default_ovmf_path():
    candidates = [
        os.environ.get("OVMF_CODE"),
        r"C:\msys64\ucrt64\share\qemu\edk2-x86_64-code.fd",
        "/ucrt64/share/qemu/edk2-x86_64-code.fd",
        "/usr/share/OVMF/OVMF_CODE_4M.fd",
        "/usr/share/OVMF/OVMF_CODE.fd",
        "/usr/share/edk2/x64/OVMF_CODE.fd",
    ]
    for candidate in candidates:
        if candidate and os.path.isfile(candidate):
            return candidate
    return candidates[1]


def default_qemu_data_path():
    candidates = [
        os.environ.get("QEMU_DATA_DIR"),
        r"C:\msys64\ucrt64\share\qemu",
        "/ucrt64/share/qemu",
        "/usr/share/qemu",
    ]
    for candidate in candidates:
        if candidate and os.path.isdir(candidate):
            return candidate
    return candidates[1]


def has_fatal_exception(text):
    return any("exception:" in line and "breakpoint" not in line
               for line in text.splitlines())


def main():
    parser = argparse.ArgumentParser(
        description="Verify SrvrOS LPSS discovery against the A1466 QEMU model.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu",
        default="build/tooling/qemu-a1466-src/build/qemu-system-x86_64" +
        (".exe" if os.name == "nt" else ""))
    parser.add_argument("--image", default="build/srvros-usb.img")
    parser.add_argument("--ovmf", default=default_ovmf_path())
    parser.add_argument("--qemu-data", default=default_qemu_data_path())
    parser.add_argument("--ssdt", default="build/a1466-input.aml")
    parser.add_argument("--output", default="build/a1466-emulator-smoke.log")
    parser.add_argument("--boot-wait", type=float, default=120)
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    qemu = args.qemu if os.path.isabs(args.qemu) else os.path.join(root, args.qemu)
    source_image = args.image if os.path.isabs(args.image) else os.path.join(root, args.image)
    output_path = args.output if os.path.isabs(args.output) else os.path.join(root, args.output)
    ssdt = args.ssdt if os.path.isabs(args.ssdt) else os.path.join(root, args.ssdt)
    ssdt_argument = os.path.relpath(ssdt, root).replace(os.sep, "/")
    serial_port = random.randint(30000, 45000)
    qmp_port = random.randint(45001, 55000)

    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(filter(None, [
        r"C:\msys64\ucrt64\bin",
        r"C:\msys64\usr\bin",
        env.get("PATH", ""),
    ]))

    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-a1466-emulator-") as temp_dir:
        image = os.path.join(temp_dir, "srvros-usb.img")
        shutil.copyfile(source_image, image)
        command = [
            qemu,
            "-M", "q35,i8042=off,usb=off",
            "-m", "2G",
            "-L", args.qemu_data,
            "-drive", f"if=pflash,format=raw,readonly=on,file={args.ovmf}",
            # QEMU treats ':' as an ACPI file-list separator, so keep this
            # relative on Windows rather than passing a drive-qualified path.
            "-acpitable", f"file={ssdt_argument}",
            "-drive", f"if=none,id=usb,file={image},format=raw",
            "-device", "ich9-ahci,id=ahci",
            "-device", "ide-hd,drive=usb,bus=ahci.0",
            "-device", "a1466-lpss-dma,bus=pcie.0,addr=0x15",
            "-device", "a1466-lpss-spi,bus=pcie.0,addr=0x16",
            "-serial", f"tcp:127.0.0.1:{serial_port},server,nowait",
            "-monitor", "none",
            "-qmp", f"tcp:127.0.0.1:{qmp_port},server=on,wait=off",
            "-display", "none",
            "-no-reboot",
        ]

        qemu_log_path = os.path.join(temp_dir, "qemu-output.log")
        with open(qemu_log_path, "wb") as qemu_log:
            process = subprocess.Popen(command, cwd=root, env=env,
                stdout=qemu_log, stderr=subprocess.STDOUT)
            try:
                serial = connect_tcp(serial_port, 20)
                serial.settimeout(0.3)
                output += read_until(serial, b"srv> ", args.boot_wait)
                qmp = connect_tcp(qmp_port, 10)
                greeting, _ = qmp_read_message(qmp)
                if "QMP" not in greeting:
                    raise RuntimeError("QMP greeting missing")
                qmp_command(qmp, "qmp_capabilities")
                qmp_type(qmp, "echo spi-input-ok\n")
                output += read_until(serial, b"spi-input-ok", 20)
                output += read_until(serial, b"srv> ", 10)
                qmp.close()
                serial.sendall(b"spiregs\n")
                output += read_until(serial, b"srv> ", 15)
                serial.sendall(b"spi\n")
                output += read_until(serial, b"srv> ", 15)
            finally:
                try:
                    process.terminate()
                    process.wait(timeout=3)
                except Exception:
                    process.kill()
                    process.wait(timeout=3)

        with open(qemu_log_path, "rb") as handle:
            qemu_output = handle.read()
        if qemu_output:
            output += b"\n--- qemu output ---\n" + qemu_output

    text = output.decode("utf-8", "replace")
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
    sys.stdout.write(text)

    expected = [
        "srv>",
        "spi-input-ok",
        "keyboard: ps/2 controller unavailable",
        "input: ps2_keyboard=no ps2_mouse=no usb_keyboards=0 usb_mice=0",
        "lpss-spi: polling Apple topcase worker started",
        "lpss-spi: spi1",
        "id=0x000000009cba8086",
        "profile=lpt private_base=0x0000000000000800 prepared=1",
        "lpss-spi-regs: enabled=0 busy=0 tx_not_full=1 rx_not_empty=0",
        "lpss-spi-priv: profile=lpt base=0x0000000000000800",
        "lpss-spi: dma",
        "id=0x000000009ce08086",
        "acpi-input-topcase:",
        "spi1_device=SPI1@",
        "spit_device=SPIT@",
        "keyboard_reports=",
    ]
    missing = [marker for marker in expected if marker not in text]
    report_counts = [int(value) for value in re.findall(r"keyboard_reports=(\d+)", text)]
    reports = max(report_counts, default=0)
    if has_fatal_exception(text):
        print(f"a1466-emulator-smoke: fatal exception; log: {output_path}",
              file=sys.stderr)
        return 2
    if missing:
        print(f"a1466-emulator-smoke: missing markers; log: {output_path}",
              file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3
    if reports == 0:
        print(f"a1466-emulator-smoke: no SPI keyboard reports; log: {output_path}",
              file=sys.stderr)
        return 4

    print(f"a1466-emulator-smoke: ok; log: {output_path}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as error:
        print(f"a1466-emulator-smoke: error: {error}", file=sys.stderr)
        raise SystemExit(1)
