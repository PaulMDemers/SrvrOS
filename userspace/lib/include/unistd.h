#ifndef SRVROS_POSIX_UNISTD_H
#define SRVROS_POSIX_UNISTD_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define _SC_PAGESIZE 1
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _SC_NPROCESSORS_ONLN 2
#define _SC_CLK_TCK 3

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

int open(const char *path, int flags, ...);
ssize_t read(int fd, void *buffer, size_t length);
ssize_t write(int fd, const void *buffer, size_t length);
ssize_t pread(int fd, void *buffer, size_t length, off_t offset);
ssize_t pwrite(int fd, const void *buffer, size_t length, off_t offset);
int close(int fd);
int fsync(int fd);
void sync(void);
int dup(int fd);
int dup2(int old_fd, int new_fd);
int pipe(int fds[2]);
off_t lseek(int fd, off_t offset, int whence);
int unlink(const char *path);
int ftruncate(int fd, off_t length);
int truncate(const char *path, off_t length);
int rmdir(const char *path);
int rename(const char *old_path, const char *new_path);
int access(const char *path, int mode);
int isatty(int fd);
int brk(void *address);
void *sbrk(intptr_t increment);
char *getcwd(char *buffer, size_t size);
int chdir(const char *path);
pid_t getpid(void);
pid_t getppid(void);
int execve(const char *path, char *const argv[], char *const envp[]);
unsigned int sleep(unsigned int seconds);
int usleep(unsigned int usec);
int getpagesize(void);
long sysconf(int name);
int mkstemp(char *template_path);
extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;
int getopt(int argc, char *const argv[], const char *optstring);
void _exit(int status) __attribute__((noreturn));

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#endif
