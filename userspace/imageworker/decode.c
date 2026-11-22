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
/* EXIF offsets are relative to the TIFF header and never to the JPEG file. */
static uint16_t exif_u16(const unsigned char *p,int little) {
    return little?(uint16_t)(p[0]|(uint16_t)p[1]<<8):(uint16_t)((uint16_t)p[0]<<8|p[1]);
}
static uint32_t exif_u32(const unsigned char *p,int little) {
    return little?((uint32_t)exif_u16(p,1)|((uint32_t)exif_u16(p+2,1)<<16)):
        (((uint32_t)exif_u16(p,0)<<16)|exif_u16(p+2,0));
}
static unsigned exif_orientation(const unsigned char *t,size_t size) {
    if(size<8) return 1;
    int little;
    if(t[0]=='I' && t[1]=='I') little=1;
    else if(t[0]=='M' && t[1]=='M') little=0;
    else return 1;
    if(exif_u16(t+2,little)!=42) return 1;
    uint32_t offset=exif_u32(t+4,little);
    if(offset<8 || offset>size || size-offset<2) return 1;
    unsigned count=exif_u16(t+offset,little);
    size_t entries=(size_t)offset+2;
    if(size-entries<4 || count>(size-entries-4)/12) return 1;
    for(unsigned i=0;i<count;i++) {
        const unsigned char *e=t+entries+(size_t)i*12;
        if(exif_u16(e,little)!=0x0112) continue;
        if(exif_u16(e+2,little)!=3 || exif_u32(e+4,little)!=1) return 1;
        unsigned value=exif_u16(e+8,little);
        return value>=1 && value<=8?value:1;
    }
    return 1;
}
static unsigned jpeg_orientation(const unsigned char *data,size_t length) {
    if(length<2 || data[0]!=0xff || data[1]!=0xd8) return 1;
    size_t pos=2;
    while(pos<length) {
        if(data[pos++]!=0xff) return 1;
        while(pos<length && data[pos]==0xff) pos++;
        if(pos==length) return 1;
        unsigned marker=data[pos++];
        if(marker==0xda || marker==0xd9 || marker==0x00) return 1;
        if(marker==0x01 || marker==0xd8 || (marker>=0xd0 && marker<=0xd7)) continue;
        if(length-pos<2) return 1;
        size_t segment=((size_t)data[pos]<<8)|data[pos+1];
        if(segment<2 || segment>length-pos) return 1;
        if(marker==0xe1 && segment>=8 && !memcmp(data+pos+2,"Exif\0\0",6))
            return exif_orientation(data+pos+8,segment-8);
        pos+=segment;
    }
    return 1;
}
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
    unsigned orientation=jpeg_orientation(data,length);
    uint32_t *out=(uint32_t *)rgba;
    int out_w=orientation>=5?h:w, out_h=orientation>=5?w:h;
    if(orientation!=1) {
        out=image_alloc((size_t)w*(size_t)h*sizeof(*out));
        if(!out) { stbi_image_free(rgba);return DESK_IMAGE_MEMORY; }
    }
    for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
        int dx=x,dy=y;
        switch(orientation) {
        case 2: dx=w-1-x; break;
        case 3: dx=w-1-x;dy=h-1-y; break;
        case 4: dy=h-1-y; break;
        case 5: dx=y;dy=x; break;
        case 6: dx=h-1-y;dy=x; break;
        case 7: dx=h-1-y;dy=w-1-x; break;
        case 8: dx=y;dy=w-1-x; break;
        }
        unsigned char *p=rgba+((size_t)y*w+x)*4;
        out[(size_t)dy*out_w+dx]=((uint32_t)p[3]<<24)|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
    }
    if(orientation!=1) stbi_image_free(rgba);
    *pixels=out;*width=(uint32_t)out_w;*height=(uint32_t)out_h;return DESK_IMAGE_OK;
}
void image_decode_free(uint32_t *pixels) { stbi_image_free(pixels); }

const char *image_decode_reason(void) { const char *reason=stbi_failure_reason();return reason?reason:"unknown"; }
