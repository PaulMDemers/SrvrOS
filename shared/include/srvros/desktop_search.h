#ifndef SRVROS_DESKTOP_SEARCH_H
#define SRVROS_DESKTOP_SEARCH_H
#include <stdint.h>
#define DESK_SEARCH_MAGIC 0x53524631u
#define DESK_SEARCH_VERSION 1u
#define DESK_SEARCH_CONTENT_VERSION 2u
#define DESK_SEARCH_NAME 0u
#define DESK_SEARCH_CONTENTS 1u
#define DESK_SEARCH_CONTENT_FILE_BYTES (64u*1024u)
#define DESK_SEARCH_CONTENT_TOTAL_BYTES (16u*1024u*1024u)
#define DESK_SEARCH_CONTENT_SCAN_LIMIT 65536u
#define DESK_SEARCH_LIMIT 256u
#define DESK_SEARCH_PATH 160u
#define DESK_SEARCH_QUERY 96u
#define DESK_SEARCH_TRUNCATED 1u
#define DESK_SEARCH_BYTE_LIMIT 2u
#define DESK_SEARCH_SCAN_LIMIT 4u
struct desk_search_header {
    uint32_t magic,version,count,flags;
    uint64_t scanned,matched,skipped;
};
struct desk_search_entry {
    char path[DESK_SEARCH_PATH];
    uint64_t inode,size,modified;
    uint32_t directory,reserved;
};
#define DESK_SEARCH_BYTES (sizeof(struct desk_search_header)+DESK_SEARCH_LIMIT*sizeof(struct desk_search_entry))
#endif
