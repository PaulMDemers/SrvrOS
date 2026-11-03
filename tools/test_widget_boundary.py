import subprocess
import unittest
from unittest.mock import patch
from check_widget_boundary import check

class BoundaryTests(unittest.TestCase):
    def audit(self, symbols):
        with patch('check_widget_boundary.subprocess.run', return_value=subprocess.CompletedProcess([], 0, symbols)):
            check('fixture.a', 'nm')

    def test_elf_helpers(self):
        self.audit('desk_button_draw T 0 1\ndesk_fill U\nmemcpy U\n')

    def test_macho_helpers(self):
        self.audit('_desk_button_draw T 0 1\n_desk_fill U\n_memcpy U\n___bzero U\n____chkstk_darwin U\n')

    def test_macho_os_call_rejected(self):
        with self.assertRaisesRegex(ValueError, 'srv_write'):
            self.audit('_desk_button_draw T 0 1\n_srv_write U\n')

    def test_elf_os_call_rejected(self):
        with self.assertRaisesRegex(ValueError, 'malloc'):
            self.audit('desk_button_draw T 0 1\nmalloc U\n')

if __name__ == '__main__':
    unittest.main()
