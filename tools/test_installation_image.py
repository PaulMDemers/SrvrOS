import json
import os
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock

import installation_image as image


def exfat_fixture(path):
    sectors = 128
    data = bytearray(sectors * 512)
    data[3:11] = b'EXFAT   '
    struct.pack_into('<Q', data, 72, sectors)
    struct.pack_into('<IIIII', data, 80, 24, 1, 25, 100, 2)
    data[108:111] = bytes((9, 0, 1))
    data[510:512] = b'\x55\xaa'
    checksum = 0
    for i, b in enumerate(data[:11 * 512]):
        if i not in (106, 107, 112):
            checksum = (((checksum >> 1) | ((checksum & 1) << 31)) + b) & 0xffffffff
    data[11*512:12*512] = struct.pack('<I', checksum) * 128
    data[12*512:24*512] = data[:12*512]
    path.write_bytes(data)


class InstallationImageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.kernel = self.root / 'kernel'
        self.init = self.root / 'init'
        self.boot = self.root / 'boot'
        self.exfat = self.root / 'source.exfat'
        self.kernel.write_bytes(b'\x7fELFkernel')
        self.init.write_bytes(b'\x7fELFinit')
        self.boot.write_bytes(b'MZefi')
        exfat_fixture(self.exfat)
        self.target = self.root / 'stage'

    def prepare(self):
        return image.prepare(self.target, self.kernel, self.boot, self.exfat, self.init)

    def test_refuses_existing_target_and_source_alias(self):
        for target in (self.exfat, self.root):
            with self.assertRaises(FileExistsError):
                image.prepare(target, self.kernel, self.boot, self.exfat, self.init)
        self.assertEqual(self.exfat.read_bytes()[3:11], b'EXFAT   ')

    def test_refuses_symlink_and_malformed_source(self):
        link = self.root / 'link'
        link.symlink_to(self.exfat)
        with self.assertRaises(ValueError):
            image.prepare(self.target, self.kernel, self.boot, link, self.init)
        self.exfat.write_bytes(b'bad')
        with self.assertRaises(ValueError):
            self.prepare()
        self.assertFalse(self.target.exists())

    def test_layout_bounds(self):
        with self.assertRaises(ValueError):
            image.layout(0)
        with self.assertRaises(ValueError):
            image.layout(513)
        with self.assertRaises(ValueError):
            image.layout(0x100000000 * 512)
        layout = image.layout(128 * 512)
        self.assertGreaterEqual(layout['exfat']['offset_bytes'],
                                layout['esp']['offset_bytes'] + layout['esp']['size_bytes'])
        self.assertLess(layout['exfat']['offset_bytes'] + layout['exfat']['size_bytes'], layout['size_bytes'])

    def test_changed_source_and_unknown_target_refused(self):
        self.prepare()
        self.kernel.write_bytes(b'\x7fELFchanged')
        with self.assertRaises(ValueError):
            image.apply(self.target)
        self.assertFalse((self.target / 'ready.json').exists())
        (self.target / 'unrecognized').write_text('do not touch')
        with self.assertRaises(ValueError):
            image.apply(self.target)
        self.assertEqual((self.target / 'unrecognized').read_text(), 'do not touch')

    def test_interrupted_build_cannot_be_reused(self):
        self.prepare()
        with mock.patch.object(image.usb, 'build_image', side_effect=RuntimeError('interrupted')):
            with self.assertRaises(RuntimeError):
                image.apply(self.target)
        self.assertFalse((self.target / 'ready.json').exists())
        self.assertFalse((self.target / 'image.img').exists())
        with self.assertRaises(ValueError):
            image.apply(self.target)

    def test_mock_build_publishes_verified_image_and_minimal_tar(self):
        plan = self.prepare()
        def build(args):
            import tarfile
            with tarfile.open(args.initramfs) as tar:
                self.assertEqual(tar.getnames(), ['init'])
                self.assertEqual(tar.extractfile('init').read(), self.init.read_bytes())
            with open(args.output, 'wb') as f:
                f.truncate(plan['layout']['size_bytes'])
                f.seek(plan['layout']['exfat']['offset_bytes'])
                f.write(self.exfat.read_bytes())
        with mock.patch.object(image.usb, 'build_image', side_effect=build), \
             mock.patch.object(image, 'verify_image', return_value={'disk_guid': 'test'}):
            ready = image.apply(self.target)
        self.assertEqual(ready['target_uuid'], plan['target_uuid'])
        self.assertEqual(json.loads((self.target / 'ready.json').read_text()), ready)
        self.assertTrue((self.target / 'image.img').is_file())
        with self.assertRaises(ValueError):
            image.apply(self.target)

    def verify_fixture(self, identity_bound):
        import binascii
        import hashlib
        import uuid
        plan = image.prepare(self.target, self.kernel, self.boot, self.exfat, self.init, identity_bound=identity_bound)
        spec = plan['layout']
        usb = image.usb
        fat = usb.Fat32Builder(spec['esp']['size_bytes'], spec['esp']['offset_bytes'] // 512)
        efi = fat.add_dir(fat.root_cluster, 'EFI')
        loader = fat.add_dir(efi, 'BOOT')
        boot = fat.add_dir(fat.root_cluster, 'BOOT')
        nested = fat.add_dir(boot, 'LIMINE')
        root_limine = fat.add_dir(fat.root_cluster, 'LIMINE')
        bootstrap = b'fixture archive'
        fat.add_file(loader, 'BOOTX64.EFI', self.boot.read_bytes())
        fat.add_file(fat.root_cluster, 'startup.nsh', b'fixture')
        fat.add_file(boot, 'SRVROS.ELF', self.kernel.read_bytes())
        fat.add_file(boot, 'initramfs.tar', bootstrap)
        for parent in (loader, fat.root_cluster, boot, nested, root_limine):
            fat.add_file(parent, 'limine.conf', plan.get('config', image.CONFIG.decode('ascii')).encode('ascii'))
        disk = self.root / 'fixture.img'
        total = spec['size_bytes'] // 512
        entries = bytearray(128 * 128)
        for n, (kind, name) in enumerate(((usb.ESP_TYPE, 'esp'), (usb.BASIC_DATA_TYPE, 'exfat'))):
            first = spec[name]['offset_bytes'] // 512
            last = first + spec[name]['size_bytes'] // 512 - 1
            part_guid = uuid.UUID(plan['gpt'][name + '_partition_guid']) if identity_bound else uuid.uuid4()
            part_name = 'srvros-esp' if name == 'esp' else ('srvros-system-v1' if identity_bound else 'srvros-fat')
            entries[n*128:(n+1)*128] = usb.gpt_entry(kind, part_guid, first, last, part_name)
        crc = binascii.crc32(entries) & 0xffffffff
        disk_guid = uuid.UUID(plan['gpt']['disk_guid']) if identity_bound else uuid.uuid4()
        with open(disk, 'wb') as f:
            f.truncate(spec['size_bytes'])
            def put(offset, data):
                f.seek(offset)
                f.write(data)
            mbr = bytearray(512)
            mbr[450], mbr[510:512] = 0xee, b'\x55\xaa'
            put(0, mbr)
            put(512, usb.gpt_header(1, total-1, 34, total-34, 2, 128, crc, disk_guid))
            put(1024, entries)
            put((total-33)*512, entries)
            put((total-1)*512, usb.gpt_header(total-1, 1, 34, total-34, total-33, 128, crc, disk_guid))
            base = spec['esp']['offset_bytes']
            put(base, fat.boot_sector())
            table = bytearray(fat.fat_sectors * 512)
            for n, value in enumerate(fat.fat):
                struct.pack_into('<I', table, n * 4, value)
            for n in range(2):
                put(base + (fat.reserved_sectors + n * fat.fat_sectors) * 512, table)
            for cluster, members in fat.dirs.items():
                block = bytearray(4096)
                pos = 0
                for name, is_dir, entry_cluster, size in members:
                    short = usb.short_alias(name)
                    for entry in usb.lfn_dir_entries(name, short) + [fat.dir_entry(short, is_dir, entry_cluster, size)]:
                        block[pos:pos+32] = entry
                        pos += 32
                put(base + fat.cluster_offset(cluster), block)
            for cluster, contents in fat.files.items():
                put(base + fat.cluster_offset(cluster), contents)
        gpt = image.verify_image(disk, plan, bootstrap)
        self.assertEqual(gpt['disk_guid'], str(disk_guid))
        if identity_bound:
            self.assertEqual(gpt['exfat_partition_guid'], plan['gpt']['exfat_partition_guid'])
            bad_plan = json.loads(json.dumps(plan))
            bad_plan['gpt']['exfat_partition_guid'] = str(uuid.uuid4())
            with self.assertRaises(ValueError):
                image.verify_image(disk, bad_plan, bootstrap)
            bad_plan = json.loads(json.dumps(plan))
            bad_plan['gpt']['exfat_partition_name'] = 'other'
            with self.assertRaises(ValueError):
                image.verify_image(disk, bad_plan, bootstrap)
            bad_plan = json.loads(json.dumps(plan))
            bad_plan['config'] += '# altered\n'
            with self.assertRaises(RuntimeError):
                image.verify_image(disk, bad_plan, bootstrap)
        with open(disk, 'r+b') as f:
            f.seek(512 + 16)
            f.write(b'\0' * 4)
        with self.assertRaises(ValueError):
            image.verify_image(disk, plan, bootstrap)

    def test_verify_generic_fixture(self):
        self.verify_fixture(False)

    def test_verify_identity_bound_fixture(self):
        self.verify_fixture(True)

    def test_identity_plan_and_apply_forwarding(self):
        plan = image.prepare(self.target, self.kernel, self.boot, self.exfat, self.init, identity_bound=True)
        guid = plan['gpt']['exfat_partition_guid']
        self.assertIn('srvros.root=' + guid, plan['config'])
        self.assertIn('srvros.root-format=exfat-v1 srvros.root-role=system', plan['config'])
        def build(args):
            self.assertEqual(args.data_guid, guid)
            self.assertEqual(args.disk_guid, plan['gpt']['disk_guid'])
            self.assertEqual(args.esp_guid, plan['gpt']['esp_partition_guid'])
            self.assertEqual(args.data_name, 'srvros-system-v1')
            self.assertEqual(Path(args.config).read_text(), plan['config'])
            with open(args.output, 'wb') as f:
                f.truncate(plan['layout']['size_bytes'])
                f.seek(plan['layout']['exfat']['offset_bytes'])
                f.write(self.exfat.read_bytes())
        gpt = {k: plan['gpt'][k] for k in ('disk_guid', 'esp_partition_guid', 'exfat_partition_guid')}
        with mock.patch.object(image.usb, 'build_image', side_effect=build), \
             mock.patch.object(image, 'verify_image', return_value=gpt):
            ready = image.apply(self.target)
        self.assertEqual(ready['selection']['partition_guid'], guid)
        self.assertEqual(ready['selection']['partition_name'], 'srvros-system-v1')

    def test_identity_inconsistent_plan_refused_even_with_updated_checksum(self):
        plan = image.prepare(self.target, self.kernel, self.boot, self.exfat, self.init, identity_bound=True)
        plan['config'] = plan['config'].replace('srvros.root-role=system', 'srvros.root-role=other')
        path = self.target / 'plan.json'
        path.write_text(json.dumps(plan))
        (self.target / 'plan.sha256').write_text(image.digest(path) + '\n')
        with self.assertRaises(ValueError):
            image.apply(self.target)
        self.assertFalse((self.target / 'in-progress.json').exists())

    def test_sparse_image_is_rejected(self):
        plan = self.prepare()
        disk = self.root / 'sparse'
        with open(disk, 'wb') as f:
            f.truncate(plan['layout']['size_bytes'])
        with self.assertRaises(ValueError):
            image.verify_image(disk, plan, b'bootstrap')

    def test_tampered_plan_and_hardlink_alias(self):
        alias = self.root / 'alias'
        os.link(self.exfat, alias)
        before = self.exfat.read_bytes()
        with self.assertRaises(FileExistsError):
            image.prepare(alias, self.kernel, self.boot, self.exfat, self.init)
        self.assertEqual(before, self.exfat.read_bytes())
        self.prepare()
        plan_path = self.target / 'plan.json'
        plan = json.loads(plan_path.read_text())
        plan['layout']['exfat']['offset_bytes'] += 512
        plan_path.write_text(json.dumps(plan))
        with self.assertRaises(ValueError):
            image.apply(self.target)
        self.assertEqual(before, self.exfat.read_bytes())

    def test_changed_source_record_and_uuid_are_refused_before_apply(self):
        for field in ('source', 'uuid'):
            with self.subTest(field=field):
                target = self.root / ('stage-' + field)
                image.prepare(target, self.kernel, self.boot, self.exfat, self.init)
                plan_path = target / 'plan.json'
                plan = json.loads(plan_path.read_text())
                if field == 'source':
                    alternate = self.root / 'alternate-kernel'
                    alternate.write_bytes(b'\x7fELFalternate')
                    plan['sources']['kernel'] = image.source(alternate)
                else:
                    plan['target_uuid'] = '00000000-0000-4000-8000-000000000001'
                plan_path.write_text(json.dumps(plan))
                before = {path.name: path.read_bytes() for path in target.iterdir()}
                with self.assertRaisesRegex(ValueError, 'plan changed since prepare'):
                    image.apply(target)
                self.assertEqual(before, {path.name: path.read_bytes() for path in target.iterdir()})
                self.assertEqual(self.kernel.read_bytes(), b'\x7fELFkernel')
                self.assertEqual(self.exfat.read_bytes()[3:11], b'EXFAT   ')

    def test_source_changed_during_build(self):
        plan = self.prepare()
        before = self.exfat.read_bytes()
        def build(args):
            with open(args.output, 'wb') as f:
                f.truncate(plan['layout']['size_bytes'])
            self.kernel.write_bytes(b'\x7fELFchanged')
        with mock.patch.object(image.usb, 'build_image', side_effect=build), \
             mock.patch.object(image, 'verify_image', return_value={}):
            with self.assertRaises(ValueError):
                image.apply(self.target)
        self.assertEqual(before, self.exfat.read_bytes())
        self.assertFalse((self.target / 'ready.json').exists())

    def test_interruption_after_image_publication_has_no_ready_marker(self):
        plan = self.prepare()
        def build(args):
            with open(args.output, 'wb') as f:
                f.truncate(plan['layout']['size_bytes'])
                f.seek(plan['layout']['exfat']['offset_bytes'])
                f.write(self.exfat.read_bytes())
        real_write = image.write_json
        def fail_ready(path, data):
            if path.name == 'ready.partial':
                raise OSError('interrupted')
            return real_write(path, data)
        with mock.patch.object(image.usb, 'build_image', side_effect=build), \
             mock.patch.object(image, 'verify_image', return_value={}), \
             mock.patch.object(image, 'write_json', side_effect=fail_ready):
            with self.assertRaises(OSError):
                image.apply(self.target)
        self.assertTrue((self.target / 'image.img').exists())
        self.assertFalse((self.target / 'image.partial').exists())
        self.assertFalse((self.target / 'ready.json').exists())
        with self.assertRaises(ValueError):
            image.apply(self.target)


if __name__ == '__main__':
    unittest.main()
