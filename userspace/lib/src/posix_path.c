#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <srvros/sys.h>

#define POSIX_PATH_MAX 160

int __posix_make_path(const char *path, char *out, size_t capacity) {
    if(!path || !out) { errno=EFAULT;return -1; }
    if(!capacity) { errno=ENAMETOOLONG;return -1; }
    long result=srv_path_context(SRV_PATH_RESOLVE,path,out,capacity);
    if(result<0) { errno=result==SRV_ERR_RANGE?ENAMETOOLONG:(int)-result;return -1; }
    return 0;
}

char *getcwd(char *buffer,size_t size) {
    /* The allocating form is a common extension; process state never depends
     * on the environment's PWD string. */
    char temporary[POSIX_PATH_MAX];
    char *destination=buffer?buffer:temporary;
    size_t capacity=buffer?size:sizeof(temporary);
    if(!capacity) { errno=ERANGE;return NULL; }
    long result=srv_path_context(SRV_PATH_GETCWD,NULL,destination,capacity);
    if(result<0) { errno=(int)-result;return NULL; }
    if(buffer) return buffer;
    size_t needed=strlen(temporary)+1;
    if(size && size<needed) { errno=ERANGE;return NULL; }
    buffer=malloc(size?size:needed);
    if(!buffer) { errno=ENOMEM;return NULL; }
    memcpy(buffer,temporary,needed);return buffer;
}

int chdir(const char *path) {
    long result=srv_path_context(SRV_PATH_CHDIR,path,NULL,0);
    if(result<0) { errno=(int)-result;return -1; }
    /* Updating PWD/OLDPWD is a shell responsibility, not a chdir side effect. */
    return 0;
}

char *realpath(const char *path,char *resolved_path) {
    char full[POSIX_PATH_MAX];struct stat st;
    if(__posix_make_path(path,full,sizeof(full))<0 || stat(full,&st)<0) return NULL;
    if(!resolved_path) {
        resolved_path=malloc(strlen(full)+1);
        if(!resolved_path) { errno=ENOMEM;return NULL; }
    }
    strcpy(resolved_path,full);return resolved_path;
}

/* Open directory handles pin the namespace against rename/removal in this
 * profile. Resolve against their path without temporarily changing the process
 * CWD (which is shared by all threads). */
int __posix_at_path(int fd,const char *path,char *out,size_t capacity) {
    if(!path) { errno=EFAULT;return -1; }
    if(!path[0]) { errno=ENOENT;return -1; }
    if(path[0]=='/' || fd==AT_FDCWD) return __posix_make_path(path,out,capacity);
    char base[POSIX_PATH_MAX],joined[POSIX_PATH_MAX];
    long result=srv_directory(SRV_DIRECTORY_PATH,fd,base,sizeof(base));
    if(result<0) { errno=(int)-result;return -1; }
    size_t a=strlen(base),b=strlen(path);
    if(a+1+b>=sizeof(joined)) { errno=ENAMETOOLONG;return -1; }
    memcpy(joined,base,a);joined[a++]='/';memcpy(joined+a,path,b+1);
    return __posix_make_path(joined,out,capacity);
}
int fchdir(int fd) {
    long result=srv_directory(SRV_DIRECTORY_CHDIR,fd,NULL,0);
    if(result<0) { errno=(int)-result;return -1; }
    return 0;
}
int openat(int fd,const char *path,int flags,...) {
    mode_t mode=0666;
    if(flags&O_CREAT) { va_list args;va_start(args,flags);mode=(mode_t)va_arg(args,int);va_end(args); }
    char full[POSIX_PATH_MAX];
    return __posix_at_path(fd,path,full,sizeof(full))<0?-1:open(full,flags,mode);
}
int unlinkat(int fd,const char *path,int flags) {
    if(flags&~AT_REMOVEDIR) { errno=EINVAL;return -1; }
    char full[POSIX_PATH_MAX];
    if(__posix_at_path(fd,path,full,sizeof(full))<0) return -1;
    return flags&AT_REMOVEDIR?rmdir(full):unlink(full);
}
int mkdirat(int fd,const char *path,mode_t mode) {
    char full[POSIX_PATH_MAX];
    return __posix_at_path(fd,path,full,sizeof(full))<0?-1:mkdir(full,mode);
}
int fstatat(int fd,const char *path,struct stat *st,int flags) {
    if(flags&~AT_SYMLINK_NOFOLLOW) { errno=EINVAL;return -1; }
    char full[POSIX_PATH_MAX];
    if(__posix_at_path(fd,path,full,sizeof(full))<0) return -1;
    return flags&AT_SYMLINK_NOFOLLOW?lstat(full,st):stat(full,st);
}
int fchmodat(int fd,const char *path,mode_t mode,int flags) {
    if(flags&~AT_SYMLINK_NOFOLLOW) { errno=EINVAL;return -1; }
    char full[POSIX_PATH_MAX];
    return __posix_at_path(fd,path,full,sizeof(full))<0?-1:chmod(full,mode);
}
int faccessat(int fd,const char *path,int mode,int flags) {
    if(flags&~(AT_EACCESS|AT_SYMLINK_NOFOLLOW)) { errno=EINVAL;return -1; }
    char full[POSIX_PATH_MAX];
    return __posix_at_path(fd,path,full,sizeof(full))<0?-1:access(full,mode);
}
int renameat(int oldfd,const char *oldpath,int newfd,const char *newpath) {
    char oldfull[POSIX_PATH_MAX],newfull[POSIX_PATH_MAX];
    if(__posix_at_path(oldfd,oldpath,oldfull,sizeof(oldfull))<0 ||
       __posix_at_path(newfd,newpath,newfull,sizeof(newfull))<0) return -1;
    return rename(oldfull,newfull);
}
