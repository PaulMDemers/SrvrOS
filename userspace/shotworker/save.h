#ifndef SHOTWORKER_SAVE_H
#define SHOTWORKER_SAVE_H
#include <srvros/desktop_capture.h>
#include <stddef.h>
/* New destination only. A failed save never reports success or replaces a file. */
typedef void (*capture_progress_fn)(void *,unsigned,uint64_t);
unsigned capture_save_progress(const char *path,const unsigned char *data,size_t length,capture_progress_fn progress,void *context);
unsigned capture_save(const char *path,const unsigned char *data,size_t length);
#endif
