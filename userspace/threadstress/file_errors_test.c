#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <termios.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)) { printf("threadstress: file-errors line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
int posix_file_errors_test(void) {
    const char *path="/fat/file-errors.tmp";unlink(path);
    int fd=open(path,O_CREAT|O_RDWR|O_TRUNC,0600);CHECK(fd>=0 && write(fd,"abc",3)==3);
    CHECK(lseek(fd,-1,SEEK_SET)==-1 && errno==EINVAL);
    CHECK(lseek(fd,0,987)==-1 && errno==EINVAL);
    CHECK(lseek(fd,INT64_MAX,SEEK_CUR)==-1 && errno==EOVERFLOW);
    CHECK(lseek(fd,0,SEEK_CUR)==3);
    CHECK(lseek(-1,0,SEEK_SET)==-1 && errno==EBADF);
    CHECK(!isatty(fd) && errno==ENOTTY && !isatty(-1) && errno==EBADF);
    struct termios attr={0};
    CHECK(tcgetattr(fd,&attr)==-1 && errno==ENOTTY);
    CHECK(tcgetattr(-1,&attr)==-1 && errno==EBADF);
    CHECK(tcsetattr(fd,TCSANOW,&attr)==-1 && errno==ENOTTY);
    CHECK(tcsetattr(-1,TCSANOW,&attr)==-1 && errno==EBADF);
    CHECK(fsync(-1)==-1 && errno==EBADF);
    CHECK(!fsync(fd));
    int p[2];CHECK(!pipe(p));
    CHECK(lseek(p[0],0,SEEK_SET)==-1 && errno==ESPIPE);
    CHECK(fsync(p[1])==-1 && errno==EINVAL);
    CHECK(!close(p[0]) && !close(p[1]));
    struct stat st;struct statvfs vfs;
    char child[64];snprintf(child,sizeof(child),"%s/child",path);
    CHECK(stat(child,&st)==-1 && errno==ENOTDIR);
    CHECK(statvfs(child,&vfs)==-1 && errno==ENOTDIR);
    CHECK(chmod(child,0600)==-1 && errno==ENOTDIR);
    CHECK(stat("/fat/missing-error-test",&st)==-1 && errno==ENOENT);
    CHECK(statvfs("/fat/missing-error-test",&vfs)==-1 && errno==ENOENT);
    char longpath[1100];memset(longpath,'a',sizeof(longpath));longpath[0]='/';longpath[sizeof(longpath)-1]=0;
    CHECK(stat(longpath,&st)==-1 && errno==ENAMETOOLONG);
    CHECK(statvfs(longpath,&vfs)==-1 && errno==ENAMETOOLONG);
    CHECK(!close(fd) && close(fd)==-1 && errno==EBADF && !unlink(path));
    puts("threadstress: file-errors ok");return 0;
}
