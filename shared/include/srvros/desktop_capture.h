#ifndef SRVROS_DESKTOP_CAPTURE_H
#define SRVROS_DESKTOP_CAPTURE_H
#include <stdint.h>
#define DESK_CAPTURE_MAGIC 0x31504853u
#define DESK_CAPTURE_PATH 160
#define DESK_CAPTURE_FOLDER "/fat/home/Screenshots"
/* BMPs stay inside Preview's 8 MiB input limit. Includes full HD and the Air. */
#define DESK_CAPTURE_MAX_BYTES (8u*1024u*1024u)
enum { DESK_CAPTURE_OK,DESK_CAPTURE_INVALID,DESK_CAPTURE_MEMORY,DESK_CAPTURE_IO,DESK_CAPTURE_EXISTS };
struct desk_capture_request { uint32_t magic,id,width,height;char path[DESK_CAPTURE_PATH]; };
struct desk_capture_response { uint32_t magic,id,status,reserved; };
static inline uint32_t desk_capture_bytes(uint32_t width,uint32_t height) {
    if(!width || !height || width>4096 || height>4096 || width>(DESK_CAPTURE_MAX_BYTES-54)/4/height) return 0;
    return width*height*4;
}
#endif
