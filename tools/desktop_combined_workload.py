"""Ordinary-shell disk bursts during live PCM; native defaults play 30 minutes."""
import re
from functools import lru_cache
from audio_stream_smoke import fixture

SEED = bytes(range(256)) * 1024


@lru_cache(maxsize=4)
def tone(seconds):
    return fixture(seconds)[0]


def files(run, rounds=30, pause=1, seconds=60, bursts=40):
    if not re.fullmatch(r'[0-9a-f]{16}', run):
        raise ValueError('Expected fresh 16-hex run identity')
    if not 1 <= rounds <= 30 or not 1 <= pause <= 60 or not 6 <= seconds <= 60 or not 1 <= bursts <= seconds-2:
        raise ValueError('Invalid bounded pacing')
    stem = 'combined-' + run
    commands = ['cd /work/srvros-work || exit 1',
                f'if test -e {stem}; then echo REFUSED existing combined trial; exit 1; fi',
                f'mkdir {stem} || exit 1', f'cd {stem} || exit 1',
                'echo COMBINED OVERLAP TRIAL RUNNING - keep this Terminal open']
    for n in range(1, rounds + 1):
        commands += [f'playwav ../{stem}.wav > audio-{n}.txt 2>&1 &', 'player=$!',
                     'ready=0', 'for attempt in 1 2 3 4 5 6 7 8 9 10; do',
                     f"if grep -q 'playwav: playing' audio-{n}.txt; then ready=1; break; fi",
                     'sleep 1 || exit 1', 'done', 'test "$ready" = 1 || exit 1']
        commands += ['for burst in '+ ' '.join(str(b) for b in range(1,bursts+1))+'; do',
                     'sleep 1 || exit 1', 'kill -0 "$player" || exit 1',
                     f'cp ../{stem}.bin payload.tmp || exit 1', 'sync || exit 1',
                     'mv payload.tmp payload.bin || exit 1',
                     f'cmp ../{stem}.bin payload.bin || exit 1',
                     'kill -0 "$player" || exit 1',
                     f"if grep -q 'playwav: complete' audio-{n}.txt; then echo FAILED playback ended before disk burst; exit 1; fi",
                     f'echo {n}:$burst >> io.txt || exit 1', 'done']
        commands += ['wait $player || exit 1', f'sleep {pause} || exit 1',
                     f'echo {n} >> rounds.txt || exit 1', 'sync || exit 1',
                     f'echo Combined overlap round {n}/{rounds} complete']
    commands += [f'echo COMPLETE {run} {rounds} {pause} {seconds} {bursts} > complete.txt || exit 1',
                 'sync || exit 1', 'echo COMBINED TRIAL COMPLETE - save Text and return to console']
    return {stem+'.sh': ('\n'.join(commands)+'\n').encode(), stem+'.bin': SEED,
            stem+'.wav': tone(seconds)}


def verify(read, run, rounds=30, pause=1, seconds=60, bursts=40):
    for name, data in files(run, rounds, pause, seconds, bursts).items():
        if read(name) != data:
            raise ValueError('Changed trial input: '+name)
    stem = 'combined-'+run+'/'
    expected = {'payload.bin': SEED,
                'io.txt': ''.join(f'{n}:{b}\n' for n in range(1,rounds+1) for b in range(1,bursts+1)).encode(),
                'rounds.txt': ''.join(f'{n}\n' for n in range(1, rounds+1)).encode(),
                'complete.txt': f'COMPLETE {run} {rounds} {pause} {seconds} {bursts}\n'.encode()}
    for name, data in expected.items():
        if read(stem+name) != data:
            raise ValueError('Missing/incomplete trial output: '+name)
    for n in range(1, rounds+1):
        data = read(stem+f'audio-{n}.txt')
        if len(re.findall(fr'playwav: complete bytes={seconds*192000} underruns=0(?:\r?\n|$)'.encode(), data)) != 1:
            raise ValueError('Missing/failed audio completion: '+str(n))
    return dict(run=run, rounds=rounds, audio_seconds=rounds*seconds,
                disk_bursts=rounds*bursts,bytes_copied=rounds*bursts*len(SEED),
                audio_completions=rounds, live_player_checks='before/after every disk burst',readback='PASS')
