import unittest
from desktop_multiwindow_smoke import validate_saved_console

COMPLETIONS=(b'multi-window: surface-pool 128 live, readback/reuse/stale IDs verified',
             b'multi-window: surface-pool retired',
             b'multi-window: PASS all windows retired; process exiting')
LIFETIMES=(b'multi-window: retired window=1 text=AlphaX',
           b'multi-window: retired window=3 text=Gamma',
           b'clients=0 children=1 clipboard_peers=0 window_capacity=16 cache_bytes=940800')
class SavedRecords(unittest.TestCase):
    def fixture(self):return b'\r\n'.join(COMPLETIONS*2+LIFETIMES+(b'deskd: cache peak_bytes=940800 retired_bytes=0 budget_bytes=100663296 windows=16',)*2)+b'\r\n'
    def test_complete(self):validate_saved_console(self.fixture())
    def test_missing_or_duplicate(self):
        for record in COMPLETIONS:
            with self.subTest(record=record):
                with self.assertRaises(ValueError):validate_saved_console(self.fixture().replace(record,b'',1))
                with self.assertRaises(ValueError):validate_saved_console(self.fixture()+record)
    def test_lifetime_required(self):
        for record in LIFETIMES:
            with self.assertRaises(ValueError):validate_saved_console(self.fixture().replace(record,b''))
    def test_interleaved_live_is_not_completion(self):
        with self.assertRaises(ValueError):validate_saved_console(self.fixture().replace(b'multi-window: PASS',b'multdeskd: resources\r\ni-window: PASS',1))
    def test_completed_cache_release_required(self):
        data=self.fixture()
        for bad in (data.replace(b'retired_bytes=0',b'retired_bytes=4',1),
                    data.replace(b'deskd: cache',b'missing cache',1),
                    data+b'deskd: cache peak_bytes=940800 retired_bytes=0 budget_bytes=100663296 windows=16'):
            with self.assertRaises(ValueError):validate_saved_console(bad)
    def test_guest_failure(self):
        for failure in (b'FAIL',b'exception: fault'):
            with self.assertRaises(ValueError):validate_saved_console(self.fixture()+failure)
if __name__=='__main__':unittest.main()
