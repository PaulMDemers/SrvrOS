#!/usr/bin/env python3
"""Exercise USB injection acknowledgements without a QEMU guest."""
import tempfile
from pathlib import Path
from qmp_hid_input import HIDQueueReceipt, HIDKeyboardQMP


def main():
    with tempfile.TemporaryDirectory() as temp:
        path = Path(temp)/'trace.log'
        def append(data):
            with path.open('ab') as out:out.write(data)
        path.write_bytes(b'hid_kbd_queue_empty queue empty\n')
        now = [0.]
        def sleep(delay):now[0] += delay
        receipt = HIDQueueReceipt(path, timeout=.02, clock=lambda:now[0], sleep=sleep)
        try:receipt.deliver(lambda:None)
        except TimeoutError:pass
        else:raise AssertionError('stale acknowledgement accepted')
        assert receipt.edges == 0
        def late():
            now[0] += .03
            append(b'hid_kbd_queue_empty queue empty\n')
        try:receipt.deliver(late)
        except TimeoutError:pass
        else:raise AssertionError('late acknowledgement accepted')
        append(b'hid_kbd_queue_')
        try:receipt.deliver(lambda:None)
        except RuntimeError:pass
        else:raise AssertionError('partial stale trace accepted')
        append(b'empty queue empty\n')
        receipt.deliver(lambda:append(b'hid_kbd_queue_empty queue empty\n'))
        assert receipt.edges == 1
        # Partial reads must combine into one complete new notification.
        def finish(delay):
            sleep(delay);append(b' queue empty\n')
        receipt.sleep = finish
        receipt.deliver(lambda:append(b'hid_kbd_queue_empty'))
        assert receipt.edges == 2
        try:receipt.deliver(lambda:append(b'hid_kbd_queue_full queue full\nhid_kbd_queue_empty queue empty\n'))
        except RuntimeError:pass
        else:raise AssertionError('overflow accepted as delivery')
        # Overflow stays fatal even after its original trace bytes were read.
        try:receipt.result()
        except RuntimeError:pass
        else:raise AssertionError('late overflow hidden')
        assert receipt.edges == 2
        path.write_bytes(b'')
        keyboard = object.__new__(HIDKeyboardQMP)
        keyboard.hid_receipt = HIDQueueReceipt(path)
        sent = []
        def call(command, args):
            assert command == 'input-send-event' and len(args['events']) == 1
            event = args['events'][0]['data']
            sent.append((event['key']['data'], event['down']))
            append(b'hid_kbd_queue_empty queue empty\n')
        keyboard.call = call
        keyboard.key('dot', True)
        keyboard.shortcut('ctrl-s')
        assert sent == [('shift', True), ('dot', True), ('dot', False), ('shift', False),
                        ('ctrl', True), ('s', True), ('s', False), ('ctrl', False)]
        assert keyboard.hid_receipt.result()['edges'] == 8
    print('qmp-hid-input: PASS stale/partial/new acknowledgements, deadline and overflow refusal')


if __name__ == '__main__':main()
