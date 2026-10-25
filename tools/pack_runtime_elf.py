#!/usr/bin/env python3
"""Omit non-runtime ELF trailers from image copies; keep host debug ELFs intact.

No relocation, segment movement, or instruction rewriting occurs. The loader uses
program headers, so section/symbol/debug tables after all segment payloads can be
removed. Every program-header-described byte is retained at its original offset.
"""
import argparse
from pathlib import Path
import struct


def runtime_elf(data: bytes) -> bytes:
    if not data.startswith(b'\x7fELF'):
        return data
    if len(data) < 64 or data[4:7] != b'\x02\x01\x01':
        raise ValueError('runtime image requires ELF64 little-endian version 1')
    header = struct.unpack_from('<16sHHIQQQIHHHHHH', data)
    _, kind, machine, version, _, phoff, _, _, ehsize, phsize, count, _, _, _ = header
    if kind not in (2, 3) or machine != 62 or version != 1 or ehsize != 64 or phsize != 56 or not 0 < count <= 1024:
        raise ValueError('unsupported runtime ELF header')
    if phoff < ehsize or phoff + count * phsize > len(data):
        raise ValueError('truncated runtime ELF program headers')
    end = phoff + count * phsize
    loads = 0
    for index in range(count):
        segment, _, offset, _, _, size, memory, _ = struct.unpack_from('<IIQQQQQQ', data, phoff + index * phsize)
        if size and (offset > len(data) or size > len(data) - offset):
            raise ValueError('truncated runtime ELF segment')
        if segment == 1:
            loads += 1
            if memory < size:
                raise ValueError('runtime ELF load segment exceeds memory size')
        if size:
            end = max(end, offset + size)
    if not loads:
        raise ValueError('runtime ELF has no load segment')
    result = bytearray(data[:end])
    struct.pack_into('<Q', result, 40, 0)  # e_shoff
    struct.pack_into('<HHH', result, 58, 0, 0, 0)  # e_shentsize, e_shnum, e_shstrndx
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path, help='build staging directory (never source/debug output)')
    args = parser.parse_args()
    if not args.directory.is_dir():
        parser.error('staging directory does not exist')
    before = after = count = 0
    for path in sorted(args.directory.rglob('*')):
        if path.is_symlink() or not path.is_file():
            continue
        with path.open('rb') as source:
            if source.read(4) != b'\x7fELF':
                continue
            source.seek(0)
            data = source.read()
        packed = runtime_elf(data)
        # Staged ELF payloads are executable even if their source file lost mode bits.
        path.chmod(path.stat().st_mode | 0o111)
        before += len(data)
        after += len(packed)
        count += 1
        if packed != data:
            path.write_bytes(packed)
    print(f'runtime-elf: {count} staged images, {before} -> {after} bytes; host debug files unchanged')


if __name__ == '__main__':
    main()
