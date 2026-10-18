#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <srvros/sys.h>

#define POSIX_PATH_MAX 160
#define POSIX_CHILD_MAX 96

int __posix_make_path(const char *path, char *out, size_t capacity);

struct DIR {
    char path[POSIX_PATH_MAX];
    char prefix[POSIX_PATH_MAX+1];
    uint64_t index;
    size_t seen_count,seen_capacity;
    char (*seen)[POSIX_CHILD_MAX];
    struct dirent entry;
};

static int starts_with(const char *text, const char *prefix) {
    while (*prefix != '\0') {
        if (*text++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

static int seen(DIR *dir, const char *name) {
    for (size_t i = 0; i < dir->seen_count; i++) {
        if (strcmp(dir->seen[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int remember(DIR *dir, const char *name) {
    if (dir->seen_count == dir->seen_capacity) {
        size_t capacity=dir->seen_capacity?dir->seen_capacity*2:16;
        if(capacity<dir->seen_capacity || capacity>SIZE_MAX/POSIX_CHILD_MAX) {
            errno=ENOMEM;return -1;
        }
        void *next=realloc(dir->seen,capacity*POSIX_CHILD_MAX);
        if(!next) { errno=ENOMEM;return -1; }
        dir->seen=next;dir->seen_capacity=capacity;
    }
    memcpy(dir->seen[dir->seen_count],name,strlen(name)+1);
    dir->seen_count++;
    return 0;
}

static void make_prefix(DIR *dir) {
    strcpy(dir->prefix, dir->path);
    size_t len = strlen(dir->prefix);
    while (len > 1 && dir->prefix[len - 1] == '/') {
        dir->prefix[--len] = '\0';
    }
    if (len + 2 <= sizeof(dir->prefix) && dir->prefix[len - 1] != '/') {
        dir->prefix[len++] = '/';
        dir->prefix[len] = '\0';
    }
}

DIR *opendir(const char *path) {
    struct stat st;
    DIR *dir = malloc(sizeof(DIR));
    if (dir == 0) {
        errno = ENOMEM;
        return 0;
    }
    memset(dir, 0, sizeof(*dir));
    if (__posix_make_path(path, dir->path, sizeof(dir->path)) < 0 || stat(dir->path, &st) < 0) {
        free(dir);
        return 0;
    }
    if (!S_ISDIR(st.st_mode)) {
        free(dir);
        errno = ENOTDIR;
        return 0;
    }
    make_prefix(dir);
    return dir;
}

struct dirent *readdir(DIR *dir) {
    char path[POSIX_PATH_MAX];
    uint64_t size = 0;
    if (dir == 0) {
        errno = EBADF;
        return 0;
    }

    for (;;) {
        long result=srv_list(dir->index,path,sizeof(path),&size);
        if(result<=0) { if(result<0) errno=EIO;return 0; }
        dir->index++;
        if (strcmp(path, dir->path) == 0 || !starts_with(path, dir->prefix)) {
            continue;
        }

        const char *rest = path + strlen(dir->prefix);
        char child[POSIX_CHILD_MAX];
        size_t child_len = 0;
        while (rest[child_len] != '\0' &&
            rest[child_len] != '/' &&
            child_len + 1 < sizeof(child)) {
            child[child_len] = rest[child_len];
            child_len++;
        }
        child[child_len] = '\0';
        if(rest[child_len] && rest[child_len]!='/') { errno=ENAMETOOLONG;return 0; }
        if (child[0] == '\0' || seen(dir, child)) {
            continue;
        }

        if(remember(dir,child)<0) { dir->index--;return 0; }
        memset(&dir->entry, 0, sizeof(dir->entry));
        memcpy(dir->entry.d_name,child,child_len+1);
        dir->entry.d_ino = dir->index;
        dir->entry.d_type = rest[child_len] == '/' ? DT_DIR : DT_UNKNOWN;
        if(!rest[child_len]) {
            struct stat st;int saved_errno=errno;
            if(!stat(path,&st)) dir->entry.d_type=S_ISDIR(st.st_mode)?DT_DIR:S_ISREG(st.st_mode)?DT_REG:DT_UNKNOWN;
            errno=saved_errno;
        }
        return &dir->entry;
    }
    return 0;
}

struct dirent *readdir64(DIR *dir) {
    return readdir(dir);
}

int closedir(DIR *dir) {
    if (dir == 0) {
        errno = EBADF;
        return -1;
    }
    free(dir->seen);
    free(dir);
    return 0;
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
