#ifndef SRVROS_ELF_H
#define SRVROS_ELF_H

#include <stdbool.h>
#include <stdint.h>

struct elf64_info {
    uint64_t entry;
    uint16_t type;
    uint16_t machine;
    uint16_t program_header_count;
    uint64_t program_header_offset;
};

bool elf64_probe(const uint8_t *data, uint64_t size, struct elf64_info *info);

#endif

