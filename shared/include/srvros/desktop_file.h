#ifndef SRVROS_DESKTOP_FILE_H
#define SRVROS_DESKTOP_FILE_H
#include <stdint.h>
#define DESK_FILE_MAGIC 0x53444632u
#define DESK_FILE_BYTES 65536
#define DESK_FILE_PATH 160
#define DESK_FILE_ENTRIES 256
enum desk_file_operation { DESK_FILE_READ=1, DESK_FILE_WRITE, DESK_FILE_LIST, DESK_FILE_MKDIR, DESK_FILE_RENAME, DESK_FILE_DELETE, DESK_FILE_COPY };
enum desk_file_status { DESK_FILE_OK, DESK_FILE_IO, DESK_FILE_EXISTS, DESK_FILE_CHANGED,
    DESK_FILE_TOO_LARGE, DESK_FILE_INVALID, DESK_FILE_NOT_FOUND, DESK_FILE_BUSY };
#define DESK_FILE_EXPECT_EXISTING 1u
struct desk_file_request {
    uint32_t magic,id,operation,length,flags,expected_crc;
    char path[DESK_FILE_PATH];
};
struct desk_file_response {
    uint32_t magic,id,status,length,count,crc,truncated,existing_crc;
};
struct desk_file_entry { char name[96]; uint64_t size,inode,modified; uint32_t directory,reserved; };
/* Mutation payload captures the selected listing identity. Rename/copy never
 * overwrite a destination. Directory changes remain limited by the filesystem. */
struct desk_file_change {
    uint64_t inode,size,modified;
    uint32_t directory,reserved;
    char destination[DESK_FILE_PATH];
};
static inline int desk_file_payload_valid(unsigned operation,unsigned length,unsigned flags) {
    if(operation<DESK_FILE_READ || operation>DESK_FILE_COPY || length>DESK_FILE_BYTES) return 0;
    if(operation>=DESK_FILE_RENAME) return length==sizeof(struct desk_file_change) && !flags;
    return (operation==DESK_FILE_WRITE || !length) && flags<=DESK_FILE_EXPECT_EXISTING;
}
static inline uint32_t desk_file_crc(const void *data,uint32_t size) {
    const unsigned char *p=data;uint32_t c=~0u;
    for(uint32_t i=0;i<size;i++) { c^=p[i];for(unsigned j=0;j<8;j++) c=(c>>1)^(0xedb88320u&(0u-(c&1))); }
    return ~c;
}
#endif
