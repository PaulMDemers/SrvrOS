#ifndef SRVROS_DESK_LOG_H
#define SRVROS_DESK_LOG_H
#include <stdarg.h>
#include <stdio.h>
#include <srvros/sys.h>
/* Desktop diagnostics must not initialize stdio buffers or query pthread /
 * signal state in the input loop. Format into bounded caller-owned storage and
 * use the console write directly. Redirected output retains the kernel gate. */
static inline void desk_log(const char *format, ...) {
    char text[384];
    va_list args;
    va_start(args,format);
    vsnprintf(text,sizeof(text),format,args);
    va_end(args);
    srv_puts(text);
}
#endif
