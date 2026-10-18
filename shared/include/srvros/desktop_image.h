#ifndef SRVROS_DESKTOP_IMAGE_H
#define SRVROS_DESKTOP_IMAGE_H
#include <stdint.h>
#include <stddef.h>
#include <srvros/image_limits.h>
#define DESK_IMAGE_MAGIC 0x53494d31u
#define DESK_IMAGE_PATH 160
#define DESK_IMAGE_HASH_INITIAL 2166136261u
enum desk_image_status { DESK_IMAGE_OK,DESK_IMAGE_NOT_FOUND,DESK_IMAGE_IO,DESK_IMAGE_FORMAT,DESK_IMAGE_LIMIT,DESK_IMAGE_MEMORY,DESK_IMAGE_INVALID };
struct desk_image_request { uint32_t magic,id;char path[DESK_IMAGE_PATH]; };
struct desk_image_response { uint32_t magic,id,status,width,height,length,checksum,reserved; };
/* Incremental FNV-1a integrity check for the local worker pipe, not authentication. */
static inline uint32_t desk_image_hash(uint32_t hash,const void *data,size_t size) {
    const unsigned char *p=data;for(size_t i=0;i<size;i++) hash=(hash^p[i])*16777619u;return hash;
}
#endif
