import io
from pathlib import Path
import struct
import subprocess
import tarfile
import tempfile
import unittest

import startup_journal as j
from mk_usb_image import Fat32Builder


class StartupJournalTest(unittest.TestCase):
    def test_profile(self):
        import hardware_boot as boot
        version = 'ram-usb-wifi-fw-v141-laptop-probe-0e535900-auto-return'
        config = boot.limine_config('0123456789abcdef', version)
        base = boot.limine_config('0123456789abcdef', version.replace('v141-', 'v140-'))
        self.assertEqual(config, base.replace('srvros.storage=ram',
            'srvros.storage=ram srvros.startup-diagnostic=1'))
        self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(version))
        with self.assertRaises(ValueError):
            boot.stage(startup_journal=Path('absent'))
        with self.assertRaises(ValueError):
            boot.limine_config('0123456789abcdef', version.replace('-auto-return', ''))

    def test_initramfs_binding(self):
        descriptor = (j.magic('SRVRSTARTDESC1')+struct.pack('<QQQQ', 100000, 2097152, 1048576, 10485760)
                      + bytes(range(16)) + bytes(64))
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)/'module.bin'
            path.write_bytes(descriptor)
            archive = Path(temp)/'initramfs.tar'
            with tarfile.open(archive, 'w'):
                pass
            j.add_to_initramfs(path, archive)
            self.assertEqual(j.verify_initramfs(archive), descriptor)
            with self.assertRaises(ValueError):
                j.add_to_initramfs(path, archive)

    def test_fat_binding(self):
        nonce = bytes(range(16))
        fat = Fat32Builder(64 * 1024 * 1024, 2048)
        fat.add_file(fat.root_cluster, 'START001.BIN', j.initial(nonce))
        image = bytearray(1048576) + fat.build()
        image[512:520] = b'EFI PART'
        with io.BytesIO(image) as stream:
            descriptor = j.bind(stream, len(image) // 512, 1048576, 'START001.BIN', nonce)
            self.assertEqual(j.validate_descriptor(descriptor), descriptor)
            offset = struct.unpack_from('<Q', descriptor, 24)[0]
            self.assertEqual(image[offset:offset+j.FILE_BYTES], j.initial(nonce))
            image[1048576+65] = 1
        # Preparing while mounted must predict the same exact clean sector
        # admitted after shutdown. Other state bits remain identity-bound.
        with io.BytesIO(image) as stream:
            mounted = j.bind(stream, len(image)//512, 1048576, 'START001.BIN', nonce)
        self.assertEqual(mounted, descriptor)
        image[1048576+65] = 3
        with io.BytesIO(image) as stream:
            other_state = j.bind(stream, len(image)//512, 1048576, 'START001.BIN', nonce)
        self.assertNotEqual(other_state[96:], descriptor[96:])
        image[1048576+65] = 1
        image[1048576+67] ^= 1
        with io.BytesIO(image) as stream:
            changed_identity = j.bind(stream, len(image)//512, 1048576, 'START001.BIN', nonce)
        self.assertNotEqual(changed_identity[96:], descriptor[96:])
        image[1048576+67] ^= 1
        image[offset] ^= 1
        with io.BytesIO(image) as stream:
            with self.assertRaises(ValueError):
                j.bind(stream, len(image)//512, 1048576, 'START001.BIN', nonce)

    def test_clean_sector_refuses_other_formats(self):
        with self.assertRaises(ValueError):
            j.clean_boot_sector(bytes(511))
        sector = bytearray(j.SECTOR)
        struct.pack_into('<H', sector, 11, j.SECTOR)
        struct.pack_into('<H', sector, 22, 1)
        sector[510:] = b'\x55\xaa'
        with self.assertRaises(ValueError):
            j.clean_boot_sector(sector)

    def test_actual_writer_and_corruption(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'journal.bin'
            subprocess.run(['build/tests/startup-journal-host', str(path)], check=True)
            data, descriptor = path.read_bytes(), path.with_suffix('.bin.desc').read_bytes()
            self.assertTrue(j.decode(data, descriptor)['complete'])
            for at in (512, 512+64, 1024, 512+6*j.SLOT_BYTES+40):
                changed = bytearray(data)
                changed[at] ^= 1
                result = j.decode(changed, descriptor)
                self.assertFalse(result['complete'])
                self.assertTrue(result['errors'])
            result = j.decode(j.initial(descriptor[48:64]), descriptor)
            self.assertFalse(result['complete'])
            self.assertEqual(result['records'], [])
            # A cut after any sector of the next checkpoint preserves all prior
            # complete records and refuses the partially written one.
            template = j.initial(descriptor[48:64])
            for phase in range(7):
                offset = 512 + phase*j.SLOT_BYTES
                for sectors in (1, 8, 15):
                    changed = data[:offset] + template[offset:]
                    changed = bytearray(changed)
                    changed[offset+512:offset+(sectors+1)*512] = data[offset+512:offset+(sectors+1)*512]
                    result = j.decode(changed, descriptor)
                    self.assertFalse(result['complete'])
                    self.assertEqual(len(result['records']), phase)

    def test_descriptor_bounds(self):
        descriptor = (j.magic('SRVRSTARTDESC1')+struct.pack('<QQQQ', 100000, 2097152, 1048576, 10485760)
                      + bytes(range(16)) + bytes(64))
        j.validate_descriptor(descriptor)
        for offset in (16, 24, 32, 40):
            changed = bytearray(descriptor)
            struct.pack_into('<Q', changed, offset, 2**64-1)
            with self.assertRaises(ValueError):
                j.validate_descriptor(changed)


if __name__ == '__main__':
    unittest.main()
