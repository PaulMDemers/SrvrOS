#include <srvros/sys.h>
#include <srvros/cli.h>

#include <stdlib.h>

static const char *srv_make_path(const char *path, char *buffer, size_t capacity) {
    if (path == 0 || path[0] == '\0' || capacity == 0) {
        return path;
    }
    if (path[0] == '/' && path[1] == '\0') {
        return path;
    }
    const char *cwd = getenv("PWD");
    cli_normalize_path(buffer, capacity, cwd != 0 && cwd[0] != '\0' ? cwd : "/", path);
    return buffer;
}

long srv_syscall0(long number) {
    __asm__ volatile ("int $0x80" : "+a"(number) : : "memory");
    return number;
}

long srv_syscall1(long number, long arg0) {
    __asm__ volatile ("int $0x80" : "+a"(number) : "D"(arg0) : "memory");
    return number;
}

long srv_syscall2(long number, long arg0, long arg1) {
    __asm__ volatile (
        "int $0x80"
        : "+a"(number)
        : "D"(arg0), "S"(arg1)
        : "memory");
    return number;
}

long srv_syscall3(long number, long arg0, long arg1, long arg2) {
    __asm__ volatile (
        "int $0x80"
        : "+a"(number)
        : "D"(arg0), "S"(arg1), "d"(arg2)
        : "memory");
    return number;
}

long srv_syscall4(long number, long arg0, long arg1, long arg2, long arg3) {
    __asm__ volatile (
        "int $0x80"
        : "+a"(number)
        : "D"(arg0), "S"(arg1), "d"(arg2), "c"(arg3)
        : "memory");
    return number;
}

long srv_syscall5(long number, long arg0, long arg1, long arg2, long arg3, long arg4) {
    register long r8 __asm__("r8") = arg4;
    __asm__ volatile (
        "int $0x80"
        : "+a"(number)
        : "D"(arg0), "S"(arg1), "d"(arg2), "c"(arg3), "r"(r8)
        : "memory");
    return number;
}

long srv_syscall6(long number, long arg0, long arg1, long arg2, long arg3, long arg4, long arg5) {
    register long r8 __asm__("r8") = arg4;
    register long r9 __asm__("r9") = arg5;
    __asm__ volatile (
        "int $0x80"
        : "+a"(number)
        : "D"(arg0), "S"(arg1), "d"(arg2), "c"(arg3), "r"(r8), "r"(r9)
        : "memory");
    return number;
}

size_t srv_strlen(const char *text) {
    size_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

long srv_write(int fd, const void *buffer, size_t length) {
    return srv_syscall3(SYS_WRITE, fd, (long)buffer, (long)length);
}

long srv_read(int fd, void *buffer, size_t length) {
    return srv_syscall3(SYS_READ, fd, (long)buffer, (long)length);
}

long srv_open(const char *path) {
    char full[CLI_PATH_MAX];
    return srv_syscall1(SYS_OPEN, (long)srv_make_path(path, full, sizeof(full)));
}

long srv_open_mode(const char *path, uint64_t flags) {
    char full[CLI_PATH_MAX];
    return srv_syscall2(SYS_OPEN_MODE, (long)srv_make_path(path, full, sizeof(full)), (long)flags);
}

long srv_close(int fd) {
    return srv_syscall1(SYS_CLOSE, fd);
}

long srv_dup(int fd) {
    return srv_syscall1(SYS_DUP, fd);
}

long srv_dup2(int old_fd, int new_fd) {
    return srv_syscall2(SYS_DUP2, old_fd, new_fd);
}

long srv_pipe(int fds[2]) {
    return srv_syscall1(SYS_PIPE, (long)fds);
}

long srv_pipe_pair(int fds[2]) {
    return srv_syscall1(SYS_PIPE_PAIR, (long)fds);
}

long srv_poll(struct srv_pollfd *fds, size_t nfds, int timeout_ms) {
    return srv_syscall3(SYS_POLL, (long)fds, (long)nfds, timeout_ms);
}

long srv_fcntl(int fd, int command, uint64_t flags) {
    return srv_syscall3(SYS_FCNTL, fd, command, (long)flags);
}

long srv_ftruncate(int fd, uint64_t length) {
    return srv_syscall2(SYS_FTRUNCATE, fd, (long)length);
}

long srv_fsync(int fd) {
    return srv_syscall1(SYS_FSYNC, fd);
}

long srv_sync(void) {
    return srv_syscall0(SYS_SYNC);
}

long srv_tty_getattr(int fd, struct srv_termios *termios) {
    return srv_syscall2(SYS_TTY_GETATTR, fd, (long)termios);
}

long srv_tty_setattr(int fd, int actions, const struct srv_termios *termios) {
    return srv_syscall3(SYS_TTY_SETATTR, fd, actions, (long)termios);
}

long srv_ioctl(int fd, uint64_t request, void *argument) {
    return srv_syscall3(SYS_IOCTL, fd, (long)request, (long)argument);
}

long srv_seek(int fd, int64_t offset, uint64_t whence) {
    return srv_syscall3(SYS_SEEK, fd, (long)offset, (long)whence);
}

long srv_fstat(int fd, struct srv_stat *info) {
    if (info != NULL) {
        info->abi_version = SRV_ABI_VERSION;
        info->struct_size = sizeof(*info);
    }
    return srv_syscall2(SYS_FSTAT, fd, (long)info);
}

long srv_chmod(const char *path, uint64_t mode) {
    char full[CLI_PATH_MAX];
    return srv_syscall2(SYS_CHMOD, (long)srv_make_path(path, full, sizeof(full)), (long)mode);
}

long srv_fchmod(int fd, uint64_t mode) {
    return srv_syscall2(SYS_FCHMOD, fd, (long)mode);
}

long srv_sbrk(int64_t increment, uint64_t *previous_out) {
    return srv_syscall2(SYS_SBRK, (long)increment, (long)previous_out);
}

long srv_mmap(void *address, size_t length, int protection, int flags, int fd, int64_t offset) {
    return srv_syscall6(SYS_MMAP,
        (long)address,
        (long)length,
        protection,
        flags,
        fd,
        (long)offset);
}

long srv_munmap(void *address, size_t length) {
    return srv_syscall2(SYS_MUNMAP, (long)address, (long)length);
}

long srv_mprotect(void *address, size_t length, int protection) {
    return srv_syscall3(SYS_MPROTECT, (long)address, (long)length, protection);
}

long srv_msync(void *address, size_t length, int flags) {
    return srv_syscall3(SYS_MSYNC, (long)address, (long)length, flags);
}

long srv_fs_write(const char *path, const void *buffer, size_t length) {
    long fd = srv_open_mode(path, SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_TRUNC);
    if (fd < 0) {
        return -1;
    }
    long written = srv_write((int)fd, buffer, length);
    long closed = srv_close((int)fd);
    return written < 0 || closed < 0 ? -1 : written;
}

long srv_fs_append(const char *path, const void *buffer, size_t length) {
    long fd = srv_open_mode(path, SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_APPEND);
    if (fd < 0) {
        return -1;
    }
    long written = srv_write((int)fd, buffer, length);
    long closed = srv_close((int)fd);
    return written < 0 || closed < 0 ? -1 : written;
}

long srv_stat(const char *path, struct srv_stat *info) {
    char full[CLI_PATH_MAX];
    if (info != NULL) {
        info->abi_version = SRV_ABI_VERSION;
        info->struct_size = sizeof(*info);
    }
    return srv_syscall2(SYS_STAT, (long)srv_make_path(path, full, sizeof(full)), (long)info);
}

long srv_statfs(const char *path, struct srv_fsinfo *info) {
    char full[CLI_PATH_MAX];
    if (info != NULL) {
        info->abi_version = SRV_ABI_VERSION;
        info->struct_size = sizeof(*info);
    }
    return srv_syscall2(SYS_STATFS, (long)srv_make_path(path, full, sizeof(full)), (long)info);
}

long srv_unlink(const char *path) {
    char full[CLI_PATH_MAX];
    return srv_syscall1(SYS_UNLINK, (long)srv_make_path(path, full, sizeof(full)));
}

long srv_mkdir(const char *path) {
    char full[CLI_PATH_MAX];
    return srv_syscall1(SYS_MKDIR, (long)srv_make_path(path, full, sizeof(full)));
}

long srv_rmdir(const char *path) {
    char full[CLI_PATH_MAX];
    return srv_syscall1(SYS_RMDIR, (long)srv_make_path(path, full, sizeof(full)));
}

long srv_rename(const char *old_path, const char *new_path) {
    char old_full[CLI_PATH_MAX];
    char new_full[CLI_PATH_MAX];
    return srv_syscall2(SYS_RENAME,
        (long)srv_make_path(old_path, old_full, sizeof(old_full)),
        (long)srv_make_path(new_path, new_full, sizeof(new_full)));
}

long srv_list(uint64_t index, char *path_buffer, size_t path_capacity, uint64_t *size_out) {
    return srv_syscall4(SYS_LIST, (long)index, (long)path_buffer, (long)path_capacity, (long)size_out);
}

long srv_spawn(const char *path) {
    char full[CLI_PATH_MAX];
    return srv_syscall1(SYS_SPAWN, (long)srv_make_path(path, full, sizeof(full)));
}

long srv_spawn_args(const char *path, const char *args) {
    char full[CLI_PATH_MAX];
    return srv_syscall2(SYS_SPAWN_ARGS, (long)srv_make_path(path, full, sizeof(full)), (long)args);
}

long srv_spawn_args_redirect(const char *path, const char *args, const char *stdout_path, int append) {
    char full[CLI_PATH_MAX];
    char out_full[CLI_PATH_MAX];
    return srv_syscall4(SYS_SPAWN_ARGS_REDIRECT,
        (long)srv_make_path(path, full, sizeof(full)),
        (long)args,
        (long)srv_make_path(stdout_path, out_full, sizeof(out_full)),
        append);
}

long srv_spawn_bg(const char *path) {
    char full[CLI_PATH_MAX];
    return srv_syscall1(SYS_SPAWN_BG, (long)srv_make_path(path, full, sizeof(full)));
}

long srv_spawn_bg_args(const char *path, const char *args) {
    char full[CLI_PATH_MAX];
    return srv_syscall2(SYS_SPAWN_BG_ARGS, (long)srv_make_path(path, full, sizeof(full)), (long)args);
}

long srv_spawn_bg_args_fds(const char *path, const char *args, int stdin_fd, int stdout_fd) {
    char full[CLI_PATH_MAX];
    return srv_syscall4(SYS_SPAWN_BG_ARGS_FDS,
        (long)srv_make_path(path, full, sizeof(full)),
        (long)args,
        stdin_fd,
        stdout_fd);
}

long srv_exec(const struct srv_exec_request *request) {
    if (request == 0 || request->path == 0) {
        return srv_syscall1(SYS_EXEC, (long)request);
    }
    char full[CLI_PATH_MAX];
    struct srv_exec_request normalized = {0};
    normalized = *request;
    normalized.path = srv_make_path(request->path, full, sizeof(full));
    return srv_syscall1(SYS_EXEC, (long)&normalized);
}

long srv_execve(const char *path, char *const argv[], char *const envp[]) {
    struct srv_exec_request request = {
        .path = path,
        .argv = argv,
        .envp = envp,
        .flags = SRV_EXEC_REPLACE,
        .stdin_fd = -1,
        .stdout_fd = -1,
        .stderr_fd = -1,
    };
    return srv_exec(&request);
}

long srv_proc_list(uint64_t index, struct srv_process_info *info) {
    if (info != NULL) {
        info->abi_version = SRV_ABI_VERSION;
        info->struct_size = sizeof(*info);
    }
    return srv_syscall2(SYS_PROC_LIST, (long)index, (long)info);
}

long srv_kill(uint64_t pid) {
    return srv_syscall2(SYS_KILL, (long)pid, SRV_SIGNAL_TERM);
}

long srv_kill_signal(uint64_t pid, uint64_t signal) {
    return srv_syscall2(SYS_KILL, (long)pid, (long)signal);
}

long srv_signal_config(uint64_t signal, uint64_t action) {
    return srv_syscall2(SYS_SIGNAL_CONFIG, (long)signal, (long)action);
}

long srv_signal_poll(uint64_t *signal_out) {
    return srv_syscall1(SYS_SIGNAL_POLL, (long)signal_out);
}

long srv_meminfo(struct srv_meminfo *info) {
    if (info != 0) {
        info->abi_version = SRV_ABI_VERSION;
        info->struct_size = sizeof(*info);
    }
    return srv_syscall1(SYS_MEMINFO, (long)info);
}

long srv_random(void *buffer, size_t length, uint64_t flags) {
    return srv_syscall3(SYS_RANDOM, (long)buffer, (long)length, (long)flags);
}

long srv_utime(const char *path, uint64_t atime, uint64_t mtime) {
    char full[160];
    return srv_syscall3(SYS_UTIME, (long)srv_make_path(path, full, sizeof(full)), (long)atime, (long)mtime);
}

long srv_futime(int fd, uint64_t atime, uint64_t mtime) {
    return srv_syscall3(SYS_FUTIME, fd, (long)atime, (long)mtime);
}

long srv_proc_group(uint64_t pid, uint64_t group, int foreground) {
    return srv_syscall3(SYS_PROC_GROUP, (long)pid, (long)group, (long)foreground);
}

long srv_wait(uint64_t pid, uint64_t *status_out, uint64_t flags) {
    return srv_syscall3(SYS_WAIT, (long)pid, (long)status_out, (long)flags);
}

long srv_net_dhcp(void) {
    return srv_syscall0(SYS_NET_DHCP);
}

long srv_net_status(void) {
    return srv_syscall0(SYS_NET_STATUS);
}

long srv_net_dns(const char *name, uint32_t *ip_out) {
    return srv_syscall2(SYS_NET_DNS, (long)name, (long)ip_out);
}

long srv_net_list(uint64_t index, struct srv_net_info *info) {
    if (info != NULL) {
        info->abi_version = SRV_NET_ABI_VERSION;
        info->struct_size = sizeof(*info);
    }
    return srv_syscall2(SYS_NET_LIST, (long)index, (long)info);
}

long srv_net_status_info(struct srv_net_status_info *info) {
    if (info != NULL) {
        info->abi_version = SRV_NET_ABI_VERSION;
        info->struct_size = sizeof(*info);
    }
    return srv_syscall1(SYS_NET_STATUS_INFO, (long)info);
}

long srv_net_arp_list(uint64_t index, struct srv_arp_info *info) {
    if (info != NULL) {
        info->abi_version = SRV_NET_ABI_VERSION;
        info->struct_size = sizeof(*info);
    }
    return srv_syscall2(SYS_NET_ARP_LIST, (long)index, (long)info);
}

long srv_net_ping(uint32_t remote_ip, uint16_t sequence, uint64_t timeout_ticks, uint64_t *elapsed_ticks_out) {
    return srv_syscall4(SYS_NET_PING, (long)remote_ip, sequence, (long)timeout_ticks, (long)elapsed_ticks_out);
}

long srv_net_listen(uint16_t port) {
    return srv_syscall1(SYS_NET_LISTEN, port);
}

long srv_net_connect(uint32_t remote_ip, uint16_t remote_port, uint64_t flags) {
    return srv_syscall3(SYS_NET_CONNECT, remote_ip, remote_port, flags);
}

long srv_net_accept(int listener_fd, char *buffer, size_t capacity, uint64_t *length_out) {
    return srv_syscall4(SYS_NET_ACCEPT, listener_fd, (long)buffer, (long)capacity, (long)length_out);
}

long srv_net_udp_open(void) {
    return srv_syscall0(SYS_NET_UDP_OPEN);
}

long srv_net_udp_bind(int fd, uint16_t port) {
    return srv_syscall2(SYS_NET_UDP_BIND, fd, port);
}

long srv_net_udp_sendto(int fd, uint32_t remote_ip, uint16_t remote_port, const void *buffer, size_t length) {
    return srv_syscall5(SYS_NET_UDP_SENDTO,
        fd,
        (long)remote_ip,
        remote_port,
        (long)buffer,
        (long)length);
}

long srv_net_udp_recvfrom(int fd,
    void *buffer,
    size_t capacity,
    uint32_t *remote_ip_out,
    uint16_t *remote_port_out) {
    return srv_syscall5(SYS_NET_UDP_RECVFROM,
        fd,
        (long)buffer,
        (long)capacity,
        (long)remote_ip_out,
        (long)remote_port_out);
}

long srv_net_sockname(int fd, uint32_t *ip_out, uint16_t *port_out) {
    return srv_syscall3(SYS_NET_SOCKNAME, fd, (long)ip_out, (long)port_out);
}

long srv_net_peername(int fd, uint32_t *ip_out, uint16_t *port_out) {
    return srv_syscall3(SYS_NET_PEERNAME, fd, (long)ip_out, (long)port_out);
}

long srv_net_shutdown(int fd, int how) {
    return srv_syscall2(SYS_NET_SHUTDOWN, fd, how);
}

long srv_net_error(int fd) {
    return srv_syscall1(SYS_NET_ERROR, fd);
}

long srv_getpid(void) {
    return srv_syscall0(SYS_GETPID);
}

long srv_getppid(void) {
    return srv_syscall0(SYS_GETPPID);
}

long srv_exepath(char *buffer, size_t capacity) {
    return srv_syscall2(SYS_EXEPATH, (long)buffer, (long)capacity);
}

long srv_ticks(void) {
    return srv_syscall0(SYS_TICKS);
}

long srv_sleep_ticks(uint64_t ticks) {
    return srv_syscall1(SYS_SLEEP_TICKS, (long)ticks);
}

long srv_thread_create(uint64_t entry, uint64_t arg, uint64_t stack_top, uint64_t flags) {
    return srv_syscall4(SYS_THREAD_CREATE, (long)entry, (long)arg, (long)stack_top, (long)flags);
}

long srv_thread_exit(uint64_t value) {
    return srv_syscall1(SYS_THREAD_EXIT, (long)value);
}

long srv_thread_join(uint64_t tid, uint64_t *value_out) {
    return srv_syscall2(SYS_THREAD_JOIN, (long)tid, (long)value_out);
}

long srv_thread_self(void) {
    return srv_syscall0(SYS_THREAD_SELF);
}

long srv_thread_detach(uint64_t tid) {
    return srv_syscall1(SYS_THREAD_DETACH, (long)tid);
}

long srv_thread_status(uint64_t tid) {
    return srv_syscall1(SYS_THREAD_STATUS, (long)tid);
}

long srv_futex_wait(uint32_t *address, uint32_t expected, uint64_t timeout_ticks) {
    return srv_syscall3(SYS_FUTEX_WAIT, (long)address, (long)expected, (long)timeout_ticks);
}

long srv_futex_wake(uint32_t *address, uint64_t max_count) {
    return srv_syscall2(SYS_FUTEX_WAKE, (long)address, (long)max_count);
}

void srv_puts(const char *text) {
    srv_write(SRV_STDOUT, text, srv_strlen(text));
}

void srv_exit(int status) {
    srv_syscall1(SYS_EXIT, status);
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void srv_yield(void) {
    srv_syscall0(SYS_YIELD);
}
