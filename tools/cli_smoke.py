#!/usr/bin/env python3
import argparse
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
            chunk = sock.recv(4096)
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


def send_serial(sock, text, delay):
    data = text.encode("ascii")
    if delay <= 0:
        sock.sendall(data)
        return
    for byte in data:
        sock.sendall(bytes([byte]))
        time.sleep(delay)


def connect_serial(port, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            return socket.create_connection(("127.0.0.1", port), timeout=1)
        except OSError:
            time.sleep(0.2)
    raise RuntimeError("serial connection failed")


def has_fatal_exception(text):
    for line in text.splitlines():
        if "exception:" in line and "breakpoint" not in line:
            return True
    return False


def main():
    parser = argparse.ArgumentParser(description="Run a srvros shell CLI serial smoke test.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--disk", default="build/srvros.exfat")
    parser.add_argument("--boot-wait", type=float, default=20)
    parser.add_argument("--shell-wait", type=float, default=2)
    parser.add_argument("--after-wait", type=float, default=10)
    parser.add_argument("--line-wait", type=float, default=0.7)
    parser.add_argument("--send-delay", type=float, default=0.001)
    parser.add_argument("--memory", default="512M")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    iso = args.iso if os.path.isabs(args.iso) else os.path.join(root, args.iso)
    source_disk = args.disk if os.path.isabs(args.disk) else os.path.join(root, args.disk)
    port = random.randint(24000, 29000)

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")

    output = b""
    script = (
        "echo shell-ok\n"
        "ls /fat/bin\n"
        "which sh true false\n"
        "export TESTVAR=cli-ok\n"
        "env\n"
        "echo var-$TESTVAR\n"
        "echo braced-${TESTVAR}\n"
        "false && echo should-not-run\n"
        "echo after-false-$?\n"
        "true || echo should-not-run\n"
        "false || echo or-ok\n"
        "echo pid-$$\n"
        "echo /fat/status*.txt\n"
        "echo /fat/hello.ht?l\n"
        "test -f /fat/status.txt && cp /fat/status.txt /fat/test-file-copy.txt\n"
        "stat /fat/test-file-copy.txt\n"
        "[ -d /fat/bin ] && cp /fat/status.txt /fat/test-dir-copy.txt\n"
        "stat /fat/test-dir-copy.txt\n"
        "[ 5 -gt 3 ] && cp /fat/status.txt /fat/test-int-copy.txt\n"
        "stat /fat/test-int-copy.txt\n"
        "test missing = missing && cp /fat/status.txt /fat/test-string-copy.txt\n"
        "stat /fat/test-string-copy.txt\n"
        "test -e /fat/nope || cp /fat/status.txt /fat/test-miss-copy.txt\n"
        "stat /fat/test-miss-copy.txt\n"
        "export PATH=/fat/bin:/\n"
        "which true\n"
        "which sleep date touch basename dirname\n"
        "sleep 0\n"
        "date\n"
        "touch /fat/touched.txt\n"
        "stat /fat/touched.txt\n"
        "echo tmpdir-$TMPDIR\n"
        "TMP=$(mktemp /fat/tmp/smoke.XXXXXX)\n"
        "echo tmp-$TMP\n"
        "stat $TMP\n"
        "AUTO=$(mktemp)\n"
        "echo auto-$AUTO\n"
        "stat $AUTO\n"
        "SPACED=\"$(echo two words)\"\n"
        "echo assign-$SPACED\n"
        "basename /fat/bin/sh\n"
        "dirname /fat/bin/sh\n"
        "/fat/bin/env FOO=bar\n"
        "/fat/bin/pwd\n"
        "true\n"
        "false\n"
        "cat /fat/status.txt\n"
        "wc /fat/status.txt\n"
        "stat /fat/status.txt\n"
        "head -n 1 /fat/status.txt\n"
        "tail -n 1 /fat/status.txt\n"
        "head -1 /fat/status.txt\n"
        "cat /fat/status.txt | tee /fat/tee-copy.txt\n"
        "stat /fat/tee-copy.txt\n"
        "uname\n"
        "uname -a\n"
        "hostname\n"
        "uptime\n"
        "find /fat/etc\n"
        "find /fat/bin -name sh\n"
        "find /fat/bin -type f -name sh\n"
        "find /fat -type d -name bin\n"
        "du /fat/status.txt\n"
        "du -s /fat/www\n"
        "df\n"
        "df /fat\n"
        "write /fat/words.txt banana\n"
        "write -a /fat/words.txt apple\n"
        "write -a /fat/words.txt banana\n"
        "write -a /fat/words.txt apple\n"
        "tail -1 /fat/words.txt\n"
        "sort /fat/words.txt > /fat/sorted.txt\n"
        "cat /fat/sorted.txt\n"
        "sort /fat/words.txt | uniq > /fat/unique.txt\n"
        "cat /fat/unique.txt\n"
        "wc -l /fat/words.txt\n"
        "wc -c /fat/status.txt\n"
        "write /fat/table.txt name:paul\n"
        "write -a /fat/table.txt name:codex\n"
        "cut -d : -f 2 /fat/table.txt\n"
        "cat /fat/words.txt | xargs echo args\n"
        "sed s/apple/orange/g /fat/words.txt > /fat/sed.txt\n"
        "cat /fat/sed.txt\n"
        "grep exFAT /fat/status.txt\n"
        "grep -n -i exfat /fat/status.txt\n"
        "grep -c apple /fat/words.txt\n"
        "grep -v banana /fat/words.txt\n"
        "grep -q exFAT /fat/status.txt && echo grep-q-ok\n"
        "cat /fat/status.txt > /fat/cat-redir.txt\n"
        "cat /fat/status.txt >> /fat/cat-redir.txt\n"
        "stat /fat/cat-redir.txt\n"
        "wc /fat/cat-redir.txt\n"
        "cat < /fat/status.txt > /fat/stdin-redir.txt\n"
        "stat /fat/stdin-redir.txt\n"
        "tap 2> /fat/tap-usage.txt > /fat/tap-stdout.txt\n"
        "stat /fat/tap-usage.txt\n"
        "stat /fat/tap-stdout.txt\n"
        "true > /fat/empty-redir.txt\n"
        "stat /fat/empty-redir.txt\n"
        "tap > /fat/tap-combined.txt 2>&1\n"
        "stat /fat/tap-combined.txt\n"
        "cat /fat/tap-combined.txt\n"
        "tap 2>> /fat/tap-usage.txt > /fat/tap-stdout2.txt\n"
        "stat /fat/tap-usage.txt\n"
        "export CHILD_ENV=from-parent\n"
        "/fat/bin/env > /fat/env-redir.txt\n"
        "grep CHILD_ENV /fat/env-redir.txt\n"
        "ls /fat/bin > /fat/bin-list.txt\n"
        "grep webd /fat/bin-list.txt\n"
        "echo \"two  spaces\"; echo semi-ok\n"
        "cat /fat/etc/init.sh\n"
        "cat /fat/etc/profile\n"
        "echo redirected > /fat/redir.txt\n"
        "echo appended >> /fat/redir.txt\n"
        "cat /fat/redir.txt\n"
        "write /fat/boot.sh \"echo script-ok\"\n"
        "write -a /fat/boot.sh \"echo appended-script-ok\"\n"
        "source /fat/boot.sh\n"
        "sh /fat/boot.sh\n"
        "sh -c 'echo command-mode-ok'\n"
        "echo \"unterminated\n"
        "for n in one two three; do echo loop-$n; done\n"
        "write /fat/for.sh 'for n in red blue; do'\n"
        "write -a /fat/for.sh 'echo color-$n'\n"
        "write -a /fat/for.sh 'done'\n"
        "sh /fat/for.sh\n"
        "greet() { echo hi-$1; }\n"
        "type greet\n"
        "greet srvros\n"
        "stopper() { echo before-return; return 7; echo after-return; }\n"
        "stopper\n"
        "echo return-status-$?\n"
        "write /fat/fn.sh 'multifn() {'\n"
        "write -a /fat/fn.sh 'echo multi-$1'\n"
        "write -a /fat/fn.sh '}'\n"
        "write -a /fat/fn.sh 'multifn ok'\n"
        "sh /fat/fn.sh\n"
        "sh -c 'echo cargs-$0-$1-$#-$@' cmain one two\n"
        "echo 'echo scriptargs-$0-$1-$#-$@' > /fat/args.sh\n"
        "sh /fat/args.sh alpha beta\n"
        "alias ll='ls /fat/bin'\n"
        "type ll sh cd missingcmd\n"
        "ll > /fat/alias-list.txt\n"
        "grep sh /fat/alias-list.txt\n"
        "FOO=barevalue\n"
        "echo bare-$FOO\n"
        "export FOO\n"
        "unset FOO\n"
        "echo unset-${FOO}\n"
        "cd /fat\n"
        "pwd\n"
        "cd -\n"
        "pwd\n"
        "cd /fat/status.txt\n"
        "read READVAR\n"
        "typed input\n"
        "echo read-$READVAR\n"
        "write /fat/sete.sh 'set -e'\n"
        "write -a /fat/sete.sh 'false'\n"
        "write -a /fat/sete.sh 'echo should-not-run'\n"
        "sh /fat/sete.sh\n"
        "echo subst-$(echo command-sub-ok)\n"
        "echo quote-\"$(echo two words)\"\n"
        "echo nested-$(echo $(echo inner-ok))\n"
        "if test -f /fat/status.txt; then echo if-ok; else echo if-bad; fi\n"
        "if test -f /fat/nope; then echo if-bad; else echo if-else-ok; fi\n"
        "write /fat/if.sh \"if test -f /fat/status.txt; then\"\n"
        "write -a /fat/if.sh \"echo multiline-if-ok\"\n"
        "write -a /fat/if.sh \"else\"\n"
        "write -a /fat/if.sh \"echo multiline-if-bad\"\n"
        "write -a /fat/if.sh \"fi\"\n"
        "sh /fat/if.sh\n"
        "write /fat/badif.sh \"if test -f /fat/status.txt; then\"\n"
        "write -a /fat/badif.sh \"echo unfinished-if\"\n"
        "sh /fat/badif.sh\n"
        "cp /fat/status.txt /fat/status-copy.txt\n"
        "stat /fat/status-copy.txt\n"
        "mkdir -p /fat/tree/a/b\n"
        "write /fat/tree/a/b/file.txt nested-copy\n"
        "cp -r /fat/tree /fat/tree-copy\n"
        "cat /fat/tree-copy/a/b/file.txt\n"
        "rm -r /fat/tree\n"
        "stat /fat/tree/a/b/file.txt\n"
        "rm -r /fat/tree-copy\n"
        "stat /fat/tree-copy/a/b/file.txt\n"
        "cp /fat/redir.txt /fat/redir-copy.txt\n"
        "cat /fat/redir-copy.txt\n"
        "cp /fat/bin/sh /fat/sh-copy\n"
        "stat /fat/sh-copy\n"
        "rm /fat/sh-copy\n"
        "ps\n"
        "fpdemo\n"
        "fpdemo &\n"
        "fpdemo &\n"
        "wait\n"
        "wait\n"
        "write /fat/shell.txt hello-from-sh\n"
        "cat /fat/shell.txt\n"
        "write /fat/move-src.txt move-me\n"
        "mv /fat/move-src.txt /fat/move-dst.txt\n"
        "cat /fat/move-dst.txt\n"
        "stat /fat/move-src.txt\n"
        "rm /fat/move-dst.txt\n"
        "mkdir -p /fat/mvdir/target\n"
        "write /fat/move-into.txt moved-into\n"
        "mv /fat/move-into.txt /fat/mvdir\n"
        "cat /fat/mvdir/move-into.txt\n"
        "mkdir /fat/mvdir/emptydir\n"
        "mv /fat/mvdir/emptydir /fat/mvdir/target\n"
        "stat /fat/mvdir/target/emptydir\n"
        "rm -r /fat/mvdir\n"
        "stat /fat/mvdir/move-into.txt\n"
        "tap /fat/tap-copy.txt /fat/status.txt\n"
        "stat /fat/tap-copy.txt\n"
        "cat /fat/tap-copy.txt\n"
        "cat /fat/status.txt | tap /fat/piped-copy.txt\n"
        "stat /fat/piped-copy.txt\n"
        "cat /fat/piped-copy.txt\n"
        "cat /fat/status.txt | grep static | tap /fat/piped3.txt\n"
        "stat /fat/piped3.txt\n"
        "cat /fat/piped3.txt\n"
        "cat /fat/status.txt | grep exFAT | wc\n"
        "cat /fat/status.txt | head -n 1 | grep webd\n"
        "cat /fat/status.txt | grep static > /fat/pipeline-redir.txt\n"
        "stat /fat/pipeline-redir.txt\n"
        "cat /fat/pipeline-redir.txt\n"
        "cat /fat/status.txt | grep exFAT >> /fat/pipeline-redir.txt\n"
        "stat /fat/pipeline-redir.txt\n"
        "rm /fat/shell.txt\n"
        "stat /fat/shell.txt\n"
        "write /fat/shell.txt hello-again\n"
        "cat /fat/shell.txt\n"
        "rm /fat/shell.txt\n"
        "stat /fat/shell.txt\n"
        "posixdemo\n"
        "\x04"
    )
    with tempfile.TemporaryDirectory(prefix="srvros-cli-") as temp_dir:
        disk = os.path.join(temp_dir, "srvros-cli.exfat")
        shutil.copyfile(source_disk, disk)
        command = [
            args.qemu,
            "-M", "q35",
            "-m", args.memory,
            "-cdrom", iso,
            "-boot", "d",
            "-serial", f"tcp:127.0.0.1:{port},server,nowait",
            "-drive", f"if=none,id=exfat,file={disk},format=raw",
            "-device", "ich9-ahci,id=ahci",
            "-device", "ide-hd,drive=exfat,bus=ahci.0",
            "-monitor", "none",
            "-no-reboot",
        ]

        process = subprocess.Popen(command, cwd=root, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        try:
            sock = connect_serial(port, 15)
            sock.settimeout(0.3)
            output += read_until(sock, b"srv> ", args.boot_wait)
            send_serial(sock, "run /fat/bin/sh\n", args.send_delay)
            output += read_until(sock, b" $ ", args.shell_wait)
            lines = script.splitlines(True)
            for line in lines:
                send_serial(sock, line, args.send_delay)
                if line == "\x04":
                    output += read_until(sock, b"srv> ", max(args.line_wait, 2.0))
                elif line.strip() == "exit":
                    output += read_for(sock, args.line_wait)
                else:
                    output += read_until(sock, b" $ ", max(args.line_wait, 2.0))
            output += read_for(sock, args.after_wait)
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    text = output.decode("utf-8", "replace")
    sys.stdout.write(text)

    expected = [
        "srvsh: interactive shell",
        "shell-ok",
        "sh",
        "/fat/bin/true",
        "/fat/bin/false",
        "TESTVAR=cli-ok",
        "var-cli-ok",
        "braced-cli-ok",
        "after-false-1",
        "or-ok",
        "pid-",
        "/fat/status.txt",
        "/fat/hello.html",
        "/fat/test-file-copy.txt: 55 bytes",
        "/fat/test-dir-copy.txt: 55 bytes",
        "/fat/test-int-copy.txt: 55 bytes",
        "/fat/test-string-copy.txt: 55 bytes",
        "/fat/test-miss-copy.txt: 55 bytes",
        "PATH=/fat/bin:/",
        "/fat/bin/sleep",
        "/fat/bin/date",
        "/fat/bin/touch",
        "/fat/bin/basename",
        "/fat/bin/dirname",
        "uptime ",
        "/fat/touched.txt: 0 bytes",
        "tmpdir-/fat/tmp",
        "tmp-/fat/tmp/smoke.",
        "/fat/tmp/smoke.",
        "auto-/fat/tmp/tmp.",
        "/fat/tmp/tmp.",
        "assign-two words",
        "sh",
        "/fat/bin",
        "FOO=bar",
        "status 1",
        "status.txt",
        "srvros webd: static file serving from exFAT is online.",
        "/fat/status.txt: 55 bytes",
        "/fat/tee-copy.txt: 55 bytes",
        "srvros srvros 0.1 x86_64",
        "up ",
        "/fat/etc/profile",
        "/fat/bin/sh",
        "/fat/bin",
        "55\t/fat/status.txt",
        "\t/fat/www",
        "Filesystem",
        "1K-blocks",
        "Mounted on",
        "exfat",
        "apple",
        "banana",
        "4 /fat/words.txt",
        "55 /fat/status.txt",
        "paul",
        "codex",
        "args banana apple banana apple",
        "orange",
        "1:srvros webd: static file serving from exFAT is online.",
        "2",
        "grep-q-ok",
        "sh: unmatched quote",
        "/fat/cat-redir.txt: 110 bytes",
        "2 18 110 /fat/cat-redir.txt",
        "/fat/stdin-redir.txt: 55 bytes",
        "/fat/tap-usage.txt: 41 bytes",
        "/fat/tap-stdout.txt: 0 bytes",
        "/fat/empty-redir.txt: 0 bytes",
        "/fat/tap-combined.txt: 41 bytes",
        "usage: tap [-a] <copy-path> [input-path]",
        "/fat/tap-usage.txt: 82 bytes",
        "CHILD_ENV=from-parent",
        "webd",
        "two  spaces",
        "semi-ok",
        "init-script-ok",
        "srvros login shell profile",
        "export PS1",
        "redirected",
        "appended",
        "script-ok",
        "appended-script-ok",
        "command-mode-ok",
        "loop-one",
        "loop-two",
        "loop-three",
        "color-red",
        "color-blue",
        "greet is a shell function",
        "hi-srvros",
        "before-return",
        "return-status-7",
        "multi-ok",
        "cargs-cmain-one-2-one two",
        "scriptargs-/fat/args.sh-alpha-2-alpha beta",
        "ll is aliased to 'ls /fat/bin'",
        "sh is /fat/bin/sh",
        "cd is a shell builtin",
        "missingcmd not found",
        "bare-barevalue",
        "unset-",
        "/fat",
        "cd: not a directory: /fat/status.txt",
        "read-typed input",
        "subst-command-sub-ok",
        "quote-two words",
        "nested-inner-ok",
        "if-ok",
        "if-else-ok",
        "multiline-if-ok",
        "source: unterminated block",
        "/fat/status-copy.txt: 55 bytes",
        "nested-copy",
        "stat: not found: /fat/tree/a/b/file.txt",
        "stat: not found: /fat/tree-copy/a/b/file.txt",
        "/fat/sh-copy:",
        "PID STATE",
        "/fat/bin/sh",
        "fpdemo: ok pid=",
        "hello-from-sh",
        "move-me",
        "stat: not found: /fat/move-src.txt",
        "moved-into",
        "/fat/mvdir/target/emptydir: 0 bytes directory",
        "stat: not found: /fat/mvdir/move-into.txt",
        "/fat/tap-copy.txt: 55 bytes",
        "/fat/piped-copy.txt: 55 bytes",
        "/fat/piped3.txt: 55 bytes",
        "1 9 55",
        "/fat/pipeline-redir.txt: 55 bytes",
        "/fat/pipeline-redir.txt: 110 bytes",
        "hello-again",
        "stat: not found: /fat/shell.txt",
        "posixdemo: start pid=",
        "posixdemo: read=hello from posix",
        "posixdemo: dup write ok",
        "posixdemo: fs api ok",
        "posixdemo: statvfs ok",
        "posixdemo: nonblock ok",
        "posixdemo: poll ok",
        "posixdemo: pipe ok",
        "posixdemo: malloc ok",
        "posixdemo: stdlib extra ok",
        "posixdemo: math ok",
        "posixdemo: pread ok",
        "posixdemo: posix misc ok",
        "posixdemo: socket bind ok",
        "posixdemo: ok",
        "exit",
    ]
    missing = [marker for marker in expected if marker not in text]
    if has_fatal_exception(text):
        print("cli-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if missing:
        print("cli-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3
    print("cli-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
