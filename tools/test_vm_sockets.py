import socket
import tempfile
import threading
import time
import unittest
from pathlib import Path
from vm_sockets import connect_unix


class SocketTests(unittest.TestCase):
    def test_delayed_private_server_and_endpoint_isolation(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'serial';other=Path(tmp)/'other'
            errors=[]
            # Bound but not listening: connects must retry ECONNREFUSED.
            with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as server, \
                 socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as unrelated:
                server.bind(str(path));unrelated.bind(str(other));unrelated.listen(1)
                unrelated.settimeout(.05)
                def serve():
                    try:
                        time.sleep(.05);server.listen(1);server.settimeout(2)
                        peer,_=server.accept()
                        with peer:peer.sendall(b'private serial\n')
                    except BaseException as error:errors.append(error)
                thread=threading.Thread(target=serve);thread.start()
                try:
                    with connect_unix(path,2) as client:
                        client.settimeout(2);self.assertEqual(client.recv(100),b'private serial\n')
                    with self.assertRaises(TimeoutError):unrelated.accept()
                finally:thread.join(3)
                self.assertFalse(thread.is_alive());self.assertEqual(errors,[])

    def test_missing_endpoint_is_bounded(self):
        with tempfile.TemporaryDirectory() as tmp:
            start=time.monotonic()
            with self.assertRaises(TimeoutError):connect_unix(Path(tmp)/'absent',.06)
            self.assertLess(time.monotonic()-start,1)
        for timeout in (0,-1,float('inf'),float('nan')):
            with self.assertRaises(ValueError):connect_unix('/not-used',timeout)


if __name__=='__main__':unittest.main()
