#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <srvros/sys.h>

#define POSIX_PATH_MAX 160
#define POSIX_CHILD_MAX 96
#define POSIX_SEEN_MAX 64

int __posix_make_path(const char *path, char *out, size_t capacity);

struct DIR {
    char path[POSIX_PATH_MAX];
    char prefix[POSIX_PATH_MAX];
    uint64_t index;
    size_t seen_count;
    char seen[POSIX_SEEN_MAX][POSIX_CHILD_MAX];
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

static void remember(DIR *dir, const char *name) {
    if (dir->seen_count >= POSIX_SEEN_MAX || seen(dir, name)) {
        return;
    }
    strncpy(dir->seen[dir->seen_count], name, POSIX_CHILD_MAX - 1);
    dir->seen[dir->seen_count][POSIX_CHILD_MAX - 1] = '\0';
    dir->seen_count++;
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
        return 0;
    }
    memset(dir, 0, sizeof(*dir));
    if (__posix_make_path(path, dir->path, sizeof(dir->path)) < 0 ||
        stat(dir->path, &st) < 0 ||
        !S_ISDIR(st.st_mode)) {
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

    while (srv_list(dir->index++, path, sizeof(path), &size) > 0) {
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
        if (child[0] == '\0' || seen(dir, child)) {
            continue;
        }

        remember(dir, child);
        memset(&dir->entry, 0, sizeof(dir->entry));
        strncpy(dir->entry.d_name, child, sizeof(dir->entry.d_name) - 1);
        dir->entry.d_ino = dir->index;
        dir->entry.d_type = rest[child_len] == '/' ? DT_DIR : DT_REG;
        return &dir->entry;
    }
    return 0;
}

int closedir(DIR *dir) {
    if (dir == 0) {
        errno = EBADF;
        return -1;
    }
    free(dir);
    return 0;
}
