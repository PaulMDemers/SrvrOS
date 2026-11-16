"""State-machine, cache and interrupted-return tests; no host install/reboot."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from workflow.runner import Runner, boot_id
from workflow.store import Cache, atomic, files, gc, lock, sha


class CacheTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.repo = self.root / 'repo'
        self.repo.mkdir()
        (self.repo / 'input').write_text('one')
        self.cache = Cache(self.root / 'cache')
        self.log = self.root / 'log'
        self.job = dict(name='proof', argv=[sys.executable, '-c',
            "from pathlib import Path; print('checked'); Path('output').write_text(Path('input').read_text())"],
            inputs=['input'], outputs=['output'], timeout=10)

    def run_job(self, **kwargs):
        return self.cache.run(self.job, self.repo, self.log, **kwargs)

    def test_bytes_not_mtime_and_outputs_are_verified(self):
        self.assertFalse(self.run_job()['cached'])
        self.assertTrue(self.run_job()['cached'])
        mtime = (self.repo / 'input').stat().st_mtime_ns
        (self.repo / 'input').write_text('two')
        os.utime(self.repo / 'input', ns=(mtime, mtime))
        self.assertFalse(self.run_job()['cached'])
        (self.repo / 'output').write_text('corrupt')
        self.assertFalse(self.run_job()['cached'])
        self.assertEqual((self.repo / 'output').read_text(), 'two')

    def test_docs_do_not_invalidate_declared_runtime_dependencies(self):
        self.run_job()
        (self.repo / 'notes.md').write_text('docs only')
        self.assertTrue(self.run_job()['cached'])
        self.assertFalse(self.run_job(fresh=True)['cached'])

    def test_transport_token_is_removed_from_child_as_well_as_cache_key(self):
        self.job['argv'][-1] += "; import os; assert 'CODEX_TOOL_CALL_ID' not in os.environ"
        with patch.dict(os.environ, {'CODEX_TOOL_CALL_ID': 'first'}):
            self.assertFalse(self.run_job()['cached'])
        with patch.dict(os.environ, {'CODEX_TOOL_CALL_ID': 'second'}):
            self.assertTrue(self.run_job()['cached'])

    def test_command_environment_and_glob_membership_change_keys(self):
        self.job['inputs'] = ['input*']
        first = self.run_job()['key']
        (self.repo / 'input2').write_text('more')
        second = self.run_job()['key']
        self.job['env'] = {'SRVROS_WORKFLOW_TEST': 'different'}
        third = self.run_job()['key']
        self.job['argv'][-1] += '; print(2)'
        fourth = self.run_job()['key']
        self.assertEqual(len({first, second, third, fourth}), 4)

    def test_mutating_input_never_seals_a_pass(self):
        self.job['argv'][-1] += "; Path('input').write_text('mutated')"
        with self.assertRaisesRegex(ValueError, 'changed during'):
            self.run_job()
        self.assertFalse(list((self.root / 'cache').glob('*/receipt.json')))

    def test_failure_and_timeout_never_seal_a_pass(self):
        self.job['argv'] = [sys.executable, '-c', 'raise SystemExit(9)']
        with self.assertRaises(subprocess.CalledProcessError):
            self.run_job()
        self.job['argv'] = [sys.executable, '-c', 'import time; time.sleep(2)']
        self.job['timeout'] = .03
        with self.assertRaises(subprocess.TimeoutExpired):
            self.run_job()
        self.assertFalse(list((self.root / 'cache').glob('*/receipt.json')))

    def test_symlink_and_missing_dependencies_refused(self):
        (self.repo / 'link').symlink_to(self.repo / 'input')
        for patterns in (['link'], ['missing'], ['../input']):
            with self.assertRaises(ValueError):
                files(self.repo, patterns)

    def test_checkout_lock_excludes_other_harness(self):
        with lock(self.root / 'lock'):
            with self.assertRaisesRegex(ValueError, 'Another workflow'):
                with lock(self.root / 'lock'):
                    pass

    def test_gc_only_managed_cache_keeps_pins_and_dry_run(self):
        self.run_job()
        bank = self.root / 'accepted'
        bank.mkdir()
        (bank / 'proof').write_text('accepted')
        dry = gc(self.root / 'cache', keep=0, max_age_days=0, max_bytes=0)
        self.assertEqual(len(dry['removed']), 1)
        self.assertTrue(Path(dry['removed'][0]['path']).exists())
        gc(self.root / 'cache', keep=1, max_age_days=0, max_bytes=0, apply=True)
        self.assertTrue(Path(dry['removed'][0]['path']).exists())
        gc(self.root / 'cache', keep=0, max_age_days=0, max_bytes=0, apply=True)
        self.assertFalse(Path(dry['removed'][0]['path']).exists())
        self.assertEqual((bank / 'proof').read_text(), 'accepted')


class FixtureRunner(Runner):
    """Simulates side effects, while exercising the real persistent state machine."""
    def __init__(self, *args):
        super().__init__(*args)
        self.effects = []
        self.fail = None

    def stage(self, directory, manifest, state):
        self.effects.append('stage')
        self.assert_intent(directory, 'stage')
        if self.fail == 'stage':
            raise RuntimeError('interrupted stage')
        state.update(release='1234567890abcdef', recipe='silent-test', manifest_digest='fixture')

    def assert_intent(self, directory, intent):
        assert json.loads((directory / 'state.json').read_text())['intent'] == intent

    def candidate(self, state):
        assert state['release'] == '1234567890abcdef'

    def vm(self, directory, state):
        self.effects.append('vm')
        if self.fail == 'vm':
            raise RuntimeError('interrupted VM')

    def guard(self, manifest, state, **kwargs):
        self.effects.append(('guard', kwargs))
        return dict(current='0123456789abcdef', previous='fedcba9876543210')

    def media(self, directory, manifest, returning=False):
        self.effects.append(('media', returning))
        return {}

    def privileged(self, manifest, argv, log=None):
        self.effects.append(tuple(map(str, argv)))
        if 'install' in argv and self.fail == 'install':
            raise RuntimeError('interrupted install')
        if 'reboot' in argv and self.fail == 'reboot':
            raise RuntimeError('reboot rejected')

    def quiet(self, state):
        self.effects.append('volume-1')


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.repo = self.root / 'repo'
        self.repo.mkdir()
        subprocess.run(['git', 'init', '-q', str(self.repo)], check=True)
        (self.repo / 'input.txt').write_text('frozen')
        subprocess.run(['git', 'add', '.'], cwd=self.repo, check=True)
        subprocess.run(['git', '-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid',
                        'commit', '-qm', 'fixture'], cwd=self.repo, check=True)
        self.patch = patch('workflow.runner.code_inputs', return_value=['input.txt'])
        self.patch.start()
        self.addCleanup(self.patch.stop)
        self.unit = dict(schema=1, repo=str(self.repo), model='Fixture1,1', volume='aabbccdd',
                         recovery='fedcba9876543210', protected_releases=['fedcba9876543210'],
                         volume_percent=1, privilege=['fixture-no-root'], ac_path='unused', payload_bank='unused')
        self.job = dict(name='check', argv=[sys.executable, '-c', "print('PASS')"],
                        inputs=['input.txt'], outputs=[], timeout=10)
        self.runner = FixtureRunner(self.repo, self.root / 'store')
        self.runner.create('candidate', self.unit, 'Can a complete lifetime retire?',
                           'matching checkpoint, clean return', jobs=dict(development=[self.job], qualification=[self.job]))

    def state(self):
        return self.runner.load('candidate')[2]

    def test_cli_exposes_combined_profile_without_starting_a_trial(self):
        import device_workflow
        (self.repo / 'build').mkdir()
        atomic(self.runner.store / 'unit.json', self.unit)
        for trial_profile in ('intel-ggtt-invalidate-v36', 'intel-command-memory-v37','intel-gt-bootstrap-v41'):
            argv = ['device_workflow.py', 'new', 'combined', '--profile',
                    trial_profile, '--question', 'Complete original hold?',
                    '--expected', 'Exact retirement']
            with patch.object(device_workflow, 'ROOT', self.repo), \
                    patch.object(device_workflow, 'Runner', return_value=self.runner), \
                    patch.object(self.runner, 'create', return_value='combined') as create, \
                    patch.object(sys, 'argv', argv):
                device_workflow.main()
            create.assert_called_once_with('combined', self.unit, 'Complete original hold?',
                    'Exact retirement', jobs=None, trial_profile=trial_profile)
            self.assertEqual(self.runner.effects, [])

    def test_full_gt_v41_freezes_wire3_and_old_profiles_are_historical(self):
        runner=FixtureRunner(self.repo,self.root/'v41-store')
        unit=dict(self.unit,model='MacBookAir7,2')
        jobs=dict(development=[self.job],qualification=[self.job])
        with self.assertRaisesRegex(ValueError,'historical'):
            runner.create('obsolete',unit,'Original lifetime?','Complete retirement',jobs=jobs,trial_profile='intel-gt-bootstrap-v39')
        with self.assertRaises(ValueError):
            runner.create('obsolete40',unit,'Original lifetime?','Complete retirement',jobs=jobs,trial_profile='intel-gt-bootstrap-v40')
        runner.create('sealed',unit,'Original lifetime?','Complete retirement',jobs=jobs,trial_profile='intel-gt-bootstrap-v41')
        _,manifest,state=runner.load('sealed')
        self.assertEqual((manifest['profile'],manifest['wire_version'],state['phase']),('intel-gt-bootstrap-v41',3,'NEW'))
        self.assertEqual(runner.effects,[])

    def test_complete_prepare_is_resumable_and_does_not_reboot(self):
        self.runner.advance('candidate')
        self.assertEqual(self.state()['phase'], 'QUALIFIED')
        effects = list(self.runner.effects)
        self.runner.advance('candidate')
        self.assertEqual(effects, self.runner.effects)
        receipts = self.state()['validation']
        self.assertFalse(receipts['development/check']['cached'])
        self.assertFalse(receipts['qualification/check']['cached'])
        self.assertFalse(any('systemctl' in item for item in effects if isinstance(item, tuple)))

    def test_durable_intent_precedes_install_and_prevents_replay(self):
        self.runner.fail = 'install'
        with self.assertRaisesRegex(RuntimeError, 'interrupted install'):
            self.runner.advance('candidate', 'INSTALLED')
        self.assertEqual(self.state()['phase'], 'QUALIFIED')
        self.assertEqual(self.state()['intent'], 'install')
        with self.assertRaisesRegex(ValueError, 'reconcile'):
            self.runner.advance('candidate', 'INSTALLED')

    def test_failed_stage_cannot_be_blindly_repeated(self):
        self.runner.fail = 'stage'
        with self.assertRaises(RuntimeError):
            self.runner.advance('candidate')
        with self.assertRaisesRegex(ValueError, 'reconcile'):
            self.runner.advance('candidate')
        self.assertEqual(self.runner.effects.count('stage'), 1)

    def test_vm_preflight_failure_stops_before_long_validation(self):
        self.runner.fail = 'vm'
        with self.assertRaises(RuntimeError):
            self.runner.advance('candidate')
        receipt = self.state()['validation']['development/check']
        self.assertNotIn('qualification/check',self.state()['validation'])
        self.assertEqual(self.state()['phase'],'STAGED')
        self.runner.fail = None
        self.runner.advance('candidate')
        self.assertEqual(self.state()['validation']['development/check'],receipt)
        self.assertIn('qualification/check',self.state()['validation'])
        self.assertEqual(self.state()['phase'], 'QUALIFIED')

    def test_final_vm_failure_resumes_without_repeating_passed_commands(self):
        original=self.runner.vm
        calls=0
        def fail_second(directory,state):
            nonlocal calls
            calls+=1
            if calls==2:raise RuntimeError('VM receipt changed after host suites')
            original(directory,state)
        with patch.object(self.runner,'vm',side_effect=fail_second):
            with self.assertRaisesRegex(RuntimeError,'VM receipt changed'):
                self.runner.advance('candidate')
            receipt=self.state()['validation']['qualification/check']
            self.runner.advance('candidate')
        self.assertEqual(self.state()['validation']['qualification/check'],receipt)
        self.assertEqual(self.state()['phase'],'QUALIFIED')

    def test_pending_run_blocks_new_run_and_cancel_is_explicit(self):
        with self.assertRaisesRegex(ValueError, 'outstanding'):
            self.runner.create('other', self.unit, 'question', 'expected')
        self.runner.cancel('candidate')
        self.assertEqual(self.state()['phase'], 'CANCELLED')
        self.runner.create('other', self.unit, 'question', 'expected',
                           jobs=dict(development=[self.job], qualification=[self.job]))

    def test_dirty_or_changed_frozen_source_blocks_progress(self):
        (self.repo / 'input.txt').write_text('changed')
        with self.assertRaisesRegex(ValueError, 'clean checkout'):
            self.runner.advance('candidate')
        self.assertFalse(self.runner.effects)

    def test_manifest_edits_are_detected(self):
        directory, _, _ = self.runner.load('candidate')
        manifest = json.loads((directory / 'manifest.json').read_text())
        manifest['source'] = 'wrong'
        atomic(directory / 'manifest.json', manifest)
        with self.assertRaisesRegex(ValueError, 'manifest changed'):
            self.runner.advance('candidate')

    def test_1_percent_and_reboot_request_are_recorded_before_effect(self):
        with patch('workflow.runner.boot_id', return_value='original-boot'):
            self.runner.advance('candidate', 'BOOT_REQUESTED')
        state = self.state()
        self.assertEqual(state['phase'], 'BOOT_REQUESTED')
        self.assertEqual(state['host_boot_id'], 'original-boot')
        effects = self.runner.effects
        self.assertIn('volume-1', effects)
        reboot = next(i for i, value in enumerate(effects) if isinstance(value, tuple) and 'systemctl' in value)
        self.assertLess(effects.index('volume-1'), reboot)
        with patch('workflow.runner.boot_id', return_value='original-boot'):
            with self.assertRaisesRegex(ValueError, 'No matching physical return'):
                self.runner.collect('candidate')
        with self.assertRaisesRegex(ValueError, 'must be reconciled'):
            self.runner.cancel('candidate')

    def test_rejected_reboot_is_disarmed_and_not_replayed(self):
        self.runner.fail = 'reboot'
        with patch('workflow.runner.boot_id', return_value='original-boot'):
            with self.assertRaisesRegex(RuntimeError, 'reboot rejected'):
                self.runner.advance('candidate', 'BOOT_REQUESTED')
        self.assertEqual(self.state()['intent'], 'reconcile')
        self.assertTrue(any(isinstance(value, tuple) and 'unset' in value for value in self.runner.effects))
        with self.assertRaisesRegex(ValueError, 'reconcile'):
            self.runner.advance('candidate', 'BOOT_REQUESTED')

    def test_acceptance_requires_matching_reviewed_native_proof(self):
        directory, _, state = self.runner.load('candidate')
        state.update(phase='COLLECTED', release='1234567890abcdef', session='session-fixture')
        atomic(directory / 'report.json', dict(native_complete=True))
        self.runner.save(directory, state)
        proof = self.root / 'proof.json'
        good = dict(native_acceptance=True, return_integrity='PASS', status='PASS_SCOPED',
                    release=state['release'], runtime_source=state['source'], session=state['session'])
        for update in ({'native_acceptance': False}, {'release': 'wrong'}, {'session': 'other'}, {'runtime_source': 'wrong'}):
            atomic(proof, dict(good, **update))
            with self.assertRaises(ValueError):
                self.runner.close('candidate', proof)
        atomic(proof, good)
        self.runner.close('candidate', proof)
        self.assertEqual(self.state()['phase'], 'ACCEPTED')

    def test_protected_grub_read_uses_only_fixed_path_and_preserves_failures(self):
        manifest = dict(unit=self.unit)
        with patch('workflow.runner.Path.read_text', return_value='normal read'), \
                patch('workflow.runner.subprocess.check_output') as helper:
            self.assertEqual(self.runner.grub_text(manifest), 'normal read')
            helper.assert_not_called()
        with patch('workflow.runner.Path.read_text', side_effect=PermissionError), \
                patch('workflow.runner.subprocess.check_output', return_value='protected config') as helper:
            self.assertEqual(self.runner.grub_text(manifest), 'protected config')
            helper.assert_called_once_with(['fixture-no-root', 'cat', '--', '/boot/grub/grub.cfg'],
                cwd=self.repo.resolve(), text=True, timeout=30)
        for error in (FileNotFoundError, OSError):
            with patch('workflow.runner.Path.read_text', side_effect=error), \
                    patch('workflow.runner.subprocess.check_output') as helper:
                with self.assertRaises(error): self.runner.grub_text(manifest)
                helper.assert_not_called()

    def test_valid_retained_native_failure_is_collected_but_cannot_be_accepted(self):
        directory, manifest, state = self.runner.load('candidate')
        manifest['wire_version'] = 2
        atomic(directory/'manifest.json', manifest)
        state['manifest_sha256'] = sha(directory/'manifest.json')
        state.update(phase='BOOT_REQUESTED', host_boot_id='old', release='1234567890abcdef', recipe='silent-test')
        self.runner.save(directory, state)
        returned = directory / 'returned'; returned.mkdir()
        session = 'session-fixture'; raw = returned / ('raw-'+session); raw.mkdir()
        names = ('boot.log', 'session-a.log', 'exit.log')
        for name in names: (raw/name).write_text('laptop-capture: boot options=silent-test\n')
        atomic(returned/'parsed.json', dict(report=dict(sessions=[dict(session=session,
            releases=[state['release']], exit_snapshot_present=True)])))
        before = directory/'before'; before.mkdir(); atomic(before/'snapshot.json', dict(files={}))
        after = dict(new_sessions=[session], raw={session:dict.fromkeys(names)},
            files=dict.fromkeys('srvros-work/Logs/'+session+'/'+name for name in names))
        parser = lambda text: dict(version=2, complete=False, native_complete=False, retained=True)
        with patch.object(self.runner, 'media', return_value=after), \
                patch('workflow.runner.trial_parsers', return_value=(None, lambda t:dict(version=1), lambda *a, **k:True)):
            with self.assertRaisesRegex(ValueError, 'frozen wire version'):
                self.runner.collect('candidate')
        self.assertEqual(self.state()['phase'], 'BOOT_REQUESTED')
        with patch.object(self.runner, 'media', return_value=after), \
                patch('workflow.runner.trial_parsers', return_value=(None, parser, lambda *a, **k: True)):
            self.runner.collect('candidate')
        self.assertEqual(self.state()['phase'], 'COLLECTED')
        report = json.loads((directory/'report.json').read_text())
        self.assertEqual(report['status'], 'COLLECTED_INCOMPLETE_REVIEW_REQUIRED')
        self.assertFalse(report['native_complete'])
        proof = self.root/'proof.json'
        atomic(proof, dict(native_acceptance=True, return_integrity='PASS', status='FORGED',
            release=state['release'], runtime_source=state['source'], session=session))
        with self.assertRaisesRegex(ValueError, 'Incomplete native chain'):
            self.runner.close('candidate', proof)
        self.runner.close('candidate', reject=True)
        self.assertEqual(self.state()['phase'], 'REJECTED')

    def test_return_without_exit_never_becomes_a_pass(self):
        directory, _, state = self.runner.load('candidate')
        state.update(phase='BOOT_REQUESTED', host_boot_id='old', release='1234567890abcdef')
        self.runner.save(directory, state)
        returned = directory / 'returned'
        returned.mkdir()
        atomic(returned / 'parsed.json', dict(report=dict(sessions=[dict(session='new', releases=[state['release']], exit_snapshot_present=False)])))
        with patch.object(self.runner, 'media', return_value=dict(new_sessions=['new'])):
            with self.assertRaisesRegex(ValueError, 'Missing matching release/exit'):
                self.runner.collect('candidate')
        self.assertEqual(self.state()['status'], 'FAILED_OR_INCOMPLETE')
        self.assertEqual(self.state()['phase'], 'BOOT_REQUESTED')


class MediaTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.dest = self.root / 'returned'
        self.mount = self.dest / 'volume-ro'
        self.logs = self.mount / 'srvros-work/Logs'
        self.logs.mkdir(parents=True)
        (self.mount / 'srvros-work/.workspace').write_text('SRVROS-WORKSPACE-V1 aabbccdd\n')
        self.device = self.root / 'device'
        self.device.write_bytes(bytes(24*512))
        self.config = dict(volume='aabbccdd', repo=str(self.root))

    def snapshot(self, before, parser, max_log_bytes=65536):
        from workflow.media import snapshot
        with patch('workflow.media.unmounted_partition', return_value=str(self.device)), \
                patch('workflow.media.Reader'), patch('workflow.media.stat.S_ISBLK', return_value=True), \
                patch('workflow.media.subprocess.run') as run, patch('workflow.media.collect', side_effect=parser):
            try:
                return snapshot(self.config, self.dest, before=before,max_log_bytes=max_log_bytes)
            finally:
                self.calls = [call.args[0] for call in run.call_args_list]

    def prepared(self):
        from workflow.media import tree
        import hashlib
        before = dict(sessions=[], files=tree(self.mount), boot_sha256=hashlib.sha256(self.device.read_bytes()).hexdigest())
        fresh = self.logs / 'session-0123456789abcdef-00'
        fresh.mkdir()
        (fresh / 'exit.log').write_text('raw before parser')
        return before, fresh

    def test_parser_failure_preserves_raw_first_and_always_unmounts(self):
        before, fresh = self.prepared()
        def parser(*args):
            self.assertEqual((self.dest / ('raw-' + fresh.name) / 'exit.log').read_bytes(), (fresh / 'exit.log').read_bytes())
            raise ValueError('bad native packet')
        with self.assertRaisesRegex(ValueError, 'bad native packet'):
            self.snapshot(before, parser)
        self.assertEqual(self.calls[-1], ['umount', str(self.mount)])
        self.assertTrue((self.dest / 'raw.json').exists())
        self.assertFalse((self.dest / 'offline.json').exists())

    def test_changed_prior_file_is_failure_even_when_new_logs_exist(self):
        before, fresh = self.prepared()
        (self.mount / 'srvros-work/.workspace').write_text('SRVROS-WORKSPACE-V1 aabbccdd\n')
        protected = self.mount / 'document'
        protected.write_text('changed')
        before['files']['document'] = dict(bytes=3, sha256='old')
        with self.assertRaisesRegex(ValueError, 'Prior USB files changed'):
            self.snapshot(before, lambda *args: self.fail('parser must not run'))
        self.assertTrue((self.dest / ('raw-' + fresh.name) / 'exit.log').exists())
        self.assertEqual(self.calls[-1], ['umount', str(self.mount)])

    def test_dirty_return_still_saves_raw_before_refusal(self):
        before, fresh = self.prepared()
        dirty = bytearray(self.device.read_bytes())
        dirty[106] = 2
        self.device.write_bytes(dirty)
        with self.assertRaisesRegex(ValueError, 'raw evidence preserved'):
            self.snapshot(before, lambda *args: self.fail('parser must not run'))
        self.assertTrue((self.dest / ('raw-' + fresh.name) / 'exit.log').exists())
        self.assertEqual(self.calls[-1], ['umount', str(self.mount)])

    def test_success_repeats_all_offline_checks(self):
        before, _ = self.prepared()
        result = self.snapshot(before, lambda *args: (self.dest / 'parsed', dict(sessions=[])))
        self.assertEqual(len(result['new_sessions']), 1)
        self.assertEqual(sum('tools/check_exfat_allocation.py' in argv for argv in self.calls), 3)
        self.assertTrue(any(argv[0] == 'fsck.exfat' and '-n' in argv for argv in self.calls))
        self.assertEqual(json.loads((self.dest / 'offline.json').read_text())['status'], 'PASS')

    def test_provider_bound_preserves_full_raw_before_parsing(self):
        before,fresh=self.prepared()
        payload=b'x'*98303+b'\n';(fresh/'exit.log').write_bytes(payload)
        def parser(*args):
            self.assertEqual((self.dest/('raw-'+fresh.name)/'exit.log').read_bytes(),payload)
            return self.dest/'parsed',dict(sessions=[])
        self.assertEqual(len(self.snapshot(before,parser,98304)['new_sessions']),1)

    def test_default_and_provider_bounds_remain_strict(self):
        before,fresh=self.prepared()
        for cap in (65536,98304):
            (fresh/'exit.log').write_bytes(b'x'*(cap+1))
            with self.assertRaisesRegex(ValueError,'oversized'):
                self.snapshot(before,lambda *args:self.fail('oversized raw must not be parsed'),cap)
            self.assertEqual(self.calls[-1],['umount',str(self.mount)])

    def test_durable_copy_refuses_changed_raw_instead_of_overwriting(self):
        from workflow.media import durable_copy
        source, dest = self.root / 'source', self.root / 'bank'
        source.mkdir()
        (source / 'log').write_text('original')
        durable_copy(source, dest)
        (source / 'log').write_text('changed')
        with self.assertRaisesRegex(ValueError, 'differs'):
            durable_copy(source, dest)
        self.assertEqual((dest / 'log').read_text(), 'original')

    def test_vm_image_archive_is_verified_before_original_removal(self):
        from workflow.artifacts import archive_image, unpacked_digest
        image = self.root / 'scratch.exfat'
        image.write_bytes(b'vm image' * 1024)
        expected = sha(image)
        receipt = archive_image(image)
        self.assertFalse(image.exists())
        self.assertTrue(receipt['roundtrip_verified'])
        self.assertEqual(unpacked_digest(self.root / receipt['archive']), expected)
        image.write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'archive differs'):
            archive_image(image)
        self.assertEqual(image.read_bytes(), b'changed')


class AutomountTests(unittest.TestCase):
    def inventory(self, mounted=True, duplicate=False):
        node = dict(path='/dev/fixture1', type='part', fstype='exfat', uuid='AABB-CCDD',
                    mountpoints=['/media/fixture'] if mounted else [])
        return dict(blockdevices=[dict(path='/dev/fixture', type='disk', tran='usb',
                    children=[node, dict(node, path='/dev/other1')] if duplicate else [node])])

    def test_unique_matching_automount_is_normally_unmounted_and_rechecked(self):
        from workflow.media import unmounted_partition
        with patch('workflow.media.inventory', side_effect=[self.inventory(), self.inventory(False)]), \
                patch('workflow.media.subprocess.run') as run:
            self.assertEqual(unmounted_partition('aabbccdd'), '/dev/fixture1')
        run.assert_called_once_with(['umount', '/dev/fixture1'], check=True, timeout=30)

    def test_duplicate_uuid_or_busy_mount_is_never_forced(self):
        from workflow.media import unmounted_partition
        with patch('workflow.media.inventory', return_value=self.inventory(duplicate=True)), \
                patch('workflow.media.subprocess.run') as run:
            with self.assertRaises(ValueError):
                unmounted_partition('aabbccdd')
            run.assert_not_called()
        with patch('workflow.media.inventory', return_value=self.inventory()), \
                patch('workflow.media.subprocess.run', side_effect=subprocess.CalledProcessError(32, ['umount'])) as run:
            with self.assertRaises(subprocess.CalledProcessError):
                unmounted_partition('aabbccdd')
        self.assertEqual(run.call_count, 1)


if __name__ == '__main__':
    unittest.main()
