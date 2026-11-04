"""Strict firmware-only Air6,1 receipts; never DMA/MMIO/ownership admission."""
import base64
import hashlib
import re
import struct

PIN = 'aafa826ce50c10388f2c4198de9a429ab51a5573ad1c5d2856ea32ed078b6307'
IDENTITY = (0x0a268086, 0x03000009, 0x011a106b, 0x0a048086, 0x9c438086)


def canonical(text, size):
    data = base64.b64decode(text, validate=True)
    if len(data) != size or base64.b64encode(data).decode() != text:
        raise ValueError('canonical record')
    return data


def catalog_valid(rows):
    if not 1 <= len(rows) <= 128:
        raise ValueError('catalog count')
    for i, (base, length, kind) in enumerate(rows):
        if not length or base + length >= 1 << 64 or kind > 8:
            raise ValueError('catalog range/type')
        if any(base < b + n and b < base + length for b, n, _ in rows[:i]):
            raise ValueError('catalog overlap')


def firmware_result(text):
    try:
        return _parse(text)
    except (ValueError, KeyError, IndexError, struct.error):
        return None


def _parse(text):
    summaries = [line for line in text.splitlines() if line.startswith('hsw-firmware:')]
    if len(summaries) != 1:
        return None
    match = re.fullmatch(r'hsw-firmware: v=1 status=([1-6]) count=(\d+) cap=128 dmar_bytes=136 limit_us=100000 mmio=0 writes=0 ownership=0', summaries[0])
    if not match:
        return None
    status, count = map(int, match.groups())
    records = {}
    for line in text.splitlines():
        if not line.startswith(('hsw-firmware-meta:', 'hsw-firmware-dmar:', 'hsw-firmware-map:')):
            continue
        m = re.fullmatch(r'hsw-firmware-(meta|dmar|map): v=1 phase=([01]) data=([A-Za-z0-9+/]*={0,2})', line)
        if not m or (m[1], int(m[2])) in records:
            raise ValueError('record shape/duplicate')
        records[m[1], int(m[2])] = m[3]
    if status != 1:
        if count or records or 'laptop-capture: completed=yes' in text:
            return None
        return dict(status='REFUSED' if status == 2 else 'FAILED', source_status=status,
                    source_completed=False, native_translation_observed=False,
                    mmio_reads=0, hardware_authority=False, ownership=False)
    if not 1 <= count <= 128 or len(records) != 6 or text.count('laptop-capture: completed=yes\n') != 1 or 'laptop-capture: completed=NO' in text:
        return None
    samples = []
    for phase in (0, 1):
        meta = struct.unpack('<6I3Q', canonical(records['meta', phase], 48))
        if meta[:5] != IDENTITY or meta[5] != count:
            raise ValueError('identity/count')
        pa, begin, end = meta[6:]
        if not pa or pa + 136 >= 1 << 64 or not 0 < begin < end or end - begin > 100000:
            raise ValueError('table/time')
        raw = canonical(records['dmar', phase], 136)
        if raw[:4] != b'DMAR' or int.from_bytes(raw[4:8], 'little') != 136 or raw[8] != 1 or sum(raw) % 256 or hashlib.sha256(raw).hexdigest() != PIN:
            raise ValueError('complete pinned DMAR')
        data = canonical(records['map', phase], count * 24)
        rows = list(struct.iter_unpack('<3Q', data))
        catalog_valid(rows)
        overlaps = [(b, n, t) for b, n, t in rows if b < pa + 136 and pa < b + n]
        if len(overlaps) != 1 or not (overlaps[0][0] <= pa < pa + 136 <= sum(overlaps[0][:2]) and overlaps[0][2] in (2, 3)):
            raise ValueError('table resource coverage')
        if phase and (rows != samples[0]['catalog'] or pa != samples[0]['table_pa'] or begin < samples[0]['pm_us'][1] or begin - samples[0]['pm_us'][1] > 100000):
            raise ValueError('changed catalog/table/clock')
        samples.append(dict(identity=list(meta[:5]), table_pa=pa, pm_us=[begin, end],
                            dmar_base64=records['dmar', phase], catalog=rows))
    intersections = {hex(base): [list(row) for row in samples[0]['catalog'] if row[0] < base + 4096 and base < row[0] + row[1]] for base in (0xfed90000, 0xfed91000)}
    return dict(status='OBSERVED', source_status=1, source_completed=True, count=count,
                dmar_sha256=PIN, samples=samples, drhd_catalog_intersections=intersections,
                native_translation_observed=False, mmio_reads=0, hardware_authority=False,
                ownership=False, pin=False, scope='Complete pinned firmware/catalog observations only; no VT-d state or resource admission')
