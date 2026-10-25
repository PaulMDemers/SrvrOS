#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <srvros/sys.h>

#include <fcntl.h>
#include <unistd.h>

struct DIR { int fd; struct dirent entry; };

DIR *fdopendir(int fd) {
    struct stat st;
    if(fstat(fd,&st)<0) return NULL;
    if(!S_ISDIR(st.st_mode)) { errno=ENOTDIR;return NULL; }
    int flags=fcntl(fd,F_GETFL);
    if(flags<0) return NULL;
    if((flags&O_ACCMODE)==O_WRONLY) { errno=EBADF;return NULL; }
    DIR *dir=calloc(1,sizeof(*dir));
    if(!dir) { errno=ENOMEM;return NULL; }
    dir->fd=fd;return dir;
}
DIR *opendir(const char *path) {
    int fd=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(fd<0) return NULL;
    DIR *dir=fdopendir(fd);
    if(!dir) { int error=errno;close(fd);errno=error; }
    return dir;
}
int dirfd(DIR *dir) {
    if(!dir) { errno=EBADF;return -1; }
    return dir->fd;
}
struct dirent *readdir(DIR *dir) {
    if(!dir) { errno=EBADF;return NULL; }
    struct srv_dirent entry;
    long result=srv_directory(SRV_DIRECTORY_READ,dir->fd,&entry,sizeof(entry));
    if(result<0) { errno=(int)-result;return NULL; }
    if(!result) return NULL;
    memset(&dir->entry,0,sizeof(dir->entry));
    dir->entry.d_ino=entry.inode;
    dir->entry.d_type=entry.type==1?DT_DIR:DT_REG;
    memcpy(dir->entry.d_name,entry.name,strlen(entry.name)+1);
    return &dir->entry;
}
struct dirent *readdir64(DIR *dir) { return readdir(dir); }
long telldir(DIR *dir) { return dir?lseek(dir->fd,0,SEEK_CUR):(errno=EBADF,-1); }
void seekdir(DIR *dir,long location) {
    if(!dir) { errno=EBADF;return; }
    if(location<0) { errno=EINVAL;return; }
    (void)lseek(dir->fd,location,SEEK_SET);
}
void rewinddir(DIR *dir) { seekdir(dir,0); }
int closedir(DIR *dir) {
    if(!dir) { errno=EBADF;return -1; }
    int result=close(dir->fd);free(dir);return result;
}

int alphasort(const struct dirent **left, const struct dirent **right) {
    return strcoll((*left)->d_name, (*right)->d_name);
}

int scandir(const char *path,
    struct dirent ***namelist,
    int (*filter)(const struct dirent *),
    int (*compar)(const struct dirent **, const struct dirent **)) {
    DIR *dir;
    struct dirent *entry;
    struct dirent **entries = 0;
    size_t count = 0;
    size_t capacity = 0;

    if (namelist == 0) {
        errno = EINVAL;
        return -1;
    }
    *namelist = 0;
    dir = opendir(path);
    if (dir == 0) {
        return -1;
    }

    int failure=ENOMEM;
    for (;;) {
        errno=0;entry=readdir(dir);
        if(!entry) { if(errno) { failure=errno;goto fail; }break; }
        if (filter != 0 && !filter(entry)) {
            continue;
        }
        if (count == capacity) {
            size_t next_capacity = capacity == 0 ? 8 : capacity * 2;
            struct dirent **next = realloc(entries, next_capacity * sizeof(*entries));
            if (next == 0) {
                goto fail;
            }
            entries = next;
            capacity = next_capacity;
        }
        entries[count] = malloc(sizeof(*entries[count]));
        if (entries[count] == 0) {
            goto fail;
        }
        memcpy(entries[count], entry, sizeof(*entries[count]));
        count++;
    }
    closedir(dir);

    if (compar != 0 && count > 1) {
        qsort(entries, count, sizeof(*entries), (int (*)(const void *, const void *))compar);
    }
    *namelist = entries;
    return (int)count;

fail:
    for (size_t i = 0; i < count; i++) {
        free(entries[i]);
    }
    free(entries);
    closedir(dir);
    errno = failure;
    return -1;
}

int scandir64(const char *path,
    struct dirent ***namelist,
    int (*filter)(const struct dirent *),
    int (*compar)(const struct dirent **, const struct dirent **)) {
    return scandir(path, namelist, filter, compar);
}
