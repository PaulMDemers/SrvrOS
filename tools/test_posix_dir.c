#define _DEFAULT_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdint.h>
#include <unistd.h>
#define _DIRENT_H 1
#include "../userspace/lib/include/dirent.h"
static int allocation_fail,open_fail,read_fail=-1,opened,closed,position,open_flags;
static void *test_realloc(void *p,size_t n) { return allocation_fail?NULL:realloc(p,n); }
static void *test_calloc(size_t n,size_t size) { return allocation_fail?NULL:calloc(n,size); }
static int test_open(const char *path,int flags,...) {
    assert(!strcmp(path,"/fat"));
    if(open_fail) { errno=EACCES;return -1; }
    opened++;position=0;open_flags=flags;return 40;
}
static int test_close(int fd) { assert(fd==40);closed++;return 0; }
static int test_fstat(int fd,struct stat *st) {
    if(fd<0) { errno=EBADF;return -1; }
    memset(st,0,sizeof(*st));st->st_mode=fd==40?S_IFDIR:S_IFREG;return 0;
}
static int test_fcntl(int fd,int command,...) { assert(fd==40 && command==F_GETFL);return O_RDONLY; }
static off_t test_lseek(int fd,off_t offset,int whence) {
    assert(fd==40);if(whence==SEEK_CUR) return position;
    assert(whence==SEEK_SET);position=offset;return position;
}
#define realloc test_realloc
#define calloc test_calloc
#define open test_open
#define close test_close
#define fstat(...) test_fstat(__VA_ARGS__)
#define fcntl test_fcntl
#define lseek test_lseek
#define opendir test_opendir
#define fdopendir test_fdopendir
#define dirfd test_dirfd
#define readdir test_readdir
#define readdir64 test_readdir64
#define telldir test_telldir
#define seekdir test_seekdir
#define rewinddir test_rewinddir
#define closedir test_closedir
#define alphasort test_alphasort
#define scandir test_scandir
#define scandir64 test_scandir64
#include "../userspace/lib/src/posix_dir.c"
#undef realloc
#undef calloc
long srv_directory(uint64_t operation,int fd,void *out,size_t size) {
    assert(operation==SRV_DIRECTORY_READ && fd==40 && size==sizeof(struct srv_dirent));
    if(position==read_fail) return SRV_ERR_IO;
    if(position==203) return 0;
    struct srv_dirent *entry=out;memset(entry,0,size);entry->inode=position+1;
    if(position<2) { strcpy(entry->name,position?"..":".");entry->type=1; }
    else if(position==202) { memset(entry->name,'x',120); }
    else snprintf(entry->name,sizeof(entry->name),"file%03d",position-2);
    position++;return 1;
}
int main(void) {
    DIR *d=opendir("/fat");assert(d && dirfd(d)==40 && (open_flags&O_CLOEXEC) && (open_flags&O_DIRECTORY));
    errno=EDOM;unsigned count=0;struct dirent *e;
    while((e=readdir(d))) { assert(e->d_ino);count++; }
    assert(count==203 && errno==EDOM);rewinddir(d);
    assert(!strcmp(readdir(d)->d_name,"."));long cookie=telldir(d);
    assert(!strcmp(readdir(d)->d_name,".."));seekdir(d,cookie);
    assert(!strcmp(readdir(d)->d_name,".."));assert(!closedir(d) && closed==opened);
    assert(!fdopendir(-1) && errno==EBADF);assert(!fdopendir(41) && errno==ENOTDIR);
    allocation_fail=1;assert(!opendir("/fat") && errno==ENOMEM && opened==closed);allocation_fail=0;
    open_fail=1;assert(!opendir("/fat") && errno==EACCES);open_fail=0;
    d=opendir("/fat");read_fail=0;errno=0;assert(!readdir(d) && errno==EIO);closedir(d);
    struct dirent **list=(void *)1;assert(scandir("/fat",&list,NULL,NULL)<0 && !list && errno==EIO);read_fail=-1;
    assert(scandir("/fat",&list,NULL,alphasort)==203);
    for(unsigned i=0;i<203;i++) free(list[i]);
    free(list);
    assert(opened==closed);
    puts("posix-dir: PASS descriptor ownership, CLOEXEC, cookies, >64 entries, long names, allocation/permission/read errors and scandir cleanup");
}
