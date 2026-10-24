#ifndef SRVROS_POSIX_UNISTD_H
#define SRVROS_POSIX_UNISTD_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define _SC_PAGESIZE 1
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _SC_NPROCESSORS_ONLN 2
#define _SC_CLK_TCK 3
#define _SC_OPEN_MAX 4
#define _SC_ARG_MAX 10
#define _SC_NPROCESSORS_CONF 5
#define _SC_HOST_NAME_MAX 6
#define _SC_LOGIN_NAME_MAX 7
#define _SC_THREAD_KEYS_MAX 8
#define _SC_THREAD_DESTRUCTOR_ITERATIONS 9

#define _PC_PATH_MAX 1
#define _PC_NAME_MAX 2
#define _PC_LINK_MAX 3
#define _PC_NO_TRUNC 4
#define _PC_CHOWN_RESTRICTED 5
#define _PC_SYMLINK_MAX 7

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

int open(const char *path, int flags, ...);
ssize_t read(int fd, void *buffer, size_t length);
ssize_t write(int fd, const void *buffer, size_t length);
ssize_t pread(int fd, void *buffer, size_t length, off_t offset);
ssize_t pread64(int fd, void *buffer, size_t length, off_t offset);
ssize_t pwrite(int fd, const void *buffer, size_t length, off_t offset);
ssize_t pwrite64(int fd, const void *buffer, size_t length, off_t offset);
int close(int fd);
int fsync(int fd);
int fdatasync(int fd);
void sync(void);
int dup(int fd);
int dup2(int old_fd, int new_fd);
int dup3(int old_fd, int new_fd, int flags);
int pipe(int fds[2]);
int pipe2(int fds[2], int flags);
off_t lseek(int fd, off_t offset, int whence);
off_t lseek64(int fd, off_t offset, int whence);
int unlink(const char *path);
int unlinkat(int dirfd, const char *path, int flags);
int link(const char *old_path, const char *new_path);
int symlink(const char *target, const char *link_path);
ssize_t readlink(const char *path, char *buffer, size_t size);
int chown(const char *path, uid_t owner, gid_t group);
int lchown(const char *path, uid_t owner, gid_t group);
int fchown(int fd, uid_t owner, gid_t group);
int ftruncate(int fd, off_t length);
int ftruncate64(int fd, off_t length);
int truncate(const char *path, off_t length);
int rmdir(const char *path);
int rename(const char *old_path, const char *new_path);
int renameat(int oldfd,const char *oldpath,int newfd,const char *newpath);
int access(const char *path, int mode);
int isatty(int fd);
int brk(void *address);
void *sbrk(intptr_t increment);
char *getcwd(char *buffer, size_t size);
int chdir(const char *path);
int fchdir(int fd);
int faccessat(int fd,const char *path,int mode,int flags);
int gethostname(char *name, size_t length);
long pathconf(const char *path, int name);
long fpathconf(int fd, int name);
pid_t fork(void);
pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);
int setuid(uid_t uid);
int seteuid(uid_t uid);
int setgid(gid_t gid);
int setegid(gid_t gid);
int getgroups(int size, gid_t list[]);
pid_t getpgrp(void);
int setpgid(pid_t pid, pid_t pgid);
pid_t getsid(pid_t pid);
pid_t setsid(void);
int kill(pid_t pid, int sig);
int execve(const char *path, char *const argv[], char *const envp[]);
int execv(const char *path,char *const argv[]);
int execl(const char *path,const char *arg,...);
int execlp(const char *file,const char *arg,...);
int execle(const char *path,const char *arg,...);
int execvp(const char *file, char *const argv[]);
unsigned int sleep(unsigned int seconds);
int usleep(unsigned int usec);
int getpagesize(void);
long sysconf(int name);
int mkstemp(char *template_path);
int mkostemp(char *template_path, int flags);
char *mkdtemp(char *template_path);
extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;
int getopt(int argc, char *const argv[], const char *optstring);
void _exit(int status) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#endif
