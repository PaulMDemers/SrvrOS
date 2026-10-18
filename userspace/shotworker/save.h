#ifndef SHOTWORKER_SAVE_H
#define SHOTWORKER_SAVE_H
#include <srvros/desktop_capture.h>
#include <stddef.h>
/* New destination only. A failed save never reports success or replaces a file. */
unsigned capture_save(const char *path,const unsigned char *data,size_t length);
#endif
