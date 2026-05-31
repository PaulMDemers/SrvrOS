#!/usr/bin/env python3
import argparse
import os
import random
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time


def read_for(sock, seconds):
    chunks = []
    deadline = time.time() + seconds
    while time.time() < deadline:
        try:
            chunk = sock.recv(4096)
            if not chunk:
                break
            chunks.append(chunk)
        except socket.timeout:
            pass
        except (ConnectionResetError, OSError):
            break
    return b"".join(chunks)


def read_until(sock, marker, seconds):
    data = b""
    deadline = time.time() + seconds
    while marker not in data and time.time() < deadline:
        data += read_for(sock, 0.5)
    return data


def read_until_all_after_launch(sock, markers, seconds):
    data = b""
    deadline = time.time() + seconds
    encoded = [marker.encode("utf-8") for marker in markers if marker]
    while time.time() < deadline:
        launch_index = data.find(b"run: entering")
        runtime = data[launch_index:] if launch_index >= 0 else b""
        if encoded and all(marker in runtime for marker in encoded):
            break
        data += read_for(sock, 0.5)
    return data


def monitor_quote_arg(arg):
    if arg == "":
        return '""'
    needs_quote = any(c in arg for c in " \t\r\n\"'\\")
    if not needs_quote:
        return arg
    escaped = []
    for c in arg:
        if c == "\\" or c == '"':
            escaped.append("\\")
        escaped.append(c)
    return '"' + "".join(escaped) + '"'


def make_node_command(path, node_args):
    parts = ["run", path]
    parts.extend(monitor_quote_arg(arg) for arg in node_args)
    return " ".join(parts) + "\n"


def connect_serial(port, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            return socket.create_connection(("127.0.0.1", port), timeout=1)
        except OSError:
            time.sleep(0.2)
    raise RuntimeError("serial connection failed")


def unescape_arg(text):
    return text.encode("utf-8").decode("unicode_escape").encode("utf-8")


class OneShotTcpServer:
    def __init__(self, port, send_text, timeout):
        self.port = port
        self.send_bytes = unescape_arg(send_text)
        self.timeout = timeout
        self.received = b""
        self.error = None
        self.ready = threading.Event()
        self.thread = threading.Thread(target=self._run, daemon=True)

    def start(self):
        self.thread.start()
        if not self.ready.wait(self.timeout):
            raise RuntimeError("host TCP server did not become ready")

    def join(self):
        self.thread.join(self.timeout)

    def _run(self):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
                server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                server.bind(("127.0.0.1", self.port))
                server.listen(1)
                server.settimeout(self.timeout)
                self.ready.set()
                conn, _addr = server.accept()
                with conn:
                    conn.settimeout(self.timeout)
                    if self.send_bytes:
                        conn.sendall(self.send_bytes)
                    self.received = read_for(conn, self.timeout)
        except Exception as exc:
            self.error = exc
            self.ready.set()


def main():
    parser = argparse.ArgumentParser(description="Launch the experimental srvros Node ELF and capture its first runtime boundary.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/node-srvros-runtime/srvros-node.exfat")
    parser.add_argument("--node-elf", default="build/node-srvros-runtime/node-srvros-stripped.elf",
        help="Stripped Node ELF used when --script-text injects a script into a temporary exFAT image.")
    parser.add_argument("--program", default="/fat/bin/node")
    parser.add_argument("--memory", default="768M")
    parser.add_argument("--display", default="none",
        help="QEMU display backend. Defaults to none so smoke runs do not open a QEMU window.")
    parser.add_argument("--net", action="store_true",
        help="Attach the srvros-supported QEMU e1000 device using user-mode networking.")
    parser.add_argument("--hostfwd", action="append", default=[],
        help="Add a QEMU user networking hostfwd rule, for example tcp:127.0.0.1:18080-10.0.2.15:8080. Implies --net.")
    parser.add_argument("--boot-wait", type=float, default=30)
    parser.add_argument("--runtime-wait", type=float, default=30)
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--allow-boundary", action="store_true",
        help="Return success after capturing an assertion or exception while exploring the next runtime blocker.")
    parser.add_argument("--eval", dest="eval_text",
        help="Run Node with -e TEXT instead of the default --version probe.")
    parser.add_argument("--script-text",
        help="Write TEXT to /fat/node-smoke.js and run that script.")
    parser.add_argument("--script-path", default="/fat/node-smoke.js")
    parser.add_argument("--include", action="append", default=[],
        help="Add NAME=HOST_PATH to the temporary exFAT image when --script-text is used.")
    parser.add_argument("--node-arg", action="append", default=[],
        help="Append a raw Node argument. Overrides the default --version probe unless --eval or --script-text is used.")
    parser.add_argument("--expect", action="append", default=[],
        help="Require this substring in the captured serial output. May be repeated.")
    parser.add_argument("--success-on-expect", action="store_true",
        help="Return success once all expected output is observed, even if the process remains running.")
    parser.add_argument("--probe-tcp-port", type=int, default=0,
        help="Connect to this localhost TCP port while QEMU is still running.")
    parser.add_argument("--probe-send", default="",
        help="Bytes to send during --probe-tcp-port, with simple backslash escapes such as \\r\\n.")
    parser.add_argument("--probe-expect", default="",
        help="Require this substring in the TCP probe response.")
    parser.add_argument("--probe-timeout", type=float, default=10)
    parser.add_argument("--probe-repeat", type=int, default=1,
        help="Run the localhost TCP probe this many times against the same guest server.")
    parser.add_argument("--serve-tcp-port", type=int, default=0,
        help="Start a one-shot localhost TCP server for guest outbound connection tests.")
    parser.add_argument("--serve-send", default="",
        help="Bytes sent by --serve-tcp-port when the guest connects.")
    parser.add_argument("--serve-expect", default="",
        help="Require this substring in data received by --serve-tcp-port.")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    iso = args.iso if os.path.isabs(args.iso) else os.path.join(root, args.iso)
    source_disk = args.disk if os.path.isabs(args.disk) else os.path.join(root, args.disk)
    node_elf = args.node_elf if os.path.isabs(args.node_elf) else os.path.join(root, args.node_elf)
    if not args.skip_build:
        subprocess.check_call(["make", "node-runtime-image"], cwd=root)
    if not os.path.exists(iso):
        raise FileNotFoundError(iso)
    if not os.path.exists(source_disk):
        raise FileNotFoundError(source_disk)

    injected_script_text = None
    if args.eval_text is not None:
        node_args = ["-e", args.eval_text]
        expected = args.expect
    elif args.script_text is not None:
        injected_script_text = args.script_text
        if args.script_path == "/fat/node-smoke.js":
            args.script_path = "/fat/bin/node-smoke.js"
        node_args = args.node_arg if args.node_arg else [args.script_path]
        expected = args.expect
    elif args.node_arg:
        node_args = args.node_arg
        expected = args.expect
    else:
        node_args = [] if args.program != "/fat/bin/node" else ["--version"]
        expected = args.expect or (["v24.16.0"] if args.program == "/fat/bin/node" else [])

    serial_port = random.randint(24000, 29000)
    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")
    qemu = shutil.which(args.qemu, path=env.get("PATH")) or args.qemu

    output = b""
    probe_output = b""
    probe_error = None
    host_server = None
    if args.serve_tcp_port > 0:
        host_server = OneShotTcpServer(args.serve_tcp_port, args.serve_send, args.probe_timeout)
        host_server.start()
    with tempfile.TemporaryDirectory(prefix="srvros-node-runtime-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-node.exfat")
        if injected_script_text is not None:
            if not os.path.exists(node_elf):
                raise FileNotFoundError(node_elf)
            script_file = os.path.join(temp_dir, "node-smoke.js")
            with open(script_file, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(injected_script_text)
                if not injected_script_text.endswith("\n"):
                    handle.write("\n")
            image_args = [
                sys.executable,
                os.path.join(root, "tools", "mk_exfat_image.py"),
                disk,
                f"node={node_elf}",
                f"node-smoke.js={script_file}",
            ]
            for item in args.include:
                if "=" not in item:
                    raise ValueError(f"--include expects NAME=HOST_PATH, got {item!r}")
                name, host_path = item.split("=", 1)
                if not os.path.isabs(host_path):
                    host_path = os.path.join(root, host_path)
                image_args.append(f"{name}={host_path}")
            subprocess.check_call(image_args, cwd=root)
        else:
            shutil.copyfile(source_disk, disk)
        command = [
            qemu,
            "-M", "q35",
            "-m", args.memory,
            "-cdrom", iso,
            "-boot", "d",
            "-serial", f"tcp:127.0.0.1:{serial_port},server,nowait",
            "-drive", f"if=none,id=exfat,file={disk},format=raw",
            "-device", "ich9-ahci,id=ahci",
            "-device", "ide-hd,drive=exfat,bus=ahci.0",
            "-monitor", "none",
            "-display", args.display,
            "-no-reboot",
        ]
        if args.net or args.hostfwd:
            netdev = "user,id=net0"
            for rule in args.hostfwd:
                netdev += f",hostfwd={rule}"
            command.extend(["-netdev", netdev, "-device", "e1000,netdev=net0"])
        startupinfo = None
        creationflags = 0
        if os.name == "nt":
            startupinfo = subprocess.STARTUPINFO()
            startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startupinfo.wShowWindow = 0
            creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        process = subprocess.Popen(command, cwd=root, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
            startupinfo=startupinfo, creationflags=creationflags)
        try:
            sock = connect_serial(serial_port, 15)
            sock.settimeout(0.3)
            output += read_until(sock, b"srv> ", args.boot_wait)
            if b"srv> " not in output:
                raise RuntimeError("monitor prompt not observed")
            sock.sendall(make_node_command(args.program, node_args).encode("utf-8"))
            if expected:
                output += read_until_all_after_launch(sock, expected, args.runtime_wait)
            else:
                output += read_for(sock, args.runtime_wait)
            if args.probe_tcp_port > 0:
                payload = unescape_arg(args.probe_send)
                for _ in range(max(1, args.probe_repeat)):
                    try:
                        with socket.create_connection(("127.0.0.1", args.probe_tcp_port), timeout=args.probe_timeout) as probe:
                            probe.settimeout(args.probe_timeout)
                            if payload:
                                probe.sendall(payload)
                            probe_output += read_for(probe, args.probe_timeout)
                    except OSError as exc:
                        probe_error = exc
                        break
                output += read_for(sock, 2)
            if host_server is not None:
                host_server.join()
                output += read_for(sock, 2)
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)
    probe_text = probe_output.decode("utf-8", "replace")
    if args.probe_tcp_port > 0:
        if probe_error is not None:
            print(f"node-runtime-smoke: tcp probe failed: {probe_error}", file=sys.stderr)
        else:
            print(f"node-runtime-smoke: tcp probe received: {probe_text!r}")
    if host_server is not None:
        if host_server.error is not None:
            print(f"node-runtime-smoke: host tcp server failed: {host_server.error}", file=sys.stderr)
        else:
            host_text = host_server.received.decode("utf-8", "replace")
            print(f"node-runtime-smoke: host tcp server received: {host_text!r}")

    launch_index = text.find("run: entering")
    if launch_index < 0:
        print("node-runtime-smoke: Node process did not launch", file=sys.stderr)
        return 2
    runtime_text = text[launch_index:]
    missing = [needle for needle in expected if needle and needle not in runtime_text]
    if args.probe_tcp_port > 0:
        if probe_error is not None:
            return 8
        if args.probe_expect and args.probe_expect not in probe_text:
            print("node-runtime-smoke: tcp probe response missing expected data", file=sys.stderr)
            return 9
    if host_server is not None:
        host_text = host_server.received.decode("utf-8", "replace")
        if host_server.error is not None:
            return 10
        if args.serve_expect and args.serve_expect not in host_text:
            print("node-runtime-smoke: host tcp server missing expected data", file=sys.stderr)
            return 11
    if not missing and args.success_on_expect:
        print("node-runtime-smoke: observed expected output")
        return 0
    if not missing and "run: pid=" in text and "exited status=0" in text:
        print("node-runtime-smoke: ok")
        return 0
    if "exception:" in text:
        print("node-runtime-smoke: captured first runtime exception", file=sys.stderr)
        return 0 if args.allow_boundary else 4
    if "Assertion failed:" in text:
        print("node-runtime-smoke: captured Node assertion boundary", file=sys.stderr)
        return 0 if args.allow_boundary else 5
    if "process: exited status=" in text or "run: pid=" in text:
        print("node-runtime-smoke: process returned without expected success", file=sys.stderr)
        return 0 if args.allow_boundary else 6

    print("node-runtime-smoke: launch captured; process did not finish before timeout", file=sys.stderr)
    return 0 if args.allow_boundary else 7


if __name__ == "__main__":
    raise SystemExit(main())
