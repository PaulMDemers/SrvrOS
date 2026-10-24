#include <errno.h>
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
