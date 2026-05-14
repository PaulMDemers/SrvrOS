#ifndef SRVROS_USER_SYS_H
#define SRVROS_USER_SYS_H

#include <stddef.h>
#include <stdint.h>

#include <srvros/syscall_numbers.h>

#define SRV_STDIN 0
#define SRV_STDOUT 1
#define SRV_STDERR 2

struct srv_process_info {
    uint64_t pid;
    char name[64];
    char state[24];
};

struct srv_stat {
    uint64_t size;
    uint64_t type;
};

struct srv_pollfd {
    int32_t fd;
    int16_t events;
    int16_t revents;
};

struct srv_exec_request {
    const char *path;
    char *const *argv;
    char *const *envp;
    uint64_t flags;
    int64_t stdin_fd;
    int64_t stdout_fd;
    int64_t stderr_fd;
};

long srv_syscall0(long number);
long srv_syscall1(long number, long arg0);
long srv_syscall2(long number, long arg0, long arg1);
long srv_syscall3(long number, long arg0, long arg1, long arg2);
long srv_syscall4(long number, long arg0, long arg1, long arg2, long arg3);
long srv_syscall5(long number, long arg0, long arg1, long arg2, long arg3, long arg4);

size_t srv_strlen(const char *text);
long srv_write(int fd, const void *buffer, size_t length);
long srv_read(int fd, void *buffer, size_t length);
long srv_open(const char *path);
long srv_open_mode(const char *path, uint64_t flags);
long srv_close(int fd);
long srv_dup(int fd);
long srv_dup2(int old_fd, int new_fd);
long srv_pipe(int fds[2]);
long srv_poll(struct srv_pollfd *fds, size_t nfds, int timeout_ms);
long srv_fcntl(int fd, int command, uint64_t flags);
long srv_ftruncate(int fd, uint64_t length);
long srv_fsync(int fd);
long srv_seek(int fd, int64_t offset, uint64_t whence);
long srv_fstat(int fd, struct srv_stat *info);
long srv_sbrk(int64_t increment, uint64_t *previous_out);
long srv_fs_write(const char *path, const void *buffer, size_t length);
long srv_fs_append(const char *path, const void *buffer, size_t length);
long srv_stat(const char *path, struct srv_stat *info);
long srv_unlink(const char *path);
long srv_mkdir(const char *path);
long srv_rmdir(const char *path);
long srv_rename(const char *old_path, const char *new_path);
long srv_list(uint64_t index, char *path_buffer, size_t path_capacity, uint64_t *size_out);
long srv_spawn(const char *path);
long srv_spawn_args(const char *path, const char *args);
long srv_spawn_args_redirect(const char *path, const char *args, const char *stdout_path, int append);
long srv_spawn_bg(const char *path);
long srv_spawn_bg_args(const char *path, const char *args);
long srv_spawn_bg_args_fds(const char *path, const char *args, int stdin_fd, int stdout_fd);
long srv_exec(const struct srv_exec_request *request);
long srv_execve(const char *path, char *const argv[], char *const envp[]);
long srv_proc_list(uint64_t index, struct srv_process_info *info);
long srv_kill(uint64_t pid);
long srv_wait(uint64_t pid, uint64_t *status_out, uint64_t flags);
long srv_net_dhcp(void);
long srv_net_status(void);
long srv_net_dns(const char *name, uint32_t *ip_out);
long srv_net_listen(uint16_t port);
long srv_net_accept(int listener_fd, char *buffer, size_t capacity, uint64_t *length_out);
long srv_getpid(void);
long srv_ticks(void);
long srv_sleep_ticks(uint64_t ticks);
void srv_puts(const char *text);
void srv_exit(int status);
void srv_yield(void);

#endif
