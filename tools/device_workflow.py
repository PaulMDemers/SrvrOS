#!/usr/bin/env python3
"""Prepare, qualify and resume a device trial from one durable manifest.

Startup resume collects only. It never installs, arms or requests a reboot.
Local settings and evidence live outside build/ and are never committed.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

from workflow.runner import Runner, TERMINAL, check_config, boot_id, now, command
from workflow.store import atomic, lock, sha, gc

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_STORE = Path.home() / '.local/share/srvros/device-workflow'


def import_legacy(runner, checkpoint):
    """Replay existing raw bytes; preserve original runtime/release attribution."""
    from hardware_boot import verify
    from intel_source_audit import native_audit_result, verify_audit_checkpoint
    checkpoint = Path(checkpoint).resolve()
    old = json.loads((checkpoint / 'run.json').read_text())
    proof = json.loads((checkpoint / 'returned/return-proof.json').read_text())
    publication = json.loads((checkpoint / 'published-native.json').read_text())
    raw = Path(proof['raw_evidence_bank'])
    raw_manifest = json.loads((raw / 'manifest.json').read_text())
    if not proof.get('native_acceptance') or proof.get('return_integrity') != 'PASS':
        raise ValueError('Legacy run has no reviewed native acceptance/integrity proof')
    for value in (old, proof, publication, raw_manifest):
        if value['release'] != old['release'] or value.get('runtime_source', value.get('source')) != old['runtime_source']:
            raise ValueError('Legacy runtime/release attribution mismatch')
    if publication['session'] != proof['session']:
        raise ValueError('Legacy session mismatch')
    if sha(raw / 'manifest.json') != publication['raw_manifest_sha256']:
        raise ValueError('Legacy raw seal changed')
    physical = Path(publication['physical_bank'])
    if sha(physical / 'manifest.json') != publication['physical_manifest_sha256']:
        raise ValueError('Legacy physical seal changed')
    sealed = json.loads((physical / 'manifest.json').read_text())
    # The old bank's full acceptance proof must match, not just a new assertion.
    name = 'return-proof.json'
    if sha(physical / name) != sealed['entries'][name]['sha256'] or sha(physical / name) != sha(checkpoint / 'returned' / name):
        raise ValueError('Reviewed legacy proof changed')
    for name, value in raw_manifest['entries'].items():
        if Path(name).name != name or sha(raw / name) != value['sha256']:
            raise ValueError('Legacy raw bytes changed')
    native = native_audit_result((raw / 'exit.log').read_text())
    if not native['native_complete'] or not verify_audit_checkpoint((raw / 'session-a.log').read_text(), (raw / 'exit.log').read_text(), native=True):
        raise ValueError('Legacy native chain no longer verifies')
    payload = verify(Path(old['payload_bank']), old['release'])
    if payload['git_head'] != old['runtime_source'] or sha(Path(old['payload_bank']) / 'manifest.json') != old['manifest_sha256']:
        raise ValueError('Legacy payload identity changed')
    active = runner.store / 'active.json'
    if active.exists() and json.loads(active.read_text())['phase'] not in TERMINAL:
        raise ValueError('An active run must be collected/closed first')
    name = 'import-' + old['release']
    directory = runner.store / 'runs' / name
    directory.mkdir(parents=True, exist_ok=True)
    original = dict(status='IMPORTED_EXISTING_ACCEPTANCE', original_run=old,
                    original_proof=proof, publication=publication, imported_utc=now(),
                    imported_by=command(['git', 'rev-parse', 'HEAD'], runner.repo),
                    new_physical_trial=False, raw_bank=str(raw), physical_bank=str(physical))
    atomic(directory / 'import.json', original)
    # Keep the accepted raw bytes and proof outside build even if the original
    # local checkpoint is pruned. Referenced banks remain explicitly pinned.
    from workflow.media import durable_copy
    durable_copy(raw, directory / 'raw')
    atomic(directory / 'report.json', dict(status=original['status'], source=old['runtime_source'],
           release=old['release'], session=proof['session'], new_physical_trial=False,
           native_acceptance=proof['native_acceptance'], scope=proof['scope'],
           physical_tearfree=proof['physical_tearfree'], managed_pages=proof['managed_pages'],
           pinned_banks=[str(raw), str(physical), old['payload_bank']]))
    runner.save(directory, dict(phase='IMPORTED', source=old['runtime_source'], release=old['release'],
                status=original['status'], session=proof['session'], intent=None, events=[]))
    return directory


def service(runner):
    # User service: access is through the already authorized local helper. No
    # arbitrary validation or reboot command is executed on a login/boot.
    def quote(value):
        return '"' + str(value).replace('%', '%%').replace('\\', '\\\\').replace('"', '\\"') + '"'
    path = Path.home() / '.config/systemd/user/srvros-return-collect.service'
    argv = [sys.executable, ROOT / 'tools/device_workflow.py', '--store', runner.store, 'resume']
    content = ('[Unit]\nDescription=Collect a pending SrvrOS hardware return\n'
               '[Service]\nType=oneshot\nWorkingDirectory=' + str(ROOT).replace('%', '%%') + '\nExecStart=' +
               ' '.join(map(quote, argv)) + '\nTimeoutStartSec=900\nUMask=0077\n'
               '[Install]\nWantedBy=default.target\n')
    atomic(path, content.encode())
    subprocess.run(['systemctl', '--user', 'daemon-reload'], check=True)
    subprocess.run(['systemctl', '--user', 'enable', path.name], check=True)
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--store', type=Path, default=DEFAULT_STORE)
    sub = parser.add_subparsers(dest='action', required=True)
    configure = sub.add_parser('configure', help='save unit-local settings; performs no install/reboot')
    configure.add_argument('--uuid', required=True)
    configure.add_argument('--recovery', required=True)
    configure.add_argument('--protected-release', action='append', default=[])
    configure.add_argument('--payload-bank', type=Path, default=Path.home() / '.local/share/srvros/boot-payloads')
    configure.add_argument('--privilege-helper', type=Path, required=True)
    configure.add_argument('--ac-path', default='/sys/class/power_supply/ADP1/online')
    new = sub.add_parser('new')
    new.add_argument('name')
    new.add_argument('--question', required=True)
    new.add_argument('--expected', required=True)
    new.add_argument('--profile', choices=('intel-source-audit-v35', 'intel-vtd-drain-v36'), default='intel-source-audit-v35')
    new.add_argument('--plan', type=Path, help='JSON additional development/qualification jobs; never removes default gates')
    advance = sub.add_parser('advance')
    advance.add_argument('--run', required=True)
    advance.add_argument('--through', choices=('validated', 'staged', 'qualified', 'installed', 'armed', 'boot_requested'), default='qualified')
    for action in ('collect', 'cancel', 'reject'):
        sub.add_parser(action).add_argument('--run', required=True)
    accept = sub.add_parser('accept')
    accept.add_argument('--run', required=True)
    accept.add_argument('--proof', type=Path, required=True)
    reconcile = sub.add_parser('reconcile')
    reconcile.add_argument('--run', required=True)
    reconcile.add_argument('--action', dest='reconcile_action', choices=('stage', 'disarm'), required=True)
    reconcile.add_argument('--release')
    sub.add_parser('resume', help='collect after return only; never starts a new trial')
    sub.add_parser('status')
    sub.add_parser('install-service')
    legacy = sub.add_parser('import-legacy')
    legacy.add_argument('checkpoint', type=Path)
    retention = sub.add_parser('gc', help='dry run by default; only managed disposable cache entries')
    retention.add_argument('--apply', action='store_true')
    retention.add_argument('--keep', type=int, default=10)
    retention.add_argument('--days', type=int, default=7)
    retention.add_argument('--max-mib', type=int, default=512)
    internal = sub.add_parser('_media', help=argparse.SUPPRESS)
    internal.add_argument('--run', required=True)
    mode = internal.add_mutually_exclusive_group(required=True)
    mode.add_argument('--returning', action='store_true')
    mode.add_argument('--before', action='store_true')
    args = parser.parse_args()
    try:
        runner = Runner(ROOT, args.store)
        if args.action == '_media':
            if os.geteuid() != 0:
                raise ValueError('The read-only media step needs the unit privilege helper')
            directory, manifest, state = runner.load(args.run)
            owner = directory.stat()
            if Path('/sys/class/dmi/id/product_name').read_text().strip() != manifest['unit']['model']:
                raise ValueError('Local model mismatch')
            from workflow.media import snapshot, as_owner
            dest = directory / ('returned' if args.returning else 'before')
            try:
                before = json.loads((directory / 'before/snapshot.json').read_text()) if args.returning else None
                snapshot(manifest['unit'], dest, before=before)
            finally:
                if dest.exists():
                    as_owner(dest, owner.st_uid, owner.st_gid)
            return
        with lock(ROOT / 'build/device-workflow.lock'):
            if args.action == 'configure':
                unit = dict(schema=1, repo=str(ROOT), model=Path('/sys/class/dmi/id/product_name').read_text().strip(),
                            volume=args.uuid.replace('-', '').lower(), recovery=args.recovery,
                            protected_releases=sorted(set([args.recovery, *args.protected_release])),
                            payload_bank=str(args.payload_bank.resolve()), volume_percent=1,
                            ac_path=args.ac_path, privilege=[sys.executable, str(args.privilege_helper.resolve())])
                if not args.privilege_helper.is_file():
                    raise ValueError('Privilege helper does not exist')
                check_config(unit, ROOT)
                path = runner.store / 'unit.json'
                if path.exists():
                    raise ValueError('Unit settings already exist; review/edit explicitly')
                atomic(path, unit)
                path.chmod(0o600)
                print(path)
            elif args.action == 'new':
                jobs = None
                if args.plan:
                    from workflow.runner import profile
                    jobs = profile(ROOT, args.profile)
                    extra = json.loads(args.plan.read_text())
                    if set(extra) - {'development', 'qualification'}:
                        raise ValueError('Plan contains unsupported validation levels')
                    for level, added in extra.items():
                        jobs[level].extend(added)
                print(runner.create(args.name, json.loads((runner.store / 'unit.json').read_text()),
                                    args.question, args.expected, jobs=jobs, trial_profile=args.profile))
            elif args.action == 'advance':
                print(json.dumps(runner.advance(args.run, args.through.upper()), indent=2))
            elif args.action == 'collect':
                print(json.dumps(runner.collect(args.run), indent=2))
            elif args.action == 'resume':
                active = runner.store / 'active.json'
                if not active.exists():
                    return
                current = json.loads(active.read_text())
                if current['phase'] != 'BOOT_REQUESTED':
                    return
                _, _, state = runner.load(current['run'])
                if state['host_boot_id'] == boot_id():
                    return
                print(json.dumps(runner.collect(current['run']), indent=2))
            elif args.action in ('accept', 'reject'):
                runner.close(args.run, getattr(args, 'proof', None), args.action == 'reject')
            elif args.action == 'cancel':
                runner.cancel(args.run)
            elif args.action == 'reconcile':
                runner.reconcile(args.run, args.reconcile_action, args.release)
            elif args.action == 'status':
                path = runner.store / 'current.md'
                print(path.read_text() if path.exists() else 'No workflow run recorded.')
            elif args.action == 'install-service':
                print(service(runner))
            elif args.action == 'import-legacy':
                print(import_legacy(runner, args.checkpoint))
            elif args.action == 'gc':
                result = gc(runner.store / 'validation-cache', keep=args.keep, max_age_days=args.days,
                            max_bytes=args.max_mib*1024*1024, apply=args.apply)
                from fixture_compile import DEFAULT_CACHE
                result['fixture_objects'] = gc(DEFAULT_CACHE, keep=args.keep, max_age_days=args.days,
                                               max_bytes=args.max_mib*1024*1024, apply=args.apply) if DEFAULT_CACHE.exists() else {}
                print(json.dumps(result, indent=2))
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        parser.exit(1, f'device-workflow: {error}\n')


if __name__ == '__main__':
    main()
