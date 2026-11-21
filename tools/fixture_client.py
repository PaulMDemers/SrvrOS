"""Serialized framed host-fixture cases, isolated with fork in the fixture.

Set SRVROS_FIXTURE_BATCH=0 for ordinary execution (also the sanitizer default).
No success is retried after a transport failure; callers receive the failure.
"""
import atexit
import os
from pathlib import Path
import resource
import select
import subprocess
import threading
import time

from workflow.store import sha

_servers = {}
_guard = threading.Lock()


def _limit():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


class Server:
    def __init__(self, binary):
        self.process = subprocess.Popen([str(binary), '--fixture-server'], stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, bufsize=0)
        self.buffer = bytearray()
        self.deadline = time.monotonic() + 65
        try:
            if self.line() != b'SRVROS-FIXTURE-1\n':
                raise ValueError('Fixture does not implement the framed server protocol')
        except BaseException:
            self.close()
            raise

    def take(self, count=None):
        while ((count is None and b'\n' not in self.buffer) or
               (count is not None and len(self.buffer) < count)):
            remaining = self.deadline - time.monotonic()
            if remaining <= 0 or not select.select([self.process.stdout], [], [], max(0, remaining))[0]:
                raise TimeoutError('Fixture server timed out')
            data = os.read(self.process.stdout.fileno(), 65536)
            if not data:
                raise RuntimeError('Fixture server closed before completing response')
            self.buffer.extend(data)
            if len(self.buffer) > 17*1024*1024:
                raise ValueError('Oversized fixture response')
        if count is None:
            count = self.buffer.index(b'\n') + 1
        result = bytes(self.buffer[:count])
        del self.buffer[:count]
        return result

    def line(self):
        result = self.take()
        if len(result) > 256:
            raise ValueError('Invalid fixture response header')
        return result

    def run(self, mode):
        if not mode or len(mode) > 1000 or any(not (c.isascii() and (c.isalnum() or c in '_:-')) for c in mode):
            raise ValueError('Invalid fixture mode')
        self.deadline = time.monotonic() + 65
        self.process.stdin.write((mode + '\n').encode())
        self.process.stdin.flush()
        values = self.line().split()
        if len(values) != 3:
            raise ValueError('Invalid fixture response')
        rc, out, err = map(int, values)
        if not -127 <= rc <= 255 or min(out, err) < 0 or max(out, err) > 8*1024*1024:
            raise ValueError('Invalid fixture result bounds')
        return subprocess.CompletedProcess([str(self.process.args[0]), '--wake-engine-' + mode], rc,
                                           self.take(out).decode(), self.take(err).decode())

    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        for stream in (self.process.stdin, self.process.stdout):
            stream.close()


def close_all():
    for server in list(_servers.values()):
        server.close()
    _servers.clear()


atexit.register(close_all)


def run(binary, mode):
    binary = Path(binary).resolve()
    # Sanitizers use the canonical subprocess path unless batching is explicitly requested.
    batch = os.environ.get('SRVROS_FIXTURE_BATCH', '0' if any(k in os.environ for k in
                           ('ASAN_OPTIONS', 'UBSAN_OPTIONS', 'LSAN_OPTIONS')) else '1') == '1'
    if not batch:
        return subprocess.run([str(binary), '--wake-engine-' + mode], text=True,
                              capture_output=True, preexec_fn=_limit, timeout=65)
    with _guard:
        identity = (str(binary), sha(binary), tuple(sorted(os.environ.items())))
        if identity not in _servers:
            for key in list(_servers):
                if key[0] == str(binary):
                    _servers.pop(key).close()
            _servers[identity] = Server(binary)
        server = _servers[identity]
        try:
            return server.run(mode)
        except BaseException:
            _servers.pop(identity).close()
            raise
