#include "decode.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
/* Decoder allocations are bounded independently of the source file and the
 * final pixel limit. The single worker services one decode at a time. */
struct allocation { size_t size;int mapped;max_align_t alignment; };
static size_t allocated;
static int allocation_failed;
static void *image_alloc(size_t size) {
    if(size>SRV_IMAGE_DECODE_MEMORY || allocated>SRV_IMAGE_DECODE_MEMORY-size || size>SIZE_MAX-sizeof(struct allocation)) { allocation_failed=1;return NULL; }
    int mapped=size>=65536;
    struct allocation *p=mapped?mmap(NULL,sizeof(*p)+size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0):malloc(sizeof(*p)+size);
    if(!p || p==MAP_FAILED) { allocation_failed=1;return NULL; }
    p->size=size;p->mapped=mapped;allocated+=size;return p+1;
}
static void image_free(void *pointer) {
    if(!pointer) return;
    struct allocation *p=(struct allocation *)pointer-1;allocated-=p->size;
    if(p->mapped) munmap(p,sizeof(*p)+p->size);else free(p);
}
static void *image_realloc(void *pointer,size_t size) {
    if(!pointer) return image_alloc(size);
    struct allocation *p=(struct allocation *)pointer-1;
    if(size<=p->size) return pointer;
    /* Charge the temporary copy too: the decode limit is a peak-memory bound. */
    void *next=image_alloc(size);if(!next) return NULL;
    memcpy(next,pointer,p->size);image_free(pointer);return next;
}

#define STBI_MALLOC image_alloc
#define STBI_REALLOC image_realloc
#define STBI_FREE image_free
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_MAX_DIMENSIONS SRV_IMAGE_MAX_DIMENSION
#define STB_IMAGE_IMPLEMENTATION
#include "../../ports/upstream/stb/stb_image.h"
unsigned image_decode(const void *data,size_t length,uint32_t **pixels,uint32_t *width,uint32_t *height) {
    if(!data || !pixels || !width || !height || !length) return DESK_IMAGE_INVALID;
    if(length>SRV_IMAGE_MAX_FILE) return DESK_IMAGE_LIMIT;
    allocation_failed=0;
    int w,h,channels;
    if(!stbi_info_from_memory(data,(int)length,&w,&h,&channels)) return DESK_IMAGE_FORMAT;
    if(w<=0 || h<=0 || w>(int)SRV_IMAGE_MAX_DIMENSION || h>(int)SRV_IMAGE_MAX_DIMENSION || (uint64_t)w*h>SRV_IMAGE_MAX_PIXELS) return DESK_IMAGE_LIMIT;
    unsigned char *rgba=stbi_load_from_memory(data,(int)length,&w,&h,&channels,4);
    if(!rgba) return allocation_failed?DESK_IMAGE_MEMORY:DESK_IMAGE_FORMAT;
    if(w<=0 || h<=0 || (uint64_t)w*h>SRV_IMAGE_MAX_PIXELS) { stbi_image_free(rgba);return DESK_IMAGE_LIMIT; }
    uint32_t *out=(uint32_t *)rgba;
    for(size_t i=0;i<(size_t)w*(size_t)h;i++) {
        unsigned char *p=rgba+i*4;out[i]=((uint32_t)p[3]<<24)|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
    }
    *pixels=out;*width=(uint32_t)w;*height=(uint32_t)h;return DESK_IMAGE_OK;
}
void image_decode_free(uint32_t *pixels) { stbi_image_free(pixels); }

const char *image_decode_reason(void) { const char *reason=stbi_failure_reason();return reason?reason:"unknown"; }
