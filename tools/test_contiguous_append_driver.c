/* Observation-only wrapper around the existing real image-backed driver.
 * The production exFAT still calls ordinary block_read/write/flush functions. */
#define main contiguous_append_backend_main
#define block_read contiguous_append_backend_read
#define console_printf contiguous_append_discard_console_printf
#include "exfat_host_driver.c"
#undef block_read
#undef main
#undef console_printf
unsigned contiguous_append_read_role(void);
extern unsigned contiguous_append_round;
bool block_read(struct block_device *d,uint64_t offset,void *buffer,uint64_t length) {
    bool ok=contiguous_append_backend_read(d,offset,buffer,length);
    if(getenv("SRVROS_TRACE_WRITES"))
        printf("contiguous-read: round=%u role=%u offset=%llu bytes=%llu ok=%u\n",
            contiguous_append_round,contiguous_append_read_role(),
            (unsigned long long)offset,(unsigned long long)length,ok);
    return ok;
}

/* Kernel console_printf uses uint64_t for its small formatter's %u/%x. */
#include <stdarg.h>
void console_printf(const char *format,...) {
    va_list args;va_start(args,format);
    while(*format) {
        if(*format!='%') {putchar(*format++);continue;}
        format++;
        if(*format=='u')printf("%llu",(unsigned long long)va_arg(args,uint64_t));
        else if(*format=='x')printf("%llx",(unsigned long long)va_arg(args,uint64_t));
        else if(*format=='s')fputs(va_arg(args,const char *),stdout);
        else if(*format=='%')putchar('%');
        else { assert(!"unexpected kernel formatter conversion"); }
        if(*format)format++;
    }
    va_end(args);
}

int main(int argc,char **argv) {
    int result=contiguous_append_backend_main(argc,argv);
    exfat_print_sidecar_equality();
    return result;
}
