import copy
import unittest
from check_exfat_allocation import select_partition


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


if __name__ == '__main__':
    unittest.main()
