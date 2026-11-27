"""An empty high-speed USB 2.0 hub for QEMU's usb-redir test transport."""

import socket
import struct
import threading


# usbredir protocol v0.7, using 32-bit message IDs and endpoint packet sizes.
HELLO, CONNECT, RESET, INTERFACES, ENDPOINTS = 0, 1, 3, 4, 5
SET_CONFIGURATION, GET_CONFIGURATION, CONFIGURATION_STATUS = 6, 7, 8
START_INTERRUPT, STOP_INTERRUPT, INTERRUPT_STATUS, CANCEL, CONTROL = 15, 16, 17, 21, 100
SUCCESS, STALL = 0, 4
VENDOR, PRODUCT = 0x0424, 0x2513
PORTS = 3
SETUP = struct.Struct("<BBBBHHH")
HEADER = struct.Struct("<III")
DEVICE = struct.pack("<BBHBBBBHHHBBBB", 18, 1, 0x0200, 9, 0, 1, 64,
                     VENDOR, PRODUCT, 0x0100, 0, 0, 0, 1)
CONFIGURATION = bytes((9, 2, 25, 0, 1, 1, 0, 0xe0, 0,
                       9, 4, 0, 0, 1, 9, 0, 0, 0,
                       7, 5, 0x81, 3, 1, 0, 12))
HUB = bytes((9, 0x29, PORTS, 1, 0, 5, 0, 0, 0xff))


class HubFixture:
    def __init__(self, path, *, stall_status_port=None):
        self.listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.listener.bind(str(path))
        self.listener.listen(1)
        self.listener.settimeout(10)
        self.thread = threading.Thread(target=self.serve, daemon=True)
        self.error = None
        self.configured = 0
        self.powered = set()
        self.status_ports = set()
        self.hub_descriptors = 0
        self.stall_status_port = stall_status_port
        self.stalled_status_requests = 0

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.listener.close()
        self.thread.join(timeout=2)
        if exc_type is None and (self.error is not None or self.thread.is_alive()):
            raise RuntimeError("USB hub fixture did not finish cleanly") from self.error

    @staticmethod
    def send(connection, kind, data=b"", message_id=0):
        connection.sendall(HEADER.pack(kind, len(data), message_id) + data)

    def connect(self, connection):
        interface_info = bytearray(struct.pack("<I", 1) + bytes(4 * 32))
        interface_info[4 + 32] = 9
        types = bytearray([255] * 32)
        types[0] = types[16] = 0
        types[17] = 3
        intervals = bytearray(32)
        intervals[17] = 12
        packets = [0] * 32
        packets[0] = packets[16] = 64
        packets[17] = 1
        self.send(connection, ENDPOINTS, types + intervals + bytes(32) + struct.pack("<32H", *packets))
        self.send(connection, INTERFACES, interface_info)
        self.send(connection, CONNECT, struct.pack("<BBBBHHH", 2, 9, 0, 1, VENDOR, PRODUCT, 0x0100))

    def control(self, raw):
        endpoint, request, request_type, _, value, index, length = SETUP.unpack_from(raw)
        key = (request_type, request, value)
        response = b""
        status = SUCCESS
        if key == (0x80, 6, 0x0100):
            response = DEVICE
        elif key == (0x80, 6, 0x0200):
            response = CONFIGURATION
        elif key == (0xa0, 6, 0x2900) and self.configured:
            self.hub_descriptors += 1
            response = HUB
        elif key == (0xa0, 0, 0) and self.configured:
            response = bytes(4)
        elif key == (0x23, 3, 8) and self.configured and 1 <= index <= PORTS:
            self.powered.add(index)
        elif key == (0xa3, 0, 0) and self.configured and 1 <= index <= PORTS:
            self.status_ports.add(index)
            if index == self.stall_status_port:
                self.stalled_status_requests += 1
                status = STALL
            else:
                response = struct.pack("<HH", 0x100 if index in self.powered else 0, 0)
        else:
            status = STALL
        response = response[:length]
        return SETUP.pack(endpoint, request, request_type, status, value, index, len(response)) + response

    def serve(self):
        try:
            connection, _ = self.listener.accept()
            with connection, connection.makefile("rb") as stream:
                connection.settimeout(30)
                self.send(connection, HELLO, struct.pack("<64sI", b"SrvrOS USB hub fixture", (1 << 1) | (1 << 4)))
                while header := stream.read(HEADER.size):
                    kind, length, message_id = HEADER.unpack(header)
                    if length > 4096:
                        raise ValueError("Oversized USB redirection fixture message")
                    data = stream.read(length)
                    if len(data) != length:
                        raise ValueError("Incomplete USB redirection fixture message")
                    if kind == HELLO:
                        self.connect(connection)
                    elif kind == RESET:
                        self.configured = 0
                        self.powered.clear()
                        self.status_ports.clear()
                        self.hub_descriptors = 0
                        self.stalled_status_requests = 0
                    elif kind == SET_CONFIGURATION:
                        self.configured = data[0]
                        self.send(connection, CONFIGURATION_STATUS, bytes((SUCCESS, self.configured)), message_id)
                    elif kind == GET_CONFIGURATION:
                        self.send(connection, CONFIGURATION_STATUS, bytes((SUCCESS, self.configured)), message_id)
                    elif kind == CONTROL:
                        self.send(connection, CONTROL, self.control(data), message_id)
                    elif kind in (START_INTERRUPT, STOP_INTERRUPT):
                        self.send(connection, INTERRUPT_STATUS, bytes((SUCCESS, data[0])), message_id)
                    elif kind != CANCEL:
                        raise ValueError(f"Unexpected USB redirection fixture message {kind}")
        except (OSError, ValueError, struct.error) as error:
            self.error = error
