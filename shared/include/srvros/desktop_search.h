#ifndef SRVROS_DESKTOP_SEARCH_H
#define SRVROS_DESKTOP_SEARCH_H
#include <stdint.h>
#define DESK_SEARCH_MAGIC 0x53524631u
#define DESK_SEARCH_VERSION 1u
#define DESK_SEARCH_LIMIT 256u
#define DESK_SEARCH_PATH 160u
#define DESK_SEARCH_QUERY 96u
#define DESK_SEARCH_TRUNCATED 1u
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
