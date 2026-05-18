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
    parser.add_argument("--after-wait", type=float, default=2)
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
        "help shell\n"
        "help service\n"
        "help -l\n"
        "help cli\n"
        "man shell\n"
        "man profile\n"
        "apropos server\n"
        "help --help\n"
        "service --help\n"
        "more --plain /fat/share/help/more.txt\n"
        "for c in ls cat more cp rm mkdir mv tap wc grep; do $c --help; done\n"
        "for c in head tail tee find du df sort uniq cut xargs seq realpath id whoami readlink cmp yes install diff tar gzip gunzip patch make; do $c --help; done\n"
        "for c in sed expr printf tr dd stat chmod which touch mktemp basename; do $c --help; done\n"
        "for c in dirname uname hostname uptime date pwd env ps kill svscan; do $c --help; done\n"
        "for c in webd httpget udpdns udpecho host ping netstat ifconfig route arp; do $c --help; done\n"
        "ls /fat/share/help\n"
        "ls -- /fat/share/examples\n"
        "cat -- /fat/share/examples/hello.sh\n"
        "sh --login -c 'echo profile-d-$PROFILE_D-$PAGER-$EDITOR-$HOME'\n"
        "ls -a /fat | grep .srvros\n"
        "ls -la /fat /fat/etc\n"
        "echo ls-d-before; ls -d /fat/bin; echo ls-d-after\n"
        "ls -1 /fat/bin | grep sh\n"
        "which sh true false\n"
        "export TESTVAR=cli-ok\n"
        "env\n"
        "echo var-$TESTVAR\n"
        "echo braced-${TESTVAR}\n"
        "echo default-${MISSING:-fallback}\n"
        "echo assign-default-${ASSIGNME:=set-by-expansion}\n"
        "echo assigned-$ASSIGNME\n"
        "echo alt-${ASSIGNME:+present}\n"
        "echo len-${#ASSIGNME}\n"
        "echo trim-prefix-${ASSIGNME#set-}\n"
        "echo trim-suffix-${ASSIGNME%-expansion}\n"
        "echo arith-$((1 + 2 * 3))\n"
        "N=5\n"
        "echo arith-var-$((N * (N + 1) / 2))\n"
        "echo arith-logic-$((N > 3 && N < 10))\n"
        "false && echo should-not-run\n"
        "echo after-false-$?\n"
        "true || echo should-not-run\n"
        "false || echo or-ok\n"
        "echo pid-$$\n"
        "echo /fat/status*.txt\n"
        "echo /fat/hello.ht?l\n"
        "test -f /fat/status.txt && cp /fat/status.txt /fat/test-file-copy.txt\n"
        "stat /fat/test-file-copy.txt\n"
        "test -s /fat/status.txt && echo test-s-ok\n"
        "test -r /fat/status.txt && echo test-r-ok\n"
        "test -w /fat/status.txt && echo test-w-ok\n"
        "test -x /fat/status.txt || echo test-not-x-ok\n"
        "[ -d /fat/bin ] && cp /fat/status.txt /fat/test-dir-copy.txt\n"
        "stat /fat/test-dir-copy.txt\n"
        "[ 5 -gt 3 ] && cp /fat/status.txt /fat/test-int-copy.txt\n"
        "stat /fat/test-int-copy.txt\n"
        "test missing = missing && cp /fat/status.txt /fat/test-string-copy.txt\n"
        "stat /fat/test-string-copy.txt\n"
        "test -e /fat/nope || cp /fat/status.txt /fat/test-miss-copy.txt\n"
        "stat /fat/test-miss-copy.txt\n"
        "test -a /fat/status.txt -a ! -e /fat/nope && echo test-and-not-ok\n"
        "test -e /fat/nope -o /fat/status.txt -nt /fat/nope && echo test-or-nt-ok\n"
        "[ /fat/status.txt -ef /fat/status.txt ] && echo test-ef-ok\n"
        "export PATH=/fat/bin:/\n"
        "which true\n"
        "which sleep date touch basename dirname expr printf tr\n"
        "sleep 0\n"
        "date\n"
        "touch /fat/touched.txt\n"
        "sync\n"
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
        "env FOO=child sh -c 'echo env-run-$FOO'\n"
        "env -i FOO=clean sh -c 'echo env-clean-$FOO'\n"
        "/fat/bin/pwd\n"
        "true\n"
        "false\n"
        "cat /fat/status.txt\n"
        "wc /fat/status.txt\n"
        "stat /fat/status.txt\n"
        "head -n 1 /fat/status.txt\n"
        "tail -n 1 /fat/status.txt\n"
        "head -- /fat/status.txt\n"
        "tail -- /fat/status.txt\n"
        "wc -- /fat/status.txt\n"
        "grep -- exFAT /fat/status.txt\n"
        "head -1 /fat/status.txt\n"
        "cat /fat/status.txt | tee /fat/tee-copy.txt\n"
        "stat /fat/tee-copy.txt\n"
        "uname\n"
        "uname -a\n"
        "hostname\n"
        "hostn\t\n"
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
        "printf 'one two three' | xargs -n2 echo batch\n"
        "printf 'uno dos tres' | xargs --max-args=2 echo xlong\n"
        "printf '' | xargs -r echo no-run\n"
        "echo xargs-r-ok\n"
        "seq 3\n"
        "seq 2 2 6\n"
        "realpath ./fat/../fat/status.txt\n"
        "echo id-root-$(id -u)-$(whoami)\n"
        "readlink -f ./fat/../fat/status.txt\n"
        "cmp -s /fat/status.txt /fat/status.txt && echo cmp-same-ok\n"
        "cmp -s /fat/status.txt /fat/words.txt || echo cmp-diff-ok\n"
        "yes port | head -n 2\n"
        "install -D -m 644 /fat/status.txt /fat/install/sub/status-copy.txt\n"
        "cat /fat/install/sub/status-copy.txt\n"
        "diff -q /fat/status.txt /fat/install/sub/status-copy.txt && echo diff-same-ok\n"
        "diff -u /fat/status.txt /fat/words.txt || echo diff-diff-ok\n"
        "tar -cf /fat/archive.tar /fat/install\n"
        "tar -tf /fat/archive.tar\n"
        "mkdir -p /fat/extract\n"
        "tar -xf /fat/archive.tar -C /fat/extract\n"
        "cat /fat/extract/fat/install/sub/status-copy.txt\n"
        "realpath /fat/extract/fat/install/sub/status-copy.txt\n"
        "gzip -c /fat/status.txt > /fat/status.txt.gz\n"
        "gunzip -c /fat/status.txt.gz > /fat/status-gunzip.txt\n"
        "cmp -s /fat/status.txt /fat/status-gunzip.txt && echo gzip-roundtrip-ok\n"
        "gzip -c /fat/archive.tar > /fat/archive.tar.gz\n"
        "gunzip -c /fat/archive.tar.gz > /fat/archive-round.tar\n"
        "tar -tf /fat/archive-round.tar\n"
        "echo gzip-tar-ok\n"
        "write /fat/patch-target.txt alpha\n"
        "write -a /fat/patch-target.txt beta\n"
        "write -a /fat/patch-target.txt gamma\n"
        "write /fat/change.patch '--- /fat/patch-target.txt'\n"
        "write -a /fat/change.patch '+++ /fat/patch-target.txt'\n"
        "write -a /fat/change.patch '@@ -1,3 +1,4 @@'\n"
        "write -a /fat/change.patch ' alpha'\n"
        "write -a /fat/change.patch '-beta'\n"
        "write -a /fat/change.patch '+BETA'\n"
        "write -a /fat/change.patch '+inserted'\n"
        "write -a /fat/change.patch ' gamma'\n"
        "patch -i /fat/change.patch\n"
        "cat /fat/patch-target.txt\n"
        "mkdir -p /fat/maketest/src /fat/maketest/build\n"
        "write /fat/maketest/src/input.txt make-source\n"
        "write /fat/maketest/Makefile 'OUT = /fat/maketest/build/out.txt'\n"
        "write -a /fat/maketest/Makefile 'all: $(OUT)'\n"
        "write -a /fat/maketest/Makefile '$(OUT): /fat/maketest/src/input.txt'\n"
        "write -a /fat/maketest/Makefile 'cp $< $@'\n"
        "write -a /fat/maketest/Makefile '.PHONY: install'\n"
        "write -a /fat/maketest/Makefile 'install: all'\n"
        "write -a /fat/maketest/Makefile 'install -D $(OUT) /fat/local/bin/make-out.txt'\n"
        "make -f /fat/maketest/Makefile install\n"
        "cat /fat/local/bin/make-out.txt\n"
        "make -f /fat/maketest/Makefile all\n"
        "make --file=/fat/maketest/Makefile --dry-run install\n"
        "make --always-make --file /fat/maketest/Makefile all\n"
        "sed s/apple/orange/g /fat/words.txt > /fat/sed.txt\n"
        "cat /fat/sed.txt\n"
        "sed -n /apple/p /fat/words.txt\n"
        "sed /banana/d /fat/words.txt\n"
        "sed -e s/banana/grape/ -e /apple/d /fat/words.txt\n"
        "write /fat/longopts.txt Alpha\n"
        "write -a /fat/longopts.txt beta\n"
        "grep --ignore-case --line-number --regexp=alpha /fat/longopts.txt\n"
        "grep --fixed-strings --quiet beta /fat/longopts.txt && echo grep-long-ok\n"
        "sed --quiet --expression=/beta/p /fat/longopts.txt\n"
        "find /fat -name longopts.txt -type f -print\n"
        "mkdir -p /fat/longsrc/sub\n"
        "write /fat/longsrc/sub/data.txt long-copy\n"
        "cp --recursive --force /fat/longsrc /fat/longdst\n"
        "cat /fat/longdst/sub/data.txt\n"
        "ls --directory --long /fat/longdst\n"
        "mv --force /fat/longopts.txt /fat/longopts-moved.txt\n"
        "cat /fat/longopts-moved.txt\n"
        "tar --create --file=/fat/long.tar /fat/longdst\n"
        "tar --list --file=/fat/long.tar\n"
        "mkdir -p /fat/longextract\n"
        "tar --extract --file=/fat/long.tar --directory /fat/longextract\n"
        "cat /fat/longextract/fat/longdst/sub/data.txt\n"
        "rm --recursive --force /fat/longsrc /fat/longdst /fat/longextract\n"
        "rm --force /fat/longopts-moved.txt /fat/long.tar\n"
        "echo expr-add-$(expr 7 + 5)\n"
        "echo expr-mul-$(expr 6 '*' 7)\n"
        "echo expr-eq-$(expr apple = apple)\n"
        "echo expr-ne-$(expr apple != banana)\n"
        "echo expr-len-$(expr length banana)\n"
        "echo expr-sub-$(expr substr banana 2 3)\n"
        "echo expr-index-$(expr index banana na)\n"
        "echo expr-lt-$(expr 1 '<' 2)\n"
        "echo expr-prefix-$(expr banana : ban)\n"
        "printf 'printf-%s-%d-%x\\n' ok 42 255 > /fat/printf.txt\n"
        "cat /fat/printf.txt\n"
        "printf 'repeat-%s\\n' red blue > /fat/printf-repeat.txt\n"
        "cat /fat/printf-repeat.txt\n"
        "printf 'abc123\\n' | tr a-z A-Z > /fat/tr-upper.txt\n"
        "cat /fat/tr-upper.txt\n"
        "printf 'abc123\\n' | tr -d 0-9 > /fat/tr-delete.txt\n"
        "cat /fat/tr-delete.txt\n"
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
        "cat /fat/status.t\t\n"
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
        "mkdir -p /fat/cp-many\n"
        "cp -f /fat/status.txt /fat/redir.txt /fat/cp-many\n"
        "stat /fat/cp-many/status.txt\n"
        "cat /fat/cp-many/redir.txt\n"
        "write /fat/boot.sh \"echo script-ok\"\n"
        "write -a /fat/boot.sh \"echo appended-script-ok\"\n"
        "source /fat/boot.sh\n"
        "sh /fat/boot.sh\n"
        "sh -c 'echo command-mode-ok'\n"
        "echo comment-ok # ignored by shell\n"
        "{ echo group-one; echo group-two; }; echo group-tail\n"
        "false || { echo group-or-ok; }\n"
        "TEMPVAR=local /fat/bin/env > /fat/local-env.txt\n"
        "grep TEMPVAR /fat/local-env.txt\n"
        "echo after-local-${TEMPVAR}\n"
        "TEMPVAR=one OTHER=two sh -c 'echo local-$TEMPVAR-$OTHER'\n"
        "printf 'empty-%s-end\\n' \"\" > /fat/empty-arg.txt\n"
        "cat /fat/empty-arg.txt\n"
        "printf 'space-%s\\n' \"two words\" > /fat/space-arg.txt\n"
        "cat /fat/space-arg.txt\n"
        "printf 'escape-%s\\n' two\\ words > /fat/escape-arg.txt\n"
        "cat /fat/escape-arg.txt\n"
        "write /fat/compat.sh 'echo script-comment-ok # ignored'\n"
        "write -a /fat/compat.sh echo\\ continuation-\\\n"
        "write -a /fat/compat.sh 'ok'\n"
        "write -a /fat/compat.sh 'cat <<EOF > /fat/heredoc.txt'\n"
        "write -a /fat/compat.sh 'heredoc-one'\n"
        "write -a /fat/compat.sh 'heredoc two words'\n"
        "write -a /fat/compat.sh 'EOF'\n"
        "write -a /fat/compat.sh 'cat /fat/heredoc.txt'\n"
        "sh /fat/compat.sh\n"
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
        "sh -c 'while test $# -gt 0; do echo while-$1; shift; done' wh alpha beta\n"
        "for n in one skip three; do test $n = skip && continue; echo cont-$n; done\n"
        "for n in one stop three; do test $n = stop && break; echo break-$n; done\n"
        "case apple in banana) echo case-bad ;; app*) echo case-glob-ok ;; esac\n"
        "case blue in red|blue) echo case-alt-ok ;; *) echo case-alt-bad ;; esac\n"
        "case none in one) echo case-one-bad ;; *) echo case-default-ok ;; esac\n"
        "if test -f /fat/status.txt; then echo if-chain-ok; fi; echo after-if-chain\n"
        "if test -f /fat/nope; then echo if-chain-bad; else echo if-chain-else; fi && echo if-and-ok\n"
        "if test -f /fat/nope; then echo if-or-bad; else false; fi || echo if-or-ok\n"
        "for n in tail; do echo for-chain-$n; done; echo after-for-chain\n"
        "case tail in t*) echo case-chain-ok ;; esac; echo after-case-chain\n"
        "write /fat/case.sh 'case two in'\n"
        "write -a /fat/case.sh 'one) echo case-script-one ;;'\n"
        "write -a /fat/case.sh 'two|three) echo case-script-ok ;;'\n"
        "write -a /fat/case.sh '*) echo case-script-default ;;'\n"
        "write -a /fat/case.sh 'esac'\n"
        "sh /fat/case.sh\n"
        "sh -c 'echo cargs-$0-$1-$#-$@' cmain one two\n"
        "echo 'echo scriptargs-$0-$1-$#-$@' > /fat/args.sh\n"
        "sh /fat/args.sh alpha beta\n"
        "alias ll='ls /fat/bin'\n"
        "command -v sh ll cd\n"
        "command -V ll cd missingcmd\n"
        "command echo command-run-ok\n"
        "command ll || echo command-bypass-ok\n"
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
        "cat status.txt\n"
        "cat ./status.txt\n"
        "cat www/../status.txt\n"
        "ls ./bin/../etc\n"
        "cd ./www/../etc\n"
        "pwd\n"
        "cd ../..\n"
        "echo norm-root-$PWD\n"
        "pwd\n"
        "cd -\n"
        "pwd\n"
        "cd /fat/status.txt\n"
        "cd /\n"
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
        "test -f /fat/tree-copy/a/b/file.txt && echo recursive-copy-ok\n"
        "rm -r /fat/tree\n"
        "stat /fat/tree/a/b/file.txt\n"
        "rm -r /fat/tree-copy\n"
        "stat /fat/tree-copy/a/b/file.txt\n"
        "cp /fat/redir.txt /fat/redir-copy.txt\n"
        "cat /fat/redir-copy.txt\n"
        "cp /fat/bin/sh /fat/sh-copy\n"
        "stat /fat/sh-copy\n"
        "cmp -s /fat/bin/sh /fat/sh-copy && echo large-copy-ok\n"
        "rm /fat/sh-copy\n"
        "dd if=/dev/zero of=/fat/dd-large.bin bs=4096 count=384 status=none\n"
        "stat /fat/dd-large.bin\n"
        "cp /fat/dd-large.bin /fat/dd-large-copy.bin\n"
        "cmp -s /fat/dd-large.bin /fat/dd-large-copy.bin && echo dd-large-copy-ok\n"
        "rm /fat/dd-large.bin /fat/dd-large-copy.bin\n"
        "ps\n"
        "fpdemo\n"
        "fpdemo &\n"
        "echo last-bg-$!\n"
        "bg $!\n"
        "fg $!\n"
        "fpdemo &\n"
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
        "write /fat/mv-a.txt move-a\n"
        "write /fat/mv-b.txt move-b\n"
        "mv -f /fat/mv-a.txt /fat/mv-b.txt /fat/mvdir\n"
        "cat /fat/mvdir/mv-a.txt\n"
        "cat /fat/mvdir/mv-b.txt\n"
        "stat /fat/mv-a.txt\n"
        "rm -f /fat/no-such-file\n"
        "echo rm-f-ok\n"
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
        "tap 2>&1 | grep usage > /fat/pipeline-stderr-merge.txt\n"
        "stat /fat/pipeline-stderr-merge.txt\n"
        "cat /fat/pipeline-stderr-merge.txt\n"
        "tap 2> /fat/pipeline-left-stderr.txt | wc -c > /fat/pipeline-left-empty.txt\n"
        "stat /fat/pipeline-left-stderr.txt\n"
        "cat /fat/pipeline-left-empty.txt\n"
        "printf 'left-pipe\\n' > /fat/pipeline-left-out.txt | wc -c > /fat/pipeline-left-wc.txt\n"
        "cat /fat/pipeline-left-out.txt\n"
        "cat /fat/pipeline-left-wc.txt\n"
        "true | grep static < /fat/status.txt > /fat/pipeline-last-stdin.txt\n"
        "cat /fat/pipeline-last-stdin.txt\n"
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
            send_serial(sock, "fsck /fat\n", args.send_delay)
            output += read_until(sock, b"srv> ", max(args.line_wait, 10.0))
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
        "srvsh shell",
        "service set <name> <key> <value>",
        "topics:",
        "cli daily driver",
        "profile",
        "webd",
        "more [-n lines] [file ...]",
        "usage: service list|status --all",
        "usage: ls [-1adl] [path ...]",
        "usage: cp [-fRr] <source>... <dest>",
        "usage: webd <root>",
        "usage: xargs [-r] [-n count] [command [arg ...]]",
        "usage: seq [first [increment]] last",
        "usage: realpath [-q] path [...]",
        "usage: id [-u] [-g] [-n] [user]",
        "usage: whoami",
        "usage: readlink [-f|-e] path",
        "usage: cmp [-s] file1 file2",
        "usage: yes [string ...]",
        "usage: install [-d] [-D] [-m mode] source dest | install -d dir [...]",
        "usage: diff [-q|-u] file1 file2",
        "usage: tar -cf archive file... | tar -tf archive | tar -xf archive [-C dir]",
        "usage: gzip [-cdk] [file ...]",
        "usage: gunzip [-ck] [file ...]",
        "usage: patch [-pN] [-i file] [target]",
        "usage: make [-nB] [-f file] [target ...]",
        "usage: udpecho server [port]",
        "shell.txt",
        "service.txt",
        "cli.txt",
        "hello.sh",
        "pipeline.sh",
        "hello from srvros",
        "profile-d-ready-more-textedit-/fat/home",
        "more.txt",
        ".srvros",
        "/fat:",
        "/fat/etc:",
        "ls-d-before",
        "/fat/bin/",
        "ls-d-after",
        "/fat/bin/true",
        "/fat/bin/false",
        "TESTVAR=cli-ok",
        "var-cli-ok",
        "braced-cli-ok",
        "default-fallback",
        "assign-default-set-by-expansion",
        "assigned-set-by-expansion",
        "alt-present",
        "len-16",
        "trim-prefix-by-expansion",
        "trim-suffix-set-by",
        "arith-7",
        "arith-var-15",
        "arith-logic-1",
        "after-false-1",
        "or-ok",
        "pid-",
        "/fat/status.txt",
        "/fat/hello.html",
        "/fat/test-file-copy.txt: 55 bytes",
        "test-s-ok",
        "test-r-ok",
        "test-w-ok",
        "test-not-x-ok",
        "/fat/test-dir-copy.txt: 55 bytes",
        "/fat/test-int-copy.txt: 55 bytes",
        "/fat/test-string-copy.txt: 55 bytes",
        "/fat/test-miss-copy.txt: 55 bytes",
        "test-and-not-ok",
        "test-or-nt-ok",
        "test-ef-ok",
        "PATH=/fat/bin:/",
        "/fat/bin/sleep",
        "/fat/bin/date",
        "/fat/bin/touch",
        "/fat/bin/basename",
        "/fat/bin/dirname",
        "/fat/bin/expr",
        "/fat/bin/printf",
        "/fat/bin/tr",
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
        "env-run-child",
        "env-clean-clean",
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
        "batch one two",
        "batch three",
        "xlong uno dos",
        "xlong tres",
        "xargs-r-ok",
        "1\r\n2\r\n3",
        "2\r\n4\r\n6",
        "/fat/status.txt",
        "id-root-0-root",
        "cmp-same-ok",
        "cmp-diff-ok",
        "port\r\nport",
        "diff-same-ok",
        "--- /fat/status.txt",
        "diff-diff-ok",
        "fat/install/sub/status-copy.txt",
        "/fat/extract/fat/install/sub/status-copy.txt",
        "gzip-roundtrip-ok",
        "gzip-tar-ok",
        "patch: /fat/patch-target.txt",
        "BETA",
        "inserted",
        "make-source",
        "make: '/fat/maketest/build/out.txt' is up to date",
        "install -D /fat/maketest/build/out.txt /fat/local/bin/make-out.txt",
        "orange",
        "grape",
        "1:Alpha",
        "grep-long-ok",
        "beta",
        "/fat/longopts.txt",
        "long-copy",
        "/fat/longdst/",
        "Alpha",
        "fat/longdst/sub/data.txt",
        "expr-add-12",
        "expr-mul-42",
        "expr-eq-1",
        "expr-ne-1",
        "expr-len-6",
        "expr-sub-ana",
        "expr-index-2",
        "expr-lt-1",
        "expr-prefix-3",
        "printf-ok-42-ff",
        "repeat-red",
        "repeat-blue",
        "ABC123",
        "abc",
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
        "/fat/cp-many/status.txt: 55 bytes",
        "script-ok",
        "appended-script-ok",
        "command-mode-ok",
        "comment-ok",
        "group-one",
        "group-two",
        "group-tail",
        "group-or-ok",
        "TEMPVAR=local",
        "after-local-",
        "local-one-two",
        "empty--end",
        "space-two words",
        "escape-two words",
        "script-comment-ok",
        "continuation-ok",
        "heredoc-one",
        "heredoc two words",
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
        "while-alpha",
        "while-beta",
        "cont-one",
        "cont-three",
        "break-one",
        "case-glob-ok",
        "case-alt-ok",
        "case-default-ok",
        "if-chain-ok",
        "after-if-chain",
        "if-chain-else",
        "if-and-ok",
        "if-or-ok",
        "for-chain-tail",
        "after-for-chain",
        "case-chain-ok",
        "after-case-chain",
        "case-script-ok",
        "cargs-cmain-one-2-one two",
        "scriptargs-/fat/args.sh-alpha-2-alpha beta",
        "alias ll='ls /fat/bin'",
        "command-run-ok",
        "command-bypass-ok",
        "ll is aliased to 'ls /fat/bin'",
        "sh is /fat/bin/sh",
        "cd is a shell builtin",
        "missingcmd not found",
        "bare-barevalue",
        "unset-",
        "/fat",
        "norm-root-/",
        "/fat/etc",
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
        "recursive-copy-ok",
        "stat: not found: /fat/tree/a/b/file.txt",
        "stat: not found: /fat/tree-copy/a/b/file.txt",
        "/fat/sh-copy:",
        "large-copy-ok",
        "usage: dd [if=path] [of=path] [bs=bytes] [count=blocks] [skip=blocks] [seek=blocks] [status=none]",
        "/fat/dd-large.bin: 1572864 bytes",
        "dd-large-copy-ok",
        "exfat-check:",
        "errors=0 ok",
        "PID STATE",
        "/fat/bin/sh",
        "fpdemo: ok pid=",
        "last-bg-",
        "[fg] pid",
        "[done] pid",
        "hello-from-sh",
        "move-me",
        "stat: not found: /fat/move-src.txt",
        "moved-into",
        "/fat/mvdir/target/emptydir: 0 bytes directory",
        "move-a",
        "move-b",
        "stat: not found: /fat/mv-a.txt",
        "rm-f-ok",
        "stat: not found: /fat/mvdir/move-into.txt",
        "/fat/tap-copy.txt: 55 bytes",
        "/fat/piped-copy.txt: 55 bytes",
        "/fat/piped3.txt: 55 bytes",
        "1 9 55",
        "/fat/pipeline-redir.txt: 55 bytes",
        "/fat/pipeline-redir.txt: 110 bytes",
        "/fat/pipeline-stderr-merge.txt: 41 bytes",
        "/fat/pipeline-left-stderr.txt: 41 bytes",
        "left-pipe",
        "0",
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
        "posixdemo: pthread compat ok",
        "posixdemo: socket bind ok",
        "posixdemo: ok",
        "exit",
    ]
    missing = [marker for marker in expected if marker not in text]
    forbidden = [
        "cat: open failed: status.txt",
        "cat: open failed: ./status.txt",
        "cat: open failed: www/../status.txt",
        "ls: not found: ./bin/../etc",
    ]
    present_forbidden = [marker for marker in forbidden if marker in text]
    if has_fatal_exception(text):
        print("cli-smoke: fatal exception detected", file=sys.stderr)
        return 2
    if present_forbidden:
        print("cli-smoke: forbidden markers:", file=sys.stderr)
        for marker in present_forbidden:
            print(f"  {marker}", file=sys.stderr)
        return 4
    if missing:
        print("cli-smoke: missing markers:", file=sys.stderr)
        for marker in missing:
            print(f"  {marker}", file=sys.stderr)
        return 3
    print("cli-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
