#!/usr/bin/env python3
"""Workspace preparation uses only a verified USB mount and preserves user files."""
import contextlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import hardware_workspace as workspace


class WorkspaceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.mount = self.root/'usb'
        self.mount.mkdir()
        stack = contextlib.ExitStack()
        self.addCleanup(stack.close)
        self.stage = stack.enter_context(patch.object(workspace.boot, 'stage', return_value='1'*16))
        stack.enter_context(patch.object(workspace.boot, 'ROOT', self.root))
        stack.enter_context(patch.object(workspace.boot, 'verify', return_value={'release':'1'*16}))
        self.select = stack.enter_context(patch.object(workspace.host, 'volume_mount', return_value=self.mount))
        stack.enter_context(patch.object(workspace.os, 'sync'))
        stack.enter_context(contextlib.redirect_stdout(io.StringIO()))

    def test_prepare_and_repeat_preserve_documents(self):
        result = workspace.prepare('37D8-C5D0')
        self.assertEqual(result['volume'], '37d8c5d0')
        self.select.assert_called_once_with('37d8c5d0')
        self.stage.assert_called_once_with(workspace_volume='37d8c5d0')
        folder = self.mount/'srvros-work'
        self.assertEqual((folder/'.workspace').read_bytes(), b'SRVROS-WORKSPACE-V1 37d8c5d0\n')
        for name in ('README.txt','welcome.sh','notes.txt','.profile'):
            (folder/name).write_text('user content')
        workspace.prepare('37D8-C5D0')
        for name in ('README.txt','welcome.sh','notes.txt','.profile'):
            self.assertEqual((folder/name).read_text(), 'user content')

    def test_wrong_marker_refuses_before_stage(self):
        folder = self.mount/'srvros-work'; folder.mkdir()
        (folder/'.workspace').write_text('wrong volume')
        with self.assertRaises(ValueError): workspace.prepare('37D8-C5D0')
        self.stage.assert_not_called()

    def test_symlink_directory_refused(self):
        (self.mount/'srvros-work').symlink_to(self.root, target_is_directory=True)
        with self.assertRaises(ValueError): workspace.prepare('37D8-C5D0')
        self.stage.assert_not_called()

    def test_symlink_marker_refused(self):
        folder = self.mount/'srvros-work'; folder.mkdir()
        (folder/'.workspace').symlink_to(self.root/'absent')
        with self.assertRaises(ValueError): workspace.prepare('37D8-C5D0')
        self.stage.assert_not_called()

    def test_failed_build_does_not_touch_usb(self):
        self.stage.side_effect = ValueError('missing build')
        with self.assertRaises(ValueError): workspace.prepare('37D8-C5D0')
        self.assertEqual(list(self.mount.iterdir()), [])

    def test_usb_selection_failure_does_not_stage(self):
        self.select.side_effect = ValueError('not USB')
        with self.assertRaises(ValueError): workspace.prepare('37D8-C5D0')
        self.stage.assert_not_called()


if __name__ == '__main__': unittest.main()
