#ifndef SRVROS_DESKTOP_FILE_H
#define SRVROS_DESKTOP_FILE_H
#include <stdint.h>
#define DESK_FILE_MAGIC 0x53444632u
#define DESK_FILE_BYTES 65536
#define DESK_FILE_COPY_BYTES (16u*1024u*1024u)
#define DESK_FILE_PATH 160
#define DESK_FILE_ENTRIES 256
enum desk_file_operation { DESK_FILE_READ=1, DESK_FILE_WRITE, DESK_FILE_LIST, DESK_FILE_MKDIR, DESK_FILE_RENAME, DESK_FILE_DELETE, DESK_FILE_COPY, DESK_FILE_STAT };
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
/* Read-only probe. An expected inode of zero accepts the current path. */
struct desk_file_probe { uint64_t inode; };
struct desk_file_info { uint64_t inode,size;int64_t modified;uint32_t kind,permissions; };
enum { DESK_FILE_KIND_FILE=1,DESK_FILE_KIND_FOLDER,DESK_FILE_KIND_OTHER };
_Static_assert(sizeof(struct desk_file_info)==32,"file info wire layout");
/* Mutation payload captures the selected listing identity. Rename/copy never
 * overwrite a destination. Directory changes remain limited by the filesystem. */
struct desk_file_change {
    uint64_t inode,size,modified;
    uint32_t directory,reserved;
    char destination[DESK_FILE_PATH];
};
static inline int desk_file_payload_valid(unsigned operation,unsigned length,unsigned flags) {
    if(operation<DESK_FILE_READ || operation>DESK_FILE_STAT || length>DESK_FILE_BYTES) return 0;
    if(operation==DESK_FILE_STAT) return length==sizeof(struct desk_file_probe) && !flags;
    if(operation>=DESK_FILE_RENAME) return length==sizeof(struct desk_file_change) && !flags;
    return (operation==DESK_FILE_WRITE || !length) && flags<=DESK_FILE_EXPECT_EXISTING;
}
/* Streaming CRC-32/ISO-HDLC: begin with ~0u and complement the final state.
 * A 16-entry nibble table keeps the header small while avoiding eight bit steps
 * per byte for larger file verification. Existing wire checksums are unchanged. */
static inline uint32_t desk_file_crc_update(uint32_t crc,const void *data,uint32_t size) {
    static const uint32_t table[16]={
        0x00000000u,0x1db71064u,0x3b6e20c8u,0x26d930acu,
        0x76dc4190u,0x6b6b51f4u,0x4db26158u,0x5005713cu,
        0xedb88320u,0xf00f9344u,0xd6d6a3e8u,0xcb61b38cu,
        0x9b64c2b0u,0x86d3d2d4u,0xa00ae278u,0xbdbdf21cu};
    const unsigned char *p=data;
    for(uint32_t i=0;i<size;i++) { crc^=p[i];crc=(crc>>4)^table[crc&15];crc=(crc>>4)^table[crc&15]; }
    return crc;
}
static inline uint32_t desk_file_crc(const void *data,uint32_t size) { return ~desk_file_crc_update(~0u,data,size); }
#endif
