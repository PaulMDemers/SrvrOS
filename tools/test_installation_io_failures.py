"""Entry points for the isolated, opt-in host I/O fault experiment."""
import os
from pathlib import Path
import tempfile
import unittest

import installation_io_faults as faults


class InstallationIOFailureTests(unittest.TestCase):
    def test_ready_barrier_and_negative_control(self):
        with tempfile.TemporaryDirectory() as temp:
            result = faults.run_process(['control', str(Path(temp) / 'case'), 'barriers'])
            self.assertEqual(result['omitted_barrier_negative_control'], 'detected')

    @unittest.skipUnless(os.environ.get('SRVROS_EXHAUSTIVE_IO_TEST') == '1',
                         'set SRVROS_EXHAUSTIVE_IO_TEST=1 for the bounded subprocess matrix')
    def test_bounded_fault_matrix(self):
        with tempfile.TemporaryDirectory() as temp:
            receipt = faults.experiment(str(Path(temp) / 'receipt.json'))
            self.assertEqual(receipt['status'], 'COMPLETE')
            self.assertTrue(receipt['cases'])


if __name__ == '__main__': unittest.main()
