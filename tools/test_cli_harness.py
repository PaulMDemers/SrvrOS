#!/usr/bin/env python3
"""Regression coverage for CLI serial synchronization, without QEMU."""
import unittest
from unittest.mock import patch

import cli_smoke


class SerialPromptTests(unittest.TestCase):
    def test_help_example_is_not_a_ready_prompt(self):
        chunks = [b'Example: / $ echo hello\r\nMore help follows\r\n',
                  b'help complete\r\n\r/ $ ']
        with patch.object(cli_smoke, 'read_for', side_effect=chunks) as receive:
            result = cli_smoke.read_until(None, b' $ ', 1)
        self.assertEqual(result, b''.join(chunks))
        self.assertEqual(receive.call_count, 2)

    def test_monitor_example_is_not_a_ready_prompt(self):
        chunks = [b'Try srv> help\r\n', b'done\r\nsrv> ']
        with patch.object(cli_smoke, 'read_for', side_effect=chunks):
            self.assertEqual(cli_smoke.read_until(None, b'srv> ', 1), b''.join(chunks))

    def test_non_prompt_marker_can_be_followed_by_output(self):
        chunk = b'sysabi: ok\r\nprocess exited\r\n'
        with patch.object(cli_smoke, 'read_for', return_value=chunk):
            self.assertEqual(cli_smoke.read_until(None, b'sysabi: ok', 1), chunk)


if __name__ == '__main__':
    unittest.main()
