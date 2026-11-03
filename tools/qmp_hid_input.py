"""Bound USB key injection by QEMU HID consumption, not host wall-clock speed."""
import time
from pathlib import Path
from desktop_foundation_smoke import QMP


class HIDQueueReceipt:
    def __init__(self, path, timeout=10, clock=time.monotonic, sleep=time.sleep):
        self.path = Path(path)
        self.timeout, self.clock, self.sleep = timeout, clock, sleep
        self.offset = 0
        self.partial = b''
        self.edges = 0
        self.overflow = False
        self.max_wait_seconds = 0

    def read(self):
        if self.overflow:
            raise RuntimeError('QEMU USB HID injection queue previously overflowed')
        with self.path.open('rb') as stream:
            stream.seek(self.offset)
            data = stream.read()
        self.offset += len(data)
        data = self.partial + data
        end = data.rfind(b'\n')+1
        self.partial, data = data[end:], data[:end]
        if b'hid_kbd_queue_full' in data:
            self.overflow = True
            raise RuntimeError('QEMU USB HID injection queue overflowed')
        return data

    def deliver(self, send):
        # Old empty notifications cannot acknowledge a new edge. A later empty
        # means QEMU consumed all queued scancodes, including this edge. This is
        # emulator delivery evidence, not an application or latency receipt.
        self.read()
        if self.partial:
            raise RuntimeError('Incomplete QEMU HID trace before injected edge')
        began = self.clock()
        send()
        pending = b''
        while True:
            pending += self.read()
            if self.clock()-began >= self.timeout:
                raise TimeoutError('QEMU USB HID edge was not consumed before deadline')
            if b'hid_kbd_queue_empty queue empty\n' in pending:
                self.edges += 1
                self.max_wait_seconds = max(self.max_wait_seconds, self.clock()-began)
                return
            self.sleep(.005)

    def result(self):
        self.read()
        if self.partial:
            raise RuntimeError('Incomplete final QEMU HID trace')
        return dict(mode='QEMU HID queue consumption per edge', edges=self.edges,
                    max_wait_seconds=round(self.max_wait_seconds, 6), overflow=False,
                    physical_latency_acceptance=False)


class HIDKeyboardQMP(QMP):
    def __init__(self, socket_path, trace_path):
        super().__init__(socket_path)
        self.trace_path = trace_path
        self.hid_receipt = None

    def enable_hid_receipts(self):
        # Firmware menu input precedes the native xHCI worker. Only arm after
        # the guest enters the desktop path; earlier boot selection stays raw.
        self.hid_receipt = HIDQueueReceipt(self.trace_path)

    def _edge(self, key, down):
        event = {'type': 'key', 'data': {'down': down,
                 'key': {'type': 'qcode', 'data': key}}}
        self.hid_receipt.deliver(lambda: self.call('input-send-event', {'events': [event]}))
        time.sleep(.04)

    def key(self, key, shift=False):
        if self.hid_receipt is None:
            return super().key(key, shift)
        if shift:self._edge('shift', True)
        self._edge(key, True)
        self._edge(key, False)
        if shift:self._edge('shift', False)

    def shortcut(self, keys):
        assert self.hid_receipt is not None
        sequence = keys.split('-')
        for key in sequence:self._edge(key, True)
        for key in reversed(sequence):self._edge(key, False)
