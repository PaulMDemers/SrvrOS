"""Paced ordinary-shell audio/file trial; native defaults require 30 minutes."""
import re

SEED = bytes(range(256)) * 1024


def files(run, rounds=30, pause=60):
    if not re.fullmatch(r'[0-9a-f]{16}', run):
        raise ValueError('Expected fresh 16-hex run identity')
    if not 1 <= rounds <= 30 or not 1 <= pause <= 60:
        raise ValueError('Invalid bounded pacing')
    stem = 'combined-' + run
    commands = ['cd /work/srvros-work || exit 1',
                f'if test -e {stem}; then echo REFUSED existing combined trial; exit 1; fi',
                f'mkdir {stem} || exit 1', f'cd {stem} || exit 1',
                'echo COMBINED TRIAL RUNNING - keep this Terminal open']
    for n in range(1, rounds + 1):
        commands += [f'cp ../{stem}.bin payload.tmp || exit 1', 'sync || exit 1',
                     'mv payload.tmp payload.bin || exit 1',
                     f'cmp ../{stem}.bin payload.bin || exit 1',
                     f'playwav /fat/share/examples/stereo.wav > audio-{n}.txt 2>&1 || exit 1',
                     f'sleep {pause} || exit 1', f'echo {n} >> rounds.txt || exit 1',
                     'sync || exit 1', f'echo Combined round {n}/{rounds} complete']
    commands += [f'echo COMPLETE {run} {rounds} {pause} > complete.txt || exit 1',
                 'sync || exit 1', 'echo COMBINED TRIAL COMPLETE - save Text and return to console']
    return {stem+'.sh': ('\n'.join(commands)+'\n').encode(), stem+'.bin': SEED}


def verify(read, run, rounds=30, pause=60):
    for name, data in files(run, rounds, pause).items():
        if read(name) != data:
            raise ValueError('Changed trial input: '+name)
    stem = 'combined-'+run+'/'
    expected = {'payload.bin': SEED,
                'rounds.txt': ''.join(f'{n}\n' for n in range(1, rounds+1)).encode(),
                'complete.txt': f'COMPLETE {run} {rounds} {pause}\n'.encode()}
    for name, data in expected.items():
        if read(stem+name) != data:
            raise ValueError('Missing/incomplete trial output: '+name)
    for n in range(1, rounds+1):
        data = read(stem+f'audio-{n}.txt')
        if len(re.findall(rb'playwav: complete bytes=1152000 underruns=0(?:\r?\n|$)', data)) != 1:
            raise ValueError('Missing/failed audio completion: '+str(n))
    return dict(run=run, rounds=rounds, minimum_pacing_seconds=rounds*pause,
                bytes_copied=rounds*len(SEED), audio_completions=rounds, readback='PASS')
