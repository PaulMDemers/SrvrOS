#include <errno.h>
#include <fcntl.h>
#include <srvros/sys.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int __posix_make_path(const char *path, char *out, size_t capacity);
int __posix_socket_close(int fd);
int __posix_socket_is_pseudo(int fd);

#define POSIX_PATH_MAX 160

static int translate_open_flags(int flags, uint64_t *srv_flags) {
    int access = flags & O_ACCMODE;
    uint64_t out = 0;

    if (access == O_RDONLY) {
        *srv_flags = SRV_OPEN_READ;
        return 0;
    }
    if (access == O_RDWR) {
        errno = ENOSYS;
        return -1;
    }
    out |= SRV_OPEN_WRITE;
    if ((flags & O_CREAT) != 0) {
        out |= SRV_OPEN_CREATE;
    }
    if ((flags & O_TRUNC) != 0) {
        out |= SRV_OPEN_TRUNC;
    }
    if ((flags & O_APPEND) != 0) {
        out |= SRV_OPEN_APPEND;
    }
    *srv_flags = out;
    return 0;
}

int open(const char *path, int flags, ...) {
    char full[POSIX_PATH_MAX];
    uint64_t srv_flags = 0;
    if (__posix_make_path(path, full, sizeof(full)) < 0 ||
        translate_open_flags(flags, &srv_flags) < 0) {
        return -1;
    }

    long fd = (flags & O_ACCMODE) == O_RDONLY ? srv_open(full) : srv_open_mode(full, srv_flags);
    if (fd < 0) {
        errno = (flags & O_CREAT) != 0 ? EIO : ENOENT;
        return -1;
    }
    return (int)fd;
}

ssize_t read(int fd, void *buffer, size_t length) {
    long result = srv_read(fd, buffer, length);
    if (result < 0) {
        errno = EBADF;
        return -1;
    }
    return (ssize_t)result;
}

ssize_t write(int fd, const void *buffer, size_t length) {
    long result = srv_write(fd, buffer, length);
    if (result < 0) {
        errno = EBADF;
        return -1;
    }
    return (ssize_t)result;
}

int close(int fd) {
    if (__posix_socket_is_pseudo(fd)) {
        return __posix_socket_close(fd);
    }
    if (srv_close(fd) < 0) {
        errno = EBADF;
        return -1;
    }
    return 0;
}

off_t lseek(int fd, off_t offset, int whence) {
    if (whence != SEEK_SET || offset < 0) {
        errno = ENOSYS;
        return -1;
    }
    if (srv_seek(fd, (uint64_t)offset) < 0) {
        errno = EBADF;
        return -1;
    }
    return offset;
}

int stat(const char *path, struct stat *st) {
    char full[POSIX_PATH_MAX];
    struct srv_stat info;
    if (st == 0 || __posix_make_path(path, full, sizeof(full)) < 0) {
        errno = EINVAL;
        return -1;
    }
    if (srv_stat(full, &info) < 0) {
        errno = ENOENT;
        return -1;
    }
    memset(st, 0, sizeof(*st));
    st->st_size = (off_t)info.size;
    st->st_mode = info.type == 1 ? (S_IFDIR | 0755) : (S_IFREG | 0644);
    st->st_nlink = 1;
    st->st_blksize = 4096;
    st->st_blocks = (info.size + 511) / 512;
    return 0;
}

int fstat(int fd, struct stat *st) {
    (void)fd;
    (void)st;
    errno = ENOSYS;
    return -1;
}

int unlink(const char *path) {
    char full[POSIX_PATH_MAX];
    if (__posix_make_path(path, full, sizeof(full)) < 0) {
        return -1;
    }
    if (srv_unlink(full) < 0) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}

int mkdir(const char *path, mode_t mode) {
    char full[POSIX_PATH_MAX];
    (void)mode;
    if (__posix_make_path(path, full, sizeof(full)) < 0) {
        return -1;
    }
    if (srv_mkdir(full) < 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

int rmdir(const char *path) {
    char full[POSIX_PATH_MAX];
    if (__posix_make_path(path, full, sizeof(full)) < 0) {
        return -1;
    }
    if (srv_rmdir(full) < 0) {
        errno = ENOTEMPTY;
        return -1;
    }
    return 0;
}

int rename(const char *old_path, const char *new_path) {
    char old_full[POSIX_PATH_MAX];
    char new_full[POSIX_PATH_MAX];
    if (__posix_make_path(old_path, old_full, sizeof(old_full)) < 0 ||
        __posix_make_path(new_path, new_full, sizeof(new_full)) < 0) {
        return -1;
    }
    if (srv_rename(old_full, new_full) < 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

pid_t getpid(void) {
    return (pid_t)srv_getpid();
}

void _exit(int status) {
    srv_exit(status);
    for (;;) {
    }
}
