"""Bounded ordinary-shell USB workload shared by native preparation and VM rehearsal."""
import re

ROUNDS = 60
SEED = bytes(range(256)) * 4096


def files(run):
    if not re.fullmatch(r'[0-9a-f]{16}', run):
        raise ValueError('Expected a fresh 16-hex run identity')
    stem = 'session-' + run
    # Refuse an existing run before mkdir: SrvrOS mkdir accepts existing directories.
    commands = [
        'cd /work/srvros-work || exit 1',
        f'if test -e {stem}; then echo REFUSED existing session; exit 1; fi',
        f'mkdir {stem} || exit 1',
        f'cd {stem} || exit 1',
        'echo USB session running - keep this Terminal open',
    ]
    for n in range(1, ROUNDS + 1):
        commands += [f'cp ../{stem}.bin payload.tmp || exit 1',
                     'sync || exit 1',
                     'mv payload.tmp payload.bin || exit 1',
                     f'cmp ../{stem}.bin payload.bin || exit 1',
                     f'echo {n} >> rounds.txt || exit 1',
                     'sync || exit 1',
                     f'echo USB round {n}/{ROUNDS}',
                     'sleep 2 || exit 1']
    commands += [f'echo COMPLETE {run} {ROUNDS} > complete.txt || exit 1',
                 'sync || exit 1', 'echo USB SESSION COMPLETE - safe to finish the desktop trial']
    return {stem + '.sh': ('\n'.join(commands) + '\n').encode(), stem + '.bin': SEED}


def verify(read, run):
    fixture = files(run)
    for name, expected in fixture.items():
        if read(name) != expected:
            raise ValueError('Changed workload input: ' + name)
    folder = 'session-' + run + '/'
    for name, expected in {
        'payload.bin': SEED,
        'rounds.txt': ''.join(f'{n}\n' for n in range(1, ROUNDS + 1)).encode(),
        'complete.txt': f'COMPLETE {run} {ROUNDS}\n'.encode(),
    }.items():
        if read(folder + name) != expected:
            raise ValueError('Missing/incomplete desktop workload: ' + name)
    return {'run': run, 'rounds': ROUNDS, 'bytes_copied': ROUNDS * len(SEED),
            'final_readback': 'PASS'}


def refusal_files(run, trial):
    """Evidence wrapper for an already completed run; never starts a fresh job."""
    fixture = files(run)  # Validate both IDs before placing them in shell text.
    files(trial)
    folder = 'refusal-' + trial
    stem = 'session-' + run
    expected = {
        'payload.bin': SEED,
        'rounds.txt': ''.join(f'{n}\n' for n in range(1, ROUNDS + 1)).encode(),
        'complete.txt': f'COMPLETE {run} {ROUNDS}\n'.encode(),
    }
    commands = ['cd /work/srvros-work || exit 1']
    checks = [f'cmp {stem}/{name} {folder}-expected-{name} || exit 1' for name in expected]
    commands += checks
    commands += [f'cmp {stem}.sh {folder}-expected-script.sh || exit 1',
                 f'sh {stem}.sh > {folder}-output.txt',
                 f'echo $? > {folder}-status.txt',
                 f'cmp {folder}-status.txt {folder}-expected-status.txt || exit 1',
                 f'cmp {folder}-output.txt {folder}-expected-output.txt || exit 1']
    commands += checks
    commands += [f'echo PASS > {folder}-verified.txt || exit 1',
                 'sync || exit 1', 'echo REFUSAL VERIFIED - workload unchanged']
    result = {f'refusal-{trial}.sh': ('\n'.join(commands) + '\n').encode(),
              f'{folder}-expected-script.sh': fixture[stem + '.sh'],
              f'{folder}-expected-status.txt': b'1\n',
              f'{folder}-expected-output.txt': b'REFUSED existing session\n'}
    result.update({f'{folder}-expected-{name}': data for name, data in expected.items()})
    return result


def verify_refusal(read, run, trial):
    for name, expected in refusal_files(run, trial).items():
        if read(name) != expected:
            raise ValueError('Changed refusal input: ' + name)
    folder = 'refusal-' + trial + '-'
    for name, expected in {'status.txt': b'1\n', 'output.txt': b'REFUSED existing session\n',
                           'verified.txt': b'PASS\n'}.items():
        if read(folder + name) != expected:
            raise ValueError('Missing/failed refusal evidence: ' + name)
    verify(read, run)
    return {'run': run, 'trial': trial, 'exit_status': 1, 'refusal': 'PASS',
            'retained_workload': 'unchanged'}
