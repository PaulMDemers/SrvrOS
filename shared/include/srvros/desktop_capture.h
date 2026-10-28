#ifndef SRVROS_DESKTOP_CAPTURE_H
#define SRVROS_DESKTOP_CAPTURE_H
#include <stdint.h>
#define DESK_CAPTURE_MAGIC 0x32504853u
#define DESK_CAPTURE_PROGRESS_MAGIC 0x32504753u
#define DESK_CAPTURE_PATH 160
#define DESK_CAPTURE_FOLDER "/fat/home/Screenshots"
/* BMPs stay inside Preview's 8 MiB input limit. Includes full HD and the Air. */
#define DESK_CAPTURE_MAX_BYTES (8u*1024u*1024u)
enum { DESK_CAPTURE_OK,DESK_CAPTURE_INVALID,DESK_CAPTURE_MEMORY,DESK_CAPTURE_IO,DESK_CAPTURE_EXISTS };
struct desk_capture_request { uint32_t magic,id,width,height;char path[DESK_CAPTURE_PATH]; };
enum { CAPTURE_RECEIVE=1,CAPTURE_ENCODE,CAPTURE_PREPARE,CAPTURE_WRITE,
    CAPTURE_FSYNC,CAPTURE_TEMP_SYNC,CAPTURE_TEMP_VERIFY,CAPTURE_RENAME,
    CAPTURE_FINAL_SYNC,CAPTURE_FINAL_VERIFY,CAPTURE_CLEANUP,CAPTURE_RELEASE,
    CAPTURE_PHASE_COUNT };
/* Version 2: progress and completion share a bounded frame. A progress frame
 * never acknowledges a durable save; only the final validated response does. */
struct desk_capture_response { uint32_t magic,id,status,reserved;uint64_t at_us,bytes; };
static inline uint32_t desk_capture_bytes(uint32_t width,uint32_t height) {
    if(!width || !height || width>4096 || height>4096 || width>(DESK_CAPTURE_MAX_BYTES-54)/4/height) return 0;
    return width*height*4;
}
/* Only the unattended capture caller opts into this policy. */
static inline unsigned desk_capture_scale(uint32_t width,uint32_t height) {
    if(!width || !height || width>4096 || height>4096) return 0;
    if(desk_capture_bytes(width,height)) return 1;
    return desk_capture_bytes((width+1)/2,(height+1)/2)?2:0;
}
#endif
