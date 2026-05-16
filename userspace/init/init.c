#include <stddef.h>
#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_OPEN 3
#define SYS_READ 4
#define SYS_CLOSE 5
#define SYS_LIST 6
#define SYS_SPAWN 7
#define SYS_FS_WRITE 10
#define SYS_SPAWN_ARGS 24
#define SYS_OPEN_MODE 33
#define SYS_WAIT 34
#define SYS_SLEEP_TICKS 43
#define SYS_SPAWN_BG_ARGS_FDS 49
#define SRV_OPEN_WRITE 0x02
#define SRV_OPEN_CREATE 0x04
#define SRV_OPEN_APPEND 0x10
#define SRV_WAIT_NOHANG 0x01

#define STDIN 0
#define STDOUT 1
#define LINE_MAX 128
#define PATH_MAX 160
#define CHILD_NAME_MAX 64
#define CHILDREN_MAX 64
#define INIT_LOG "/fat/var/log/init.log"

static const char *path_entries[] = {
    "/fat/bin",
    "/",
};

static long syscall0(long number) {
    __asm__ volatile ("int $0x80" : "+a"(number) : : "memory");
    return number;
}

static long syscall1(long number, long arg0) {
    __asm__ volatile ("int $0x80" : "+a"(number) : "D"(arg0) : "memory");
    return number;
}

static long syscall2(long number, long arg0, long arg1) {
    __asm__ volatile (
        "int $0x80"
        : "+a"(number)
        : "D"(arg0), "S"(arg1)
        : "memory");
    return number;
}

static long syscall3(long number, long arg0, long arg1, long arg2) {
    __asm__ volatile (
        "int $0x80"
        : "+a"(number)
        : "D"(arg0), "S"(arg1), "d"(arg2)
        : "memory");
    return number;
}

static long syscall4(long number, long arg0, long arg1, long arg2, long arg3) {
    __asm__ volatile (
        "int $0x80"
        : "+a"(number)
        : "D"(arg0), "S"(arg1), "d"(arg2), "c"(arg3)
        : "memory");
    return number;
}

static size_t strlen(const char *text) {
    size_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static int streq(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static int starts_with(const char *text, const char *prefix) {
    while (*prefix != '\0') {
        if (*text++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

static int contains_slash(const char *text) {
    while (*text != '\0') {
        if (*text == '/') {
            return 1;
        }
        text++;
    }
    return 0;
}

static void copy_string(char *destination, size_t capacity, const char *source) {
    size_t i = 0;
    if (capacity == 0) {
        return;
    }

    while (source[i] != '\0' && i + 1 < capacity) {
        destination[i] = source[i];
        i++;
    }
    destination[i] = '\0';
}

static int join_path(char *destination, size_t capacity, const char *directory, const char *name) {
    size_t out = 0;
    if (capacity == 0 || directory[0] == '\0') {
        return 0;
    }

    while (directory[out] != '\0' && out + 1 < capacity) {
        destination[out] = directory[out];
        out++;
    }
    if (out == 0 || out + 1 >= capacity) {
        return 0;
    }
    if (destination[out - 1] != '/') {
        destination[out++] = '/';
    }

    for (size_t i = 0; name[i] != '\0'; i++) {
        if (out + 1 >= capacity) {
            return 0;
        }
        destination[out++] = name[i];
    }
    destination[out] = '\0';
    return 1;
}

static char *trim(char *line) {
    while (*line == ' ' || *line == '\t') {
        line++;
    }

    char *end = line;
    while (*end != '\0') {
        end++;
    }

    while (end > line && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
    *end = '\0';
    return line;
}

static void write_buf(const char *buffer, size_t length) {
    syscall3(SYS_WRITE, STDOUT, (long)buffer, (long)length);
}

static void write_fd_text(int fd, const char *text) {
    syscall3(SYS_WRITE, fd, (long)text, (long)strlen(text));
}

static void write_text(const char *text) {
    write_buf(text, strlen(text));
}

static char read_char(void) {
    char c = 0;
    syscall3(SYS_READ, STDIN, (long)&c, 1);
    return c;
}

static void read_line(char *line, size_t capacity) {
    size_t length = 0;

    for (;;) {
        char c = read_char();
        if (c == '\n') {
            write_text("\n");
            line[length] = '\0';
            return;
        }

        if (c == '\b') {
            if (length > 0) {
                length--;
                write_text("\b");
            }
            continue;
        }

        if (c >= 32 && c < 127 && length + 1 < capacity) {
            line[length++] = c;
            write_buf(&c, 1);
        }
    }
}

static void print_u64(uint64_t value) {
    char digits[21];
    uint64_t count = 0;

    if (value == 0) {
        write_text("0");
        return;
    }

    while (value > 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (count > 0) {
        write_buf(&digits[--count], 1);
    }
}

static void command_help(void) {
    write_text("builtins: help path ls [dir] cat <path> write <path> <text> run <path> exit\n");
    write_text("path: command names are searched in /fat/bin then /\n");
}

static int child_seen(char children[CHILDREN_MAX][CHILD_NAME_MAX], size_t count, const char *name) {
    for (size_t i = 0; i < count; i++) {
        if (streq(children[i], name)) {
            return 1;
        }
    }
    return 0;
}

static void remember_child(char children[CHILDREN_MAX][CHILD_NAME_MAX], size_t *count, const char *name) {
    if (*count >= CHILDREN_MAX || child_seen(children, *count, name)) {
        return;
    }

    copy_string(children[*count], CHILD_NAME_MAX, name);
    (*count)++;
}

static void print_path_entry(const char *name, uint64_t size, int directory) {
    write_text(name);
    if (directory) {
        write_text("/\n");
        return;
    }

    write_text(" ");
    print_u64(size);
    write_text(" bytes\n");
}

static void command_path(void) {
    for (size_t i = 0; i < sizeof(path_entries) / sizeof(path_entries[0]); i++) {
        write_text(path_entries[i]);
        write_text("\n");
    }
}

static void command_ls(const char *directory) {
    char path[160];
    char normalized[PATH_MAX];
    char prefix[PATH_MAX];
    char children[CHILDREN_MAX][CHILD_NAME_MAX];
    size_t child_count = 0;
    uint64_t size = 0;
    int found = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = "/";
    }
    copy_string(normalized, sizeof(normalized), directory);
    size_t normalized_length = strlen(normalized);
    while (normalized_length > 1 && normalized[normalized_length - 1] == '/') {
        normalized[--normalized_length] = '\0';
    }

    if (streq(normalized, "/")) {
        copy_string(prefix, sizeof(prefix), "/");
    } else {
        if (!join_path(prefix, sizeof(prefix), normalized, "")) {
            write_text("ls: path too long\n");
            return;
        }
    }

    for (uint64_t i = 0;; i++) {
        long result = syscall4(SYS_LIST, (long)i, (long)path, sizeof(path), (long)&size);
        if (result <= 0) {
            break;
        }

        if (streq(path, normalized)) {
            print_path_entry(path, size, 0);
            found = 1;
            continue;
        }

        if (!starts_with(path, prefix)) {
            continue;
        }

        const char *rest = path + strlen(prefix);
        if (rest[0] == '\0') {
            continue;
        }

        char child[CHILD_NAME_MAX];
        size_t child_length = 0;
        int directory_child = 0;
        while (rest[child_length] != '\0' && rest[child_length] != '/' && child_length + 1 < sizeof(child)) {
            child[child_length] = rest[child_length];
            child_length++;
        }
        child[child_length] = '\0';
        if (child[0] == '\0') {
            continue;
        }
        if (rest[child_length] == '/') {
            directory_child = 1;
        }

        if (directory_child) {
            if (!child_seen(children, child_count, child)) {
                print_path_entry(child, 0, 1);
                remember_child(children, &child_count, child);
            }
            found = 1;
            continue;
        }

        print_path_entry(child, size, 0);
        found = 1;
    }

    if (!found) {
        write_text("ls: not found\n");
    }
}

static void command_cat(const char *path) {
    int fd = (int)syscall1(SYS_OPEN, (long)path);
    if (fd < 0) {
        write_text("cat: not found\n");
        return;
    }

    char buffer[128];
    for (;;) {
        long count = syscall3(SYS_READ, fd, (long)buffer, sizeof(buffer));
        if (count < 0) {
            write_text("cat: read failed\n");
            break;
        }
        if (count == 0) {
            break;
        }
        write_buf(buffer, (size_t)count);
    }

    syscall1(SYS_CLOSE, fd);
}

static long spawn_path(const char *path) {
    return syscall1(SYS_SPAWN, (long)path);
}

static long spawn_bg_args_fds(const char *path, const char *args, int stdin_fd, int stdout_fd) {
    return syscall4(SYS_SPAWN_BG_ARGS_FDS, (long)path, (long)args, stdin_fd, stdout_fd);
}

static long wait_nohang(uint64_t *status) {
    return syscall3(SYS_WAIT, 0, (long)status, SRV_WAIT_NOHANG);
}

static long wait_pid(uint64_t pid, uint64_t *status) {
    return syscall3(SYS_WAIT, (long)pid, (long)status, 0);
}

static void sleep_ticks(uint64_t ticks) {
    syscall1(SYS_SLEEP_TICKS, (long)ticks);
}

static int open_init_log(void) {
    return (int)syscall2(SYS_OPEN_MODE, (long)INIT_LOG, SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_APPEND);
}

static void write_fd_u64(int fd, uint64_t value) {
    char digits[21];
    uint64_t count = 0;
    if (value == 0) {
        write_fd_text(fd, "0");
        return;
    }
    while (value > 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (count > 0) {
        syscall3(SYS_WRITE, fd, (long)&digits[--count], 1);
    }
}

static void log_init_status(uint64_t status) {
    int fd = open_init_log();
    if (fd < 0) {
        return;
    }
    write_fd_text(fd, "init: /fat/etc/init.sh status ");
    write_fd_u64(fd, status);
    write_fd_text(fd, "\n");
    syscall1(SYS_CLOSE, fd);
}

static void print_exit_status(long status) {
    write_text("run: exited ");
    print_u64((uint64_t)status);
    write_text("\n");
}

static void command_run(const char *path) {
    long status = spawn_path(path);
    if (status < 0) {
        write_text("run: failed\n");
        return;
    }

    print_exit_status(status);
}

static int command_resolve(char *path, size_t capacity, const char *command) {
    if (contains_slash(command)) {
        copy_string(path, capacity, command);
        return 1;
    }

    for (size_t i = 0; i < sizeof(path_entries) / sizeof(path_entries[0]); i++) {
        if (!join_path(path, capacity, path_entries[i], command)) {
            continue;
        }

        int fd = (int)syscall1(SYS_OPEN, (long)path);
        if (fd >= 0) {
            syscall1(SYS_CLOSE, fd);
            return 1;
        }
    }

    return 0;
}

static void command_exec(const char *command) {
    char path[PATH_MAX];
    if (!command_resolve(path, sizeof(path), command)) {
        write_text("unknown command\n");
        return;
    }

    long status = spawn_path(path);
    if (status < 0) {
        write_text("exec: failed\n");
        return;
    }

    print_exit_status(status);
}

static void command_write(char *args) {
    char *path = args;
    while (*args != '\0' && *args != ' ' && *args != '\t') {
        args++;
    }
    if (*args == '\0') {
        write_text("usage: write /fat/name text\n");
        return;
    }
    *args++ = '\0';
    args = trim(args);
    if (*args == '\0') {
        write_text("usage: write /fat/name text\n");
        return;
    }

    long result = syscall3(SYS_FS_WRITE, (long)path, (long)args, (long)strlen(args));
    if (result < 0) {
        write_text("write: failed\n");
        return;
    }
    write_text("write: ok\n");
}

static void run_command(char *line) {
    char *command = trim(line);
    if (*command == '\0') {
        return;
    }

    if (streq(command, "help")) {
        command_help();
    } else if (streq(command, "path")) {
        command_path();
    } else if (streq(command, "ls")) {
        command_ls("/");
    } else if (starts_with(command, "ls ")) {
        command_ls(trim(command + 3));
    } else if (starts_with(command, "cat ")) {
        command_cat(trim(command + 4));
    } else if (starts_with(command, "write ")) {
        command_write(trim(command + 6));
    } else if (starts_with(command, "run ")) {
        command_run(trim(command + 4));
    } else if (streq(command, "notes")) {
        command_run("/fat/bin-cat");
    } else if (streq(command, "exit")) {
        syscall1(SYS_EXIT, 0);
    } else {
        command_exec(command);
    }
}

static int system_main(void) {
    int log_fd = open_init_log();
    uint64_t status = 0;
    long pid = -1;
    if (log_fd >= 0) {
        write_fd_text(log_fd, "init: system init starting\n");
    }
    pid = spawn_bg_args_fds("/fat/bin/sh", "/fat/etc/init.sh", -1, log_fd);
    if (log_fd >= 0) {
        syscall1(SYS_CLOSE, log_fd);
        log_fd = -1;
    }
    if (pid < 0) {
        log_init_status(1);
        write_text("init: startup failed\n");
    } else {
        long waited = wait_pid((uint64_t)pid, &status);
        log_init_status(waited > 0 ? status : 1);
        write_text("init: startup complete\n");
    }

    for (;;) {
        uint64_t child_status = 0;
        while (wait_nohang(&child_status) > 0) {
        }
        sleep_ticks(100);
    }
}

int main(int argc, char **argv) {
    char line[LINE_MAX];

    if (argc > 1 && streq(argv[1], "--system")) {
        return system_main();
    }

    write_text("srvsh: userspace shell ready\n");
    command_help();

    for (;;) {
        write_text("srvsh> ");
        read_line(line, sizeof(line));
        run_command(line);
    }

    return (int)syscall0(SYS_EXIT);
}
