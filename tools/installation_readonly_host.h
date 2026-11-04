/* Shared host platform shim for read-only kernel exFAT drivers. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <srvros/block.h>
#include <srvros/exfat.h>
#include <srvros/vfs.h>

#define HEAP_LIMIT (128u * 1024u * 1024u)
#define READ_LIMIT (2ULL * 1024 * 1024 * 1024)
#define CALL_LIMIT 1000000u
static int source_fd;
static uint64_t base, length, read_bytes, read_calls, live_bytes;
static unsigned writes, flushes, io_depth;
static bool read_failed;
struct allocation { max_align_t align; uint64_t size; };
void *kmalloc(uint64_t n) {
    if(n>SIZE_MAX-sizeof(struct allocation) || n>HEAP_LIMIT-live_bytes)return NULL;
    struct allocation *a=malloc(sizeof(*a)+(size_t)n); if(!a)return NULL;
    a->size=n;live_bytes+=n;return a+1;
}
void *kcalloc(uint64_t n,uint64_t s) {
    if(s && n>UINT64_MAX/s)return NULL;
    void *p=kmalloc(n*s);if(p)memset(p,0,(size_t)(n*s));return p;
}
void kfree(void *p) { if(p) { struct allocation *a=(struct allocation *)p-1;live_bytes-=a->size;free(a); } }
bool scheduler_io_begin(void) { io_depth++;return true; }
void scheduler_io_end(void) { assert(io_depth);io_depth--; }
bool scheduler_io_active(void) { return io_depth!=0; }
uint64_t timer_ticks(void) { return 100; }
int64_t timer_realtime_us(void) { return 0; }
void console_write(const char *s) { (void)s; }
void console_printf(const char *fmt,...) { (void)fmt; }
uint64_t block_device_size(const struct block_device *d) { return d->block_size*d->block_count; }
struct block_device *block_parent(struct block_device *d) { return d; }
bool block_read(struct block_device *d,uint64_t o,void *p,uint64_t n) {
    if(o>block_device_size(d) || n>block_device_size(d)-o || n>SSIZE_MAX ||
       n>READ_LIMIT-read_bytes || ++read_calls>CALL_LIMIT) { read_failed=true;return false; }
    read_bytes+=n;
    if(pread(source_fd,p,(size_t)n,(off_t)(base+o))!=(ssize_t)n) { read_failed=true;return false; }
    return true;
}
bool block_write(struct block_device *d,uint64_t o,const void *p,uint64_t n) {
    (void)d;(void)o;(void)p;(void)n;writes++;return false;
}
static bool refuse_flush(struct block_device *d) { (void)d;flushes++;return false; }
bool process_has_open_vfs_prefix(const char *p) { (void)p;return false; }
#define SRVROS_BLOCK_CLAIM_HOST_TEST
#include "../kernel/src/block_claim.c"

