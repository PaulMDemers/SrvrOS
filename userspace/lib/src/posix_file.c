#include <errno.h>
#include <fcntl.h>
#include <srvros/sys.h>
#include <stdarg.h>
#include <string.h>
#include <termios.h>
#include <sys/stat.h>
#include <unistd.h>

int __posix_make_path(const char *path, char *out, size_t capacity);
int __posix_socket_close(int fd);
int __posix_socket_fcntl(int fd, int command, uint64_t flags);
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
        out |= SRV_OPEN_READ | SRV_OPEN_WRITE;
    } else {
        out |= SRV_OPEN_WRITE;
    }
    if ((flags & O_CREAT) != 0) {
        out |= SRV_OPEN_CREATE;
    }
    if ((flags & O_TRUNC) != 0) {
        out |= SRV_OPEN_TRUNC;
    }
    if ((flags & O_APPEND) != 0) {
        out |= SRV_OPEN_APPEND;
    }
    if ((flags & O_NONBLOCK) != 0) {
        out |= SRV_OPEN_NONBLOCK;
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
    if ((flags & O_NONBLOCK) != 0) {
        (void)srv_fcntl((int)fd, SRV_F_SETFL, SRV_FD_NONBLOCK);
    }
    return (int)fd;
}

ssize_t read(int fd, void *buffer, size_t length) {
    long result = srv_read(fd, buffer, length);
    if (result < 0) {
        if (result == SRV_ERR_AGAIN) {
            errno = EAGAIN;
            return -1;
        }
        errno = EBADF;
        return -1;
    }
    return (ssize_t)result;
}

ssize_t write(int fd, const void *buffer, size_t length) {
    long result = srv_write(fd, buffer, length);
    if (result < 0) {
        if (result == SRV_ERR_AGAIN) {
            errno = EAGAIN;
            return -1;
        }
        errno = EBADF;
        return -1;
    }
    return (ssize_t)result;
}

ssize_t pread(int fd, void *buffer, size_t length, off_t offset) {
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }
    off_t saved = lseek(fd, 0, SEEK_CUR);
    if (saved < 0 || lseek(fd, offset, SEEK_SET) < 0) {
        return -1;
    }
    ssize_t result = read(fd, buffer, length);
    int saved_errno = errno;
    (void)lseek(fd, saved, SEEK_SET);
    errno = saved_errno;
    return result;
}

ssize_t pwrite(int fd, const void *buffer, size_t length, off_t offset) {
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }
    off_t saved = lseek(fd, 0, SEEK_CUR);
    if (saved < 0 || lseek(fd, offset, SEEK_SET) < 0) {
        return -1;
    }
    ssize_t result = write(fd, buffer, length);
    int saved_errno = errno;
    (void)lseek(fd, saved, SEEK_SET);
    errno = saved_errno;
    return result;
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

int fsync(int fd) {
    if (srv_fsync(fd) < 0) {
        errno = EBADF;
        return -1;
    }
    return 0;
}

int dup(int fd) {
    long result = srv_dup(fd);
    if (result < 0) {
        errno = EBADF;
        return -1;
    }
    return (int)result;
}

int dup2(int old_fd, int new_fd) {
    long result = srv_dup2(old_fd, new_fd);
    if (result < 0) {
        errno = old_fd < 0 || new_fd < 0 ? EBADF : ENOSYS;
        return -1;
    }
    return (int)result;
}

int pipe(int fds[2]) {
    if (fds == 0) {
        errno = EINVAL;
        return -1;
    }
    if (srv_pipe(fds) < 0) {
        errno = EMFILE;
        return -1;
    }
    return 0;
}

int fcntl(int fd, int command, ...) {
    if (command == F_GETFD) {
        if (__posix_socket_is_pseudo(fd)) {
            long socket_flags = __posix_socket_fcntl(fd, SRV_F_GETFD, 0);
            if (socket_flags < 0) {
                return -1;
            }
            return (socket_flags & SRV_FD_CLOEXEC) != 0 ? FD_CLOEXEC : 0;
        }
        long flags = srv_fcntl(fd, SRV_F_GETFD, 0);
        if (flags < 0) {
            errno = EBADF;
            return -1;
        }
        return (flags & SRV_FD_CLOEXEC) != 0 ? FD_CLOEXEC : 0;
    }
    if (command == F_SETFD) {
        va_list args;
        va_start(args, command);
        int flags = va_arg(args, int);
        va_end(args);
        uint64_t srv_flags = (flags & FD_CLOEXEC) != 0 ? SRV_FD_CLOEXEC : 0;
        if (__posix_socket_is_pseudo(fd)) {
            return __posix_socket_fcntl(fd, SRV_F_SETFD, srv_flags);
        }
        if (srv_fcntl(fd, SRV_F_SETFD, srv_flags) < 0) {
            errno = EBADF;
            return -1;
        }
        return 0;
    }
    if (command == F_GETFL) {
        if (__posix_socket_is_pseudo(fd)) {
            long socket_flags = __posix_socket_fcntl(fd, SRV_F_GETFL, 0);
            if (socket_flags < 0) {
                return -1;
            }
            return (socket_flags & SRV_FD_NONBLOCK) != 0 ? O_NONBLOCK : 0;
        }
        long flags = srv_fcntl(fd, SRV_F_GETFL, 0);
        if (flags < 0) {
            errno = EBADF;
            return -1;
        }
        return (flags & SRV_FD_NONBLOCK) != 0 ? O_NONBLOCK : 0;
    }
    if (command == F_SETFL) {
        va_list args;
        va_start(args, command);
        int flags = va_arg(args, int);
        va_end(args);
        uint64_t srv_flags = (flags & O_NONBLOCK) != 0 ? SRV_FD_NONBLOCK : 0;
        if (__posix_socket_is_pseudo(fd)) {
            return __posix_socket_fcntl(fd, SRV_F_SETFL, srv_flags);
        }
        if (srv_fcntl(fd, SRV_F_SETFL, srv_flags) < 0) {
            errno = EBADF;
            return -1;
        }
        return 0;
    }
    if (command == F_GETLK || command == F_SETLK || command == F_SETLKW) {
        va_list args;
        va_start(args, command);
        struct flock *lock = va_arg(args, struct flock *);
        va_end(args);
        if (lock == 0) {
            errno = EINVAL;
            return -1;
        }
        if (lock->l_type != F_RDLCK && lock->l_type != F_WRLCK && lock->l_type != F_UNLCK) {
            errno = EINVAL;
            return -1;
        }

        struct srv_flock srv_lock;
        srv_lock.type = lock->l_type == F_RDLCK ? SRV_F_RDLCK :
            lock->l_type == F_WRLCK ? SRV_F_WRLCK : SRV_F_UNLCK;
        srv_lock.whence = (int16_t)lock->l_whence;
        srv_lock.reserved = 0;
        srv_lock.start = (int64_t)lock->l_start;
        srv_lock.len = (int64_t)lock->l_len;
        srv_lock.pid = (int64_t)lock->l_pid;

        int srv_command = command == F_GETLK ? SRV_F_GETLK :
            command == F_SETLK ? SRV_F_SETLK : SRV_F_SETLKW;
        long result = srv_fcntl(fd, srv_command, (uint64_t)&srv_lock);
        if (result < 0) {
            errno = result == SRV_ERR_AGAIN ? EAGAIN : EBADF;
            return -1;
        }

        if (command == F_GETLK) {
            lock->l_type = srv_lock.type == SRV_F_RDLCK ? F_RDLCK :
                srv_lock.type == SRV_F_WRLCK ? F_WRLCK : F_UNLCK;
            lock->l_whence = (short)srv_lock.whence;
            lock->l_start = (off_t)srv_lock.start;
            lock->l_len = (off_t)srv_lock.len;
            lock->l_pid = (pid_t)srv_lock.pid;
        }
        return 0;
    }
    errno = ENOSYS;
    return -1;
}

off_t lseek(int fd, off_t offset, int whence) {
    long result = srv_seek(fd, (int64_t)offset, (uint64_t)whence);
    if (result < 0) {
        errno = EBADF;
        return -1;
    }
    return (off_t)result;
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
    struct srv_stat info;
    if (st == 0) {
        errno = EINVAL;
        return -1;
    }
    if (srv_fstat(fd, &info) < 0) {
        errno = EBADF;
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

int access(const char *path, int mode) {
    struct stat st;
    if ((mode & ~(R_OK | W_OK | X_OK)) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (stat(path, &st) < 0) {
        return -1;
    }
    (void)st;
    return 0;
}

int isatty(int fd) {
    struct termios termios;
    int saved_errno = errno;
    if (tcgetattr(fd, &termios) == 0) {
        errno = saved_errno;
        return 1;
    }
    errno = 0;
    return 0;
}

int ftruncate(int fd, off_t length) {
    if (length < 0) {
        errno = EINVAL;
        return -1;
    }
    if (srv_ftruncate(fd, (uint64_t)length) < 0) {
        errno = EBADF;
        return -1;
    }
    return 0;
}

int truncate(const char *path, off_t length) {
    if (length < 0) {
        errno = EINVAL;
        return -1;
    }
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        return -1;
    }
    int result = ftruncate(fd, length);
    int saved_errno = errno;
    if (close(fd) < 0 && result == 0) {
        return -1;
    }
    errno = saved_errno;
    return result;
}

int chmod(const char *path, mode_t mode) {
    struct stat st;
    (void)mode;
    if (stat(path, &st) < 0) {
        return -1;
    }
    return 0;
}

int fchmod(int fd, mode_t mode) {
    struct stat st;
    (void)mode;
    if (fstat(fd, &st) < 0) {
        return -1;
    }
    return 0;
}

mode_t umask(mode_t mask) {
    static mode_t current;
    mode_t previous = current;
    current = mask & 0777;
    return previous;
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

void *sbrk(intptr_t increment) {
    uint64_t previous = 0;
    if (srv_sbrk((int64_t)increment, &previous) < 0) {
        errno = ENOMEM;
        return (void *)-1;
    }
    return (void *)(uintptr_t)previous;
}

int brk(void *address) {
    uint64_t current = 0;
    if (srv_sbrk(0, &current) < 0) {
        errno = ENOMEM;
        return -1;
    }
    intptr_t delta = (intptr_t)((uintptr_t)address - current);
    return sbrk(delta) == (void *)-1 ? -1 : 0;
}
