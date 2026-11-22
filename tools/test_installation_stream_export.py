"""Offline v2 export publication and native integration tests."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import installation_export_publication as publication
import profile_bundle
import profile_stream
import test_installation_export as fixture
from test_installation_export import EXPORT_DRIVER, HOME, INSPECT_DRIVER, PROFILE_ID, ROOT, bundle, snapshot


class StreamPublicationTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name).resolve()
        self.source = self.root / 'source'
        self.source.mkdir()
        self.spool = self.root / 'spool'
        self.payload = b'abc123' * 25000
        manifest = {'format': profile_bundle.FORMAT, 'schema_version': 2,
                    'profile': bundle()['profile'], 'preferences': {'text_size': 16},
                    'documents': [{'path': 'Documents/data.bin', 'size': len(self.payload),
                                   'sha256': hashlib.sha256(self.payload).hexdigest()}]}
        encoded = json.dumps(manifest, sort_keys=True, separators=(',', ':'), ensure_ascii=True).encode()
        self.spool.write_bytes(profile_stream.HEADER.pack(profile_stream.MAGIC, len(encoded), 0) + encoded + self.payload)
        self.expected = self.spool.read_bytes()
        self.source_snapshot = {'stage': str(self.source), 'plan_sha256': '1' * 64,
                                'image_sha256': '2' * 64, 'home': HOME}

    def interrupted(self, checkpoint):
        attempt, output = self.root / 'attempt', self.root / 'output'
        def stop(name):
            if name == checkpoint:
                raise OSError('injected interruption')
        result = publication.publish_stream(attempt, output, self.spool, self.source_snapshot, stop)
        self.assertEqual(result['state'], 'refused', result)
        return attempt, output

    def readonly_inspect(self, attempt):
        # A read-only inspection must never mutate the record, aliases or output.
        def digest_tree():
            return {str(p.relative_to(self.root)): (p.stat().st_ino, p.stat().st_nlink,
                    hashlib.sha256(p.read_bytes()).hexdigest())
                    for p in self.root.rglob('*') if p.is_file()}
        before = digest_tree()
        with mock.patch.object(publication.os, 'fsync', side_effect=AssertionError('fsync during inspect')), \
             mock.patch.object(publication.os, 'write', side_effect=AssertionError('write during inspect')), \
             mock.patch.object(publication.os, 'link', side_effect=AssertionError('link during inspect')), \
             mock.patch.object(publication.os, 'unlink', side_effect=AssertionError('unlink during inspect')):
            result = publication.inspect(attempt)
        self.assertEqual(digest_tree(), before)
        self.assertTrue(result['read_only'])
        return result

    def test_every_checkpoint_recovery_without_source(self):
        for checkpoint in publication.CHECKPOINTS:
            with self.subTest(checkpoint=checkpoint), tempfile.TemporaryDirectory(dir=self.root) as directory:
                # Isolate all names and keep the recorded source path valid but disposable.
                old_root, old_source, old_spool, old_snapshot = self.root, self.source, self.spool, self.source_snapshot
                self.root = Path(directory)
                self.source = self.root / 'source'; self.source.mkdir()
                self.spool = self.root / 'spool'; self.spool.write_bytes(self.expected)
                self.source_snapshot = dict(old_snapshot, stage=str(self.source))
                try:
                    attempt, output = self.interrupted(checkpoint)
                    observed = self.readonly_inspect(attempt)
                    if checkpoint in ('recorded', 'partial-write'):
                        self.assertEqual(observed['state'], 'incomplete', observed)
                        self.assertEqual(publication.reconcile(attempt)['state'], 'refused')
                        self.assertFalse(output.exists())
                    else:
                        self.assertTrue(observed['can_reconcile'], observed)
                        if checkpoint == 'linked':
                            self.assertTrue((attempt / 'bundle.partial').exists())
                            self.assertTrue(output.exists())
                            self.assertEqual(output.stat().st_ino, (attempt / 'bundle.partial').stat().st_ino)
                        shutil.rmtree(self.source); self.spool.unlink()
                        recovered = publication.reconcile(attempt)
                        self.assertEqual(recovered['state'], 'complete', recovered)
                        self.assertEqual(output.read_bytes(), self.expected)
                        self.assertEqual(self.readonly_inspect(attempt)['bundle_sha256'], hashlib.sha256(self.expected).hexdigest())
                finally:
                    self.root, self.source, self.spool, self.source_snapshot = old_root, old_source, old_spool, old_snapshot

    def test_small_stream_interrupts_before_all_bytes_are_written(self):
        manifest = {'format': profile_bundle.FORMAT, 'schema_version': 2,
                    'profile': bundle()['profile'], 'preferences': {}, 'documents': []}
        encoded = json.dumps(manifest, sort_keys=True, separators=(',', ':'), ensure_ascii=True).encode()
        self.spool.write_bytes(profile_stream.HEADER.pack(profile_stream.MAGIC, len(encoded), 0) + encoded)
        attempt, output = self.interrupted('partial-write')
        self.assertFalse(output.exists())
        self.assertEqual((attempt / 'bundle.partial').stat().st_size, self.spool.stat().st_size // 2)
        self.assertEqual(self.readonly_inspect(attempt)['state'], 'incomplete')

    def test_restore_codec_accepts_noncanonical_manifest(self):
        manifest = {'format': profile_bundle.FORMAT, 'schema_version': 2,
                    'profile': bundle()['profile'], 'preferences': {}, 'documents': []}
        encoded = json.dumps(manifest, indent=2).encode()
        self.spool.write_bytes(profile_stream.HEADER.pack(profile_stream.MAGIC, len(encoded), 0) + encoded)
        with profile_stream.open_bundle(self.spool) as opened:
            self.assertEqual(opened.metadata, manifest)

    def test_tamper_extra_link_and_existing_output_refused(self):
        for violation in ('tamper', 'extra-link', 'existing'):
            with self.subTest(violation=violation), tempfile.TemporaryDirectory(dir=self.root) as directory:
                root = Path(directory); attempt = root / 'attempt'; output = root / 'output'
                if violation == 'existing':
                    output.write_bytes(b'preserve')
                    result = publication.publish_stream(attempt, output, self.spool, self.source_snapshot)
                    self.assertEqual(result['state'], 'refused', result)
                    self.assertEqual(output.read_bytes(), b'preserve')
                    continue
                def stop(name):
                    if name == 'linked': raise OSError('interrupted')
                publication.publish_stream(attempt, output, self.spool, self.source_snapshot, stop)
                if violation == 'tamper':
                    with output.open('r+b') as f:
                        f.seek(-1, os.SEEK_END); f.write(b'!')
                else:
                    os.link(output, root / 'extra')
                self.assertEqual(publication.inspect(attempt)['state'], 'refused')
                self.assertEqual(publication.reconcile(attempt)['state'], 'refused')
                self.assertTrue(output.exists())
                self.assertTrue((attempt / 'bundle.partial').exists())


class StreamInstalledTests(unittest.TestCase):
    setUp = fixture.InstalledExportTests.setUp
    publish = fixture.InstalledExportTests.publish
    call = fixture.InstalledExportTests.call

    def test_api_and_cli_stream_export(self):
        self.publish()
        before = snapshot(self.stage)
        output = self.root / 'export-v2'
        missing = self.call(output=output, documents=['Documents/notes.txt'], publication_dir=None)
        self.assertEqual(missing['state'], 'refused', missing)
        self.assertFalse(output.exists())
        result = self.call(output=output, documents=['Documents/notes.txt'],
                           publication_dir=self.root / 'api-attempt')
        self.assertEqual(result['state'], 'exported', result)
        with profile_stream.open_bundle(output) as opened:
            self.assertEqual(opened.metadata['schema_version'], 2)
            self.assertEqual(opened.metadata['profile'], bundle()['profile'])
            self.assertEqual(opened.metadata['documents'][0]['sha256'], hashlib.sha256(b'portable data').hexdigest())
        self.assertEqual(snapshot(self.stage), before)
        cli_output = self.root / 'cli-v2'
        cmd = [sys.executable, str(ROOT / 'tools/installation_image.py'), 'export-profile', str(self.stage),
               str(cli_output), '--profile-id', PROFILE_ID, '--home', HOME, '--document', 'Documents/notes.txt',
               '--inspect-driver', str(INSPECT_DRIVER), '--export-driver', str(EXPORT_DRIVER)]
        missing_cli = subprocess.run(cmd, cwd=self.root, capture_output=True, text=True)
        self.assertNotEqual(missing_cli.returncode, 0)
        self.assertFalse(cli_output.exists())
        args = cmd + ['--publication-dir', str(self.root / 'cli-attempt')]
        old_selector = subprocess.run(args + ['--bundle-version', '1'], cwd=self.root,
                                      capture_output=True, text=True)
        self.assertEqual(old_selector.returncode, 2, old_selector.stdout)
        self.assertFalse(cli_output.exists())
        proc = subprocess.run(args, cwd=self.root, capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0, proc.stderr + proc.stdout)
        self.assertEqual(json.loads(proc.stdout)['state'], 'exported')
        with profile_stream.open_bundle(cli_output) as opened:
            self.assertEqual(opened.metadata['schema_version'], 2)
            self.assertEqual(opened.metadata['documents'][0]['sha256'], hashlib.sha256(b'portable data').hexdigest())
        self.assertEqual(snapshot(self.stage), before)


if __name__ == '__main__':
    unittest.main()
