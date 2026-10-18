#ifndef SRVROS_TRIAL_FILE_H
#define SRVROS_TRIAL_FILE_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
static inline uint32_t trial_crc(const char *data,size_t n) {
    uint32_t c=~0u;
    for (size_t i=0;i<n;i++) {
        c^=(unsigned char)data[i];
        for (unsigned j=0;j<8;j++) c=(c>>1)^(0xedb88320u&(0u-(c&1)));
    }
    return ~c;
}
static inline int trial_write_all(int fd,const void *data,size_t n) {
    const char *p=data;
    while (n) { ssize_t w=write(fd,p,n); if (w<=0) return -1; p+=w; n-=(size_t)w; }
    return 0;
}
static inline int trial_record(const char *path,const char *body) {
    char tail[40]; size_t n=strlen(body);
    int len=snprintf(tail,sizeof(tail),"crc32=%u\n",trial_crc(body,n));
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);
    if (fd<0) return -1;
    int ok=trial_write_all(fd,body,n)==0 && trial_write_all(fd,tail,(size_t)len)==0 && fsync(fd)==0;
    if (close(fd)!=0) ok=0;
    return ok?0:-1;
}
#endif
