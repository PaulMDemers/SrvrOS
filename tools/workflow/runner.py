"""Manifest-driven trial orchestration. Host caches never grant native acceptance."""
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

from hardware_boot import verify, RELEASE_RE
from hardware_test import check_ubuntu_default
from intel_source_audit import audit_recipe, native_audit_result, verify_audit_checkpoint
from workflow.store import Cache, atomic, encoded, files, sha, fsync_dir

PHASES = ('NEW', 'VALIDATED', 'STAGED', 'QUALIFIED', 'INSTALLED', 'ARMED', 'BOOT_REQUESTED', 'COLLECTED')
TERMINAL = ('ACCEPTED', 'REJECTED', 'CANCELLED', 'IMPORTED', 'DIAGNOSED')
INPUTS = ['Makefile', 'tools/**/*.py', 'tools/**/*.c', 'tools/**/*.h', 'tools/**/*.inc', 'tools/fixtures/**/*.json',
          'kernel/**/*.c', 'kernel/**/*.h', 'kernel/**/*.inc', 'shared/**/*.h',
          'kernel/**/*.S', 'kernel/**/*.ld', 'build/limine-protocol/include/*.h']
# Resolve the actual protocol location below; no reliance on a sibling host layout.


def now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def command(argv, cwd=None):
    return subprocess.check_output(argv, cwd=cwd, text=True).strip()


def boot_id():
    return Path('/proc/sys/kernel/random/boot_id').read_text().strip()


def code_inputs(repo):
    # Git tracked code plus initialized submodule contents are included. Local
    # untracked code is refused by frozen(), and glob membership is hashed too.
    import glob
    return [pattern for pattern in INPUTS if glob.glob(str(Path(repo) / pattern), recursive=True)]


PROFILES = {'intel-stolen-v48':'--intel-stolen-capture','intel-stolen-v47':'--intel-stolen-capture','intel-bcs-context-v46':'--intel-bcs-context','intel-gt-arena-v45':'--intel-gt-arena','intel-gt-memory-v44':'--intel-gt-memory','intel-gt-practical-v43':'--intel-gt-bootstrap-practical','intel-gt-seconds-v42':'--intel-gt-bootstrap-seconds','intel-gt-bootstrap-v41':'--intel-gt-bootstrap','intel-gt-bootstrap-v40':'--intel-gt-bootstrap','intel-gt-bootstrap-v39':'--intel-gt-bootstrap','intel-source-audit-v35': '--intel-source-audit', 'intel-vtd-drain-v36': '--intel-vtd-drain', 'intel-ggtt-invalidate-v36':'--intel-ggtt-invalidate','intel-command-memory-v37':'--intel-command-memory','intel-command-ownership-v38':'--intel-command-ownership'}
VTD_FIXTURE = 'build/device-fixtures/macbookair7-2-dmar.bin'


def trial_parsers(name):
    def exact_bootstrap(text,version):
        from intel_gt_bootstrap import bootstrap_result
        result=bootstrap_result(text,native=True)
        if not result or result['version']!=version:raise ValueError('Full-GT profile requires exact wire'+str(version))
        return result
    if name in ('intel-stolen-v47','intel-stolen-v48'):
        from intel_stolen_scanout import stolen_recipe,stolen_result,verify_stolen_checkpoint
        wire=2 if name=='intel-stolen-v48' else 1
        return stolen_recipe,lambda text:stolen_result(text,native=True,required_version=wire),lambda pre,text,**kw:verify_stolen_checkpoint(pre,text,required_version=wire,**kw)
    if name=='intel-bcs-context-v46':
        from intel_gt_bootstrap import bootstrap_recipe,verify_bootstrap_checkpoint
        return lambda cmd:bootstrap_recipe(cmd,practical=True,memory=True,arena=True,context=True),lambda text:exact_bootstrap(text,8),verify_bootstrap_checkpoint
    if name=='intel-gt-arena-v45':
        from intel_gt_bootstrap import bootstrap_recipe,bootstrap_result,verify_bootstrap_checkpoint
        return lambda cmd:bootstrap_recipe(cmd,practical=True,memory=True,arena=True),lambda text:exact_bootstrap(text,7),verify_bootstrap_checkpoint
    if name=='intel-gt-memory-v44':
        from intel_gt_bootstrap import bootstrap_recipe,bootstrap_result,verify_bootstrap_checkpoint
        return lambda cmd:bootstrap_recipe(cmd,practical=True,memory=True),lambda text:exact_bootstrap(text,6),verify_bootstrap_checkpoint
    if name=='intel-gt-practical-v43':
        from intel_gt_bootstrap import bootstrap_recipe,bootstrap_result,verify_bootstrap_checkpoint
        return lambda cmd:bootstrap_recipe(cmd,practical=True),lambda text:bootstrap_result(text,native=True),verify_bootstrap_checkpoint
    if name=='intel-gt-seconds-v42':
        from intel_gt_bootstrap import bootstrap_recipe,bootstrap_result,verify_bootstrap_checkpoint
        return lambda cmd:bootstrap_recipe(cmd,seconds=True),lambda text:bootstrap_result(text,native=True),verify_bootstrap_checkpoint
    if name in ('intel-gt-bootstrap-v39','intel-gt-bootstrap-v40','intel-gt-bootstrap-v41'):
        from intel_gt_bootstrap import bootstrap_recipe,bootstrap_result,verify_bootstrap_checkpoint
        return bootstrap_recipe,lambda text:bootstrap_result(text,native=True),verify_bootstrap_checkpoint
    if name == 'intel-source-audit-v35':
        return audit_recipe, native_audit_result, verify_audit_checkpoint
    if name in ('intel-vtd-drain-v36','intel-ggtt-invalidate-v36','intel-command-memory-v37','intel-command-ownership-v38'):
        from intel_vtd_native import drain_recipe, drain_result, verify_drain_checkpoint
        return drain_recipe, lambda text: drain_result(text, native=True), verify_drain_checkpoint
    raise ValueError('Unsupported exact trial profile: ' + str(name))


def profile(repo, name='intel-source-audit-v35'):
    trial_parsers(name)
    dependencies = code_inputs(repo)
    if name in ('intel-stolen-v47','intel-stolen-v48'):
        from workflow.stolen_scanout import jobs
        return jobs(dependencies)
    if name=='intel-bcs-context-v46':
        from workflow.bcs_context import jobs
        return jobs(dependencies)
    if name=='intel-gt-arena-v45':
        from workflow.gt_arena import jobs
        return jobs(code_inputs(repo))
    if name=='intel-gt-memory-v44':
        from workflow.gt_memory import jobs
        return jobs(dependencies)
    if name=='intel-gt-practical-v43':
        from workflow.gt_practical import jobs
        return jobs(dependencies)
    if name=='intel-gt-seconds-v42':
        from workflow.gt_seconds import jobs
        return jobs(dependencies)
    if name in ('intel-gt-bootstrap-v39','intel-gt-bootstrap-v40','intel-gt-bootstrap-v41'):
        from workflow.gt_bootstrap import jobs
        return jobs(dependencies)
    # All support fixtures are built once. Full qualification still runs every
    # fault case; development runs only two useful methods and is never launchable.
    build = dict(name='fixtures', argv=[sys.executable, 'tools/fixture_compile.py',
                 '--under-workflow-lock', 'build/tests/intel-source-audit-adapter',
                 'build/tests/boot-source-audit', 'build/tests/intel-cache-probe',
                 'build/tests/intel-handoff-probe', 'build/tests/exfat-driver'], cache=False,
                 inputs=dependencies, outputs=['build/tests/intel-source-audit-adapter',
                 'build/tests/boot-source-audit', 'build/tests/intel-cache-probe',
                 'build/tests/intel-handoff-probe', 'build/tests/exfat-driver'], timeout=1200)
    test_dependencies = dependencies + build['outputs']
    fast = dict(name='audit-development', argv=[sys.executable, '-m', 'unittest',
                'test_intel_source_audit.Audit.test_two_original_routes_live_owner_and_exact_raw_vectors',
                'test_intel_source_audit.Audit.test_final_parent_restore_requires_original_completed_audit'],
                env={'PYTHONPATH': 'tools', 'SRVROS_FIXTURE_BATCH': '1'}, inputs=test_dependencies,
                outputs=[], timeout=300)
    full = dict(name='audit-full', argv=[sys.executable, '-m', 'unittest', '-v',
                'test_intel_source_audit'], env={'PYTHONPATH': 'tools', 'SRVROS_FIXTURE_BATCH': '1'},
                inputs=test_dependencies, outputs=[], timeout=1800)
    boot = dict(name='boot-recipe', argv=['build/tests/boot-source-audit'],
                inputs=test_dependencies, outputs=[], timeout=60)
    jobs = dict(development=[build, fast], qualification=[build, full, boot])
    if name in ('intel-vtd-drain-v36','intel-ggtt-invalidate-v36','intel-command-memory-v37','intel-command-ownership-v38'):
        build['argv'].extend(['build/tests/intel-vtd-native', 'build/tests/boot-vtd-drain'])
        build['outputs'].extend(['build/tests/intel-vtd-native', 'build/tests/boot-vtd-drain'])
        extra_inputs = test_dependencies + build['outputs'] + [VTD_FIXTURE]
        native = dict(name='vtd-native', argv=[sys.executable, '-m', 'unittest', '-v', 'test_intel_vtd_native'],
                      env={'PYTHONPATH': 'tools', 'SRVROS_FIXTURE_BATCH': '1', 'SRVROS_VTD_DMAR': VTD_FIXTURE},
                      inputs=extra_inputs, outputs=[], timeout=1800)
        quick_native = dict(native, name='vtd-development', timeout=300,
            argv=[sys.executable, '-m', 'unittest', '-v', 'test_intel_vtd_native.Recipe',
                  'test_intel_vtd_native.Native.test_original_routes_and_pending_operation',
                  'test_intel_vtd_native.Native.test_native_receiver_and_checkpoint_original_binding'])
        jobs['development'].append(quick_native)
        jobs['qualification'].extend([native, dict(name='vtd-core', argv=['make', 'test-intel-vtd-drain'],
             inputs=dependencies, outputs=[], timeout=300),
             dict(name='vtd-boot', argv=['build/tests/boot-vtd-drain'], inputs=extra_inputs, outputs=[], timeout=60),
             ])
        image = dict(name='vtd-image', argv=['make', 'build/intel-vtd-drain/srvros.elf', 'build/initramfs.tar'],
                     inputs=dependencies, outputs=['build/intel-vtd-drain/srvros.elf', 'build/initramfs.tar'],
                     cache=False, timeout=1800)
        jobs['development'].append(image)
        jobs['qualification'].append(image)
        if name in ('intel-ggtt-invalidate-v36','intel-command-memory-v37','intel-command-ownership-v38'):
            build['argv'].extend(['build/tests/intel-ggtt-native','build/tests/intel-ggtt-invalidate','build/tests/intel-ggtt-invalidate-policy'])
            build['outputs'].extend(['build/tests/intel-ggtt-native','build/tests/intel-ggtt-invalidate','build/tests/intel-ggtt-invalidate-policy'])
            image['argv'][1]='build/intel-ggtt-native/srvros.elf'
            image['outputs'][0]='build/intel-ggtt-native/srvros.elf'
            image['argv'].append('build/intel-vtd-drain/srvros.elf')
            image['outputs'].append('build/intel-vtd-drain/srvros.elf')
            ggtt=dict(name='ggtt-native',argv=[sys.executable,'-m','unittest','-v','test_intel_ggtt_native'],
                env={'PYTHONPATH':'tools','SRVROS_FIXTURE_BATCH':'1','SRVROS_VTD_DMAR':VTD_FIXTURE},
                inputs=extra_inputs+build['outputs'],outputs=[],timeout=1800)
            jobs['development'].insert(-1,dict(ggtt,name='ggtt-development',argv=[sys.executable,'-m','unittest','-v','test_intel_ggtt_native.Native.test_complete_original_routes_and_checkpoint']))
            jobs['qualification'].extend([ggtt,dict(name='ggtt-core',argv=['make','test-intel-ggtt-invalidate'],inputs=dependencies,outputs=[],timeout=300)])
        if name in ('intel-command-memory-v37','intel-command-ownership-v38'):
            build['argv'].append('build/tests/intel-command-native')
            build['outputs'].append('build/tests/intel-command-native')
            image['argv'][1]='build/intel-command-memory/srvros.elf'
            image['outputs'][0]='build/intel-command-memory/srvros.elf'
            image['argv'].append('build/intel-ggtt-native/srvros.elf')
            image['outputs'].append('build/intel-ggtt-native/srvros.elf')
            command=dict(name='command-native',argv=[sys.executable,'-m','unittest','-v','test_intel_command_native'],
                env={'PYTHONPATH':'tools','SRVROS_FIXTURE_BATCH':'1','SRVROS_VTD_DMAR':VTD_FIXTURE},
                inputs=extra_inputs+build['outputs'],outputs=[],timeout=1800)
            jobs['development'].insert(-1,dict(command,name='command-development',argv=[sys.executable,'-m','unittest','-v',
                'test_intel_command_native.Native.test_original_routes_and_checkpoint',
                'test_intel_command_native.Native.test_stage_and_image_isolation']))
            jobs['qualification'].append(command)
        if name=='intel-command-ownership-v38':
            build['argv'].extend(['build/tests/intel-command-ownership','build/tests/bootlog-ownership','build/tests/workspace-log-ownership'])
            build['outputs'].extend(['build/tests/intel-command-ownership','build/tests/bootlog-ownership','build/tests/workspace-log-ownership'])
            image['argv'].append('build/intel-command-memory/srvros.elf')
            image['outputs'].append('build/intel-command-memory/srvros.elf')
            image['argv'][1]='build/intel-command-ownership/srvros.elf'
            image['outputs'][0]='build/intel-command-ownership/srvros.elf'
            ownership=dict(name='command-ownership',argv=[sys.executable,'-m','unittest','-v','test_intel_command_ownership'],
                env={'PYTHONPATH':'tools','SRVROS_FIXTURE_BATCH':'1','SRVROS_VTD_DMAR':VTD_FIXTURE},
                inputs=extra_inputs+build['outputs'],outputs=[],timeout=1800)
            jobs['development'].insert(-1,dict(ownership,name='ownership-development',argv=[sys.executable,'-m','unittest','-v',
                'test_intel_command_ownership.Ownership.test_original_routes_and_checkpoint',
                'test_intel_command_ownership.Ownership.test_stage_and_profile_isolation']))
            jobs['qualification'].append(ownership)
            regression=dict(name='ownership-workflow',argv=[sys.executable,'-m','unittest','-v','test_device_workflow','test_hardware_desktop'],
                env={'PYTHONPATH':'tools'},inputs=dependencies,outputs=[],timeout=300)
            jobs['development'].insert(-1,regression)
            jobs['qualification'].extend([regression,dict(name='default-workspace-log',argv=['make','test-workspace-log'],
                inputs=dependencies,outputs=[],timeout=300)])
        # Cheap structural failures must stop before lengthy fault matrices.
        # Full qualification still repeats them; no prior gate is removed.
        structure=dict(native,name='image-structure',timeout=300,
            argv=[sys.executable,'-m','unittest','-v',
                  'test_intel_vtd_drain.Drain.test_native_image_is_explicit_and_old_images_stay_isolated',
                  'test_intel_vtd_native.Recipe','test_hardware_boot'])
        if name in ('intel-ggtt-invalidate-v36','intel-command-memory-v37','intel-command-ownership-v38'):
            structure['inputs']=extra_inputs+build['outputs']
            structure['argv'].extend([
                'test_intel_ggtt_invalidate.Invalidation.test_only_dedicated_combined_image_caller',
                'test_intel_ggtt_native.Native.test_collector_preserves_raw_and_legacy_profile_isolation'])
        if name in ('intel-command-memory-v37','intel-command-ownership-v38'):
            structure['argv'].extend([
                'test_intel_command_native.Native.test_unstarted_owner_is_untouched_by_cleanup',
                'test_intel_command_native.Native.test_mapping_observation_masks_and_restores_incoming_irq_state',
                'test_intel_command_native.Native.test_stage_and_image_isolation',
                'test_intel_command_native.Native.test_collector_keeps_original_raw_and_cpu_only_scope'])
        jobs['development'].insert(1,structure)
        jobs['qualification'].insert(1,structure)
    return jobs


def check_config(config, repo):
    if config.get('schema') != 1 or Path(config['repo']).resolve() != Path(repo).resolve():
        raise ValueError('Wrong unit configuration schema/checkout')
    if not re.fullmatch(r'[A-Za-z0-9,._-]+', config['model']):
        raise ValueError('Invalid model')
    if not re.fullmatch(r'[0-9a-f]{8}', config['volume']):
        raise ValueError('Expected the local compact exFAT UUID')
    for release in [config['recovery'], *config['protected_releases']]:
        if not RELEASE_RE.fullmatch(release):
            raise ValueError('Invalid protected release')
    if config['volume_percent'] != 1:
        raise ValueError('Automated trials require the requested 1% volume')
    if not isinstance(config['privilege'], list) or not config['privilege']:
        raise ValueError('Explicit local privilege command required')


class Runner:
    def __init__(self, repo, store):
        self.repo, self.store = Path(repo).resolve(), Path(store).resolve()
        self.store.mkdir(parents=True, exist_ok=True)
        self.cache = Cache(self.store / 'validation-cache')

    def load(self, run):
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,79}', run):
            raise ValueError('Invalid run name')
        directory = self.store / 'runs' / run
        manifest = json.loads((directory / 'manifest.json').read_text())
        state = json.loads((directory / 'state.json').read_text())
        if sha(directory / 'manifest.json') != state['manifest_sha256']:
            raise ValueError('Run manifest changed')
        check_config(manifest['unit'], self.repo)
        return directory, manifest, state

    def save(self, directory, state):
        state['updated_utc'] = now()
        atomic(directory / 'state.json', state)
        active = dict(run=directory.name, phase=state['phase'], intent=state.get('intent'),
                      status=state.get('status'), updated_utc=state['updated_utc'])
        atomic(self.store / 'active.json', active)
        summary = (f"{directory.name}: {state['phase']} ({state.get('status', 'pending')})\n"
                   f"Source: {state.get('source', 'unknown')}\n"
                   f"Release: {state.get('release', 'not staged')}\n"
                   f"Intent: {state.get('intent') or 'none'}\n"
                   f"Evidence: {directory}\n"
                   "Host/VM completion does not grant native acceptance.\n")
        atomic(self.store / 'current.md', summary.encode())

    def frozen(self, manifest):
        if command(['git', 'rev-parse', 'HEAD'], self.repo) != manifest['source']:
            raise ValueError('Checkout changed from frozen runtime source')
        if command(['git', 'status', '--porcelain', '--untracked-files=normal'], self.repo):
            raise ValueError('Freeze a clean checkout before staging or launch')
        actual = files(self.repo, manifest['inputs'])
        if actual != manifest['input_hashes']:
            raise ValueError('Frozen source/dependency contents changed')
        submodules = command(['git', 'submodule', 'status', '--recursive'], self.repo)
        if submodules != manifest['submodules']:
            raise ValueError('Submodule state changed')

    def create(self, name, config, question, expected, *, jobs=None, trial_profile='intel-source-audit-v35'):
        active = self.store / 'active.json'
        if active.exists():
            previous = json.loads(active.read_text())
            if previous['phase'] not in TERMINAL:
                raise ValueError('Finish or explicitly cancel outstanding run ' + previous['run'])
        check_config(config, self.repo)
        if not question.strip() or not expected.strip():
            raise ValueError('A hardware question and expected result are required')
        trial_parsers(trial_profile)
        if trial_profile=='intel-stolen-v47':
            raise ValueError('v47 is historical; use intel-stolen-v48 for UC admission diagnostics')
        if trial_profile in ('intel-gt-bootstrap-v39','intel-gt-bootstrap-v40'):
            raise ValueError('v39/v40 are historical; use intel-gt-bootstrap-v41 for original GT lease maintenance')
        if trial_profile in ('intel-vtd-drain-v36','intel-ggtt-invalidate-v36','intel-command-memory-v37','intel-command-ownership-v38') and (config['model'] != 'MacBookAir7,2' or not (self.repo / VTD_FIXTURE).is_file()):
            raise ValueError('v36 requires this Air7,2 unit and its captured local DMAR fixture')
        if trial_profile in ('intel-vtd-drain-v36','intel-ggtt-invalidate-v36','intel-command-memory-v37','intel-command-ownership-v38'):
            from intel_vtd_native import _table
            _table((self.repo / VTD_FIXTURE).read_bytes())
        if trial_profile in ('intel-stolen-v48','intel-stolen-v47','intel-gt-bootstrap-v39','intel-gt-bootstrap-v40','intel-gt-bootstrap-v41','intel-gt-seconds-v42','intel-gt-practical-v43','intel-gt-memory-v44','intel-gt-arena-v45','intel-bcs-context-v46') and config['model'] != 'MacBookAir7,2':
            raise ValueError('Full-GT bootstrap requires this independently captured Air7,2 profile')
        jobs = jobs or profile(self.repo, trial_profile)
        for level in ('development', 'qualification'):
            names = set()
            if not jobs.get(level):
                raise ValueError('Missing validation level: ' + level)
            for job in jobs[level]:
                if (not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', job['name']) or job['name'] in names
                        or not 0 < job['timeout'] <= 3600):
                    raise ValueError('Invalid/duplicate job name or timeout')
                names.add(job['name'])
                if not job['inputs']:
                    raise ValueError('Every validation job needs explicit dependencies')
        inputs = code_inputs(self.repo)
        if trial_profile in ('intel-vtd-drain-v36','intel-ggtt-invalidate-v36','intel-command-memory-v37','intel-command-ownership-v38'):
            inputs.append(VTD_FIXTURE)
        manifest = dict(schema=1, unit=config, profile=trial_profile, wire_version=2 if trial_profile=='intel-stolen-v48' else 1 if trial_profile=='intel-stolen-v47' else 8 if trial_profile=='intel-bcs-context-v46' else 7 if trial_profile=='intel-gt-arena-v45' else 6 if trial_profile=='intel-gt-memory-v44' else 5 if trial_profile=='intel-gt-practical-v43' else 4 if trial_profile=='intel-gt-seconds-v42' else 3 if trial_profile=='intel-gt-bootstrap-v41' else 2 if trial_profile=='intel-gt-bootstrap-v40' else 1 if trial_profile=='intel-gt-bootstrap-v39' else 6 if trial_profile=='intel-command-ownership-v38' else 5 if trial_profile=='intel-command-memory-v37' else 4 if trial_profile=='intel-ggtt-invalidate-v36' else 3 if trial_profile=='intel-vtd-drain-v36' else None, question=question,
                        expected=expected, source=command(['git', 'rev-parse', 'HEAD'], self.repo),
                        branch=command(['git', 'branch', '--show-current'], self.repo),
                        submodules=command(['git', 'submodule', 'status', '--recursive'], self.repo),
                        inputs=inputs, input_hashes=files(self.repo, inputs),
                        jobs=jobs, created_utc=now(),
                        hardware_acceptance='Review matching raw native chain, original lifetime retirement and offline integrity; no GPU grant inferred')
        self.frozen(manifest)
        directory = self.store / 'runs' / name
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,79}', name):
            raise ValueError('Invalid run name')
        directory.mkdir(parents=True, exist_ok=False)
        atomic(directory / 'manifest.json', manifest)
        self.save(directory, dict(phase='NEW', manifest_sha256=sha(directory / 'manifest.json'),
                                source=manifest['source'], status='pending', events=[]))
        return name

    def begin(self, directory, state, action):
        state['intent'] = action
        state['events'].append(dict(action=action, at=now(), status='STARTED'))
        self.save(directory, state)  # side-effect intent is durable before any operation

    def finish(self, directory, state, phase, **values):
        state.update(values, phase=phase, intent=None, status=values.get('status', 'pending'))
        state['events'].append(dict(phase=phase, at=now(), status='COMPLETE'))
        self.save(directory, state)

    def jobs(self, directory, manifest, state, level):
        receipts = state.setdefault('validation', {})
        for job in manifest['jobs'][level]:
            self.frozen(manifest)
            # Qualification deliberately reruns the complete suite for the frozen
            # candidate. A successful receipt in THIS run makes resume idempotent.
            slot = level + '/' + job['name']
            if slot in receipts:
                saved = receipts[slot]
                log = directory / 'logs' / (level + '-' + job['name'] + '.log')
                if sha(log) != saved['log_sha256'] or (job['outputs'] and files(self.repo, job['outputs']) != saved['outputs']):
                    raise ValueError('Completed validation output changed: ' + slot)
                continue
            result = self.cache.run(job, self.repo, directory / 'logs' / (level + '-' + job['name'] + '.log'),
                                    fresh=level == 'qualification')
            receipts[slot] = result
            self.save(directory, state)

    def privileged(self, manifest, argv, log=None):
        kwargs = dict(cwd=self.repo, check=True, timeout=600)
        if log:
            with Path(log).open('wb') as stream:
                subprocess.run([*manifest['unit']['privilege'], *map(str, argv)], stdout=stream,
                               stderr=subprocess.STDOUT, **kwargs)
        else:
            subprocess.run([*manifest['unit']['privilege'], *map(str, argv)], **kwargs)

    def media(self, directory, manifest, returning=False):
        dest = directory / ('returned' if returning else 'before')
        self.privileged(manifest, [sys.executable, 'tools/device_workflow.py', '--store', self.store,
            '_media', '--run', directory.name, '--returning' if returning else '--before'])
        return json.loads((dest / 'snapshot.json').read_text())

    def grub_text(self, manifest):
        # Ubuntu can protect this one recovery configuration with mode0600.
        # Keep the agent/VM/PipeWire user context; use the approved helper only
        # for the fixed read, then run every original recovery assertion.
        path = Path('/boot/grub/grub.cfg')
        try:
            return path.read_text()
        except PermissionError:
            return subprocess.check_output([*manifest['unit']['privilege'],
                'cat', '--', str(path)], cwd=self.repo, text=True, timeout=30)

    def guard(self, manifest, state, *, installed=False, armed=False):
        unit = manifest['unit']
        if Path('/sys/class/dmi/id/product_name').read_text().strip() != unit['model']:
            raise ValueError('Local hardware model changed')
        if Path('/sys/firmware/efi/fw_platform_size').read_text().strip() != '64':
            raise ValueError('Requires EFI64 Ubuntu recovery')
        if Path(unit['ac_path']).read_text().strip() != '1':
            raise ValueError('Connect AC before an automated trial')
        base = Path('/boot/efi/EFI/srvros')
        boot = json.loads((base / 'state.json').read_text())
        if boot['previous'] != unit['recovery']:
            raise ValueError('Installed recovery changed')
        expected = state.get('release') if installed else state.get('prior_current', boot['current'])
        if boot['current'] != expected:
            raise ValueError('Installed current release changed')
        for release in set([boot['current'], unit['recovery'], *unit['protected_releases']]):
            if verify(base / release, release) != verify(Path(unit['payload_bank']) / release, release):
                raise ValueError('Installed/banked payload mismatch: ' + release)
        text = self.grub_text(manifest)
        check_ubuntu_default(Path('/etc/default/grub').read_text(), text)
        for role, release in (('current', boot['current']), ('previous', unit['recovery'])):
            if 'srvros-' + role not in text or ('/EFI/srvros/' + release + '/loader/BOOTX64.EFI').lower() not in text.lower():
                raise ValueError('GRUB recovery/current target mismatch')
        self.privileged(manifest, ['grub-script-check', '/boot/grub/grub.cfg'])
        output = subprocess.check_output([*unit['privilege'], 'grub-editenv', '/boot/grub/grubenv', 'list'], text=True)
        pending = [line for line in output.splitlines() if line.startswith('next_entry=') and line != 'next_entry=']
        if pending != (['next_entry=srvros-current'] if armed else []):
            raise ValueError('Unexpected GRUB one-shot state')
        return boot

    def stage(self, directory, manifest, state):
        subprocess.run([sys.executable, 'tools/hardware_boot.py', 'stage', '--laptop-probe-volume',
                        manifest['unit']['volume'], PROFILES[manifest['profile']]], cwd=self.repo, check=True)
        release = (self.repo / 'build/hardware-boot/latest').read_text().strip()
        self.adopt_stage(directory, manifest, state, release)

    def candidate(self, state):
        stage = Path(state['stage'])
        if (sha(stage / 'manifest.json') != state['manifest_digest'] or
                verify(stage, state['release']) != verify(Path(state['payload_bank']), state['release'])):
            raise ValueError('Frozen staged/banked payload changed')

    def quiet(self, state):
        subprocess.run(['wpctl', 'set-volume', '@DEFAULT_AUDIO_SINK@', '1%'], check=True)
        if command(['wpctl', 'get-volume', '@DEFAULT_AUDIO_SINK@']) != 'Volume: 0.01':
            raise ValueError('Cannot verify 1% host volume')
        # This profile is silent and its exact recipe forbids enabling HDA.
        if 'srvros.audio=hda' in state['recipe'].split():
            raise ValueError('This automated profile must be silent')

    def adopt_stage(self, directory, manifest, state, release):
        if not RELEASE_RE.fullmatch(release):
            raise ValueError('Invalid exact release')
        stage = self.repo / 'build/hardware-boot' / release
        payload = verify(stage, release)
        if payload['git_head'] != manifest['source'] or payload['git_dirty']:
            raise ValueError('Staged payload is not the frozen source')
        recipe = next(line.split('cmdline:', 1)[1].strip() for line in (stage / 'limine.conf').read_text().splitlines() if 'cmdline:' in line)
        recipe_parser, _, _ = trial_parsers(manifest['profile'])
        if recipe_parser(recipe) != dict(release=release, volume=manifest['unit']['volume']):
            raise ValueError('Staged recipe does not match the exact run/USB')
        bank = Path(manifest['unit']['payload_bank']) / release
        if not bank.exists():
            from workflow.media import durable_copy
            bank.parent.mkdir(parents=True, exist_ok=True)
            durable_copy(stage, bank)
        if verify(bank, release) != payload:
            raise ValueError('Payload bank mismatch')
        state.update(release=release, stage=str(stage), manifest_digest=sha(stage / 'manifest.json'), recipe=recipe,
                     payload_bank=str(bank))

    def vm(self, directory, state):
        for name, extra in (('save-return', []), ('missing-workspace', ['--probe-missing-workspace'])):
            receipt = directory / ('vm-' + name + '.json')
            if receipt.is_file():
                saved = json.loads(receipt.read_text())
                from workflow.media import tree
                if (saved['release'] != state['release'] or sha(Path(saved['log'])) != saved['log_sha256']
                        or tree(Path(saved['output'])) != saved['artifacts']):
                    raise ValueError('Exact-stage VM receipt changed')
                continue
            attempt = len(list(directory.glob('vm-' + name + '-*')))
            output = directory / ('vm-' + name + '-' + str(attempt))
            log = directory / ('vm-' + name + '-' + str(attempt) + '.log')
            with log.open('wb') as stream:
                subprocess.run([sys.executable, 'tools/hardware_boot_smoke.py', '--release', state['release'],
                    '--output-dir', str(output), '--probe-check', '--workspace-table', 'mbr', *extra], cwd=self.repo,
                    stdout=stream, stderr=subprocess.STDOUT, check=True, timeout=360)
            from workflow.artifacts import archive_image
            from workflow.media import tree
            for image in output.glob('*.exfat'):
                archive_image(image)
            atomic(receipt, dict(release=state['release'], log=str(log), log_sha256=sha(log),
                                output=str(output), artifacts=tree(output),
                                manifest_sha256=state['manifest_digest'], native_acceptance=False))
        atomic(directory / 'vm.json', dict(status='PASS', release=state['release'],
               manifest_sha256=state['manifest_digest'], native_acceptance=False))

    def advance(self, run, through='QUALIFIED'):
        directory, manifest, state = self.load(run)
        if through not in PHASES[1:7]:
            raise ValueError('advance target must be VALIDATED through BOOT_REQUESTED')
        if state['phase'] not in PHASES:
            raise ValueError('Run is already closed')
        if state.get('intent') and state['intent'] not in ('validate', 'qualify'):
            raise ValueError('Interrupted side effect needs explicit reconcile: ' + state['intent'])
        while PHASES.index(state['phase']) < PHASES.index(through):
            self.frozen(manifest)
            phase = state['phase']
            if phase not in ('NEW', 'VALIDATED'):
                self.candidate(state)
            action = dict(NEW='validate', VALIDATED='stage', STAGED='qualify', QUALIFIED='install',
                          INSTALLED='arm', ARMED='reboot')[phase]
            self.begin(directory, state, action)
            try:
                if action == 'validate':
                    self.jobs(directory, manifest, state, 'development')
                    self.finish(directory, state, 'VALIDATED')
                elif action == 'stage':
                    self.stage(directory, manifest, state)
                    self.finish(directory, state, 'STAGED')
                elif action == 'qualify':
                    # Fail exact-stage VM/setup errors before long host matrices;
                    # verify the same receipts again before qualification ends.
                    self.vm(directory, state)
                    self.jobs(directory, manifest, state, 'qualification')
                    self.vm(directory, state)
                    self.finish(directory, state, 'QUALIFIED')
                elif action == 'install':
                    boot = self.guard(manifest, state)
                    state.update(host_boot_id=boot_id(), prior_current=boot['current'])
                    self.save(directory, state)
                    self.media(directory, manifest)
                    self.privileged(manifest, [sys.executable, 'tools/hardware_boot.py', 'install',
                                             '--release', state['release'], '--keep-previous'], directory / 'install.log')
                    self.guard(manifest, state, installed=True)
                    self.finish(directory, state, 'INSTALLED')
                elif action == 'arm':
                    self.guard(manifest, state, installed=True)
                    self.privileged(manifest, ['grub-reboot', 'srvros-current'])
                    self.guard(manifest, state, installed=True, armed=True)
                    self.finish(directory, state, 'ARMED')
                else:
                    if boot_id() != state['host_boot_id']:
                        raise ValueError('Host boot changed before reboot request')
                    self.guard(manifest, state, installed=True, armed=True)
                    self.media(directory, manifest)  # fresh offline checks and file/session baseline
                    self.quiet(state)
                    self.finish(directory, state, 'BOOT_REQUESTED', status='awaiting native return')
                    os.sync()
                    self.privileged(manifest, ['systemctl', 'reboot', '-i', '--no-block'])
            except BaseException as error:
                state.update(status='FAILED', error=str(error))
                self.save(directory, state)
                if action in ('arm', 'reboot'):
                    self.privileged(manifest, ['grub-editenv', '/boot/grub/grubenv', 'unset', 'next_entry'])
                    state.update(intent='reconcile', status='FAILED one-shot cleared')
                    self.save(directory, state)
                raise
        return state

    def collect(self, run):
        directory, manifest, state = self.load(run)
        if state['phase'] == 'COLLECTED':
            return state
        if state['phase'] != 'BOOT_REQUESTED' or boot_id() == state['host_boot_id']:
            raise ValueError('No matching physical return to collect')
        # Collection can survive docs-only commits, but code changes need an explicit
        # reviewed migration. Startup must never execute a newly edited collector.
        if files(self.repo, manifest['inputs']) != manifest['input_hashes']:
            raise ValueError('Collector/source changed; archive raw evidence and review migration')
        self.begin(directory, state, 'collect')
        try:
            after = self.media(directory, manifest, returning=True)
            if len(after['new_sessions']) != 1:
                raise ValueError('Expected one new session; raw evidence preserved')
            session = after['new_sessions'][0]
            parsed = json.loads((directory / 'returned/parsed.json').read_text())
            row, = [row for row in parsed['report']['sessions'] if row['session'] == session]
            if row['releases'] != [state['release']] or not row['exit_snapshot_present']:
                raise ValueError('Missing matching release/exit marker; not a pass')
            logs = directory / 'returned' / ('raw-' + session)
            if set(after['raw'][session]) != {'boot.log', 'session-a.log', 'exit.log'}:
                raise ValueError('Unexpected raw files for the frozen v35 profile')
            before = json.loads((directory / 'before/snapshot.json').read_text())
            added = set(after['files']) - set(before['files'])
            if added != {f'srvros-work/Logs/{session}/{name}' for name in after['raw'][session]}:
                raise ValueError('Unexpected new USB files outside matching session')
            for name in after['raw'][session]:
                options = [line for line in (logs / name).read_text().splitlines()
                           if line.startswith('laptop-capture: boot options=')]
                if options != ['laptop-capture: boot options=' + state['recipe']]:
                    raise ValueError('Raw session recipe differs from the frozen stage')
            text, pre = (logs / 'exit.log').read_text(), (logs / 'session-a.log').read_text()
            _, native_parser, verify_checkpoint = trial_parsers(manifest['profile'])
            if not verify_checkpoint(pre, text, native=True):
                raise ValueError('Native checkpoint mismatch')
            native = native_parser(text)
            if manifest.get('wire_version') is not None and native.get('version') != manifest['wire_version']:
                raise ValueError('Native receipt differs from frozen wire version')
            native_complete = bool(native['complete'] and native['native_complete'])
            diagnostic_complete = bool(((manifest['profile'] in ('intel-stolen-v47','intel-stolen-v48') and native.get('diagnostic_stolen')) or (manifest['profile']=='intel-bcs-context-v46' and native.get('diagnostic_context')) or (manifest['profile']=='intel-gt-arena-v45' and native.get('diagnostic_arena')) or (manifest['profile']=='intel-gt-memory-v44' and native.get('diagnostic_memory')) or (manifest['profile']=='intel-gt-seconds-v42' and native.get('diagnostic_seconds')) or (manifest['profile']=='intel-gt-practical-v43' and native.get('diagnostic_practical'))) and native.get('diagnostic_complete'))
            self.guard(manifest, state, installed=True, armed=False)
            report = dict(status='COLLECTED_DIAGNOSTIC_REVIEW_REQUIRED' if diagnostic_complete else 'COLLECTED_REVIEW_REQUIRED' if native_complete else 'COLLECTED_INCOMPLETE_REVIEW_REQUIRED', diagnostic_complete=diagnostic_complete,native_complete=native_complete, source=manifest['source'], release=state['release'],
                          session=session, parser_source=command(['git', 'rev-parse', 'HEAD'], self.repo),
                          raw_manifest=after['raw'][session], native=native, offline_integrity='PASS',
                          native_acceptance=False, physical_tearfree=False)
            atomic(directory / 'report.json', report)
            status='review read-only census; native ownership withheld' if manifest['profile'] in ('intel-stolen-v47','intel-stolen-v48') and diagnostic_complete else 'review wide-policy diagnostic; original timing acceptance withheld' if diagnostic_complete else 'review full retirement and scope before acceptance' if native_complete else 'incomplete native chain; review retained failure'
            if manifest['profile'] in ('intel-stolen-v47','intel-stolen-v48') and not diagnostic_complete:
                status='census refused at '+native['failure_stage']+'; incomplete, no ownership acceptance'
                report['failure_stage']=native['failure_stage'];atomic(directory/'report.json',report)
            self.finish(directory, state, 'COLLECTED', status=status, session=session)
        except BaseException as error:
            state.update(status='FAILED_OR_INCOMPLETE', error=str(error))
            self.save(directory, state)
            raise
        return state

    def close(self, run, proof=None, reject=False):
        directory, manifest, state = self.load(run)
        if state['phase'] != 'COLLECTED':
            raise ValueError('Only a collected run can be closed')
        if reject:
            self.finish(directory, state, 'REJECTED', status='reviewed failure; raw evidence retained')
            return
        report = json.loads((directory / 'report.json').read_text())
        if manifest['profile'] in ('intel-stolen-v48','intel-stolen-v47','intel-gt-seconds-v42','intel-gt-practical-v43','intel-gt-memory-v44','intel-gt-arena-v45','intel-bcs-context-v46'):
            raise ValueError('Diagnostic profiles cannot grant native ownership; use diagnose')
        if not report.get('native_complete', report.get('native', {}).get('native_complete', False)):
            raise ValueError('Incomplete native chain cannot be accepted')
        evidence = json.loads(Path(proof).read_text())
        if not evidence.get('native_acceptance') or evidence.get('return_integrity') != 'PASS':
            raise ValueError('A reviewed full native acceptance/integrity proof is required')
        if (evidence.get('release'), evidence.get('runtime_source', evidence.get('source')), evidence.get('session')) != (
                state['release'], manifest['source'], state['session']):
            raise ValueError('Acceptance proof is for another run')
        atomic(directory / 'acceptance.json', evidence)
        report = json.loads((directory / 'report.json').read_text()) if (directory / 'report.json').exists() else {}
        report.update(acceptance_status=evidence['status'], native_acceptance=True,
                      acceptance_sha256=sha(directory / 'acceptance.json'), scope=evidence.get('scope'))
        atomic(directory / 'report.json', report)
        self.finish(directory, state, 'ACCEPTED', status=evidence['status'], proof_sha256=sha(directory / 'acceptance.json'))

    def diagnose(self,run):
        directory,manifest,state=self.load(run)
        if state['phase']!='COLLECTED' or manifest['profile'] not in ('intel-stolen-v48','intel-stolen-v47','intel-gt-seconds-v42','intel-gt-practical-v43','intel-gt-memory-v44','intel-gt-arena-v45','intel-bcs-context-v46'):
            raise ValueError('Only collected explicit diagnostics may be diagnosed')
        report=json.loads((directory/'report.json').read_text())
        if not report.get('diagnostic_complete') or report.get('native_acceptance') or report.get('offline_integrity')!='PASS':
            raise ValueError('Incomplete or unverified diagnostic cannot be closed successfully')
        logs=directory/'returned'/('raw-'+state['session'])
        for name,item in report['raw_manifest'].items():
            path=logs/name
            if path.stat().st_size!=item['bytes'] or sha(path)!=item['sha256']:raise ValueError('Diagnostic raw changed')
        _,parse,checkpoint=trial_parsers(manifest['profile'])
        text=(logs/'exit.log').read_text();pre=(logs/'session-a.log').read_text()
        if not checkpoint(pre,text,native=True) or parse(text)!=report['native']:raise ValueError('Diagnostic proof changed')
        if (report['source'],report['release'],report['session'])!=(manifest['source'],state['release'],state['session']):raise ValueError('Diagnostic provenance changed')
        self.guard(manifest,state,installed=True,armed=False)
        if manifest['profile'] in ('intel-stolen-v47','intel-stolen-v48'):
            record=dict(status='READ_ONLY_LAYOUT_DIAGNOSTIC_COMPLETE',source=manifest['source'],release=state['release'],session=state['session'],
                        elapsed_us=[f['end']-f['begin'] for f in report['native']['rows']['facts']],
                        native_acceptance=False,hardware_ownership=False,physical_tearfree=False,return_integrity='PASS')
            atomic(directory/'diagnostic.json',record)
            report.update(status=record['status'],native_acceptance=False)
            atomic(directory/'report.json',report)
            self.finish(directory,state,'DIAGNOSED',status='read-only layout census complete; ownership and tear-free remain pending',diagnostic_sha256=sha(directory/'diagnostic.json'))
            return record
        native=report['native'];s=native['summary'];b=native['binding'];w=native['entry']['wake']['summary']
        t=native['timing']
        elapsed=dict(prepare=s['end']-s['begin'],gt_phase=native['maintain']['end']-native['maintain']['phase'],gt=t['gt_end']-b['gt'],hda=t['hda_end']-b['hda'],outer=t['outer_end']-b['outer'])
        if native.get('diagnostic_memory'):
            elapsed['gt_phase']=max(elapsed['gt_phase'],*(m['end']-m['phase'] for m in native['memory']['maintain']))
        from intel_gt_bootstrap import ORIGINAL
        record=dict(status='DIAGNOSTIC_COMPLETE',source=manifest['source'],release=state['release'],session=state['session'],
                    policy={k:t[k] for k in ORIGINAL},elapsed_us=elapsed,closing_us=dict(guard=native['retire']['guard_end']-native['retire']['guard_begin'],borrow_return=native['retire']['return_end']-native['retire']['return_begin'],final_guard=native['retire']['final_end']-native['retire']['final_begin']),within_original={k:elapsed[k]<=ORIGINAL[k] for k in elapsed},
                    native_acceptance=False,original_timing_acceptance=False,physical_tearfree=False,return_integrity='PASS')
        if native.get('diagnostic_arena'):
            c=native['arena']['summary']
            record['cpu_arena_us']=dict(prepare=c['storage_end']-c['storage_begin'],held=c['end']-c['begin'],cleanup=c['close_end']-c['close_begin'])
        atomic(directory/'diagnostic.json',record)
        report.update(status='DIAGNOSTIC_COMPLETE',original_timing_acceptance=False,native_acceptance=False)
        atomic(directory/'report.json',report)
        self.finish(directory,state,'DIAGNOSED',status='wide-policy diagnostic complete; original timing acceptance withheld',diagnostic_sha256=sha(directory/'diagnostic.json'))
        return record

    def reconcile(self, run, action, release=None):
        directory, manifest, state = self.load(run)
        self.frozen(manifest)
        if action == 'stage' and state['phase'] == 'VALIDATED' and state.get('intent') == 'stage':
            self.adopt_stage(directory, manifest, state, release)
            self.finish(directory, state, 'STAGED')
        elif action == 'disarm' and state['phase'] in ('QUALIFIED', 'INSTALLED', 'ARMED', 'BOOT_REQUESTED'):
            if state.get('host_boot_id') and state['host_boot_id'] != boot_id():
                raise ValueError('A reboot occurred; collect before cancelling')
            self.privileged(manifest, ['grub-editenv', '/boot/grub/grubenv', 'unset', 'next_entry'])
            try:
                self.guard(manifest, state, installed=False, armed=False)
                phase = 'QUALIFIED'  # exact prior installation survived; no candidate install occurred
            except ValueError:
                self.guard(manifest, state, installed=True, armed=False)
                phase = 'INSTALLED'
            self.finish(directory, state, phase, status='disarmed; verified installation may be resumed')
        else:
            raise ValueError('No safe automatic reconciliation for this state; inspect durable intent')

    def cancel(self, run):
        directory, manifest, state = self.load(run)
        if state['phase'] not in ('NEW', 'VALIDATED', 'STAGED', 'QUALIFIED') or state.get('intent') in ('install', 'arm', 'reboot'):
            raise ValueError('Installed/possibly booted runs must be reconciled/collected before cancellation')
        self.finish(directory, state, 'CANCELLED', status='cancelled before installation; evidence retained')
