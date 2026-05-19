#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <srvros/cli.h>

#define POSIX_PATH_MAX 160

static char current_directory[POSIX_PATH_MAX] = "/";
static int current_directory_initialized;

static void initialize_current_directory(void) {
    if (current_directory_initialized) {
        return;
    }
    current_directory_initialized = 1;
    const char *pwd = getenv("PWD");
    if (pwd != 0 && pwd[0] != '\0') {
        cli_normalize_path(current_directory, sizeof(current_directory), "/", pwd);
    }
}

int __posix_make_path(const char *path, char *out, size_t capacity) {
    if (path == 0 || path[0] == '\0' || capacity == 0) {
        errno = EINVAL;
        return -1;
    }
    initialize_current_directory();
    cli_normalize_path(out, capacity, current_directory, path);
    if (strlen(out) + 1 > capacity) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

char *getcwd(char *buffer, size_t size) {
    initialize_current_directory();
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
    if (strcmp(full, "/") != 0) {
        if (stat(full, &st) < 0) {
            return -1;
        }
        if (!S_ISDIR(st.st_mode)) {
            errno = ENOTDIR;
            return -1;
        }
    }
    cli_normalize_path(current_directory, sizeof(current_directory), "/", full);
    setenv("PWD", current_directory, 1);
    return 0;
}
