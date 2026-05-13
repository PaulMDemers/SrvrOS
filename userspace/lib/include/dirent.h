#ifndef SRVROS_POSIX_DIRENT_H
#define SRVROS_POSIX_DIRENT_H

#include <sys/types.h>

#define DT_UNKNOWN 0
#define DT_REG 8
#define DT_DIR 4

struct dirent {
    ino_t d_ino;
    unsigned char d_type;
    char d_name[96];
};

typedef struct DIR DIR;

DIR *opendir(const char *path);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);

#endif
