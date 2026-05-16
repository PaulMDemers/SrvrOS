#!/usr/bin/env python3
import struct
import sys

SECTOR_SIZE = 512
SECTORS_PER_CLUSTER = 8
CLUSTER_SIZE = SECTOR_SIZE * SECTORS_PER_CLUSTER
# Keep enough room for the growing base userspace and early ports. The builder
# validates cluster usage below so future growth fails loudly instead of writing
# data past the declared exFAT volume.
TOTAL_SECTORS = 131072
FAT_OFFSET = 24
FAT_LENGTH = 128
CLUSTER_HEAP_OFFSET = FAT_OFFSET + FAT_LENGTH
CLUSTER_COUNT = (TOTAL_SECTORS - CLUSTER_HEAP_OFFSET) // SECTORS_PER_CLUSTER
ROOT_DIRECTORY_CLUSTER = 4
ROOT_DIRECTORY_CLUSTERS = 16
BIN_DIRECTORY_CLUSTERS = 4
SMALL_DIRECTORY_CLUSTERS = 1
ROOT_DIRECTORY_SIZE = CLUSTER_SIZE * ROOT_DIRECTORY_CLUSTERS
BIN_DIRECTORY_SIZE = CLUSTER_SIZE * BIN_DIRECTORY_CLUSTERS
SMALL_DIRECTORY_SIZE = CLUSTER_SIZE * SMALL_DIRECTORY_CLUSTERS


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


def append_directory_entry(directory, offset, entry_set, label):
    remaining_in_cluster = CLUSTER_SIZE - (offset % CLUSTER_SIZE)
    if len(entry_set) > remaining_in_cluster:
        while offset % CLUSTER_SIZE != 0:
            directory[offset] = 0x01
            offset += 32
    if offset + len(entry_set) > len(directory):
        raise ValueError(f"{label} directory is too small for generated entries")
    directory[offset:offset + len(entry_set)] = entry_set
    return offset + len(entry_set)


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
    struct.pack_into("<I", boot, 96, ROOT_DIRECTORY_CLUSTER)
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
    default_names = ["hello", "cat", "webd", "httpget", "udpdns", "udpecho", "netstat", "ifconfig", "route",
                     "arp", "ping", "host", "netcheck", "netabi", "sysabi", "spin", "ui", "desktop", "calcgui", "notesgui", "textedit",
                     "imgedit"]
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

    allocated_clusters = set([2, 3])
    allocated_clusters.update(range(ROOT_DIRECTORY_CLUSTER, ROOT_DIRECTORY_CLUSTER + ROOT_DIRECTORY_CLUSTERS))
    next_cluster = ROOT_DIRECTORY_CLUSTER + ROOT_DIRECTORY_CLUSTERS

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
            b"<html><head><meta charset=\"utf-8\"><title>srvros</title>\n"
            b"<link rel=\"stylesheet\" href=\"/assets/site.css\"></head>\n"
            b"<body><h1>srvros</h1><p>This page is served by a ring-3 web server reading files from /fat/www.</p>\n"
            b"<p><a href=\"/hello.html\">hello page</a> <a href=\"/status.txt\">status</a></p></body></html>\n"),
        ("hello.html",
            b"<!doctype html>\n"
            b"<html><head><meta charset=\"utf-8\"><title>Hello from srvros</title></head>\n"
            b"<body><h1>Hello from srvros</h1><p>The HTTP response body came from /fat/www/hello.html.</p></body></html>\n"),
        ("status.txt",
            b"srvros webd: static file serving from /fat/www is online.\n"),
        ("large.txt",
            b"srvros large tcp payload begins\n" +
            (b"0123456789abcdefghijklmnopqrstuvwxyz\n" * 160) +
            b"srvros large tcp payload ends\n"),
    ]
    www_asset_files = [
        ("site.css",
            b"body{font-family:sans-serif;max-width:48rem;margin:3rem auto;line-height:1.5}"
            b"h1{color:#2f7d78}a{color:#4f6bed}\n"),
    ]
    etc_files = [
        ("profile",
            b"# srvros login shell profile\n"
            b"export PATH=/fat/bin:/:/fat\n"
            b"export TMPDIR=/fat/tmp\n"
            b"export PS1='\\w $ '\n"
            b"alias ll='ls /fat/bin'\n"),
        ("init.sh",
            b"# srvros service startup\n"
            b"echo init-script-ok\n"
            b"service webd start\n"),
        ("resolv.conf",
            b"# srvros resolver fallback\n"
            b"nameserver 10.0.2.3\n"),
    ]
    service_files = [
        ("webd.svc",
            b"# srvros web server service\n"
            b"command=/fat/bin/webd\n"
            b"args=/fat/www\n"
            b"process=webd\n"
            b"log=/fat/var/log/webd.log\n"),
    ]
    files = []
    for name, data in static_files:
        files.append((name, allocate_clusters(len(data)), data))

    bin_dir_cluster = allocate_clusters(BIN_DIRECTORY_SIZE)
    etc_dir_cluster = allocate_clusters(SMALL_DIRECTORY_SIZE)
    services_dir_cluster = allocate_clusters(SMALL_DIRECTORY_SIZE)
    var_dir_cluster = allocate_clusters(SMALL_DIRECTORY_SIZE)
    log_dir_cluster = allocate_clusters(SMALL_DIRECTORY_SIZE)
    www_dir_cluster = allocate_clusters(SMALL_DIRECTORY_SIZE)
    www_assets_dir_cluster = allocate_clusters(SMALL_DIRECTORY_SIZE)
    etc_entries_data = []
    for name, data in etc_files:
        etc_entries_data.append((name, allocate_clusters(len(data)), data))
    service_entries_data = []
    for name, data in service_files:
        service_entries_data.append((name, allocate_clusters(len(data)), data))
    www_entries_data = []
    for name, data in www_files:
        www_entries_data.append((name, allocate_clusters(len(data)), data))
    www_assets_entries_data = []
    for name, data in www_asset_files:
        www_assets_entries_data.append((name, allocate_clusters(len(data)), data))
    app_clusters = {}
    for name in app_names:
        app_clusters[name] = allocate_clusters(len(app_data[name])) if app_data[name] else 0
    used_clusters = next_cluster - 2
    if used_clusters > CLUSTER_COUNT:
        print(
            f"image is too small: need {used_clusters} clusters, have {CLUSTER_COUNT}",
            file=sys.stderr,
        )
        return 1
    struct.pack_into("<I", image, fat_base + 0 * 4, 0xFFFFFFF8)
    struct.pack_into("<I", image, fat_base + 1 * 4, 0xFFFFFFFF)
    for cluster in allocated_clusters:
        struct.pack_into("<I", image, fat_base + cluster * 4, 0xFFFFFFFF)
    for cluster in range(ROOT_DIRECTORY_CLUSTER, ROOT_DIRECTORY_CLUSTER + ROOT_DIRECTORY_CLUSTERS - 1):
        struct.pack_into("<I", image, fat_base + cluster * 4, cluster + 1)

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

    root = bytearray(ROOT_DIRECTORY_SIZE)
    offset = 0
    root[offset] = 0x81
    struct.pack_into("<I", root, offset + 20, 2)
    struct.pack_into("<Q", root, offset + 24, (CLUSTER_COUNT + 7) // 8)
    offset += 32

    root[offset] = 0x82
    struct.pack_into("<I", root, offset + 20, 3)
    struct.pack_into("<Q", root, offset + 24, CLUSTER_SIZE)
    offset += 32

    bin_entries = bytearray(BIN_DIRECTORY_SIZE)
    bin_offset = 0
    etc_entries = bytearray(SMALL_DIRECTORY_SIZE)
    etc_offset = 0
    offset = append_directory_entry(
        root,
        offset,
        file_entry("bin", bin_dir_cluster, b"", attributes=0x10, data_length=BIN_DIRECTORY_SIZE),
        "root",
    )
    offset = append_directory_entry(
        root,
        offset,
        file_entry("etc", etc_dir_cluster, b"", attributes=0x10, data_length=SMALL_DIRECTORY_SIZE),
        "root",
    )
    offset = append_directory_entry(
        root,
        offset,
        file_entry("var", var_dir_cluster, b"", attributes=0x10, data_length=SMALL_DIRECTORY_SIZE),
        "root",
    )
    offset = append_directory_entry(
        root,
        offset,
        file_entry("www", www_dir_cluster, b"", attributes=0x10, data_length=SMALL_DIRECTORY_SIZE),
        "root",
    )

    for name, cluster, data in files:
        entry_set = file_entry(name, cluster, data)
        offset = append_directory_entry(root, offset, entry_set, "root")
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
            bin_offset = append_directory_entry(bin_entries, bin_offset, nested_entry_set, "bin")

    services_entry_set = file_entry("services", services_dir_cluster, b"", attributes=0x10, data_length=SMALL_DIRECTORY_SIZE)
    etc_offset = append_directory_entry(etc_entries, etc_offset, services_entry_set, "etc")
    for name, cluster, data in etc_entries_data:
        entry_set = file_entry(name, cluster, data)
        etc_offset = append_directory_entry(etc_entries, etc_offset, entry_set, "etc")
        for i in range(0, len(data), CLUSTER_SIZE):
            chunk_cluster = cluster + (i // CLUSTER_SIZE)
            file_data = bytearray(CLUSTER_SIZE)
            file_data[:min(CLUSTER_SIZE, len(data) - i)] = data[i:i + CLUSTER_SIZE]
            image[cluster_offset(chunk_cluster):cluster_offset(chunk_cluster) + CLUSTER_SIZE] = file_data

    services_entries = bytearray(SMALL_DIRECTORY_SIZE)
    services_offset = 0
    for name, cluster, data in service_entries_data:
        entry_set = file_entry(name, cluster, data)
        services_offset = append_directory_entry(services_entries, services_offset, entry_set, "etc/services")
        for i in range(0, len(data), CLUSTER_SIZE):
            chunk_cluster = cluster + (i // CLUSTER_SIZE)
            file_data = bytearray(CLUSTER_SIZE)
            file_data[:min(CLUSTER_SIZE, len(data) - i)] = data[i:i + CLUSTER_SIZE]
            image[cluster_offset(chunk_cluster):cluster_offset(chunk_cluster) + CLUSTER_SIZE] = file_data

    var_entries = bytearray(SMALL_DIRECTORY_SIZE)
    var_offset = 0
    log_entry_set = file_entry("log", log_dir_cluster, b"", attributes=0x10, data_length=SMALL_DIRECTORY_SIZE)
    var_offset = append_directory_entry(var_entries, var_offset, log_entry_set, "var")
    log_entries = bytearray(SMALL_DIRECTORY_SIZE)

    www_entries = bytearray(SMALL_DIRECTORY_SIZE)
    www_offset = 0
    assets_entry_set = file_entry("assets", www_assets_dir_cluster, b"", attributes=0x10, data_length=SMALL_DIRECTORY_SIZE)
    www_offset = append_directory_entry(www_entries, www_offset, assets_entry_set, "www")
    for name, cluster, data in www_entries_data:
        entry_set = file_entry(name, cluster, data)
        www_offset = append_directory_entry(www_entries, www_offset, entry_set, "www")
        for i in range(0, len(data), CLUSTER_SIZE):
            chunk_cluster = cluster + (i // CLUSTER_SIZE)
            file_data = bytearray(CLUSTER_SIZE)
            file_data[:min(CLUSTER_SIZE, len(data) - i)] = data[i:i + CLUSTER_SIZE]
            image[cluster_offset(chunk_cluster):cluster_offset(chunk_cluster) + CLUSTER_SIZE] = file_data

    www_assets_entries = bytearray(SMALL_DIRECTORY_SIZE)
    www_assets_offset = 0
    for name, cluster, data in www_assets_entries_data:
        entry_set = file_entry(name, cluster, data)
        www_assets_offset = append_directory_entry(www_assets_entries, www_assets_offset, entry_set, "www/assets")
        for i in range(0, len(data), CLUSTER_SIZE):
            chunk_cluster = cluster + (i // CLUSTER_SIZE)
            file_data = bytearray(CLUSTER_SIZE)
            file_data[:min(CLUSTER_SIZE, len(data) - i)] = data[i:i + CLUSTER_SIZE]
            image[cluster_offset(chunk_cluster):cluster_offset(chunk_cluster) + CLUSTER_SIZE] = file_data

    image[cluster_offset(ROOT_DIRECTORY_CLUSTER):cluster_offset(ROOT_DIRECTORY_CLUSTER) + ROOT_DIRECTORY_SIZE] = root
    image[cluster_offset(bin_dir_cluster):cluster_offset(bin_dir_cluster) + BIN_DIRECTORY_SIZE] = bin_entries
    image[cluster_offset(etc_dir_cluster):cluster_offset(etc_dir_cluster) + SMALL_DIRECTORY_SIZE] = etc_entries
    image[cluster_offset(services_dir_cluster):cluster_offset(services_dir_cluster) + SMALL_DIRECTORY_SIZE] = services_entries
    image[cluster_offset(var_dir_cluster):cluster_offset(var_dir_cluster) + SMALL_DIRECTORY_SIZE] = var_entries
    image[cluster_offset(log_dir_cluster):cluster_offset(log_dir_cluster) + SMALL_DIRECTORY_SIZE] = log_entries
    image[cluster_offset(www_dir_cluster):cluster_offset(www_dir_cluster) + SMALL_DIRECTORY_SIZE] = www_entries
    image[cluster_offset(www_assets_dir_cluster):cluster_offset(www_assets_dir_cluster) + SMALL_DIRECTORY_SIZE] = www_assets_entries

    with open(sys.argv[1], "wb") as output:
        output.write(image)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
