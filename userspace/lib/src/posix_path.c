#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#define POSIX_PATH_MAX 160

static char current_directory[POSIX_PATH_MAX] = "/";

static int is_absolute(const char *path) {
    return path != 0 && path[0] == '/';
}

static int append_path(char *out, size_t capacity, const char *prefix, const char *name) {
    size_t used = 0;
    if (capacity == 0 || prefix == 0 || name == 0) {
        return 0;
    }
    while (prefix[used] != '\0' && used + 1 < capacity) {
        out[used] = prefix[used];
        used++;
    }
    if (used == 0 || used + 1 >= capacity) {
        return 0;
    }
    if (out[used - 1] != '/') {
        out[used++] = '/';
    }
    for (size_t i = 0; name[i] != '\0'; i++) {
        if (used + 1 >= capacity) {
            return 0;
        }
        out[used++] = name[i];
    }
    out[used] = '\0';
    return 1;
}

int __posix_make_path(const char *path, char *out, size_t capacity) {
    if (path == 0 || path[0] == '\0' || capacity == 0) {
        errno = EINVAL;
        return -1;
    }
    if (is_absolute(path)) {
        if (strlen(path) + 1 > capacity) {
            errno = ENAMETOOLONG;
            return -1;
        }
        strcpy(out, path);
        return 0;
    }
    if (!append_path(out, capacity, current_directory, path)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

char *getcwd(char *buffer, size_t size) {
    size_t length = strlen(current_directory);
    if (buffer == 0 || size <= length) {
        errno = ERANGE;
        return 0;
    }
    strcpy(buffer, current_directory);
    return buffer;
}

int chdir(const char *path) {
    char full[POSIX_PATH_MAX];
    struct stat st;
    if (__posix_make_path(path, full, sizeof(full)) < 0) {
        return -1;
    }
    if (stat(full, &st) < 0) {
        return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }
    strcpy(current_directory, full);
    return 0;
}
