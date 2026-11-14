import copy
import contextlib
import io
import unittest
from unittest.mock import patch
from check_exfat_allocation import inspection_mountpoint, main, select_partition


class SelectionTests(unittest.TestCase):
    def setUp(self):
        self.part = dict(path='/dev/fixture1', type='part', fstype='exfat',
                         uuid='1234-5678', mountpoints=[None])
        self.inventory = {'blockdevices': [dict(tran='usb', children=[self.part])]}

    def test_unmounted_usb_partition(self):
        self.assertEqual(select_partition(self.inventory, '12345678'), '/dev/fixture1')

    def test_mounted_wrong_type_filesystem_transport_and_ambiguous(self):
        for key, value in [('mountpoints', ['/media/user/volume']),
                           ('type', 'disk'), ('fstype', 'vfat'), ('uuid', '0000-0000')]:
            inventory = copy.deepcopy(self.inventory)
            inventory['blockdevices'][0]['children'][0][key] = value
            with self.assertRaises(ValueError):
                select_partition(inventory, '12345678')
        self.inventory['blockdevices'][0]['tran'] = 'sata'
        with self.assertRaises(ValueError):
            select_partition(self.inventory, '12345678')
        self.inventory['blockdevices'][0]['tran'] = 'usb'
        self.inventory['blockdevices'].append(copy.deepcopy(self.inventory['blockdevices'][0]))
        with self.assertRaises(ValueError):
            select_partition(self.inventory, '12345678')


class MountpointTests(unittest.TestCase):
    def test_native_and_fixture_namespaces_are_explicit(self):
        for value in ('/work', '/vol', '/guest/work space'):
            self.assertEqual(inspection_mountpoint(value), value)

    def test_invalid_namespace_refuses_before_device_discovery(self):
        for value in ('', '/', 'work', '/work/', '//work', '/a/../work', '/a/./work',
                      '/a//work', '/wo\x00rk', '/wo\nrk', '/wörk', '/' + 'a' * 1023):
            with self.subTest(value=value), patch('sys.argv', [
                    'check_exfat_allocation.py', '--uuid', '12345678', '--mountpoint', value]), \
                    patch('check_exfat_allocation.subprocess.check_output') as discover, \
                    patch('check_exfat_allocation.subprocess.run') as driver, \
                    contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(main(), 2)
                discover.assert_not_called()
                driver.assert_not_called()

    def test_derived_inspection_path_overflow_refuses_before_discovery(self):
        for mode in ('--require-headroom', '--battery-image'):
            with self.subTest(mode=mode), patch('sys.argv', [
                    'check_exfat_allocation.py', '--uuid', '12345678',
                    '--mountpoint', '/' + 'a' * 1018, mode]), \
                    patch('check_exfat_allocation.subprocess.check_output') as discover, \
                    patch('check_exfat_allocation.subprocess.run') as driver, \
                    contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(main(), 2)
                discover.assert_not_called()
                driver.assert_not_called()


if __name__ == '__main__':
    unittest.main()
