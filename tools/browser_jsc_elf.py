"""Validate the static SrvrOS JSC ELF and its loader-visible TLS contract."""
from pathlib import Path
import struct


def inspect_target_elf(path):
    path = Path(path)
    size = path.stat().st_size
    with path.open('rb') as stream:
        def read(offset, length):
            if offset < 0 or length < 0 or offset + length > size:
                raise ValueError('truncated target ELF')
            stream.seek(offset)
            data = stream.read(length)
            if len(data) != length:
                raise ValueError('truncated target ELF')
            return data
        header = struct.unpack('<16sHHIQQQIHHHHHH', read(0, 64))
        if header[0][:7] != b'\x7fELF\x02\x01\x01' or header[1:4] != (2, 62, 1):
            raise ValueError('expected ELF64 little-endian x86_64 ET_EXEC')
        if header[8] != 64 or header[9] != 56 or not 0 < header[10] <= 1024:
            raise ValueError('unsupported target program headers')
        programs = [struct.unpack('<IIQQQQQQ', read(header[5]+i*56, 56)) for i in range(header[10])]
        if any(p[0] in (2, 3) for p in programs):
            raise ValueError('target ELF has a dynamic/interpreter segment')
        loads = [p for p in programs if p[0] == 1]
        if not loads or any(p[5] > p[6] or p[2]+p[5] > size for p in loads):
            raise ValueError('invalid target load segments')
        if header[11] != 64 or not 0 < header[12] <= 65535:
            raise ValueError('target ELF must retain section headers for TLS validation')
        sections = [struct.unpack('<IIQQQQIIQQ', read(header[6]+i*64, 64)) for i in range(header[12])]
        tls_sections = [s for s in sections if s[2] & 0x400 and s[5]]
        tls_programs = [p for p in programs if p[0] == 7]
        if not tls_sections or len(tls_programs) != 1:
            raise ValueError('expected target TLS sections and one PT_TLS')
        tls = tls_programs[0]
        if not 0 < tls[6] <= 4096 or tls[5] > tls[6] or tls[2]+tls[5] > size:
            raise ValueError('PT_TLS is empty, truncated, or exceeds the 4096-byte kernel limit')
        if not 0 < tls[7] <= 4096 or tls[7] & (tls[7]-1):
            raise ValueError('unsupported target TLS alignment')
        for section in tls_sections:
            delta = section[3]-tls[3]
            if delta < 0 or delta+section[5] > tls[6]:
                raise ValueError('PT_TLS does not cover all SHF_TLS sections')
            if section[1] != 8 and (delta+section[5] > tls[5] or section[4] != tls[2]+delta):
                raise ValueError('PT_TLS does not describe initialized TLS bytes')
        return {'type': 'ET_EXEC', 'machine': 'x86_64', 'entry': header[4],
                'program_headers': len(programs), 'load_memory_sum': sum(p[6] for p in loads),
                'tls': {'file_bytes': tls[5], 'memory_bytes': tls[6], 'alignment': tls[7],
                        'section_count': len(tls_sections), 'all_sections_covered': True}}
