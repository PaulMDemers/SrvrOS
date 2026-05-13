#include <srvros/console.h>
#include <srvros/initramfs.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TAR_BLOCK_SIZE 512
#define INITRAMFS_MAX_FILES 64

struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
} __attribute__((packed));

static struct initramfs_file files[INITRAMFS_MAX_FILES];
static uint64_t file_count;
static char paths[INITRAMFS_MAX_FILES][256];

static bool memory_is_zero(const uint8_t *data, uint64_t size) {
    for (uint64_t i = 0; i < size; i++) {
        if (data[i] != 0) {
            return false;
        }
    }
    return true;
}

static uint64_t align_up(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static uint64_t parse_octal(const char *s, uint64_t max_len) {
    uint64_t value = 0;

    for (uint64_t i = 0; i < max_len; i++) {
        if (s[i] == '\0' || s[i] == ' ') {
            break;
        }
        if (s[i] < '0' || s[i] > '7') {
            break;
        }
        value = (value << 3) + (uint64_t)(s[i] - '0');
    }

    return value;
}

static bool streq(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static void copy_component(char *dst, uint64_t *offset, uint64_t dst_size, const char *src, uint64_t src_size) {
    for (uint64_t i = 0; i < src_size && src[i] != '\0' && *offset + 1 < dst_size; i++) {
        dst[(*offset)++] = src[i];
    }
}

static void build_path(char *dst, uint64_t dst_size, const struct tar_header *header) {
    uint64_t offset = 0;

    if (dst_size == 0) {
        return;
    }

    if (header->prefix[0] != '\0') {
        if (offset + 1 < dst_size) {
            dst[offset++] = '/';
        }
        copy_component(dst, &offset, dst_size, header->prefix, sizeof(header->prefix));
        if (offset + 1 < dst_size) {
            dst[offset++] = '/';
        }
    } else if (offset + 1 < dst_size) {
        dst[offset++] = '/';
    }

    const char *name = header->name;
    if (name[0] == '.' && name[1] == '/') {
        name += 2;
    }

    copy_component(dst, &offset, dst_size, name, sizeof(header->name));
    dst[offset] = '\0';
}

void initramfs_init(const void *address, uint64_t size) {
    const uint8_t *archive = address;
    uint64_t offset = 0;

    file_count = 0;

    if (archive == NULL || size < TAR_BLOCK_SIZE) {
        console_write("initramfs: unavailable\n");
        return;
    }

    while (offset + TAR_BLOCK_SIZE <= size) {
        const struct tar_header *header = (const struct tar_header *)(archive + offset);
        if (memory_is_zero((const uint8_t *)header, TAR_BLOCK_SIZE)) {
            break;
        }

        uint64_t file_size = parse_octal(header->size, sizeof(header->size));
        const uint8_t *file_data = archive + offset + TAR_BLOCK_SIZE;

        if ((header->typeflag == '0' || header->typeflag == '\0') &&
            file_count < INITRAMFS_MAX_FILES) {
            build_path(paths[file_count], sizeof(paths[file_count]), header);
            files[file_count] = (struct initramfs_file) {
                .path = paths[file_count],
                .data = file_data,
                .size = file_size,
            };
            file_count++;
        }

        offset += TAR_BLOCK_SIZE + align_up(file_size, TAR_BLOCK_SIZE);
    }

    console_printf("initramfs: files=%u size=%u bytes\n", file_count, size);
}

uint64_t initramfs_file_count(void) {
    return file_count;
}

const struct initramfs_file *initramfs_file_at(uint64_t index) {
    if (index >= file_count) {
        return NULL;
    }

    return &files[index];
}

const struct initramfs_file *initramfs_find(const char *path) {
    for (uint64_t i = 0; i < file_count; i++) {
        if (streq(files[i].path, path)) {
            return &files[i];
        }
    }

    return NULL;
}
