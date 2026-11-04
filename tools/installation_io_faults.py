"""Host-only deterministic publication fault experiment (no production hooks).

The adapter uses the real verifier and a sparse GPT/FAT test builder, not the
production image writer. The profile driver is a deterministic subprocess stub.
Each operation and each inspection/reconciliation runs in a fresh interpreter.
"""
import argparse
import builtins
import hashlib
import io
import inspect as pyinspect
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import traceback
from unittest import mock
import uuid

import installation_image as image
import profile_bundle as profiles
import profile_image
import source_provenance
from test_installation_image import exfat_fixture
from test_installation_profile import sparse_builder

HERE = Path(__file__).resolve()


def digest(path):
    h = hashlib.sha256()
    with open(path, 'rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def snapshot(root):
    result = {}
    for p in sorted(root.rglob("*")):
        st = p.lstat()
        item = {'mode': stat.S_IFMT(st.st_mode), 'permissions': stat.S_IMODE(st.st_mode),
                'inode': st.st_ino, 'device': st.st_dev, 'links': st.st_nlink, 'size': st.st_size}
        if stat.S_ISREG(st.st_mode): item['sha256'] = digest(p)
        if stat.S_ISLNK(st.st_mode): item['target'] = os.readlink(p)
        result[str(p.relative_to(root))] = item
    return result


def source_snapshot(root):
    # All generated source inputs are root-level regular files. The stage and
    # the optional result file are excluded; do not follow links.
    result = {}
    for p in sorted(root.iterdir()):
        if p.name in ('stage', 'result.json'):
            continue
        info = p.lstat()
        item = {'mode': stat.S_IFMT(info.st_mode), 'permissions': stat.S_IMODE(info.st_mode),
                'inode': info.st_ino, 'device': info.st_dev, 'links': info.st_nlink, 'size': info.st_size}
        if stat.S_ISREG(info.st_mode): item['sha256'] = digest(p)
        if stat.S_ISLNK(info.st_mode): item['target'] = os.readlink(p)
        result[p.name] = item
    return result


class Faults:
    """Wrap real I/O only for paths/fds inside the generated stage."""
    def __init__(self, stage, selected=None, mode=None):
        self.stage = str(stage)
        self.selected, self.mode = selected, mode
        self.events, self.counts, self.fds = [], {}, {}
        self.written = set()
        self.fired = False
        self.open = builtins.open
        self.io_open = io.open
        self.os_open, self.close = os.open, os.close
        self.fsync, self.link, self.unlink = os.fsync, os.link, os.unlink

    def inside(self, path):
        try:
            path = os.path.abspath(os.fspath(path))
            return path == self.stage or path.startswith(self.stage + os.sep)
        except (TypeError, ValueError):
            return False

    def resolve(self, path, dir_fd=None):
        if dir_fd is not None and not os.path.isabs(os.fspath(path)) and dir_fd in self.fds:
            return os.path.join(self.fds[dir_fd], os.fspath(path))
        return path

    def event(self, kind, path, action, prefix=None):
        name = os.path.relpath(os.fspath(path), self.stage)
        parts = name.split(os.sep)
        name = os.sep.join('<profile-temp>' if p.startswith('.profile-image-') else p for p in parts)
        key = (kind, name)
        self.counts[key] = self.counts.get(key, 0) + 1
        label = '%s:%s:%d' % (kind, name, self.counts[key])
        self.events.append(label)
        if label != self.selected:
            return action()
        self.fired = True
        if self.mode == 'before': raise OSError('injected before ' + label)
        if self.mode == 'short':
            if prefix is None: raise ValueError('short mode requires a write')
            return prefix()
        if self.mode == 'prefix':
            if prefix is not None: prefix()
            raise OSError('injected partial ' + label)
        if self.mode == 'after':
            action()
            raise OSError('injected after ' + label)
        raise ValueError('unknown fault mode')

    class Stream:
        def __init__(self, owner, raw, path): self.owner, self.raw, self.path = owner, raw, path
        def __getattr__(self, name): return getattr(self.raw, name)
        def __enter__(self): self.raw.__enter__(); return self
        def __exit__(self, *args):
            fd = self.raw.fileno()
            try: return self.raw.__exit__(*args)
            finally: self.owner.fds.pop(fd, None)
        def close(self):
            fd = self.raw.fileno()
            try: return self.raw.close()
            finally: self.owner.fds.pop(fd, None)
        def write(self, data):
            # A logical file write is sampled once, avoiding json.dump's fragments.
            if self.path in self.owner.written: return self.raw.write(data)
            self.owner.written.add(self.path)
            return self.owner.event('write', self.path, lambda: self.raw.write(data),
                                    lambda: self.raw.write(data[:max(1, len(data)//2)]))
        def flush(self): return self.owner.event('flush', self.path, self.raw.flush)

    def wrapped_open(self, opener, path, *args, **kwargs):
        stream = opener(path, *args, **kwargs)
        if not self.inside(path): return stream
        self.fds[stream.fileno()] = os.path.abspath(os.fspath(path))
        return self.Stream(self, stream, os.path.abspath(os.fspath(path)))

    def enter(self):
        def os_open(path, *args, **kwargs):
            fd = self.os_open(path, *args, **kwargs)
            resolved = self.resolve(path, kwargs.get('dir_fd'))
            if self.inside(resolved): self.fds[fd] = os.path.abspath(os.fspath(resolved))
            return fd
        def close(fd):
            self.fds.pop(fd, None)
            return self.close(fd)
        def sync(fd):
            path = self.fds.get(fd)
            return self.event('fsync', path, lambda: self.fsync(fd)) if path else self.fsync(fd)
        def link(src, dst, *args, **kwargs):
            action = lambda: self.link(src, dst, *args, **kwargs)
            resolved = self.resolve(dst, kwargs.get('dst_dir_fd'))
            return self.event('link', resolved, action) if self.inside(resolved) else action()
        def unlink(path, *args, **kwargs):
            action = lambda: self.unlink(path, *args, **kwargs)
            resolved = self.resolve(path, kwargs.get('dir_fd'))
            return self.event('unlink', resolved, action) if self.inside(resolved) else action()
        self.patches = [mock.patch.object(builtins, 'open', lambda p, *a, **k: self.wrapped_open(self.open, p, *a, **k)),
                        mock.patch.object(io, 'open', lambda p, *a, **k: self.wrapped_open(self.io_open, p, *a, **k)),
                        mock.patch.object(os, 'open', os_open), mock.patch.object(os, 'close', close),
                        mock.patch.object(os, 'fsync', sync), mock.patch.object(os, 'link', link),
                        mock.patch.object(os, 'unlink', unlink)]
        for patch in self.patches: patch.start()
        return self

    def exit(self):
        for patch in reversed(self.patches): patch.stop()


def setup(root, variant):
    root = root.resolve()
    root.mkdir(parents=True, exist_ok=True)
    for name, data in [('kernel', b'\x7fELFkernel'), ('init', b'\x7fELFinit'), ('boot', b'MZefi')]:
        (root / name).write_bytes(data)
    exfat_fixture(root / 'source.exfat')
    driver = root / 'driver'
    driver.write_text('#!/usr/bin/env python3\nimport sys\nwith open(sys.argv[1], "r+b") as f:\n f.seek(-1,2); f.write(b"X")\n')
    driver.chmod(0o755)
    bundle = root / 'bundle.json'
    bundle.write_bytes(profiles.encode_json({'format': profiles.FORMAT, 'schema_version': 1,
        'profile': {'id': str(uuid.uuid4()), 'display_name': 'A'}, 'preferences': {}, 'documents': []}))
    options = {}
    if variant == 'profile':
        options = dict(profile_bundle=bundle, profile_home='/fat/home/alice', profile_alias='alice',
                       profile_uid=1001, profile_gid=1001, profile_driver=driver)
    image.prepare(root / 'stage', root / 'kernel', root / 'boot', root / 'source.exfat', root / 'init',
                  identity_bound=variant != 'legacy', **options)


def build(args):
    # The shared sparse fixture expects explicit GUIDs, including for legacy plans.
    for field in ('disk_guid', 'esp_guid', 'data_guid'):
        if getattr(args, field) is None: setattr(args, field, str(uuid.uuid4()))
    if args.data_name is None: args.data_name = 'srvros-fat'
    sparse_builder(args)


class Stop(Exception): pass


def worker(root, variant, operation, checkpoint, label, mode):
    root = root.resolve()
    setup(root, variant)
    stage = root / 'stage'
    with mock.patch.object(image.usb, 'build_image', side_effect=build):
        if operation == 'reconcile':
            def stop(name):
                if name == checkpoint: raise Stop(name)
            try: image.apply(stage, checkpoint=stop)
            except Stop: pass
            else: raise AssertionError('checkpoint not reached')
        before = snapshot(stage)
        sources = source_snapshot(root)
        faults = Faults(stage, label, mode).enter()
        error = None
        try:
            getattr(image, operation)(stage)
        except Exception:
            error = traceback.format_exc()
        finally:
            faults.exit()
        return {'events': faults.events, 'fired': faults.fired, 'error': error,
                'before': before, 'after': snapshot(stage), 'sources': sources,
                'sources_after': source_snapshot(root)}


def examine(root):
    root = root.resolve()
    stage = root / 'stage'
    sources_before = source_snapshot(root)
    before = snapshot(stage)
    inspection = image.inspect(stage)
    after_inspect = snapshot(stage)
    assert before == after_inspect, 'inspect mutated stage'
    if inspection['can_reconcile']:
        expected_hash = inspection['ready']['image_sha256']
        ready = image.reconcile(stage)
        assert ready['image_sha256'] == expected_hash
        assert image.inspect(stage)['state'] == 'ready'
        once = snapshot(stage)
        assert image.reconcile(stage) == ready
        assert snapshot(stage) == once, 'repeat reconcile mutated stage'
        assert digest(stage / 'image.img') == expected_hash
    else:
        try: image.reconcile(stage)
        except (ValueError, OSError): pass
        else: raise AssertionError('unreconcilable stage accepted')
        assert snapshot(stage) == before, 'refusal mutated stage'
    sources_after = source_snapshot(root)
    assert sources_before == sources_after, 'inspect or reconcile changed source'
    return {'inspection': inspection, 'before': before, 'after': snapshot(stage),
            'sources_before': sources_before, 'sources_after': sources_after}


def assert_ready_barriers(stage, events):
    """Each distinct regular inode and the directory must be synced."""
    names = {}
    for path in sorted(stage.iterdir()):
        st = path.lstat()
        if stat.S_ISREG(st.st_mode): names.setdefault((st.st_dev, st.st_ino), path.name)
    synced = {event.split(':')[1] for event in events if event.startswith('fsync:')}
    assert set(names.values()) <= synced, ('missing inode syncs', set(names.values()) - synced)
    assert '.' in synced, 'missing stage directory sync'


def assert_fault_failed(result):
    assert result['fired'], 'selected fault did not fire'
    assert result['error'], 'injected host I/O error was swallowed'


def mutated_reconcile(old, new):
    source = pyinspect.getsource(image.reconcile)
    assert source.count(old) == 1, 'production source no longer matches mutant location'
    namespace = dict(image.__dict__)
    exec(compile(source.replace(old, new), '<in-memory-reconcile-mutant>', 'exec'), namespace)
    return namespace['reconcile']


def run_mutant(stage, function, label=None):
    recorder = Faults(stage, label, 'before' if label else None).enter()
    error = None
    try: function(stage)
    except Exception: error = traceback.format_exc()
    finally: recorder.exit()
    return {'events': recorder.events, 'fired': recorder.fired, 'error': error}


def controls(root, control):
    root = root.resolve()
    setup(root, 'identity')
    stage = root / 'stage'
    with mock.patch.object(image.usb, 'build_image', side_effect=build):
        image.apply(stage)
    if control == 'barriers':
        before = snapshot(stage)
        recorder = Faults(stage).enter()
        try: image.reconcile(stage)
        finally: recorder.exit()
        assert snapshot(stage) == before
        assert_ready_barriers(stage, recorder.events)
        mutant = mutated_reconcile('        synced = set()\n',
            "        if result['state'] == 'ready':\n            return result['ready']\n        synced = set()\n")
        omitted = run_mutant(stage, mutant)
        assert not omitted['error'], omitted['error']
        try: assert_ready_barriers(stage, omitted['events'])
        except AssertionError: pass
        else: raise AssertionError('omitted-barrier mutant escaped oracle')
        mutant = mutated_reconcile('                    os.fsync(fd)\n',
            '                    try:\n                        os.fsync(fd)\n                    except OSError:\n                        pass\n')
        swallowed = run_mutant(stage, mutant, 'fsync:publication.json:1')
        assert swallowed['fired'], 'swallowed-fsync mutant did not encounter fault'
        try: assert_fault_failed(swallowed)
        except AssertionError: pass
        else: raise AssertionError('swallowed-fsync mutant escaped oracle')
        assert snapshot(stage) == before
        return {'control': control, 'events': recorder.events, 'before': before, 'after': snapshot(stage),
                'omitted_barrier_negative_control': 'detected', 'omitted_mutant': omitted,
                'swallowed_fsync_negative_control': 'detected', 'swallowed_mutant': swallowed}
    if control == 'torn-journal': (stage / 'publication.json').write_bytes(b'{')
    elif control == 'torn-ready': (stage / 'ready.json').write_bytes(b'{')
    elif control == 'unknown': (stage / 'unknown').write_bytes(b'preserve')
    elif control == 'symlink': (stage / 'unknown').symlink_to(root / 'kernel')
    elif control == 'outside-hardlink': os.link(stage / 'image.img', root / 'outside')
    elif control == 'changed-source': (root / 'kernel').write_bytes(b'changed')
    elif control == 'changed-candidate':
        with open(stage / 'image.img', 'r+b') as stream:
            stream.seek(-1, 2); stream.write(b'X')
    else: raise ValueError(control)
    before = snapshot(stage)
    result = image.inspect(stage)
    assert not result['can_reconcile'], (control, result)
    assert snapshot(stage) == before, 'inspect mutated refused stage'
    try: image.reconcile(stage)
    except (ValueError, OSError): pass
    else: raise AssertionError('unsafe state accepted: ' + control)
    after = snapshot(stage)
    assert before == after, 'refusal mutated stage'
    return {'control': control, 'inspection': result, 'before': before, 'after': after}


def run_process(args):
    result = subprocess.run([sys.executable, str(HERE)] + args, text=True, capture_output=True)
    if result.returncode: raise RuntimeError('worker failed: ' + result.stderr + result.stdout)
    return json.loads(result.stdout)


def experiment(output):
    receipt = {'status': 'INCOMPLETE', 'cases': [], 'baselines': [], 'controls': [], 'errors': [],
               'scope': 'host Python I/O; sparse GPT/FAT builder and deterministic profile driver stub',
               'limits': ['No power-loss simulation or kernel/device fault injection; profile subprocess I/O is not intercepted.',
                          'Logical writes sample the first write per file; sparse image sample is the MBR write, and profile temporary payloads are sampled once.',
                          'Short-return injection covers top-level publisher writes; raw short returns in profile adapter temporary files are not qualified.',
                          'Production metadata uses one checked write; the separate production-image smoke samples its first image write.'],
               'code_sha256': {p.name: digest(p) for p in (HERE, Path(__file__).with_name('test_installation_io_failures.py'),
                   Path(image.__file__), Path(image.usb.__file__), Path(profiles.__file__), Path(profile_image.__file__),
                   Path(__file__).with_name('test_installation_image.py'), Path(__file__).with_name('test_installation_profile.py'),
                   Path(source_provenance.__file__))}}
    def save(): Path(output).write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
    save()
    try:
        receipt['source'] = source_provenance.capture(HERE.parent.parent)
        save()
        with tempfile.TemporaryDirectory(prefix='install-io-fault-') as temp:
            temp = Path(temp)
            for name in ('barriers', 'torn-journal', 'torn-ready', 'unknown', 'symlink',
                         'outside-hardlink', 'changed-source', 'changed-candidate'):
                root = temp / 'case'
                receipt['controls'].append(run_process(['control', str(root), name]))
                save()
                shutil.rmtree(root)
            for variant, operation, checkpoint in [('identity', 'apply', '-'), ('profile', 'apply', '-'),
                    ('legacy', 'apply', '-'), ('identity', 'reconcile', 'verified'),
                    ('identity', 'reconcile', 'ready-written'), ('identity', 'reconcile', 'ready-linked'),
                    ('identity', 'reconcile', 'ready-published'),
                    ('profile', 'reconcile', 'verified'), ('profile', 'reconcile', 'ready-written'),
                    ('profile', 'reconcile', 'ready-linked'), ('profile', 'reconcile', 'ready-published')]:
                root = temp / 'case'
                args = ['worker', str(root), variant, operation, checkpoint]
                baseline = run_process(args)
                receipt['baselines'].append({'variant': variant, 'operation': operation,
                                             'checkpoint': checkpoint, 'result': baseline})
                save()
                assert not baseline['error'], baseline['error']
                assert baseline['sources'] == baseline['sources_after'], 'baseline changed source'
                events = [e for e in baseline['events'] if e.startswith(('fsync:', 'link:', 'unlink:', 'write:', 'flush:'))]
                shutil.rmtree(root)
                for label in events:
                    modes = ('before', 'prefix') if label.startswith('write:') else ('before', 'after')
                    if label.split(':')[0:2] in ([
                            'write', 'in-progress.json'], ['write', 'bootstrap.tar'], ['write', 'limine.conf'],
                            ['write', 'image.partial'], ['write', 'publication.json'], ['write', 'ready.partial']):
                        modes += ('short',)
                    for mode in modes:
                        result = run_process(args + [label, mode])
                        case = {'variant': variant, 'operation': operation, 'checkpoint': checkpoint,
                                'label': label, 'mode': mode, 'fault': result}
                        receipt['cases'].append(case)
                        save()  # Preserve the faulty operation's evidence even if a later assertion fails.
                        assert_fault_failed(result)
                        assert result['sources'] == result['sources_after'], 'source changed'
                        case['recovery'] = run_process(['examine', str(root)])
                        save()
                        assert case['recovery']['sources_before'] == result['sources_after'], 'source changed between processes'
                        expected = ('publication.json' in result['after'] and
                                    not label.startswith(('write:publication.json:', 'write:ready.partial:')))
                        case['expected_reconcilable'] = expected
                        save()
                        assert case['recovery']['inspection']['can_reconcile'] == expected, (
                            'unexpected recovery eligibility', variant, operation, checkpoint, label, mode)
                        shutil.rmtree(root)
        receipt['status'] = 'COMPLETE'
    except Exception:
        receipt['errors'].append(traceback.format_exc())
        save()
        raise
    save()
    return receipt


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest='command', required=True)
    run = sub.add_parser('run'); run.add_argument('--output', required=True)
    work = sub.add_parser('worker'); work.add_argument('root', type=Path); work.add_argument('variant'); work.add_argument('operation'); work.add_argument('checkpoint'); work.add_argument('label', nargs='?'); work.add_argument('mode', nargs='?')
    check = sub.add_parser('examine'); check.add_argument('root', type=Path)
    control = sub.add_parser('control'); control.add_argument('root', type=Path); control.add_argument('name')
    args = parser.parse_args()
    if args.command == 'run': experiment(args.output)
    elif args.command == 'worker': print(json.dumps(worker(args.root, args.variant, args.operation, args.checkpoint, args.label, args.mode)))
    elif args.command == 'control': print(json.dumps(controls(args.root, args.name)))
    else: print(json.dumps(examine(args.root)))


if __name__ == '__main__': main()
