#!/usr/bin/env python3
"""Keep subprocess lifecycle receipts from truncating the enclosing VM test."""
import unittest
from unittest.mock import patch
import thread_smoke


class ResultReader(unittest.TestCase):
    def read(self, chunks):
        with patch.object(thread_smoke, 'read_for', side_effect=chunks) as reader:
            result = thread_smoke.read_test_result(object(), 5)
            self.assertEqual(reader.call_count, len(chunks))
            self.assertEqual(result, b''.join(chunks))

    def test_child_exits_are_not_suite_completion(self):
        self.read([b'process: exited status=0\r\n',
                   b'process: exited status=23\r\n',
                   b'threadstress: main-exit ok\r\n',
                   b'threadstress: main-join ok\r\n',
                   b'threadstress: ok\r\n'])

    def test_top_level_exit_ends_failed_command(self):
        self.read([b'process: exited status=1\r\n', b'run: pid=42 exited status=1\r\n'])

    def test_fragmented_monitor_receipt(self):
        self.read([b'process: exited status=91\r\nrun: pi', b'd=4 exited status=1\r\n'])

    def test_explicit_test_failure_ends_command(self):
        self.read([b'threadstress: futex failed\r\n'])

    def test_eof_without_completion_is_bounded(self):
        with patch.object(thread_smoke, 'read_for', return_value=b'') as reader:
            with patch.object(thread_smoke.time, 'time', side_effect=[0, 0, 6]):
                self.assertEqual(thread_smoke.read_test_result(object(), 5), b'')
            self.assertEqual(reader.call_count, 1)


if __name__ == '__main__':
    unittest.main()
