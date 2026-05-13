#include <srvros/sys.h>

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
    return srv_syscall1(SYS_OPEN, (long)path);
}

long srv_open_mode(const char *path, uint64_t flags) {
    return srv_syscall2(SYS_OPEN_MODE, (long)path, (long)flags);
}

long srv_close(int fd) {
    return srv_syscall1(SYS_CLOSE, fd);
}

long srv_seek(int fd, uint64_t offset) {
    return srv_syscall2(SYS_SEEK, fd, (long)offset);
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
    return srv_syscall2(SYS_STAT, (long)path, (long)info);
}

long srv_unlink(const char *path) {
    return srv_syscall1(SYS_UNLINK, (long)path);
}

long srv_mkdir(const char *path) {
    return srv_syscall1(SYS_MKDIR, (long)path);
}

long srv_rmdir(const char *path) {
    return srv_syscall1(SYS_RMDIR, (long)path);
}

long srv_rename(const char *old_path, const char *new_path) {
    return srv_syscall2(SYS_RENAME, (long)old_path, (long)new_path);
}

long srv_list(uint64_t index, char *path_buffer, size_t path_capacity, uint64_t *size_out) {
    return srv_syscall4(SYS_LIST, (long)index, (long)path_buffer, (long)path_capacity, (long)size_out);
}

long srv_spawn(const char *path) {
    return srv_syscall1(SYS_SPAWN, (long)path);
}

long srv_spawn_args(const char *path, const char *args) {
    return srv_syscall2(SYS_SPAWN_ARGS, (long)path, (long)args);
}

long srv_spawn_args_redirect(const char *path, const char *args, const char *stdout_path, int append) {
    return srv_syscall4(SYS_SPAWN_ARGS_REDIRECT, (long)path, (long)args, (long)stdout_path, append);
}

long srv_spawn_bg(const char *path) {
    return srv_syscall1(SYS_SPAWN_BG, (long)path);
}

long srv_spawn_bg_args(const char *path, const char *args) {
    return srv_syscall2(SYS_SPAWN_BG_ARGS, (long)path, (long)args);
}

long srv_proc_list(uint64_t index, struct srv_process_info *info) {
    return srv_syscall2(SYS_PROC_LIST, (long)index, (long)info);
}

long srv_kill(uint64_t pid) {
    return srv_syscall1(SYS_KILL, (long)pid);
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
