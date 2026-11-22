#!/usr/bin/env python3
"""Actual C observer/production formatter through exact command parser."""
import argparse
from pathlib import Path
import re
import subprocess
from commit_wait import commit_wait
from commit_command import commit_command

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary', type=Path, required=True)
a = p.parse_args()
raw = subprocess.check_output([str(a.binary.resolve())])


def observe(at, maximum):
    blob = b'\n'.join(l for l in raw.splitlines()
                      if l.startswith((b'capture-commit-', b'capture-owner-command:'))
                      and f' snapshot_us={at} '.encode() in l)+b'\n'
    old = commit_wait([blob], dict(snapshot_us=at),
                      dict(snapshot_us=at, workers=[dict(pid=11, calls=1, max_us=maximum)]))
    return blob, old, commit_command([blob], old)


blob, old, result = observe(1200, 90)
assert result['status'] == 'VALID_COMPLETE_PHASES'
assert result['command']['tag'] == 77 and result['command']['lba'] == 64
assert result['command']['blocks']*result['command']['sector'] == 16384
assert result['residual_us'] == 4
assert observe(41, 30)[2]['status'] == 'VALID_COMMAND_ONLY'
assert observe(51, 40)[2]['status'] == 'VALID_UNAVAILABLE'
assert observe(110, 90)[2]['command']['tag'] == 2
try:
    observe(70, 50)
except ValueError:
    pass
else:
    raise AssertionError('Successful bulk callback masked failed command validation')
source = (Path(__file__).resolve().parents[1]/'kernel/src/workspace_log.c').read_text()
formats = re.findall(r'console_printf\("((?:capture-commit-|capture-owner-command:)[^"\n]+)', source)
sizes = [len(f.replace('\\n', '\n').replace('%u', str(2**64-1)).replace('%s', 'X'*31)) for f in formats]
assert len(sizes) == 5 and max(sizes) < 1024 and sum(sizes) < 4096


def reject(data):
    try:
        commit_command([data], old)
    except ValueError:
        return
    raise AssertionError('Invalid command accepted')


line = next(l for l in blob.splitlines(keepends=True) if l.startswith(b'capture-owner-command:'))
reject(blob.replace(line, b''))
reject(blob+line.replace(b'final=1', b'final=0'))
reject(blob+line.replace(b'lba=64', b'lba=65'))
reject(blob.replace(line, line.rstrip()+b' extra=1\n'))
for field, value in [('version', 2), ('snapshot_us', 1201), ('pid', 12), ('available', 2),
                     ('phases', 2), ('tag', 78), ('opcode', 0x28), ('lba', 2**32),
                     ('blocks', 31), ('sector', 1024), ('flags', 8), ('valid', 0),
                     ('complete', 0), ('ok', 0), ('role', 7), ('start_us', 999),
                     ('end_us', 1081), ('csw_start', 1021), ('csw_end', 1061),
                     ('cbw_calls', 2), ('data_calls', 3), ('data_actual', 16383),
                     ('data_req', 16385), ('data_us', 49), ('csw_calls', 2),
                     ('csw_us', 39), ('ck_calls', 3), ('ck_us', 50), ('lba', 2**64)]:
    changed = re.sub(rb'(?<= )'+field.encode()+rb'=\d+', field.encode()+b'='+str(value).encode(), line)
    assert changed != line
    reject(blob.replace(line, changed))
print('commit-command-wire: PASS actual C/formatter, exact tag/interval, full phases, explicit command-only/unavailable, malformed/refused evidence; maximum total bytes', sum(sizes))
