#!/usr/bin/env python3
"""Bind and decode the opt-in, preallocated internal EFI startup journal."""
import hashlib
import io
from pathlib import Path
import struct
import tarfile

SECTOR = 512
SLOT_BYTES = 8192
SLOT_COUNT = 8
FILE_BYTES = SECTOR + SLOT_BYTES * SLOT_COUNT
MODULE = 'startup-journal.bin'
PHASES = ['before-usb', 'after-usb', 'before-workspace', 'after-workspace',
          'after-logger', 'after-save-unmount', 'returning']


def magic(text):
    return text.encode('ascii').ljust(16, b'\0')


def header(text, nonce):
    assert len(nonce) == 16 and any(nonce)
    return bytearray(magic(text) + nonce + bytes(SECTOR - 32))


def initial(nonce):
    head = header('SRVRSTARTFILE1', nonce)
    struct.pack_into('<Q', head, 32, FILE_BYTES)
    result = head
    for slot in range(SLOT_COUNT):
        entry = header('SRVRSTARTEMPTY1', nonce)
        struct.pack_into('<I', entry, 32, slot)
        result += entry + bytes(SLOT_BYTES - SECTOR)
    assert len(result) == FILE_BYTES
    return bytes(result)


def read_at(stream, offset, size):
    stream.seek(offset)
    value = stream.read(size)
    if len(value) != size:
        raise ValueError('short disk read')
    return value


def clean_boot_sector(bpb):
    """Pin the exact FAT32 sector expected after Linux cleanly unmounts it.

    Linux changes FAT_STATE_DIRTY at byte 65 on mount/unmount. Only that bit
    changes in this host-side prediction. Native admission still hashes every
    byte of the actual sector and requires exact equality with the descriptor.
    """
    if (len(bpb) != SECTOR or struct.unpack_from('<H', bpb, 11)[0] != SECTOR or
            struct.unpack_from('<H', bpb, 22)[0] != 0 or bpb[510:] != b'\x55\xaa'):
        raise ValueError('clean boot-sector prediction requires FAT32')
    clean = bytearray(bpb)
    clean[65] &= ~1
    return bytes(clean)


def file_extent(stream, start, name):
    """Read both FAT chains for one root 8.3 file. Never modify filesystem metadata."""
    from mk_usb_image import short_name
    short = short_name(name)
    bpb = read_at(stream, start, SECTOR)
    bps, spc, reserved, copies = struct.unpack_from('<HBHB', bpb, 11)
    total, fat_size, root = (struct.unpack_from('<I', bpb, p)[0] for p in (32, 36, 44))
    if (bps != SECTOR or not spc or spc & (spc - 1) or spc > 128 or
            copies != 2 or not reserved or not fat_size or bpb[510:] != b'\x55\xaa'):
        raise ValueError('unsupported FAT32 geometry')
    first_data = reserved + copies * fat_size
    if total <= first_data or fat_size * SECTOR // 4 < (total - first_data) // spc + 2:
        raise ValueError('invalid FAT32 capacity')
    clusters = (total - first_data) // spc
    cluster_bytes = spc * SECTOR

    def chain(first, limit):
        result = []
        while True:
            if first < 2 or first >= clusters + 2 or first in result or len(result) >= limit:
                raise ValueError('invalid or excessive FAT chain')
            result.append(first)
            pair = [struct.unpack('<I', read_at(stream,
                    start + (reserved + copy * fat_size) * SECTOR + first * 4, 4))[0] & 0xfffffff
                    for copy in range(copies)]
            if pair[0] != pair[1]:
                raise ValueError('FAT copies disagree')
            first = pair[0]
            if first >= 0xffffff8:
                return result

    root_chain = chain(root, 4096)
    found = []
    for cluster in root_chain:
        data = read_at(stream, start + (first_data + (cluster - 2) * spc) * SECTOR, cluster_bytes)
        for at in range(0, len(data), 32):
            entry = data[at:at + 32]
            if entry[0] == 0:
                break
            if entry[0] == 0xe5 or entry[11] == 15:
                continue
            if entry[:11] == short:
                if entry[11] & 0x18:
                    raise ValueError('journal path is not a regular file')
                first = struct.unpack_from('<H', entry, 20)[0] << 16 | struct.unpack_from('<H', entry, 26)[0]
                size = struct.unpack_from('<I', entry, 28)[0]
                found.append((first, size))
        else:
            continue
        if entry[0] == 0:
            break
    if len(found) != 1 or found[0][1] != FILE_BYTES:
        raise ValueError('missing, duplicate or incorrectly sized journal file')
    required = (FILE_BYTES + cluster_bytes - 1) // cluster_bytes
    allocation = chain(found[0][0], required + 1)
    if (len(allocation) != required or set(allocation) & set(root_chain) or
            allocation != list(range(allocation[0], allocation[0] + required))):
        raise ValueError('journal file needs a distinct contiguous FAT allocation')
    offset = start + (first_data + (allocation[0] - 2) * spc) * SECTOR
    return offset, total * SECTOR, bpb


def bind(stream, disk_sectors, partition_start, name, nonce):
    offset, size, bpb = file_extent(stream, partition_start, name)
    gpt = read_at(stream, SECTOR, SECTOR)
    if gpt[:8] != b'EFI PART' or offset + FILE_BYTES > disk_sectors * SECTOR:
        raise ValueError('journal requires GPT and bounded disk geometry')
    if read_at(stream, offset, FILE_BYTES) != initial(nonce):
        raise ValueError('journal has stale or modified contents')
    return (magic('SRVRSTARTDESC1') + struct.pack('<QQQQ', disk_sectors, offset, partition_start, size)
            + nonce + hashlib.sha256(gpt).digest() + hashlib.sha256(clean_boot_sector(bpb)).digest())


def validate_descriptor(value):
    if len(value) != 128 or value[:16] != magic('SRVRSTARTDESC1') or not any(value[48:64]):
        raise ValueError('invalid startup journal descriptor')
    sectors, offset, start, size = struct.unpack_from('<QQQQ', value, 16)
    if (sectors > (2**64 - 1) // SECTOR or min(start, size) == 0 or start < 1048576 or
            (offset | start | size) % SECTOR or start + size > sectors * SECTOR or
            offset < start + SECTOR or offset + FILE_BYTES > start + size):
        raise ValueError('out-of-bounds startup journal descriptor')
    return value


def add_to_initramfs(descriptor, archive):
    value = validate_descriptor(Path(descriptor).read_bytes())
    with tarfile.open(archive) as stream:
        if any(p.name.lstrip('./') == MODULE for p in stream.getmembers()):
            raise ValueError('duplicate startup journal module')
    with tarfile.open(archive, 'a', format=tarfile.USTAR_FORMAT) as stream:
        item = tarfile.TarInfo(MODULE)
        item.size, item.mode, item.mtime = len(value), 0o400, 0
        stream.addfile(item, io.BytesIO(value))


def verify_initramfs(archive):
    with tarfile.open(archive) as stream:
        entries = [p for p in stream.getmembers() if p.name.lstrip('./') == MODULE]
        if len(entries) != 1 or not entries[0].isreg():
            raise ValueError('missing or duplicate startup journal module')
        return validate_descriptor(stream.extractfile(entries[0]).read())


def decode(data, descriptor):
    descriptor = validate_descriptor(descriptor)
    nonce = descriptor[48:64]
    template = initial(nonce)
    if len(data) != FILE_BYTES or data[:SECTOR] != template[:SECTOR]:
        raise ValueError('startup journal file identity mismatch')
    records, errors = [], []
    last_total = 0
    gap = False
    for slot in range(SLOT_COUNT):
        at = SECTOR + slot * SLOT_BYTES
        entry, payload = data[at:at + SECTOR], data[at + SECTOR:at + SLOT_BYTES]
        if data[at:at + SLOT_BYTES] == template[at:at + SLOT_BYTES]:
            gap = True
            continue
        phase = slot + 1
        flags, count, total = struct.unpack_from('<QQQ', entry, 40)
        expected = header('SRVRSTARTREC1', nonce)
        struct.pack_into('<IIQQQ', expected, 32, slot, phase, flags, count, total)
        expected[64:96] = hashlib.sha256(payload).digest()
        expected[96:128] = hashlib.sha256(expected[:96]).digest()
        if (gap or phase > len(PHASES) or entry != expected or flags > 63 or
                count > len(payload) or count > total or total < last_total or any(payload[count:])):
            errors.append({'slot': slot, 'reason': 'incomplete or invalid committed record'})
            gap = True
            continue
        records.append(dict(phase=phase, name=PHASES[slot], flags=flags, count=count, total=total,
                            tail=payload[:count].decode('utf-8', errors='replace')))
        last_total = total
    return dict(complete=len(records) == 7 and not errors, records=records, errors=errors,
                workspace_ready=bool(records[-1]['flags'] & 1) if records else None,
                logger_started=bool(records[-1]['flags'] & 2) if records else None,
                saved=bool(records[-1]['flags'] & 16) if records else None,
                unmounted=bool(records[-1]['flags'] & 32) if records else None,
                native_accepted=False)
