#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

struct tms {
    clock_t tms_utime;
    clock_t tms_stime;
    clock_t tms_cutime;
    clock_t tms_cstime;
};

int _open(const char *path, int flags, int mode) {
    (void)mode;
    return open(path, flags);
}

int _close(int fd) {
    return close(fd);
}

ssize_t _read(int fd, void *buffer, size_t length) {
    return read(fd, buffer, length);
}

ssize_t _write(int fd, const void *buffer, size_t length) {
    return write(fd, buffer, length);
}

ssize_t _pread(int fd, void *buffer, size_t length, off_t offset) {
    return pread(fd, buffer, length, offset);
}

ssize_t _pwrite(int fd, const void *buffer, size_t length, off_t offset) {
    return pwrite(fd, buffer, length, offset);
}

off_t _lseek(int fd, off_t offset, int whence) {
    return lseek(fd, offset, whence);
}

int _dup(int fd) {
    return dup(fd);
}

int _dup2(int old_fd, int new_fd) {
    return dup2(old_fd, new_fd);
}

int _pipe(int fds[2]) {
    return pipe(fds);
}

int _fstat(int fd, struct stat *st) {
    return fstat(fd, st);
}

int _stat(const char *path, struct stat *st) {
    return stat(path, st);
}

int _isatty(int fd) {
    return isatty(fd);
}

void *_sbrk(intptr_t increment) {
    return sbrk(increment);
}

int _getpid(void) {
    return (int)getpid();
}

int _kill(int pid, int signal) {
    (void)pid;
    (void)signal;
    errno = ENOSYS;
    return -1;
}

clock_t _times(struct tms *buffer) {
    if (buffer != 0) {
        buffer->tms_utime = 0;
        buffer->tms_stime = 0;
        buffer->tms_cutime = 0;
        buffer->tms_cstime = 0;
    }
    return 0;
}

int _gettimeofday(struct timeval *tv, void *tz) {
    return gettimeofday(tv, tz);
}

int _unlink(const char *path) {
    return unlink(path);
}

int _rename(const char *old_path, const char *new_path) {
    return rename(old_path, new_path);
}

int _fsync(int fd) {
    return fsync(fd);
}

int _ftruncate(int fd, off_t length) {
    return ftruncate(fd, length);
}

int _truncate(const char *path, off_t length) {
    return truncate(path, length);
}

int _access(const char *path, int mode) {
    return access(path, mode);
}

int _chmod(const char *path, mode_t mode) {
    return chmod(path, mode);
}

int _fchmod(int fd, mode_t mode) {
    return fchmod(fd, mode);
}
