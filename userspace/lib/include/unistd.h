#ifndef SRVROS_POSIX_UNISTD_H
#define SRVROS_POSIX_UNISTD_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

int open(const char *path, int flags, ...);
ssize_t read(int fd, void *buffer, size_t length);
ssize_t write(int fd, const void *buffer, size_t length);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
int unlink(const char *path);
int rmdir(const char *path);
int rename(const char *old_path, const char *new_path);
char *getcwd(char *buffer, size_t size);
int chdir(const char *path);
pid_t getpid(void);
unsigned int sleep(unsigned int seconds);
int usleep(unsigned int usec);
void _exit(int status) __attribute__((noreturn));

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#endif
