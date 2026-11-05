#!/usr/bin/env python3
"""Check real linked GOT/TLS fixtures, including the prior linker negative control."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def inspect(path):
    data = path.read_bytes()
    header = struct.unpack_from('<16sHHIQQQIHHHHHH', data)
    assert header[0][:6] == b'\x7fELF\x02\x01' and header[2] == 62
    assert header[9] == 56 and header[11] == 64
    sections = [struct.unpack_from('<IIQQQQIIQQ', data, header[6] + i * 64)
                for i in range(header[12])]
    names_section = sections[header[13]]
    names = data[names_section[4]:names_section[4] + names_section[5]]
    name = lambda s: names[s[0]:].split(b'\0', 1)[0].decode()
    tls = [p for i in range(header[10])
           if (p := struct.unpack_from('<IIQQQQQQ', data, header[5] + i * 56))[0] == 7]
    assert len(tls) == 1
    program = tls[0]
    parts = [s for s in sections if s[2] & 0x400 and s[5]]
    got = [s for s in sections if name(s) == '.got']
    assert parts and len(got) == 1 and got[0][5] > 0
    covered = all(s[3] >= program[3] and s[3] + s[5] <= program[3] + program[6]
                  and (s[1] == 8 or s[3] + s[5] <= program[3] + program[5])
                  for s in parts)
    return {'path': str(path.resolve()), 'sha256': hashlib.sha256(data).hexdigest(),
            'tls_filesz': program[5], 'tls_memsz': program[6], 'tls_align': program[7],
            'tls_sections': [{'name': name(s), 'address': s[3], 'bytes': s[5]} for s in parts],
            'covered': covered, 'got_bytes': got[0][5],
            'got_before_tls': got[0][3] + got[0][5] <= program[3],
            'within_kernel_tls_limit': program[6] <= 4096}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixed', nargs=2, type=Path, required=True)
    parser.add_argument('--legacy', nargs=2, type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    results = []
    for layout, paths in [('fixed', args.fixed), ('legacy', args.legacy)]:
        for mode, path in zip(['zero', 'initialized'], paths):
            result = inspect(path)
            result.update(layout=layout, mode=mode)
            results.append(result)
            if layout == 'fixed':
                assert result['covered'] and result['got_before_tls'] and result['within_kernel_tls_limit'], result
            else:
                assert not result['covered'], result
    args.output.write_text(json.dumps({'result': 'pass', 'scope': 'static ELF layout only',
                                       'cases': results}, indent=2) + '\n')
    print('tls-got-layout: four linked fixtures pass')


if __name__ == '__main__':
    main()
