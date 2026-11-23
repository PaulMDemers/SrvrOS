"""Diagnostic receipts outside the immutable release on the unit's ESP."""
import re
import secrets

GRUB = 'AIRBOOT.ENV'
PROBE = 'AIRBOOT.LOG'
HEADER = b'SRVR-AIR-EFI-V1 '
STAGES = ('entered', 'capture', 'locate', 'allocate', 'install')


def seed(release):
    if not re.fullmatch('[0-9a-f]{16}', release):
        raise ValueError('Invalid EFI trace release')
    nonce = secrets.token_hex(8)
    header = HEADER + f'{release} {nonce}\n'.encode()
    env = f'# GRUB Environment Block\nair_release={release}\nair_nonce={nonce}\nair_state=prepared\n'
    return {GRUB: env.encode().ljust(1024, b'#'), PROBE: header.ljust(512, b'\0')}


def decode(env, probe, release, nonce=None):
    header = re.match(rb'SRVR-AIR-EFI-V1 ([0-9a-f]{16}) ([0-9a-f]{16})\n', probe)
    if len(probe) != 512 or not header or any(probe[header.end():64]) or any(probe[384:]):
        raise ValueError('Invalid EFI progress file')
    tag, token = (s.decode() for s in header.groups())
    rows = re.fullmatch(rb'# GRUB Environment Block\n((?:air_(?:release|nonce|state)=[a-z0-9-]+\n){3})#+', env)
    if not rows or len(env) != 1024:
        raise ValueError('Invalid GRUB progress file')
    fields = dict(line.split('=', 1) for line in rows[1].decode().splitlines())
    if tag != release or fields.get('air_release') != tag or fields.get('air_nonce') != token or (nonce and token != nonce):
        raise ValueError('Boot progress identity mismatch')
    state = fields.get('air_state')
    if state not in ('prepared','selected','loaded','load-failed','returned','start-failed'):
        raise ValueError('Unknown GRUB progress')
    progress = {}
    for i, stage in enumerate(STAGES, 1):
        block = probe[i*64:(i+1)*64]
        if any(block):
            row = re.fullmatch(stage.encode() + rb' ([0-9a-f]{16})\n\x00*', block)
            if not row or len(progress) != i-1:
                raise ValueError('Invalid EFI progress sequence')
            progress[stage] = int(row[1], 16)
    return dict(release=tag, nonce=token, grub=state, probe=progress)


def read(base, release, nonce=None):
    return decode((base / GRUB).read_bytes(), (base / PROBE).read_bytes(), release, nonce)
