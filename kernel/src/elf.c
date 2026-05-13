#include <srvros/elf.h>

#include <stddef.h>
#include <stdint.h>

#define EI_NIDENT 16
#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EV_CURRENT 1

struct elf64_header {
    uint8_t ident[EI_NIDENT];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed));

bool elf64_probe(const uint8_t *data, uint64_t size, struct elf64_info *info) {
    if (data == NULL || size < sizeof(struct elf64_header) || info == NULL) {
        return false;
    }

    const struct elf64_header *header = (const struct elf64_header *)data;
    if (header->ident[0] != ELFMAG0 ||
        header->ident[1] != ELFMAG1 ||
        header->ident[2] != ELFMAG2 ||
        header->ident[3] != ELFMAG3 ||
        header->ident[4] != ELFCLASS64 ||
        header->ident[5] != ELFDATA2LSB ||
        header->ident[6] != EV_CURRENT ||
        header->version != EV_CURRENT) {
        return false;
    }

    if (header->phoff > size ||
        (header->phnum != 0 && header->phentsize > (size - header->phoff) / header->phnum)) {
        return false;
    }

    *info = (struct elf64_info) {
        .entry = header->entry,
        .type = header->type,
        .machine = header->machine,
        .program_header_count = header->phnum,
        .program_header_offset = header->phoff,
    };
    return true;
}
