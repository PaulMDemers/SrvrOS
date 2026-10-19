"""Private serial endpoints for concurrently isolated QEMU fixtures."""
import errno
import math
import socket
import time


def connect_unix(path, timeout):
    """Connect within a finite deadline; never select/release a shared TCP port."""
    if not math.isfinite(timeout) or timeout <= 0:
        raise ValueError("A positive finite connection timeout is required")
    deadline = time.monotonic() + timeout
    while True:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            sock.settimeout(max(.001, min(.25, deadline-time.monotonic())))
            sock.connect(str(path))
            return sock
        except OSError as error:
            sock.close()
            if not isinstance(error, TimeoutError) and error.errno not in (errno.ENOENT, errno.ECONNREFUSED, errno.EAGAIN):
                raise
            remaining = deadline-time.monotonic()
            if remaining <= 0:
                raise TimeoutError(f"QEMU serial socket did not become ready: {path}") from error
            time.sleep(min(.02, remaining))
        except BaseException:
            sock.close()
            raise
