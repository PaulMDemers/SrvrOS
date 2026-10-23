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
