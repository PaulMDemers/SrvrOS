#!/usr/bin/env python3
import struct
import sys

SECTOR_SIZE = 512
SECTORS_PER_CLUSTER = 8
CLUSTER_SIZE = SECTOR_SIZE * SECTORS_PER_CLUSTER
TOTAL_SECTORS = 8192
FAT_OFFSET = 24
FAT_LENGTH = 128
CLUSTER_HEAP_OFFSET = FAT_OFFSET + FAT_LENGTH
CLUSTER_COUNT = (TOTAL_SECTORS - CLUSTER_HEAP_OFFSET) // SECTORS_PER_CLUSTER


def put_utf16_name(entry, text):
    encoded = text.encode("utf-16le")
    entry[0] = 0xC1
    entry[1] = 0
    entry[2:2 + len(encoded)] = encoded


def file_entry(name, first_cluster, data, attributes=0x20, data_length=None):
    if data_length is None:
        data_length = len(data)
    name_entries = (len(name) + 14) // 15
    entries = []

    primary = bytearray(32)
    primary[0] = 0x85
    primary[1] = 1 + name_entries
    struct.pack_into("<H", primary, 4, attributes)
    entries.append(primary)

    stream = bytearray(32)
    stream[0] = 0xC0
    stream[1] = 0x02
    stream[3] = len(name)
    struct.pack_into("<Q", stream, 8, data_length)
    struct.pack_into("<I", stream, 20, first_cluster)
    struct.pack_into("<Q", stream, 24, data_length)
    entries.append(stream)

    for i in range(name_entries):
        name_entry = bytearray(32)
        put_utf16_name(name_entry, name[i * 15:(i + 1) * 15])
        entries.append(name_entry)

    return b"".join(entries)


def cluster_offset(cluster):
    return (CLUSTER_HEAP_OFFSET * SECTOR_SIZE) + (cluster - 2) * CLUSTER_SIZE


def main():
    if len(sys.argv) < 2:
        print("usage: mk_exfat_image.py output.img [name=app.elf ...]", file=sys.stderr)
        return 2

    image = bytearray(TOTAL_SECTORS * SECTOR_SIZE)

    boot = bytearray(SECTOR_SIZE)
    boot[0:3] = b"\xeb\x76\x90"
    boot[3:11] = b"EXFAT   "
    struct.pack_into("<Q", boot, 64, 0)
    struct.pack_into("<Q", boot, 72, TOTAL_SECTORS)
    struct.pack_into("<I", boot, 80, FAT_OFFSET)
    struct.pack_into("<I", boot, 84, FAT_LENGTH)
    struct.pack_into("<I", boot, 88, CLUSTER_HEAP_OFFSET)
    struct.pack_into("<I", boot, 92, CLUSTER_COUNT)
    struct.pack_into("<I", boot, 96, 4)
    struct.pack_into("<I", boot, 100, 0x53525652)
    struct.pack_into("<H", boot, 104, 0x0100)
    boot[108] = 9
    boot[109] = 3
    boot[110] = 1
    boot[111] = 0x80
    boot[112] = 1
    struct.pack_into("<H", boot, 510, 0xAA55)
    image[0:SECTOR_SIZE] = boot

    fat_base = FAT_OFFSET * SECTOR_SIZE
    default_names = ["hello", "cat", "webd", "spin", "ui", "desktop", "calcgui", "notesgui", "textedit", "imgedit"]
    app_names = []
    app_data = {}
    for index, arg in enumerate(sys.argv[2:]):
        if "=" in arg:
            name, path = arg.split("=", 1)
        else:
            if index >= len(default_names):
                print("too many positional apps; use name=path", file=sys.stderr)
                return 2
            name, path = default_names[index], arg
        if not name or "/" in name:
            print(f"invalid app name: {name}", file=sys.stderr)
            return 2
        app_names.append(name)
        with open(path, "rb") as app:
            app_data[name] = app.read()

    allocated_clusters = {2, 3, 4}
    next_cluster = 5

    def allocate_clusters(data_length):
        nonlocal next_cluster
        cluster = next_cluster
        count = max(1, (data_length + CLUSTER_SIZE - 1) // CLUSTER_SIZE)
        allocated_clusters.update(range(cluster, cluster + count))
        next_cluster += count
        return cluster

    static_files = [
        ("hello", b"Hello from the srvros exFAT volume.\n"),
        ("notes", b"This image is mounted read-only at /fat.\n"),
        ("index.html",
            b"<!doctype html>\n"
            b"<html><head><meta charset=\"utf-8\"><title>srvros</title></head>\n"
            b"<body><h1>srvros</h1><p>This page is served by a ring-3 web server reading files from exFAT.</p>\n"
            b"<p><a href=\"/hello.html\">hello page</a> <a href=\"/status.txt\">status</a></p></body></html>\n"),
        ("hello.html",
            b"<!doctype html>\n"
            b"<html><head><meta charset=\"utf-8\"><title>Hello from srvros</title></head>\n"
            b"<body><h1>Hello from srvros</h1><p>The HTTP response body came from /fat/hello.html.</p></body></html>\n"),
        ("status.txt",
            b"srvros webd: static file serving from exFAT is online.\n"),
    ]
    www_files = [
        ("index.html",
            b"<!doctype html>\n"
            b"<html><head><meta charset=\"utf-8\"><title>srvros</title></head>\n"
            b"<body><h1>srvros</h1><p>This page is served by a ring-3 web server reading files from /fat/www.</p>\n"
            b"<p><a href=\"/hello.html\">hello page</a> <a href=\"/status.txt\">status</a></p></body></html>\n"),
        ("hello.html",
            b"<!doctype html>\n"
            b"<html><head><meta charset=\"utf-8\"><title>Hello from srvros</title></head>\n"
            b"<body><h1>Hello from srvros</h1><p>The HTTP response body came from /fat/www/hello.html.</p></body></html>\n"),
        ("status.txt",
            b"srvros webd: static file serving from /fat/www is online.\n"),
    ]
    etc_files = [
        ("init.sh",
            b"# srvros service startup\n"
            b"echo init-script-ok\n"
            b"webd &\n"),
    ]
    files = []
    for name, data in static_files:
        files.append((name, allocate_clusters(len(data)), data))

    bin_dir_cluster = allocate_clusters(CLUSTER_SIZE)
    etc_dir_cluster = allocate_clusters(CLUSTER_SIZE)
    www_dir_cluster = allocate_clusters(CLUSTER_SIZE)
    etc_entries_data = []
    for name, data in etc_files:
        etc_entries_data.append((name, allocate_clusters(len(data)), data))
    www_entries_data = []
    for name, data in www_files:
        www_entries_data.append((name, allocate_clusters(len(data)), data))
    app_clusters = {}
    for name in app_names:
        app_clusters[name] = allocate_clusters(len(app_data[name])) if app_data[name] else 0
    struct.pack_into("<I", image, fat_base + 0 * 4, 0xFFFFFFF8)
    struct.pack_into("<I", image, fat_base + 1 * 4, 0xFFFFFFFF)
    for cluster in allocated_clusters:
        struct.pack_into("<I", image, fat_base + cluster * 4, 0xFFFFFFFF)

    bitmap = bytearray(CLUSTER_SIZE)
    for cluster in allocated_clusters:
        index = cluster - 2
        bitmap[index // 8] |= 1 << (index % 8)
    image[cluster_offset(2):cluster_offset(2) + len(bitmap)] = bitmap

    upcase = bytearray(CLUSTER_SIZE)
    image[cluster_offset(3):cluster_offset(3) + len(upcase)] = upcase

    for name in app_names:
        if app_data[name]:
            files.append(("bin-" + name, app_clusters[name], app_data[name]))

    root = bytearray(CLUSTER_SIZE)
    offset = 0
    root[offset] = 0x81
    struct.pack_into("<I", root, offset + 20, 2)
    struct.pack_into("<Q", root, offset + 24, (CLUSTER_COUNT + 7) // 8)
    offset += 32

    root[offset] = 0x82
    struct.pack_into("<I", root, offset + 20, 3)
    struct.pack_into("<Q", root, offset + 24, CLUSTER_SIZE)
    offset += 32

    bin_entries = bytearray(CLUSTER_SIZE)
    bin_offset = 0
    etc_entries = bytearray(CLUSTER_SIZE)
    etc_offset = 0
    root[offset:offset + len(file_entry("bin", bin_dir_cluster, b"", attributes=0x10, data_length=CLUSTER_SIZE))] = \
        file_entry("bin", bin_dir_cluster, b"", attributes=0x10, data_length=CLUSTER_SIZE)
    offset += len(file_entry("bin", bin_dir_cluster, b"", attributes=0x10, data_length=CLUSTER_SIZE))
    root[offset:offset + len(file_entry("etc", etc_dir_cluster, b"", attributes=0x10, data_length=CLUSTER_SIZE))] = \
        file_entry("etc", etc_dir_cluster, b"", attributes=0x10, data_length=CLUSTER_SIZE)
    offset += len(file_entry("etc", etc_dir_cluster, b"", attributes=0x10, data_length=CLUSTER_SIZE))
    root[offset:offset + len(file_entry("www", www_dir_cluster, b"", attributes=0x10, data_length=CLUSTER_SIZE))] = \
        file_entry("www", www_dir_cluster, b"", attributes=0x10, data_length=CLUSTER_SIZE)
    offset += len(file_entry("www", www_dir_cluster, b"", attributes=0x10, data_length=CLUSTER_SIZE))

    for name, cluster, data in files:
        entry_set = file_entry(name, cluster, data)
        root[offset:offset + len(entry_set)] = entry_set
        offset += len(entry_set)
        for i in range(0, len(data), CLUSTER_SIZE):
            chunk_cluster = cluster + (i // CLUSTER_SIZE)
            file_data = bytearray(CLUSTER_SIZE)
            file_data[:min(CLUSTER_SIZE, len(data) - i)] = data[i:i + CLUSTER_SIZE]
            image[cluster_offset(chunk_cluster):cluster_offset(chunk_cluster) + CLUSTER_SIZE] = file_data

        nested_name = None
        if name.startswith("bin-"):
            nested_name = name[4:]
        if nested_name is not None:
            nested_entry_set = file_entry(nested_name, cluster, data)
            bin_entries[bin_offset:bin_offset + len(nested_entry_set)] = nested_entry_set
            bin_offset += len(nested_entry_set)

    for name, cluster, data in etc_entries_data:
        entry_set = file_entry(name, cluster, data)
        etc_entries[etc_offset:etc_offset + len(entry_set)] = entry_set
        etc_offset += len(entry_set)
        for i in range(0, len(data), CLUSTER_SIZE):
            chunk_cluster = cluster + (i // CLUSTER_SIZE)
            file_data = bytearray(CLUSTER_SIZE)
            file_data[:min(CLUSTER_SIZE, len(data) - i)] = data[i:i + CLUSTER_SIZE]
            image[cluster_offset(chunk_cluster):cluster_offset(chunk_cluster) + CLUSTER_SIZE] = file_data

    www_entries = bytearray(CLUSTER_SIZE)
    www_offset = 0
    for name, cluster, data in www_entries_data:
        entry_set = file_entry(name, cluster, data)
        www_entries[www_offset:www_offset + len(entry_set)] = entry_set
        www_offset += len(entry_set)
        for i in range(0, len(data), CLUSTER_SIZE):
            chunk_cluster = cluster + (i // CLUSTER_SIZE)
            file_data = bytearray(CLUSTER_SIZE)
            file_data[:min(CLUSTER_SIZE, len(data) - i)] = data[i:i + CLUSTER_SIZE]
            image[cluster_offset(chunk_cluster):cluster_offset(chunk_cluster) + CLUSTER_SIZE] = file_data

    image[cluster_offset(4):cluster_offset(4) + CLUSTER_SIZE] = root
    image[cluster_offset(bin_dir_cluster):cluster_offset(bin_dir_cluster) + CLUSTER_SIZE] = bin_entries
    image[cluster_offset(etc_dir_cluster):cluster_offset(etc_dir_cluster) + CLUSTER_SIZE] = etc_entries
    image[cluster_offset(www_dir_cluster):cluster_offset(www_dir_cluster) + CLUSTER_SIZE] = www_entries

    with open(sys.argv[1], "wb") as output:
        output.write(image)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
