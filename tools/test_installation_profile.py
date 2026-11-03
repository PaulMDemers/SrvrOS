import base64
import binascii
import hashlib
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock
import uuid

import installation_image as install
import profile_bundle as profiles
from test_installation_image import exfat_fixture


def sparse_builder(args):
    """Build a sparse but valid disk with the production GPT/FAT helpers."""
    usb = install.usb
    data = Path(args.exfat).read_bytes()
    spec = install.layout(len(data))
    fat = usb.Fat32Builder(spec['esp']['size_bytes'], spec['esp']['offset_bytes'] // 512)
    efi = fat.add_dir(fat.root_cluster, 'EFI')
    loader = fat.add_dir(efi, 'BOOT')
    boot = fat.add_dir(fat.root_cluster, 'BOOT')
    nested = fat.add_dir(boot, 'LIMINE')
    root_limine = fat.add_dir(fat.root_cluster, 'LIMINE')
    fat.add_file(loader, 'BOOTX64.EFI', Path(args.bootx64).read_bytes())
    fat.add_file(fat.root_cluster, 'startup.nsh', b'fixture')
    fat.add_file(boot, 'SRVROS.ELF', Path(args.kernel).read_bytes())
    fat.add_file(boot, 'initramfs.tar', Path(args.initramfs).read_bytes())
    for parent in (loader, fat.root_cluster, boot, nested, root_limine):
        fat.add_file(parent, 'limine.conf', Path(args.config).read_bytes())
    total = spec['size_bytes'] // 512
    entries = bytearray(128 * 128)
    for n, (kind, key, guid, name) in enumerate(((usb.ESP_TYPE, 'esp', args.esp_guid, 'srvros-esp'),
            (usb.BASIC_DATA_TYPE, 'exfat', args.data_guid, args.data_name))):
        first = spec[key]['offset_bytes'] // 512
        last = first + spec[key]['size_bytes'] // 512 - 1
        entries[n*128:(n+1)*128] = usb.gpt_entry(kind, uuid.UUID(guid), first, last, name)
    crc = binascii.crc32(entries) & 0xffffffff
    disk_guid = uuid.UUID(args.disk_guid)
    with open(args.output, 'xb') as f:
        f.truncate(spec['size_bytes'])
        def put(offset, content):
            f.seek(offset)
            f.write(content)
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
        put(spec['exfat']['offset_bytes'], data)


class ProfileInstallationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.target = self.root / 'stage'
        self.kernel, self.init, self.boot = (self.root / n for n in ('kernel', 'init', 'boot'))
        self.kernel.write_bytes(b'\x7fELFkernel')
        self.init.write_bytes(b'\x7fELFinit')
        self.boot.write_bytes(b'MZefi')
        self.exfat = self.root / 'source.exfat'
        exfat_fixture(self.exfat)
        self.driver = self.root / 'driver'
        # Exercising actual provision subprocess calls, without a built driver.
        self.driver.write_text('#!/usr/bin/env python3\nimport sys\nwith open(sys.argv[1], "r+b") as f:\n f.seek(-1,2); f.write(b"X")\n')
        self.driver.chmod(0o755)
        self.bundle_path = self.root / 'profile.json'
        content = b'portable document'
        self.bundle = {'format': profiles.FORMAT, 'schema_version': 1,
                       'profile': {'id': str(uuid.uuid4()), 'display_name': 'A'}, 'preferences': {},
                       'documents': [{'path': 'Documents/test.txt', 'size': len(content),
                                      'sha256': hashlib.sha256(content).hexdigest(),
                                      'data_base64': base64.b64encode(content).decode()}]}
        self.bundle_path.write_bytes(profiles.encode_json(self.bundle))
        self.options = dict(identity_bound=True, profile_bundle=self.bundle_path,
                            profile_home='/fat/home/alice', profile_alias='alice',
                            profile_uid=1001, profile_gid=1001, profile_driver=self.driver)

    def prepare(self, **changes):
        return install.prepare(self.target, self.kernel, self.boot, self.exfat, self.init,
                               **dict(self.options, **changes))

    def test_refuse_partial_invalid_local_home_and_paths_before_stage(self):
        for changes in ({'profile_gid': None}, {'identity_bound': False},
                        {'profile_home': '/fat/home/../evil'}, {'profile_alias': '../bad'},
                        {'profile_uid': 0}, {'profile_gid': True}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                self.prepare(**changes)
            self.assertFalse(self.target.exists())
        self.bundle['documents'][0]['path'] = 'Documents/' + 'a' * 60 + '/' + 'b' * 60 + '/x'
        self.bundle_path.write_bytes(profiles.encode_json(self.bundle))
        with self.assertRaises(ValueError):
            self.prepare()
        self.assertFalse(self.target.exists())

    def test_reject_malformed_truncated_and_tampered_bundle(self):
        for data in (b'{', b'{}', profiles.encode_json(dict(self.bundle, documents=[dict(self.bundle['documents'][0], sha256='0'*64)]))):
            self.bundle_path.write_bytes(data)
            with self.assertRaises(ValueError):
                self.prepare()
            self.assertFalse(self.target.exists())

    def test_changed_driver_bundle_or_plan_refused(self):
        plan = self.prepare()
        original = self.driver.read_bytes()
        self.driver.write_bytes(original + b'#changed\n')
        with self.assertRaises(ValueError):
            install.apply(self.target)
        self.driver.write_bytes(original)
        # Bundle contents differ while preserving valid schema.
        original_bundle = self.bundle_path.read_bytes()
        self.bundle['profile']['display_name'] = 'Changed'
        self.bundle_path.write_bytes(profiles.encode_json(self.bundle))
        with self.assertRaises(ValueError):
            install.apply(self.target)
        self.bundle_path.write_bytes(original_bundle)
        plan['profile']['alias'] = 'different'
        path = self.target / 'plan.json'
        path.write_text(json.dumps(plan))
        (self.target / 'plan.sha256').write_text(install.digest(path) + '\n')
        # A changed, yet valid alias is a different binding; ordinary checksum
        # detects unintended edits, not an adversary replacing both files.
        # Ensure a mismatch in normative profile ID is rejected after rehash.
        plan['profile']['profile_id'] = str(uuid.uuid4())
        path.write_text(json.dumps(plan))
        (self.target / 'plan.sha256').write_text(install.digest(path) + '\n')
        with self.assertRaises(ValueError):
            install.apply(self.target)
        self.assertFalse((self.target / 'image.img').exists())

    def test_driver_failure_leaves_no_publication_and_cannot_resume(self):
        self.driver.write_text('#!/bin/sh\nexit 1\n')
        self.prepare()
        with self.assertRaises(Exception):
            install.apply(self.target)
        self.assertFalse((self.target / 'ready.json').exists())
        self.assertFalse((self.target / 'image.img').exists())
        with self.assertRaises(ValueError):
            install.apply(self.target)

    def test_provisioned_data_is_verified_and_source_untouched(self):
        original = self.exfat.read_bytes()
        plan = self.prepare()
        with mock.patch.object(install.usb, 'build_image', side_effect=sparse_builder):
            ready = install.apply(self.target)
        self.assertEqual(self.exfat.read_bytes(), original)
        self.assertEqual(ready['profile']['profile_id'], self.bundle['profile']['id'])
        self.assertEqual(ready['profile']['launch_command'], 'run /fat/bin/deskd --profile=/fat/home/alice')
        self.assertNotEqual(ready['profile_exfat_sha256'], plan['sources']['exfat']['sha256'])
        self.assertEqual(ready['profile_exfat_sha256'], install.digest(self.target / 'profile.exfat'))
        offset = ready['layout']['exfat']['offset_bytes']
        with (self.target / 'image.img').open('rb') as disk:
            disk.seek(offset)
            self.assertEqual(hashlib.sha256(disk.read(len(original))).hexdigest(), ready['profile_exfat_sha256'])


if __name__ == '__main__':
    unittest.main()
