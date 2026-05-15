#include "isr_frame.h"

#include <srvros/console.h>
#include <srvros/exfat.h>
#include <srvros/gui.h>
#include <srvros/heap.h>
#include <srvros/halt.h>
#include <srvros/keyboard.h>
#include <srvros/mouse.h>
#include <srvros/net.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <srvros/syscall_numbers.h>
#include <srvros/timer.h>
#include <srvros/vfs.h>
#include <srvros/vmm.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_PATH_LENGTH 160
#define MAX_SYSCALL_COPY (256 * 1024)
#define MAX_POLL_FDS 32
#define MAX_EXEC_ARGS 16
#define MAX_EXEC_ENV 16
#define MAX_EXEC_STRING 256

struct syscall_stat {
    uint64_t size;
    uint64_t type;
};

struct syscall_console_info {
    uint64_t columns;
    uint64_t rows;
};

struct syscall_gfx_info {
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
};

struct syscall_mouse_event {
    int32_t dx;
    int32_t dy;
    uint8_t buttons;
    uint8_t flags;
    uint16_t reserved;
};

struct syscall_process_info {
    uint64_t pid;
    char name[64];
    char state[24];
};

struct syscall_pollfd {
    int32_t fd;
    int16_t events;
    int16_t revents;
};

struct syscall_exec_request {
    const char *path;
    char *const *argv;
    char *const *envp;
    uint64_t flags;
    int64_t stdin_fd;
    int64_t stdout_fd;
    int64_t stderr_fd;
};

static struct srv_termios console_termios = {
    .iflag = SRV_TTY_IFLAG_ICRNL,
    .oflag = 0,
    .cflag = 0,
    .lflag = SRV_TTY_LFLAG_ICANON | SRV_TTY_LFLAG_ISIG,
    .cc = {
        [SRV_TTY_VEOF] = 4,
        [SRV_TTY_VEOL] = '\n',
        [SRV_TTY_VERASE] = 127,
        [SRV_TTY_VINTR] = 3,
        [SRV_TTY_VKILL] = 21,
        [SRV_TTY_VTIME] = 0,
        [SRV_TTY_VMIN] = 1,
    },
};

static struct srv_winsize console_winsize;
static bool console_winsize_set;

static uint64_t irq_save(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void irq_restore(uint64_t flags) {
    if ((flags & (1ull << 9)) != 0) {
        __asm__ volatile ("sti" : : : "memory");
    }
}

static bool user_buffer_ok(const void *buffer, uint64_t length, bool writable) {
    if (length == 0) {
        return true;
    }
    if (buffer == NULL) {
        return false;
    }

    return vmm_user_range_mapped((uint64_t)buffer, length, writable);
}

static bool copy_from_user(void *destination, const void *source, uint64_t length) {
    if (!user_buffer_ok(source, length, false) || (destination == NULL && length != 0)) {
        return false;
    }

    uint8_t *out = destination;
    const uint8_t *in = source;
    for (uint64_t i = 0; i < length; i++) {
        out[i] = in[i];
    }
    return true;
}

static bool copy_to_user(void *destination, const void *source, uint64_t length) {
    if (!user_buffer_ok(destination, length, true) || (source == NULL && length != 0)) {
        return false;
    }

    uint8_t *out = destination;
    const uint8_t *in = source;
    for (uint64_t i = 0; i < length; i++) {
        out[i] = in[i];
    }
    return true;
}

static int64_t syscall_write(uint64_t fd, const char *buffer, uint64_t length) {
    if (!user_buffer_ok(buffer, length, false)) {
        return -1;
    }

    if (fd != 1 && fd != 2) {
        struct process_file *file = process_file_at(process_current(), fd);
        if (file == NULL) {
            return -1;
        }
        if (file->type == PROCESS_FILE_STDIO && (file->handle == 1 || file->handle == 2)) {
            int64_t redirected = process_output_write(process_current(), file->handle, (const uint8_t *)buffer, length);
            if (redirected != -2) {
                return redirected;
            }
            for (uint64_t i = 0; i < length; i++) {
                console_putc(buffer[i]);
            }
            return (int64_t)length;
        }
        if (file->type == PROCESS_FILE_VFS_WRITE) {
            return process_file_write(process_current(), fd, (const uint8_t *)buffer, length);
        }
        if (file->type == PROCESS_FILE_PIPE_WRITE) {
            return process_file_pipe_write(process_current(), fd, (const uint8_t *)buffer, length);
        }
        if (file->type == PROCESS_FILE_NET_CONNECTION) {
            return net_respond(file->handle, buffer, length);
        }
        return -1;
    }

    int64_t redirected = process_output_write(process_current(), fd, (const uint8_t *)buffer, length);
    if (redirected != -2) {
        return redirected;
    }

    for (uint64_t i = 0; i < length; i++) {
        console_putc(buffer[i]);
    }
    return (int64_t)length;
}

static char tty_filter_input(char c) {
    if ((console_termios.iflag & SRV_TTY_IFLAG_ICRNL) != 0 && c == '\r') {
        return '\n';
    }
    return c;
}

static int64_t tty_read_console(char *buffer, uint64_t length) {
    if (length == 0) {
        return 0;
    }

    __asm__ volatile ("sti" : : : "memory");
    if ((console_termios.lflag & SRV_TTY_LFLAG_ICANON) == 0) {
        char c = 0;
        if (console_termios.cc[SRV_TTY_VMIN] == 0 && !keyboard_scan_char(&c)) {
            return 0;
        }
        if (c == 0) {
            c = keyboard_read_char();
        }
        c = tty_filter_input(c);
        buffer[0] = c;
        if ((console_termios.lflag & SRV_TTY_LFLAG_ECHO) != 0) {
            console_putc(c);
        }
        return 1;
    }

    uint64_t count = 0;
    while (count < length) {
        char c = tty_filter_input(keyboard_read_char());
        char erase = console_termios.cc[SRV_TTY_VERASE] != 0 ? (char)console_termios.cc[SRV_TTY_VERASE] : '\b';
        char eof = console_termios.cc[SRV_TTY_VEOF] != 0 ? (char)console_termios.cc[SRV_TTY_VEOF] : 4;
        char kill = console_termios.cc[SRV_TTY_VKILL] != 0 ? (char)console_termios.cc[SRV_TTY_VKILL] : 21;
        char eol = console_termios.cc[SRV_TTY_VEOL] != 0 ? (char)console_termios.cc[SRV_TTY_VEOL] : '\n';

        if (c == eof) {
            return (int64_t)count;
        }
        if ((c == erase || c == '\b') && count > 0) {
            count--;
            if ((console_termios.lflag & SRV_TTY_LFLAG_ECHO) != 0) {
                console_putc('\b');
                console_putc(' ');
                console_putc('\b');
            }
            continue;
        }
        if (c == kill) {
            while (count > 0) {
                count--;
                if ((console_termios.lflag & SRV_TTY_LFLAG_ECHO) != 0) {
                    console_putc('\b');
                    console_putc(' ');
                    console_putc('\b');
                }
            }
            continue;
        }

        buffer[count++] = c;
        if ((console_termios.lflag & SRV_TTY_LFLAG_ECHO) != 0) {
            console_putc(c);
        }
        if (c == '\n' || c == eol) {
            break;
        }
    }
    return (int64_t)count;
}

static int64_t syscall_read(uint64_t fd, char *buffer, uint64_t length) {
    if (!user_buffer_ok(buffer, length, true)) {
        return -1;
    }

    if (fd == 0) {
        int64_t redirected = process_stdin_read(process_current(), (uint8_t *)buffer, length);
        if (redirected != -2) {
            return redirected;
        }
        return tty_read_console(buffer, length);
    }

    struct process_file *file = process_file_at(process_current(), fd);
    if (file == NULL) {
        return -1;
    }
    if (file->type == PROCESS_FILE_STDIO && file->handle == 0) {
        return tty_read_console(buffer, length);
    }
    if (file->type == PROCESS_FILE_NET_CONNECTION) {
        return net_read(file->handle, buffer, length, process_file_nonblocking(process_current(), fd));
    }
    if (file->type == PROCESS_FILE_PIPE_READ) {
        return process_file_pipe_read(process_current(), fd, (uint8_t *)buffer, length);
    }
    if (file->type != PROCESS_FILE_VFS && file->type != PROCESS_FILE_VFS_WRITE) {
        return -1;
    }

    return process_file_read(process_current(), fd, (uint8_t *)buffer, length);
}

static bool syscall_tty_fd_ok(uint64_t fd) {
    if (fd <= 2) {
        return true;
    }
    struct process_file *file = process_file_at(process_current(), fd);
    return file != NULL && file->type == PROCESS_FILE_STDIO && file->handle <= 2;
}

static int64_t syscall_tty_getattr(uint64_t fd, struct srv_termios *termios) {
    if (!syscall_tty_fd_ok(fd) || !user_buffer_ok(termios, sizeof(*termios), true)) {
        return -1;
    }
    return copy_to_user(termios, &console_termios, sizeof(console_termios)) ? 0 : -1;
}

static int64_t syscall_tty_setattr(uint64_t fd, uint64_t actions, const struct srv_termios *termios) {
    struct srv_termios copy;
    if (!syscall_tty_fd_ok(fd) || actions > SRV_TCSAFLUSH ||
        !copy_from_user(&copy, termios, sizeof(copy))) {
        return -1;
    }
    console_termios = copy;
    return 0;
}

static struct srv_winsize tty_current_winsize(void) {
    if (console_winsize_set) {
        return console_winsize;
    }

    uint64_t columns = 0;
    uint64_t rows = 0;
    uint64_t width = 0;
    uint64_t height = 0;
    uint64_t pitch = 0;
    console_get_size(&columns, &rows);
    (void)console_framebuffer_info(&width, &height, &pitch);
    struct srv_winsize winsize = {
        .row = rows > UINT16_MAX ? UINT16_MAX : (uint16_t)rows,
        .col = columns > UINT16_MAX ? UINT16_MAX : (uint16_t)columns,
        .xpixel = width > UINT16_MAX ? UINT16_MAX : (uint16_t)width,
        .ypixel = height > UINT16_MAX ? UINT16_MAX : (uint16_t)height,
    };
    return winsize;
}

static int64_t syscall_ioctl(uint64_t fd, uint64_t request, void *argument) {
    if (!syscall_tty_fd_ok(fd)) {
        return -1;
    }

    if (request == SRV_TIOCGWINSZ) {
        struct srv_winsize winsize = tty_current_winsize();
        return copy_to_user(argument, &winsize, sizeof(winsize)) ? 0 : -1;
    }
    if (request == SRV_TIOCSWINSZ) {
        struct srv_winsize winsize;
        if (!copy_from_user(&winsize, argument, sizeof(winsize)) ||
            winsize.row == 0 ||
            winsize.col == 0) {
            return -1;
        }
        console_winsize = winsize;
        console_winsize_set = true;
        return 0;
    }

    return -1;
}

static bool copy_user_string(const char *source, char *destination, uint64_t capacity) {
    if (source == NULL || destination == NULL || capacity == 0) {
        return false;
    }

    for (uint64_t i = 0; i < capacity; i++) {
        if (!user_buffer_ok(source + i, 1, false)) {
            return false;
        }
        destination[i] = source[i];
        if (destination[i] == '\0') {
            return true;
        }
    }

    destination[capacity - 1] = '\0';
    return false;
}

static int64_t syscall_open(const char *user_path) {
    char path[MAX_PATH_LENGTH];
    if (!copy_user_string(user_path, path, sizeof(path))) {
        return -1;
    }

    const struct vfs_node *node = vfs_lookup(path);
    const uint8_t *data;
    uint64_t size;
    if (node == NULL || !vfs_read_all(node, &data, &size)) {
        return -1;
    }

    return process_file_alloc(process_current(), node, data, size);
}

static int64_t syscall_open_mode(const char *user_path, uint64_t flags) {
    char path[MAX_PATH_LENGTH];
    if (!copy_user_string(user_path, path, sizeof(path))) {
        return -1;
    }

    if ((flags & SRV_OPEN_WRITE) != 0) {
        return process_file_open_write(process_current(), path, flags);
    }
    return syscall_open(user_path);
}

static int64_t syscall_close(uint64_t fd) {
    return process_file_close(process_current(), fd);
}

static int64_t syscall_list(uint64_t index, char *path_buffer, uint64_t path_capacity, uint64_t *size_out) {
    if (path_buffer == NULL ||
        size_out == NULL ||
        path_capacity == 0 ||
        !user_buffer_ok(path_buffer, path_capacity, true) ||
        !user_buffer_ok(size_out, sizeof(*size_out), true)) {
        return -1;
    }

    const struct vfs_node *node = vfs_node_at(index);
    if (node == NULL) {
        return 0;
    }

    uint64_t i = 0;
    while (node->path[i] != '\0' && i + 1 < path_capacity) {
        path_buffer[i] = node->path[i];
        i++;
    }
    path_buffer[i] = '\0';
    *size_out = node->size;
    return 1;
}

static int64_t syscall_spawn(const char *user_path) {
    char path[MAX_PATH_LENGTH];
    int64_t result;
    if (!copy_user_string(user_path, path, sizeof(path))) {
        return -1;
    }

    result = process_spawn_elf(path);
    process_refresh_mappings(process_current());
    return result;
}

static int64_t syscall_spawn_args(const char *user_path, const char *user_args) {
    char path[MAX_PATH_LENGTH];
    char args[256];
    int64_t result;
    if (!copy_user_string(user_path, path, sizeof(path))) {
        return -1;
    }
    if (user_args == NULL) {
        args[0] = '\0';
    } else if (!copy_user_string(user_args, args, sizeof(args))) {
        return -1;
    }

    result = process_spawn_elf_args(path, args);
    process_refresh_mappings(process_current());
    return result;
}

static int64_t syscall_spawn_bg(const char *user_path) {
    char path[MAX_PATH_LENGTH];
    uint64_t flags;
    int64_t result;
    if (!copy_user_string(user_path, path, sizeof(path))) {
        return -1;
    }

    flags = irq_save();
    result = process_spawn_background_elf(path);
    irq_restore(flags);
    process_refresh_mappings(process_current());
    return result;
}

static int64_t syscall_spawn_bg_args(const char *user_path, const char *user_args) {
    char path[MAX_PATH_LENGTH];
    char args[256];
    uint64_t flags;
    int64_t result;
    if (!copy_user_string(user_path, path, sizeof(path))) {
        return -1;
    }
    if (user_args == NULL) {
        args[0] = '\0';
    } else if (!copy_user_string(user_args, args, sizeof(args))) {
        return -1;
    }

    flags = irq_save();
    result = process_spawn_background_elf_args(path, args);
    irq_restore(flags);
    process_refresh_mappings(process_current());
    return result;
}

static int64_t syscall_spawn_bg_args_fds(const char *user_path,
    const char *user_args,
    int64_t stdin_fd,
    int64_t stdout_fd) {
    char path[MAX_PATH_LENGTH];
    char args[256];
    uint64_t flags;
    int64_t result;
    if (!copy_user_string(user_path, path, sizeof(path))) {
        return -1;
    }
    if (user_args == NULL) {
        args[0] = '\0';
    } else if (!copy_user_string(user_args, args, sizeof(args))) {
        return -1;
    }

    flags = irq_save();
    result = process_spawn_background_elf_args_fds(path, args, stdin_fd, stdout_fd);
    irq_restore(flags);
    process_refresh_mappings(process_current());
    return result;
}

static int64_t syscall_spawn_args_redirect(const char *user_path,
    const char *user_args,
    const char *user_stdout_path,
    uint64_t append) {
    char path[MAX_PATH_LENGTH];
    char args[256];
    char stdout_path[MAX_PATH_LENGTH];
    if (!copy_user_string(user_path, path, sizeof(path)) ||
        !copy_user_string(user_stdout_path, stdout_path, sizeof(stdout_path))) {
        return -1;
    }
    if (user_args == NULL) {
        args[0] = '\0';
    } else if (!copy_user_string(user_args, args, sizeof(args))) {
        return -1;
    }

    int64_t result = process_spawn_elf_args_redirect(path, args, stdout_path, append != 0);
    process_refresh_mappings(process_current());
    return result;
}

static bool copy_user_string_vector(char *const *user_vector,
    char storage[][MAX_EXEC_STRING],
    const char *values[],
    uint64_t capacity,
    uint64_t *count_out) {
    *count_out = 0;
    if (user_vector == NULL) {
        return true;
    }
    for (uint64_t i = 0; i < capacity; i++) {
        const char *user_string = NULL;
        if (!copy_from_user(&user_string, user_vector + i, sizeof(user_string))) {
            return false;
        }
        if (user_string == NULL) {
            *count_out = i;
            return true;
        }
        if (!copy_user_string(user_string, storage[i], MAX_EXEC_STRING)) {
            return false;
        }
        values[i] = storage[i];
    }
    const char *terminator = NULL;
    if (!copy_from_user(&terminator, user_vector + capacity, sizeof(terminator)) || terminator != NULL) {
        return false;
    }
    *count_out = capacity;
    return true;
}

static int64_t syscall_exec(const struct syscall_exec_request *user_request) {
    struct syscall_exec_request request;
    char path[MAX_PATH_LENGTH];
    char argv_storage[MAX_EXEC_ARGS][MAX_EXEC_STRING];
    char env_storage[MAX_EXEC_ENV][MAX_EXEC_STRING];
    const char *argv[MAX_EXEC_ARGS];
    const char *envp[MAX_EXEC_ENV];
    uint64_t argc = 0;
    uint64_t envc = 0;
    uint64_t flags;
    int64_t result;

    if (!copy_from_user(&request, user_request, sizeof(request)) ||
        !copy_user_string(request.path, path, sizeof(path)) ||
        !copy_user_string_vector(request.argv, argv_storage, argv, MAX_EXEC_ARGS, &argc) ||
        !copy_user_string_vector(request.envp, env_storage, envp, MAX_EXEC_ENV, &envc)) {
        return -1;
    }
    if (argc == 0) {
        argv[argc++] = path;
    }

    flags = irq_save();
    if ((request.flags & SRV_EXEC_REPLACE) != 0) {
        result = process_exec_replace(path,
            argc,
            argv,
            envc,
            envp,
            request.stdin_fd,
            request.stdout_fd,
            request.stderr_fd);
    } else {
        result = process_spawn_exec(path,
            argc,
            argv,
            envc,
            envp,
            (request.flags & SRV_EXEC_BACKGROUND) != 0,
            request.stdin_fd,
            request.stdout_fd,
            request.stderr_fd);
    }
    irq_restore(flags);
    process_refresh_mappings(process_current());
    return result;
}

static int64_t syscall_fs_write(const char *user_path, const uint8_t *buffer, uint64_t length) {
    char path[MAX_PATH_LENGTH];
    uint8_t *copy;
    int64_t result;
    if (!copy_user_string(user_path, path, sizeof(path)) ||
        length == 0 ||
        length > MAX_SYSCALL_COPY) {
        return -1;
    }

    copy = kmalloc(length);
    if (copy == NULL) {
        return -1;
    }
    if (!copy_from_user(copy, buffer, length)) {
        kfree(copy);
        return -1;
    }

    result = exfat_write_file(path, copy, length) ? (int64_t)length : -1;
    kfree(copy);
    return result;
}

static int64_t syscall_fs_append(const char *user_path, const uint8_t *buffer, uint64_t length) {
    char path[MAX_PATH_LENGTH];
    const struct vfs_node *node;
    const uint8_t *old_data = NULL;
    uint64_t old_size = 0;
    uint8_t *joined;
    uint8_t *copy = NULL;
    int64_t result = -1;

    if (!copy_user_string(user_path, path, sizeof(path)) ||
        length > MAX_SYSCALL_COPY) {
        return -1;
    }
    if (length != 0) {
        copy = kmalloc(length);
        if (copy == NULL) {
            return -1;
        }
        if (!copy_from_user(copy, buffer, length)) {
            kfree(copy);
            return -1;
        }
    }

    node = vfs_lookup(path);
    if (node != NULL && !vfs_read_all(node, &old_data, &old_size)) {
        if (copy != NULL) {
            kfree(copy);
        }
        return -1;
    }

    joined = kmalloc(old_size + length + 1);
    if (joined == NULL) {
        if (node != NULL) {
            vfs_release_data(node, old_data);
        }
        if (copy != NULL) {
            kfree(copy);
        }
        return -1;
    }

    for (uint64_t i = 0; i < old_size; i++) {
        joined[i] = old_data[i];
    }
    for (uint64_t i = 0; i < length; i++) {
        joined[old_size + i] = copy[i];
    }

    if (exfat_write_file(path, joined, old_size + length)) {
        result = (int64_t)length;
    }

    kfree(joined);
    if (copy != NULL) {
        kfree(copy);
    }
    if (node != NULL) {
        vfs_release_data(node, old_data);
    }
    return result;
}

static int64_t syscall_stat(const char *user_path, struct syscall_stat *info) {
    char path[MAX_PATH_LENGTH];
    struct syscall_stat copy;
    if (!copy_user_string(user_path, path, sizeof(path))) {
        return -1;
    }

    const struct vfs_node *node = vfs_lookup(path);
    if (node == NULL) {
        return -1;
    }

    copy.size = node->size;
    copy.type = (uint64_t)node->type;
    return copy_to_user(info, &copy, sizeof(copy)) ? 0 : -1;
}

static int64_t syscall_unlink(const char *user_path) {
    char path[MAX_PATH_LENGTH];
    if (!copy_user_string(user_path, path, sizeof(path))) {
        return -1;
    }
    if (process_has_open_vfs_prefix(path)) {
        return -1;
    }

    return exfat_delete_file(path) ? 0 : -1;
}

static int64_t syscall_mkdir(const char *user_path) {
    char path[MAX_PATH_LENGTH];
    if (!copy_user_string(user_path, path, sizeof(path))) {
        return -1;
    }
    return exfat_create_directory(path) ? 0 : -1;
}

static int64_t syscall_rmdir(const char *user_path) {
    char path[MAX_PATH_LENGTH];
    if (!copy_user_string(user_path, path, sizeof(path))) {
        return -1;
    }
    if (process_has_open_vfs_prefix(path)) {
        return -1;
    }
    return exfat_delete_directory(path) ? 0 : -1;
}

static int64_t syscall_rename(const char *user_old_path, const char *user_new_path) {
    char old_path[MAX_PATH_LENGTH];
    char new_path[MAX_PATH_LENGTH];
    if (!copy_user_string(user_old_path, old_path, sizeof(old_path)) ||
        !copy_user_string(user_new_path, new_path, sizeof(new_path))) {
        return -1;
    }
    if (process_has_open_vfs_prefix(old_path) || process_has_open_vfs_prefix(new_path)) {
        return -1;
    }
    return exfat_rename(old_path, new_path) ? 0 : -1;
}

static int64_t syscall_seek(uint64_t fd, int64_t offset, uint64_t whence) {
    return process_file_seek(process_current(), fd, offset, whence);
}

static int64_t syscall_fstat(uint64_t fd, struct syscall_stat *info) {
    struct syscall_stat copy;
    if (info == NULL || !user_buffer_ok(info, sizeof(*info), true)) {
        return -1;
    }
    if (process_file_stat(process_current(), fd, &copy.size, &copy.type) < 0) {
        return -1;
    }
    return copy_to_user(info, &copy, sizeof(copy)) ? 0 : -1;
}

static int64_t syscall_sbrk(int64_t increment, uint64_t *previous_out) {
    uint64_t previous;
    if (previous_out == NULL || !user_buffer_ok(previous_out, sizeof(*previous_out), true)) {
        return -1;
    }
    if (process_sbrk(process_current(), increment, &previous) < 0) {
        return -1;
    }
    return copy_to_user(previous_out, &previous, sizeof(previous)) ? 0 : -1;
}

static int64_t syscall_dup(uint64_t fd) {
    return process_file_dup(process_current(), fd);
}

static int64_t syscall_dup2(uint64_t old_fd, uint64_t new_fd) {
    return process_file_dup2(process_current(), old_fd, new_fd);
}

static int64_t syscall_pipe(int32_t *fds_out) {
    uint64_t fds[2];
    int32_t copy[2];
    if (fds_out == NULL || !user_buffer_ok(fds_out, sizeof(copy), true)) {
        return -1;
    }
    if (process_file_pipe(process_current(), fds) < 0) {
        return -1;
    }
    copy[0] = (int32_t)fds[0];
    copy[1] = (int32_t)fds[1];
    return copy_to_user(fds_out, copy, sizeof(copy)) ? 0 : -1;
}

static int64_t poll_once(struct process *process, struct syscall_pollfd *fds, uint64_t nfds) {
    int64_t ready = 0;
    for (uint64_t i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].fd < 0) {
            continue;
        }
        uint16_t revents = process_file_poll(process, fds[i].fd, (uint16_t)fds[i].events);
        fds[i].revents = (int16_t)revents;
        if (revents != 0) {
            ready++;
        }
    }
    return ready;
}

struct syscall_poll_wait {
    struct process *process;
    struct syscall_pollfd *fds;
    uint64_t nfds;
};

static bool poll_wait_ready(void *arg) {
    struct syscall_poll_wait *wait = arg;
    return wait == NULL ||
        process_should_exit_current() ||
        poll_once(wait->process, wait->fds, wait->nfds) != 0;
}

static uint64_t poll_timeout_ticks(int64_t timeout_ms) {
    if (timeout_ms <= 0) {
        return 0;
    }
    return ((uint64_t)timeout_ms * 100 + 999) / 1000;
}

static int64_t syscall_poll(struct syscall_pollfd *user_fds, uint64_t nfds, int64_t timeout_ms) {
    if (nfds > MAX_POLL_FDS ||
        (nfds != 0 && !user_buffer_ok(user_fds, nfds * sizeof(*user_fds), true))) {
        return -1;
    }

    struct syscall_pollfd fds[MAX_POLL_FDS];
    if (nfds != 0 && !copy_from_user(fds, user_fds, nfds * sizeof(*user_fds))) {
        return -1;
    }

    struct process *process = process_current();
    uint64_t start = timer_ticks();
    uint64_t timeout_ticks = poll_timeout_ticks(timeout_ms);
    uint64_t deadline_ticks = timeout_ms > 0 ? start + timeout_ticks : 0;
    struct syscall_poll_wait wait = {
        .process = process,
        .fds = fds,
        .nfds = nfds,
    };
    __asm__ volatile ("sti" : : : "memory");

    for (;;) {
        if (process_should_exit_current()) {
            return -1;
        }

        int64_t ready = poll_once(process, fds, nfds);
        if (ready != 0 || timeout_ms == 0) {
            return copy_to_user(user_fds, fds, nfds * sizeof(*user_fds)) ? ready : -1;
        }
        if (timeout_ms > 0 && timer_ticks() - start >= timeout_ticks) {
            return copy_to_user(user_fds, fds, nfds * sizeof(*user_fds)) ? 0 : -1;
        }
        if (!scheduler_wait_timeout(process_file_poll_wait_queue(), poll_wait_ready, &wait, deadline_ticks)) {
            scheduler_yield();
        }
    }
}

static int64_t syscall_fcntl(uint64_t fd, uint64_t command, uint64_t arg) {
    if (command == SRV_F_GETFD) {
        return process_file_get_fd_flags(process_current(), fd);
    }
    if (command == SRV_F_SETFD) {
        return process_file_set_fd_flags(process_current(), fd, arg);
    }
    if (command == SRV_F_GETFL) {
        return process_file_get_flags(process_current(), fd);
    }
    if (command == SRV_F_SETFL) {
        return process_file_set_flags(process_current(), fd, arg);
    }
    if (command == SRV_F_GETLK) {
        struct srv_flock lock;
        if (!copy_from_user(&lock, (const void *)arg, sizeof(lock))) {
            return -1;
        }
        int64_t result = process_file_get_lock(process_current(), fd, &lock);
        if (result < 0 || !copy_to_user((void *)arg, &lock, sizeof(lock))) {
            return -1;
        }
        return result;
    }
    if (command == SRV_F_SETLK || command == SRV_F_SETLKW) {
        struct srv_flock lock;
        if (!copy_from_user(&lock, (const void *)arg, sizeof(lock))) {
            return -1;
        }
        return process_file_set_lock(process_current(), fd, &lock, command == SRV_F_SETLKW);
    }
    return -1;
}

static int64_t syscall_ftruncate(uint64_t fd, uint64_t length) {
    return process_file_truncate(process_current(), fd, length);
}

static int64_t syscall_fsync(uint64_t fd) {
    return process_file_flush(process_current(), fd);
}

static int64_t syscall_console_info(struct syscall_console_info *info) {
    struct syscall_console_info copy;
    console_get_size(&copy.columns, &copy.rows);
    return copy_to_user(info, &copy, sizeof(copy)) ? 0 : -1;
}

static int64_t syscall_console_clear(void) {
    uint64_t previous = vmm_current_address_space();
    uint64_t kernel = vmm_kernel_address_space();
    if (kernel != 0 && previous != kernel) {
        vmm_switch_address_space(kernel);
    }
    console_clear();
    if (kernel != 0 && previous != kernel) {
        vmm_switch_address_space(previous);
    }
    return 0;
}

static int64_t syscall_console_gotoxy(uint64_t x, uint64_t y) {
    uint64_t previous = vmm_current_address_space();
    uint64_t kernel = vmm_kernel_address_space();
    if (kernel != 0 && previous != kernel) {
        vmm_switch_address_space(kernel);
    }
    console_set_cursor(x, y);
    if (kernel != 0 && previous != kernel) {
        vmm_switch_address_space(previous);
    }
    return 0;
}

static int64_t syscall_key_scan(void) {
    char c = 0;
    if (!keyboard_scan_char(&c)) {
        return 0;
    }
    return (uint8_t)c;
}

static int64_t syscall_gfx_info(struct syscall_gfx_info *info) {
    struct syscall_gfx_info copy;

    if (!console_framebuffer_info(&copy.width, &copy.height, &copy.pitch)) {
        return -1;
    }
    return copy_to_user(info, &copy, sizeof(copy)) ? 0 : -1;
}

static int64_t syscall_gfx_put_pixel(uint64_t x, uint64_t y, uint32_t rgb) {
    uint64_t previous = vmm_current_address_space();
    uint64_t kernel = vmm_kernel_address_space();
    bool ok;
    if (kernel != 0 && previous != kernel) {
        vmm_switch_address_space(kernel);
    }
    ok = console_put_pixel(x, y, rgb);
    if (kernel != 0 && previous != kernel) {
        vmm_switch_address_space(previous);
    }
    return ok ? 0 : -1;
}

static int64_t syscall_gfx_fill_rect(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t rgb) {
    uint64_t previous = vmm_current_address_space();
    uint64_t kernel = vmm_kernel_address_space();
    bool ok;
    if (kernel != 0 && previous != kernel) {
        vmm_switch_address_space(kernel);
    }
    ok = console_fill_rectangle(x, y, width, height, rgb);
    if (kernel != 0 && previous != kernel) {
        vmm_switch_address_space(previous);
    }
    return ok ? 0 : -1;
}

static int64_t syscall_mouse_scan(struct syscall_mouse_event *event) {
    struct mouse_event kernel_event;
    struct syscall_mouse_event copy;
    if (!mouse_scan(&kernel_event)) {
        return 0;
    }

    copy.dx = kernel_event.dx;
    copy.dy = kernel_event.dy;
    copy.buttons = kernel_event.buttons;
    copy.flags = kernel_event.flags;
    copy.reserved = 0;
    return copy_to_user(event, &copy, sizeof(copy)) ? 1 : -1;
}

static int64_t syscall_gui_register(void) {
    return gui_register_server();
}

static int64_t syscall_gui_send(const struct gui_message *message) {
    struct gui_message copy;
    if (!copy_from_user(&copy, message, sizeof(copy))) {
        return -1;
    }
    return gui_send(&copy);
}

static int64_t syscall_gui_recv(struct gui_message *message) {
    struct gui_message copy;
    int64_t result;
    if (!user_buffer_ok(message, sizeof(*message), true)) {
        return -1;
    }
    result = gui_recv(&copy);
    if (result > 0) {
        if (!copy_to_user(message, &copy, sizeof(copy))) {
            return -1;
        }
    }
    return result;
}

static int64_t syscall_yield(void) {
    scheduler_yield();
    return 0;
}

static void copy_kernel_string(char *destination, uint64_t capacity, const char *source) {
    uint64_t i = 0;
    if (capacity == 0) {
        return;
    }
    while (source != NULL && source[i] != '\0' && i + 1 < capacity) {
        destination[i] = source[i];
        i++;
    }
    destination[i] = '\0';
}

static int64_t syscall_proc_list(uint64_t index, struct syscall_process_info *info) {
    uint64_t pid = 0;
    const char *name = "";
    const char *state = "";
    struct syscall_process_info copy;
    uint64_t next = process_list(index, &pid, &name, &state);
    if (next == 0) {
        return 0;
    }
    copy.pid = pid;
    copy_kernel_string(copy.name, sizeof(copy.name), name);
    copy_kernel_string(copy.state, sizeof(copy.state), state);
    return copy_to_user(info, &copy, sizeof(copy)) ? (int64_t)next : -1;
}

static int64_t syscall_kill(uint64_t pid) {
    return process_kill_pid(pid) ? 0 : -1;
}

static int64_t syscall_wait(uint64_t pid, uint64_t *status_out, uint64_t flags) {
    uint64_t status = 0;
    __asm__ volatile ("sti" : : : "memory");
    int64_t result = process_wait(pid, &status, (flags & SRV_WAIT_NOHANG) != 0);
    if (result > 0 && status_out != NULL && !copy_to_user(status_out, &status, sizeof(status))) {
        return -1;
    }
    return result;
}

static int64_t syscall_net_listen(uint16_t port) {
    struct process *process = process_current();
    if (process == NULL) {
        return -1;
    }

    int64_t handle = net_listen(port);
    if (handle < 0) {
        return -1;
    }

    int64_t fd = process_handle_alloc(process, PROCESS_FILE_NET_LISTENER, (uint64_t)handle);
    if (fd < 0) {
        net_close((uint64_t)handle);
    }
    return fd;
}

static int64_t syscall_net_dhcp(void) {
    return net_dhcp_request();
}

static int64_t syscall_net_status(void) {
    net_print_status();
    return 0;
}

static int64_t syscall_net_dns(const char *user_name, uint32_t *ip_out) {
    char name[128];
    uint32_t ip = 0;
    if (!copy_user_string(user_name, name, sizeof(name)) ||
        (ip_out != NULL && !user_buffer_ok(ip_out, sizeof(*ip_out), true))) {
        return -1;
    }
    int64_t result = net_dns_resolve(name, &ip);
    if (result < 0) {
        return -1;
    }
    if (ip_out != NULL && !copy_to_user(ip_out, &ip, sizeof(ip))) {
        return -1;
    }
    return result;
}

static int64_t syscall_net_accept(uint64_t listener_fd, char *buffer, uint64_t capacity, uint64_t *length_out) {
    if ((capacity != 0 && !user_buffer_ok(buffer, capacity, true)) ||
        (length_out != NULL && !user_buffer_ok(length_out, sizeof(*length_out), true))) {
        return -1;
    }

    struct process *process = process_current();
    struct process_file *listener = process_file_at(process, listener_fd);
    if (listener == NULL || listener->type != PROCESS_FILE_NET_LISTENER) {
        return -1;
    }

    int64_t handle = net_accept(listener->handle,
        buffer,
        capacity,
        length_out,
        process_file_nonblocking(process, listener_fd));
    if (handle < 0) {
        return handle;
    }

    int64_t fd = process_handle_alloc(process, PROCESS_FILE_NET_CONNECTION, (uint64_t)handle);
    if (fd < 0) {
        net_close((uint64_t)handle);
    }
    return fd;
}

static int64_t syscall_getpid(void) {
    struct process *process = process_current();
    return process == NULL ? 0 : (int64_t)process_pid(process);
}

static int64_t syscall_sleep_ticks(uint64_t ticks) {
    uint64_t start = timer_ticks();
    __asm__ volatile ("sti" : : : "memory");
    while (timer_ticks() - start < ticks) {
        if (process_should_exit_current()) {
            return -1;
        }
        scheduler_yield();
    }
    return 0;
}

void syscall_dispatch(struct isr_frame *frame) {
    switch (frame->rax) {
    case SYS_WRITE:
        frame->rax = (uint64_t)syscall_write(frame->rdi, (const char *)frame->rsi, frame->rdx);
        return;
    case SYS_EXIT:
        if (!process_current_quiet()) {
            console_printf("\nprocess: exited status=%u\n", frame->rdi);
        }
        process_exit(frame->rdi);
    case SYS_OPEN:
        frame->rax = (uint64_t)syscall_open((const char *)frame->rdi);
        return;
    case SYS_READ:
        frame->rax = (uint64_t)syscall_read(frame->rdi, (char *)frame->rsi, frame->rdx);
        return;
    case SYS_CLOSE:
        frame->rax = (uint64_t)syscall_close(frame->rdi);
        return;
    case SYS_LIST:
        frame->rax = (uint64_t)syscall_list(frame->rdi, (char *)frame->rsi, frame->rdx, (uint64_t *)frame->rcx);
        return;
    case SYS_SPAWN:
        frame->rax = (uint64_t)syscall_spawn((const char *)frame->rdi);
        return;
    case SYS_NET_LISTEN:
        frame->rax = (uint64_t)syscall_net_listen((uint16_t)frame->rdi);
        return;
    case SYS_NET_ACCEPT:
        frame->rax = (uint64_t)syscall_net_accept(frame->rdi, (char *)frame->rsi, frame->rdx, (uint64_t *)frame->rcx);
        return;
    case SYS_NET_DHCP:
        frame->rax = (uint64_t)syscall_net_dhcp();
        return;
    case SYS_NET_STATUS:
        frame->rax = (uint64_t)syscall_net_status();
        return;
    case SYS_NET_DNS:
        frame->rax = (uint64_t)syscall_net_dns((const char *)frame->rdi, (uint32_t *)frame->rsi);
        return;
    case SYS_FS_WRITE:
        frame->rax = (uint64_t)syscall_fs_write((const char *)frame->rdi, (const uint8_t *)frame->rsi, frame->rdx);
        return;
    case SYS_CONSOLE_INFO:
        frame->rax = (uint64_t)syscall_console_info((struct syscall_console_info *)frame->rdi);
        return;
    case SYS_CONSOLE_CLEAR:
        frame->rax = (uint64_t)syscall_console_clear();
        return;
    case SYS_CONSOLE_GOTOXY:
        frame->rax = (uint64_t)syscall_console_gotoxy(frame->rdi, frame->rsi);
        return;
    case SYS_KEY_SCAN:
        frame->rax = (uint64_t)syscall_key_scan();
        return;
    case SYS_GFX_INFO:
        frame->rax = (uint64_t)syscall_gfx_info((struct syscall_gfx_info *)frame->rdi);
        return;
    case SYS_GFX_PUT_PIXEL:
        frame->rax = (uint64_t)syscall_gfx_put_pixel(frame->rdi, frame->rsi, (uint32_t)frame->rdx);
        return;
    case SYS_GFX_FILL_RECT:
        frame->rax = (uint64_t)syscall_gfx_fill_rect(frame->rdi, frame->rsi, frame->rdx, frame->rcx, (uint32_t)frame->r8);
        return;
    case SYS_MOUSE_SCAN:
        frame->rax = (uint64_t)syscall_mouse_scan((struct syscall_mouse_event *)frame->rdi);
        return;
    case SYS_SPAWN_BG:
        frame->rax = (uint64_t)syscall_spawn_bg((const char *)frame->rdi);
        return;
    case SYS_GUI_REGISTER:
        frame->rax = (uint64_t)syscall_gui_register();
        return;
    case SYS_GUI_SEND:
        frame->rax = (uint64_t)syscall_gui_send((const struct gui_message *)frame->rdi);
        return;
    case SYS_GUI_RECV:
        frame->rax = (uint64_t)syscall_gui_recv((struct gui_message *)frame->rdi);
        return;
    case SYS_YIELD:
        frame->rax = (uint64_t)syscall_yield();
        return;
    case SYS_SPAWN_ARGS:
        frame->rax = (uint64_t)syscall_spawn_args((const char *)frame->rdi, (const char *)frame->rsi);
        return;
    case SYS_PROC_LIST:
        frame->rax = (uint64_t)syscall_proc_list(frame->rdi, (struct syscall_process_info *)frame->rsi);
        return;
    case SYS_KILL:
        frame->rax = (uint64_t)syscall_kill(frame->rdi);
        return;
    case SYS_SPAWN_BG_ARGS:
        frame->rax = (uint64_t)syscall_spawn_bg_args((const char *)frame->rdi, (const char *)frame->rsi);
        return;
    case SYS_STAT:
        frame->rax = (uint64_t)syscall_stat((const char *)frame->rdi, (struct syscall_stat *)frame->rsi);
        return;
    case SYS_FS_APPEND:
        frame->rax = (uint64_t)syscall_fs_append((const char *)frame->rdi, (const uint8_t *)frame->rsi, frame->rdx);
        return;
    case SYS_SEEK:
        frame->rax = (uint64_t)syscall_seek(frame->rdi, (int64_t)frame->rsi, frame->rdx);
        return;
    case SYS_SPAWN_ARGS_REDIRECT:
        frame->rax = (uint64_t)syscall_spawn_args_redirect((const char *)frame->rdi,
            (const char *)frame->rsi,
            (const char *)frame->rdx,
            frame->rcx);
        return;
    case SYS_UNLINK:
        frame->rax = (uint64_t)syscall_unlink((const char *)frame->rdi);
        return;
    case SYS_OPEN_MODE:
        frame->rax = (uint64_t)syscall_open_mode((const char *)frame->rdi, frame->rsi);
        return;
    case SYS_WAIT:
        frame->rax = (uint64_t)syscall_wait(frame->rdi, (uint64_t *)frame->rsi, frame->rdx);
        return;
    case SYS_MKDIR:
        frame->rax = (uint64_t)syscall_mkdir((const char *)frame->rdi);
        return;
    case SYS_RENAME:
        frame->rax = (uint64_t)syscall_rename((const char *)frame->rdi, (const char *)frame->rsi);
        return;
    case SYS_RMDIR:
        frame->rax = (uint64_t)syscall_rmdir((const char *)frame->rdi);
        return;
    case SYS_GETPID:
        frame->rax = (uint64_t)syscall_getpid();
        return;
    case SYS_TICKS:
        frame->rax = timer_ticks();
        return;
    case SYS_SLEEP_TICKS:
        frame->rax = (uint64_t)syscall_sleep_ticks(frame->rdi);
        return;
    case SYS_FSTAT:
        frame->rax = (uint64_t)syscall_fstat(frame->rdi, (struct syscall_stat *)frame->rsi);
        return;
    case SYS_SBRK:
        frame->rax = (uint64_t)syscall_sbrk((int64_t)frame->rdi, (uint64_t *)frame->rsi);
        return;
    case SYS_DUP:
        frame->rax = (uint64_t)syscall_dup(frame->rdi);
        return;
    case SYS_DUP2:
        frame->rax = (uint64_t)syscall_dup2(frame->rdi, frame->rsi);
        return;
    case SYS_PIPE:
        frame->rax = (uint64_t)syscall_pipe((int32_t *)frame->rdi);
        return;
    case SYS_SPAWN_BG_ARGS_FDS:
        frame->rax = (uint64_t)syscall_spawn_bg_args_fds((const char *)frame->rdi,
            (const char *)frame->rsi,
            (int64_t)frame->rdx,
            (int64_t)frame->rcx);
        return;
    case SYS_POLL:
        frame->rax = (uint64_t)syscall_poll((struct syscall_pollfd *)frame->rdi,
            frame->rsi,
            (int64_t)frame->rdx);
        return;
    case SYS_FCNTL:
        frame->rax = (uint64_t)syscall_fcntl(frame->rdi, frame->rsi, frame->rdx);
        return;
    case SYS_FTRUNCATE:
        frame->rax = (uint64_t)syscall_ftruncate(frame->rdi, frame->rsi);
        return;
    case SYS_FSYNC:
        frame->rax = (uint64_t)syscall_fsync(frame->rdi);
        return;
    case SYS_EXEC:
        frame->rax = (uint64_t)syscall_exec((const struct syscall_exec_request *)frame->rdi);
        return;
    case SYS_TTY_GETATTR:
        frame->rax = (uint64_t)syscall_tty_getattr(frame->rdi, (struct srv_termios *)frame->rsi);
        return;
    case SYS_TTY_SETATTR:
        frame->rax = (uint64_t)syscall_tty_setattr(frame->rdi, frame->rsi, (const struct srv_termios *)frame->rdx);
        return;
    case SYS_IOCTL:
        frame->rax = (uint64_t)syscall_ioctl(frame->rdi, frame->rsi, (void *)frame->rdx);
        return;
    default:
        frame->rax = (uint64_t)-1;
        return;
    }
}
