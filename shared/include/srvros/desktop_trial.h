#ifndef SRVROS_DESKTOP_TRIAL_H
#define SRVROS_DESKTOP_TRIAL_H
#include <stdint.h>
#define DESK_IO_MAGIC 0x53494f31u
#define DESK_IO_BYTES (256u*1024u)
#define DESK_IO_ROUNDS 128u
enum { DESK_IO_STARTING,DESK_IO_RUNNING,DESK_IO_COMPLETE,DESK_IO_STOPPED,DESK_IO_FAILED };
struct desk_io_progress { uint32_t magic,rounds,state,reserved; };
static inline int desk_trial_run_valid(const char *s) {
    unsigned i=0;
    for (;s[i];i++) if (i>=16 || !((s[i]>='0' && s[i]<='9') || (s[i]>='a' && s[i]<='f'))) return 0;
    return i==16;
}
static inline unsigned char desk_trial_byte(const char *run,unsigned round,unsigned offset) {
    return (unsigned char)(offset*17u+round*29u+(unsigned char)run[offset%16]);
}
#endif
