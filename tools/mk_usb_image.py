#!/usr/bin/env python3
import argparse
import binascii
import os
import struct
import uuid


SECTOR = 512
ESP_TYPE = uuid.UUID("c12a7328-f81f-11d2-ba4b-00a0c93ec93b")
BASIC_DATA_TYPE = uuid.UUID("ebd0a0a2-b9e5-4433-87c0-68b6b72699c7")


def align_up(value, alignment):
    return ((value + alignment - 1) // alignment) * alignment


def le_guid(guid):
    return guid.bytes_le


def short_name(name):
    base, _, ext = name.partition(".")
    base = base.upper()
    ext = ext.upper()
    if not base or len(base) > 8 or len(ext) > 3:
        raise ValueError(f"{name!r} is not an 8.3 FAT name")
    return base.ljust(8).encode("ascii") + ext.ljust(3).encode("ascii")


def can_use_short_name(name):
    try:
        short_name(name)
        return True
    except ValueError:
        return False


def short_alias(name):
    if can_use_short_name(name):
        return short_name(name)
    base, _, ext = name.partition(".")
    clean_base = "".join(ch for ch in base.upper() if ch.isalnum())[:6] or "FILE"
    clean_ext = "".join(ch for ch in ext.upper() if ch.isalnum())[:3]
    return (clean_base + "~1").ljust(8).encode("ascii") + clean_ext.ljust(3).encode("ascii")


def short_checksum(short):
    total = 0
    for byte in short:
        total = (((total & 1) << 7) + (total >> 1) + byte) & 0xff
    return total


def lfn_dir_entries(name, short):
    if can_use_short_name(name):
        return []

    chars = [ord(ch) for ch in name]
    chars.append(0)
    while len(chars) % 13 != 0:
        chars.append(0xffff)
    chunks = [chars[i:i + 13] for i in range(0, len(chars), 13)]
    checksum = short_checksum(short)
    entries = []
    for index, chunk in enumerate(reversed(chunks), 1):
        sequence = len(chunks) - index + 1
        if index == 1:
            sequence |= 0x40
        entry = bytearray(32)
        entry[0] = sequence
        entry[11] = 0x0f
        entry[13] = checksum
        positions = [1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30]
        for pos, value in zip(positions, chunk):
            struct.pack_into("<H", entry, pos, value)
        entries.append(bytes(entry))
    return entries


class Fat32Builder:
    def __init__(self, size_bytes, hidden_sectors=0):
        self.size_bytes = size_bytes
        self.hidden_sectors = hidden_sectors
        self.total_sectors = size_bytes // SECTOR
        self.sectors_per_cluster = 8
        self.reserved_sectors = 32
        self.fat_count = 2
        self.root_cluster = 2
        self.entries = []

        clusters_estimate = self.total_sectors // self.sectors_per_cluster
        self.fat_sectors = align_up((clusters_estimate + 2) * 4, SECTOR) // SECTOR
        while True:
            data_sectors = self.total_sectors - self.reserved_sectors - self.fat_count * self.fat_sectors
            clusters = data_sectors // self.sectors_per_cluster
            needed = align_up((clusters + 2) * 4, SECTOR) // SECTOR
            if needed == self.fat_sectors:
                break
            self.fat_sectors = needed
        self.cluster_count = clusters
        self.data_sector = self.reserved_sectors + self.fat_count * self.fat_sectors
        self.next_cluster = self.root_cluster + 1
        self.fat = [0] * (self.cluster_count + 2)
        self.fat[0] = 0x0ffffff8
        self.fat[1] = 0x0fffffff
        self.fat[self.root_cluster] = 0x0fffffff
        self.dirs = {self.root_cluster: []}
        self.files = {}

    def allocate_clusters(self, byte_count):
        count = max(1, align_up(byte_count, self.sectors_per_cluster * SECTOR) //
            (self.sectors_per_cluster * SECTOR))
        start = self.next_cluster
        clusters = list(range(start, start + count))
        if clusters[-1] >= len(self.fat):
            raise RuntimeError("FAT32 image is too small")
        for index, cluster in enumerate(clusters):
            self.fat[cluster] = clusters[index + 1] if index + 1 < len(clusters) else 0x0fffffff
        self.next_cluster += count
        return clusters

    def add_dir(self, parent_cluster, name):
        clusters = self.allocate_clusters(0)
        cluster = clusters[0]
        self.dirs[cluster] = []
        self.dirs[parent_cluster].append((name, True, cluster, 0))
        return cluster

    def add_file(self, parent_cluster, name, data):
        clusters = self.allocate_clusters(len(data))
        self.files[clusters[0]] = data
        self.dirs[parent_cluster].append((name, False, clusters[0], len(data)))
        return clusters[0]

    def cluster_offset(self, cluster):
        sector = self.data_sector + (cluster - 2) * self.sectors_per_cluster
        return sector * SECTOR

    def dir_entry(self, short, is_dir, cluster, size):
        attr = 0x10 if is_dir else 0x20
        return struct.pack("<11sBBBHHHHHHHI",
            short,
            attr,
            0,
            0,
            0,
            0,
            0,
            (cluster >> 16) & 0xffff,
            0,
            0,
            cluster & 0xffff,
            size)

    def boot_sector(self):
        sector = bytearray(SECTOR)
        sector[0:3] = b"\xeb\x58\x90"
        sector[3:11] = b"SRVROS  "
        struct.pack_into("<H", sector, 11, SECTOR)
        sector[13] = self.sectors_per_cluster
        struct.pack_into("<H", sector, 14, self.reserved_sectors)
        sector[16] = self.fat_count
        struct.pack_into("<H", sector, 17, 0)
        struct.pack_into("<H", sector, 19, 0)
        sector[21] = 0xf8
        struct.pack_into("<H", sector, 22, 0)
        struct.pack_into("<H", sector, 24, 63)
        struct.pack_into("<H", sector, 26, 255)
        struct.pack_into("<I", sector, 28, self.hidden_sectors)
        struct.pack_into("<I", sector, 32, self.total_sectors)
        struct.pack_into("<I", sector, 36, self.fat_sectors)
        struct.pack_into("<H", sector, 40, 0)
        struct.pack_into("<H", sector, 42, 0)
        struct.pack_into("<I", sector, 44, self.root_cluster)
        struct.pack_into("<H", sector, 48, 1)
        struct.pack_into("<H", sector, 50, 6)
        sector[64] = 0x80
        sector[66] = 0x29
        struct.pack_into("<I", sector, 67, 0x53525652)
        sector[71:82] = b"SRVROS ESP "
        sector[82:90] = b"FAT32   "
        sector[510:512] = b"\x55\xaa"
        return sector

    def fsinfo_sector(self):
        sector = bytearray(SECTOR)
        struct.pack_into("<I", sector, 0, 0x41615252)
        struct.pack_into("<I", sector, 484, 0x61417272)
        struct.pack_into("<I", sector, 488, 0xffffffff)
        struct.pack_into("<I", sector, 492, self.next_cluster)
        struct.pack_into("<I", sector, 508, 0xaa550000)
        return sector

    def build(self):
        image = bytearray(self.size_bytes)
        image[0:SECTOR] = self.boot_sector()
        image[SECTOR:SECTOR * 2] = self.fsinfo_sector()
        image[SECTOR * 6:SECTOR * 7] = self.boot_sector()
        image[SECTOR * 7:SECTOR * 8] = self.fsinfo_sector()

        fat_bytes = bytearray(self.fat_sectors * SECTOR)
        for index, value in enumerate(self.fat):
            struct.pack_into("<I", fat_bytes, index * 4, value)
        for fat_index in range(self.fat_count):
            offset = (self.reserved_sectors + fat_index * self.fat_sectors) * SECTOR
            image[offset:offset + len(fat_bytes)] = fat_bytes

        for dir_cluster, entries in self.dirs.items():
            data = bytearray(self.sectors_per_cluster * SECTOR)
            pos = 0
            for entry in entries:
                name, is_dir, entry_cluster, size = entry
                short = short_alias(name)
                for lfn in lfn_dir_entries(name, short):
                    data[pos:pos + 32] = lfn
                    pos += 32
                data[pos:pos + 32] = self.dir_entry(short, is_dir, entry_cluster, size)
                pos += 32
            image[self.cluster_offset(dir_cluster):self.cluster_offset(dir_cluster) + len(data)] = data

        for cluster, data in self.files.items():
            offset = self.cluster_offset(cluster)
            image[offset:offset + len(data)] = data

        return bytes(image)


def fat32_entry_name(short):
    base = short[0:8].decode("ascii", "replace").rstrip()
    ext = short[8:11].decode("ascii", "replace").rstrip()
    return base if not ext else f"{base}.{ext}"


def fat32_lfn_name(lfn_entries):
    chars = []
    for entry in lfn_entries:
        for pos in [1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30]:
            value = struct.unpack_from("<H", entry, pos)[0]
            if value == 0 or value == 0xffff:
                return "".join(chars)
            chars.append(chr(value))
    return "".join(chars)


def fat32_dir_entries(image, fat, cluster):
    entries = {}
    data = image[fat.cluster_offset(cluster):fat.cluster_offset(cluster) + fat.sectors_per_cluster * SECTOR]
    lfn_entries = []
    for pos in range(0, len(data), 32):
        entry = data[pos:pos + 32]
        if entry[0] == 0:
            break
        if entry[0] == 0xe5:
            lfn_entries = []
            continue
        attr = entry[11]
        if attr == 0x0f:
            lfn_entries.insert(0, entry)
            continue
        name = fat32_lfn_name(lfn_entries) if lfn_entries else fat32_entry_name(entry[0:11])
        lfn_entries = []
        cluster_low = struct.unpack_from("<H", entry, 26)[0]
        cluster_high = struct.unpack_from("<H", entry, 20)[0]
        entries[name.upper()] = {
            "attr": attr,
            "cluster": (cluster_high << 16) | cluster_low,
            "size": struct.unpack_from("<I", entry, 28)[0],
        }
    return entries


def fat32_file_data(image, fat, start_cluster, size):
    data = bytearray()
    cluster = start_cluster
    seen = set()
    while cluster < 0x0ffffff8 and cluster not in seen:
        seen.add(cluster)
        offset = fat.cluster_offset(cluster)
        data.extend(image[offset:offset + fat.sectors_per_cluster * SECTOR])
        cluster = fat.fat[cluster] & 0x0fffffff
    return bytes(data[:size])


def validate_fat32_image(image, fat, expected_config=None):
    checks = [
        ("EFI", 0x10),
        ("BOOT", 0x10),
        ("STARTUP.NSH", 0x20),
        ("EFI/BOOT/BOOTX64.EFI", 0x20),
        ("EFI/BOOT/LIMINE.CONF", 0x20),
        ("BOOT/SRVROS.ELF", 0x20),
        ("BOOT/INITRAMFS.TAR", 0x20),
        ("BOOT/LIMINE.CONF", 0x20),
        ("BOOT/LIMINE/LIMINE.CONF", 0x20),
        ("LIMINE/LIMINE.CONF", 0x20),
        ("LIMINE.CONF", 0x20),
    ]
    for path, expected_attr in checks:
        cluster = fat.root_cluster
        parts = path.split("/")
        entry = None
        for part in parts:
            entries = fat32_dir_entries(image, fat, cluster)
            entry = entries.get(part.upper())
            if entry is None:
                raise RuntimeError(f"FAT32 image is missing required path {path!r}")
            cluster = entry["cluster"]
        if (entry["attr"] & expected_attr) != expected_attr:
            raise RuntimeError(f"FAT32 image path {path!r} has attribute 0x{entry['attr']:02x}")
        if expected_config is not None and path.endswith("LIMINE.CONF"):
            data = fat32_file_data(image, fat, entry["cluster"], entry["size"])
            if data != expected_config:
                raise RuntimeError(f"FAT32 image path {path!r} does not contain the expected Limine config")


def gpt_entry(part_type, unique_id, first_lba, last_lba, name):
    encoded_name = name.encode("utf-16le")[:72]
    encoded_name = encoded_name + b"\x00" * (72 - len(encoded_name))
    return struct.pack("<16s16sQQQ72s",
        le_guid(part_type),
        le_guid(unique_id),
        first_lba,
        last_lba,
        0,
        encoded_name)


def gpt_header(current_lba, backup_lba, first_usable, last_usable, entries_lba, entries_count, entries_crc, disk_guid):
    header = bytearray(92)
    header[0:8] = b"EFI PART"
    struct.pack_into("<I", header, 8, 0x00010000)
    struct.pack_into("<I", header, 12, 92)
    struct.pack_into("<I", header, 16, 0)
    struct.pack_into("<I", header, 20, 0)
    struct.pack_into("<Q", header, 24, current_lba)
    struct.pack_into("<Q", header, 32, backup_lba)
    struct.pack_into("<Q", header, 40, first_usable)
    struct.pack_into("<Q", header, 48, last_usable)
    header[56:72] = le_guid(disk_guid)
    struct.pack_into("<Q", header, 72, entries_lba)
    struct.pack_into("<I", header, 80, entries_count)
    struct.pack_into("<I", header, 84, 128)
    struct.pack_into("<I", header, 88, entries_crc)
    crc = binascii.crc32(header) & 0xffffffff
    struct.pack_into("<I", header, 16, crc)
    return bytes(header).ljust(SECTOR, b"\x00")


def build_image(args):
    esp_size = args.esp_size_mib * 1024 * 1024
    esp_first = 2048
    esp_sectors = esp_size // SECTOR
    esp_last = esp_first + esp_sectors - 1
    exfat_data = open(args.exfat, "rb").read() if args.exfat else b""
    data_first = align_up(esp_last + 1, 2048)
    data_sectors = align_up(len(exfat_data), SECTOR) // SECTOR if exfat_data else 0
    data_last = data_first + data_sectors - 1 if data_sectors else 0
    backup_lba = align_up(max(esp_last, data_last) + 2048, 2048) - 1
    total_sectors = backup_lba + 1

    fat = Fat32Builder(esp_size, esp_first)
    efi = fat.add_dir(fat.root_cluster, "EFI")
    boot = fat.add_dir(efi, "BOOT")
    boot_dir = fat.add_dir(fat.root_cluster, "BOOT")
    limine_dir = fat.add_dir(boot_dir, "LIMINE")
    root_limine_dir = fat.add_dir(fat.root_cluster, "LIMINE")
    fat.add_file(boot, "BOOTX64.EFI", open(args.bootx64, "rb").read())
    if args.bootia32:
        fat.add_file(boot, "BOOTIA32.EFI", open(args.bootia32, "rb").read())
    fat.add_file(fat.root_cluster, "startup.nsh", b"FS0:\\EFI\\BOOT\\BOOTX64.EFI\r\n")
    fat.add_file(boot_dir, "SRVROS.ELF", open(args.kernel, "rb").read())
    fat.add_file(boot_dir, "initramfs.tar", open(args.initramfs, "rb").read())
    config = open(args.config, "rb").read()
    fat.add_file(boot, "limine.conf", config)
    fat.add_file(fat.root_cluster, "limine.conf", config)
    fat.add_file(boot_dir, "limine.conf", config)
    fat.add_file(limine_dir, "limine.conf", config)
    fat.add_file(root_limine_dir, "limine.conf", config)
    esp = fat.build()
    validate_fat32_image(esp, fat, expected_config=config)

    entries_count = 128
    entries = bytearray(entries_count * 128)
    entries[0:128] = gpt_entry(ESP_TYPE, uuid.uuid4(), esp_first, esp_last, "srvros-esp")
    if exfat_data:
        entries[128:256] = gpt_entry(BASIC_DATA_TYPE, uuid.uuid4(), data_first, data_last, "srvros-fat")
    entries_crc = binascii.crc32(entries) & 0xffffffff

    first_usable = 34
    last_usable = backup_lba - 33
    disk_guid = uuid.uuid4()
    image = bytearray(total_sectors * SECTOR)

    image[0:SECTOR] = b"\x00" * SECTOR
    image[446:462] = struct.pack("<B3sB3sII", 0, b"\x00\x02\x00", 0xee, b"\xff\xff\xff", 1, min(total_sectors - 1, 0xffffffff))
    image[510:512] = b"\x55\xaa"
    image[SECTOR:SECTOR * 2] = gpt_header(1, backup_lba, first_usable, last_usable, 2, entries_count, entries_crc, disk_guid)
    image[2 * SECTOR:2 * SECTOR + len(entries)] = entries
    image[esp_first * SECTOR:(esp_first + esp_sectors) * SECTOR] = esp
    if exfat_data:
        start = data_first * SECTOR
        image[start:start + len(exfat_data)] = exfat_data

    backup_entries_lba = backup_lba - 32
    image[backup_entries_lba * SECTOR:backup_entries_lba * SECTOR + len(entries)] = entries
    image[backup_lba * SECTOR:(backup_lba + 1) * SECTOR] = gpt_header(
        backup_lba, 1, first_usable, last_usable, backup_entries_lba, entries_count, entries_crc, disk_guid)

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "wb") as out:
        out.write(image)


def main():
    parser = argparse.ArgumentParser(description="Create a srvros GPT USB boot image with a FAT32 ESP.")
    parser.add_argument("--output", required=True)
    parser.add_argument("--kernel", required=True)
    parser.add_argument("--initramfs", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--bootx64", required=True)
    parser.add_argument("--bootia32")
    parser.add_argument("--exfat")
    parser.add_argument("--esp-size-mib", type=int, default=384)
    args = parser.parse_args()
    build_image(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
