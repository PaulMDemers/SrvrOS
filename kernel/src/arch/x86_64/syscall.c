#include <srvros/devices.h>
#include <srvros/thermal.h>
#include <srvros/smc.h>
#include <srvros/battery_snapshot.h>
#include <srvros/battery_control.h>
#include <srvros/scheduler_abi.h>
#include <srvros/audio.h>
#include <srvros/audio_abi.h>
#include <srvros/audio_stream_abi.h>
#include "isr_frame.h"

#include <srvros/block.h>
#include <srvros/console.h>
#include <srvros/exfat.h>
#include <srvros/gui.h>
#include <srvros/heap.h>
#include <srvros/halt.h>
#include <srvros/keyboard.h>
#include <srvros/intel_gfx.h>
#include <srvros/mouse.h>
#include <srvros/net.h>
#include <srvros/pmm.h>
#include <srvros/process.h>
#include <srvros/pty.h>
#include <srvros/scheduler.h>
#include <srvros/serial.h>
#include <srvros/syscall_numbers.h>
#include <srvros/timer.h>
#include <srvros/vfs.h>
#include <srvros/vmm.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_PATH_LENGTH 160
#define MAX_SYSCALL_COPY (256 * 1024)
#define MAX_POLL_FDS 64
#define MAX_EXEC_ARGS 16
#define MAX_EXEC_ENV 64
#define MAX_EXEC_STRING 256

struct syscall_stat {
    uint64_t abi_version;
    uint64_t struct_size;
    uint64_t size;
    uint64_t type;
    uint64_t inode;
    uint64_t mode;
    uint64_t nlink;
    uint64_t uid;
    uint64_t gid;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint64_t blksize;
    uint64_t blocks;
};

struct syscall_fsinfo {
    uint64_t abi_version;
    uint64_t struct_size;
    uint64_t block_size;
    uint64_t blocks;
    uint64_t blocks_free;
    uint64_t blocks_available;
    uint64_t files;
    uint64_t files_free;
    char filesystem[16];
    char mountpoint[160];
};

struct syscall_console_info {
    uint64_t abi_version;
    uint64_t struct_size;
    uint64_t columns;
    uint64_t rows;
};

struct syscall_gfx_info {
    uint64_t abi_version;
    uint64_t struct_size;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint64_t flags;
    uint32_t bpp;
    uint32_t memory_model;
    uint32_t red_mask_size;
    uint32_t red_mask_shift;
    uint32_t green_mask_size;
    uint32_t green_mask_shift;
    uint32_t blue_mask_size;
    uint32_t blue_mask_shift;
    uint64_t scale_num;
    uint64_t scale_den;
    uint64_t accel_backend;
    uint64_t reserved;
};

struct syscall_mouse_event {
    uint64_t abi_version;
    uint64_t struct_size;
    int32_t dx;
    int32_t dy;
    uint8_t buttons;
    uint8_t flags;
    uint16_t reserved;
    uint32_t padding;
    int32_t wheel_x, wheel_y;
};

struct syscall_process_info {
    uint64_t abi_version;
    uint64_t struct_size;
    uint64_t pid;
    char name[64];
    char state[24];
    uint64_t parent_pid;
    uint64_t user_mapped_bytes; /* Private mapped user pages; excludes kernel and shared surfaces. */
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
    uint64_t process_group;
    uint64_t foreground;
    const struct process_spawn_file_action *file_actions;
    uint64_t file_action_count;
    uint64_t signal_mask;
    uint64_t signal_default;
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
static uint64_t random_state;

static uint64_t irq_save(void) {
#ifdef SRVROS_SYSCALL_POLL_HOST_TEST
    return 0;
#else
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
#endif
}

static void irq_restore(uint64_t flags) {
#ifdef SRVROS_SYSCALL_POLL_HOST_TEST
    (void)flags;
#else
    if ((flags & (1ull << 9)) != 0) {
        __asm__ volatile ("sti" : : : "memory");
    }
#endif
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

static uint64_t min_u64(uint64_t a, uint64_t b) {
    return a < b ? a : b;
}

static uint64_t abi_user_struct_size(const void *user_info, uint64_t kernel_size, uint64_t abi_version) {
    struct srv_abi_header header;
    if (user_info == NULL ||
        !copy_from_user(&header, user_info, sizeof(header)) ||
        header.abi_version != abi_version ||
        header.struct_size < sizeof(header)) {
        return 0;
    }
    return min_u64(header.struct_size, kernel_size);
}

static bool copy_abi_struct_to_user(void *user_info,
    const void *kernel_info,
    uint64_t kernel_size,
    uint64_t abi_version) {
    uint64_t user_size = abi_user_struct_size(user_info, kernel_size, abi_version);
    if (user_size == 0) {
        return false;
    }
    return copy_to_user(user_info, kernel_info, user_size);
}

static bool copy_net_struct_to_user(void *user_info, const void *kernel_info, uint64_t kernel_size) {
    return copy_abi_struct_to_user(user_info, kernel_info, kernel_size, SRV_NET_ABI_VERSION);
}

static void cpuid_leaf(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile (
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(0));
}

static bool cpu_has_rdrand(void) {
    static bool initialized;
    static bool supported;
    if (!initialized) {
        uint32_t eax;
        uint32_t ebx;
        uint32_t ecx;
        uint32_t edx;
        cpuid_leaf(1, &eax, &ebx, &ecx, &edx);
        supported = (ecx & (1u << 30)) != 0;
        initialized = true;
    }
    return supported;
}

static bool rdrand64(uint64_t *value) {
    uint8_t ok;
    if (!cpu_has_rdrand()) {
        return false;
    }
    for (uint64_t attempt = 0; attempt < 10; attempt++) {
        __asm__ volatile (
            "rdrand %0; setc %1"
            : "=r"(*value), "=qm"(ok));
        if (ok != 0) {
            return true;
        }
    }
    return false;
}

static uint64_t rdtsc(void) {
    uint32_t low;
    uint32_t high;
    __asm__ volatile ("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

static uint64_t splitmix64_next(uint64_t *state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

static uint64_t random_u64(void) {
    uint64_t hardware;
    uint64_t flags = irq_save();
    if (random_state == 0) {
        random_state = rdtsc() ^
            (timer_ticks() << 32) ^
            (process_pid(process_current()) << 16) ^
            (uint64_t)(uintptr_t)&random_state ^
            0xa5a55a5ac3c33c3cull;
    }
    random_state ^= rdtsc() + (timer_ticks() << 7) + process_pid(process_current());
    uint64_t value = splitmix64_next(&random_state);
    if (rdrand64(&hardware)) {
        random_state ^= hardware;
        value ^= hardware;
    }
    irq_restore(flags);
    return value;
}

static int64_t syscall_random(void *buffer, uint64_t length, uint64_t flags) {
    if (flags != 0 || length > MAX_SYSCALL_COPY || !user_buffer_ok(buffer, length, true)) {
        return -1;
    }

    uint8_t chunk[64];
    uint64_t copied = 0;
    while (copied < length) {
        for (uint64_t offset = 0; offset < sizeof(chunk); offset += sizeof(uint64_t)) {
            uint64_t value = random_u64();
            uint64_t remaining = sizeof(chunk) - offset;
            uint64_t count = remaining < sizeof(value) ? remaining : sizeof(value);
            for (uint64_t i = 0; i < count; i++) {
                chunk[offset + i] = (uint8_t)(value >> (i * 8));
            }
        }
        uint64_t remaining = length - copied;
        uint64_t count = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
        if (!copy_to_user((uint8_t *)buffer + copied, chunk, count)) {
            return -1;
        }
        copied += count;
    }
    return (int64_t)length;
}

/* Copy while the caller's mappings are stable, then allow interrupts while
 * consuming only the owned chunk. Revalidate each subsequent user range. */
static int64_t console_write_user(const char *buffer, uint64_t length) {
    uint64_t written = 0;
    while (written < length) {
        char chunk[1024]; /* Match the bounded saved-console record size. */
        uint64_t count = length - written;
        if (count > sizeof(chunk)) count = sizeof(chunk);
        if (!copy_from_user(chunk, buffer + written, count)) return written ? (int64_t)written : -1;
        __asm__ volatile ("sti" : : : "memory");
        console_write_n(chunk, count);
        __asm__ volatile ("cli" : : : "memory");
        written += count;
        if (process_should_exit_current()) break;
    }
    return (int64_t)written;
}

static bool io_try_pipe_write(uint64_t fd) {
    struct process_file *f=process_file_at(process_current(),fd);
    return f && (f->type==PROCESS_FILE_PIPE_WRITE || f->type==PROCESS_FILE_PIPE_DUPLEX ||
        f->type==PROCESS_FILE_PTY_MASTER || f->type==PROCESS_FILE_PTY_SLAVE) &&
        (f->flags&SRV_FD_NONBLOCK);
}
static bool io_defer_pipe_write(uint64_t fd) {
    if(io_try_pipe_write(fd)) {
        /* Match the try-read contract: never enter descriptor/signal cleanup
         * while another process owns storage. The UI retries next iteration. */
        if(scheduler_io_busy()) return true;
        scheduler_io_set_user_safe(false);
    }
    return false;
}
static int64_t syscall_write(uint64_t fd, const char *buffer, uint64_t length) {
    if(io_defer_pipe_write(fd)) return length?SRV_ERR_AGAIN:0;
    if (!user_buffer_ok(buffer, length, false)) {
        return -1;
    }

    if (fd == 0) {
        int64_t redirected = process_input_write(process_current(), (const uint8_t *)buffer, length);
        return redirected != -2 ? redirected : -1;
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
            return console_write_user(buffer, length);
        }
        if (file->type == PROCESS_FILE_VFS_WRITE || file->type == PROCESS_FILE_PTY_MASTER ||
            file->type == PROCESS_FILE_PTY_SLAVE) {
            return process_file_write(process_current(), fd, (const uint8_t *)buffer, length);
        }
        if (file->type == PROCESS_FILE_PIPE_WRITE || file->type == PROCESS_FILE_PIPE_DUPLEX) {
            return process_file_pipe_write(process_current(), fd, (const uint8_t *)buffer, length);
        }
        if (file->type == PROCESS_FILE_NET_CONNECTION) {
            return net_respond(file->handle, buffer, length, process_file_nonblocking(process_current(), fd));
        }
        return -1;
    }

    int64_t redirected = process_output_write(process_current(), fd, (const uint8_t *)buffer, length);
    if (redirected != -2) {
        return redirected;
    }

    return console_write_user(buffer, length);
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
        if (process_should_exit_current()) {
            return -1;
        }
        if (process_signal_pending_current()) {
            return SRV_ERR_INTR;
        }
        if (console_termios.cc[SRV_TTY_VMIN] == 0 && !keyboard_scan_char(&c)) {
            return 0;
        }
        if (c == 0) {
            c = keyboard_read_char();
        }
        if (process_signal_pending_current()) {
            return SRV_ERR_INTR;
        }
        if (process_should_exit_current()) {
            return -1;
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
        if (process_should_exit_current()) {
            return -1;
        }
        char c = tty_filter_input(keyboard_read_char());
        if (process_signal_pending_current()) {
            return count == 0 ? SRV_ERR_INTR : (int64_t)count;
        }
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

/* Only explicit nonblocking pipe/PTY descriptors get the try-read path. No
 * redirected stdio guessing, network operations or regular-file reads. */
static bool io_try_pipe_read(uint64_t fd) {
    struct process_file *f=process_file_at(process_current(),fd);
    return f && (f->type==PROCESS_FILE_PIPE_READ || f->type==PROCESS_FILE_PIPE_DUPLEX ||
        f->type==PROCESS_FILE_PTY_MASTER || f->type==PROCESS_FILE_PTY_SLAVE) &&
        (f->flags & SRV_FD_NONBLOCK);
}
static bool io_defer_pipe_read(uint64_t fd) {
    if(io_try_pipe_read(fd)) {
        if(scheduler_io_busy()) return true;
        /* A read can discard transferred file rights and close/flush them.
         * Defer that work during I/O; retain ordinary eligibility otherwise. */
        scheduler_io_set_user_safe(false);
    }
    return false;
}
static int64_t syscall_read(uint64_t fd, char *buffer, uint64_t length) {
    if(io_defer_pipe_read(fd)) return length?SRV_ERR_AGAIN:0;
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
    if (file->type == PROCESS_FILE_PIPE_READ || file->type == PROCESS_FILE_PIPE_DUPLEX) {
        return process_file_pipe_read(process_current(), fd, (uint8_t *)buffer, length);
    }
    if (file->type != PROCESS_FILE_VFS && file->type != PROCESS_FILE_VFS_WRITE &&
        file->type != PROCESS_FILE_PTY_MASTER && file->type != PROCESS_FILE_PTY_SLAVE) {
        return -1;
    }

    return process_file_read(process_current(), fd, (uint8_t *)buffer, length);
}

static bool syscall_tty_fd_ok(uint64_t fd) {
    struct process *process=process_current();
    return process_tty_model(process,fd) || process_fd_is_console(process,fd,true) ||
        process_fd_is_console(process,fd,false);
}

static int64_t syscall_tty_getattr(uint64_t fd, struct srv_termios *termios) {
    if (!syscall_tty_fd_ok(fd) || !user_buffer_ok(termios, sizeof(*termios), true)) {
        return -1;
    }
    struct pty_model *terminal=process_tty_model(process_current(),fd);
    return copy_to_user(termios, terminal?&terminal->attributes:&console_termios, sizeof(console_termios)) ? 0 : -1;
}

static int64_t syscall_tty_setattr(uint64_t fd, uint64_t actions, const struct srv_termios *termios) {
    struct srv_termios copy;
    if (!syscall_tty_fd_ok(fd) || actions > SRV_TCSAFLUSH ||
        !copy_from_user(&copy, termios, sizeof(copy))) {
        return -1;
    }
    uint64_t terminal=process_tty_handle(process_current(),fd);
    if(terminal) return pty_setattr(terminal,&copy,(unsigned)actions);
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

    struct pty_model *terminal=process_tty_model(process_current(),fd);
    if(request==SRV_TIOCGPGRP) {
        int32_t group=(int32_t)(terminal?terminal->foreground_group:process_foreground_group());
        return copy_to_user(argument,&group,sizeof(group))?0:-1;
    }
    if(request==SRV_TIOCSPGRP) {
        int32_t group;
        if(!copy_from_user(&group,argument,sizeof(group)) || group<=0) return -1;
        if(terminal) return process_tty_set_group(process_current(),fd,(uint64_t)group)?0:-1;
        process_set_foreground_group((uint64_t)group);return 0;
    }
    if (request == SRV_TIOCGWINSZ) {
        struct srv_winsize winsize = terminal?terminal->size:tty_current_winsize();
        return copy_to_user(argument, &winsize, sizeof(winsize)) ? 0 : -1;
    }
    if (request == SRV_TIOCSWINSZ) {
        struct srv_winsize winsize;
        if (!copy_from_user(&winsize, argument, sizeof(winsize)) ||
            winsize.row == 0 ||
            winsize.col == 0) {
            return -1;
        }
        if(terminal) {
            bool changed=terminal->size.row!=winsize.row || terminal->size.col!=winsize.col ||
                terminal->size.xpixel!=winsize.xpixel || terminal->size.ypixel!=winsize.ypixel;
            terminal->size=winsize;
            if(changed && terminal->foreground_group)
                process_tty_signal(process_tty_handle(process_current(),fd),SRV_SIGNAL_WINCH);
            return 0;
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
    if (!copy_user_string(user_path,path,sizeof(path))) return SRV_ERR_FAULT;
    if(!(flags&(SRV_OPEN_READ|SRV_OPEN_WRITE)) ||
        (flags&~(SRV_OPEN_READ|SRV_OPEN_WRITE|SRV_OPEN_CREATE|SRV_OPEN_TRUNC|SRV_OPEN_APPEND|SRV_OPEN_NONBLOCK|SRV_OPEN_EXCL|SRV_OPEN_CLOEXEC))) return SRV_ERR_INVAL;
    /* Hold filesystem admission across lookup, exclusive create and descriptor
     * publication. O_EXCL must not be a userspace stat/create race. */
    if(!scheduler_io_begin()) return SRV_ERR_IO;
    int64_t result;
    if(flags&(SRV_OPEN_WRITE|SRV_OPEN_CREATE)) result=process_file_open_write(process_current(),path,flags);
    else {
        const struct vfs_node *node=vfs_lookup(path);const uint8_t *data;uint64_t size;
        if(!node) result=SRV_ERR_NOENT;
        else if(node->type==VFS_NODE_DIRECTORY) result=SRV_ERR_ISDIR;
        else if(!vfs_read_all(node,&data,&size)) result=SRV_ERR_IO;
        else {
            result=process_file_alloc(process_current(),node,data,size);
            if(result<0) result=SRV_ERR_MFILE;
            else {
                (void)process_file_set_flags(process_current(),result,((flags&SRV_OPEN_NONBLOCK)?SRV_FD_NONBLOCK:0)|((flags&SRV_OPEN_APPEND)?SRV_FD_APPEND:0));
                (void)process_file_set_fd_flags(process_current(),result,(flags&SRV_OPEN_CLOEXEC)?SRV_FD_CLOEXEC:0);
            }
        }
    }
    scheduler_io_end();return result;
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

static int64_t copy_exec_string(const char *source,char *destination,uint64_t capacity,int64_t too_long) {
    if(!source) return SRV_ERR_FAULT;
    for(uint64_t i=0;i<capacity;i++) {
        if(!copy_from_user(destination+i,source+i,1)) return SRV_ERR_FAULT;
        if(!destination[i]) return 0;
    }
    return too_long;
}
static int64_t copy_user_string_vector(char *const *user_vector,
    char storage[][MAX_EXEC_STRING], const char *values[],uint64_t capacity,uint64_t *count_out) {
    *count_out=0;
    if(!user_vector) return 0;
    for(uint64_t i=0;i<=capacity;i++) {
        const char *source=NULL;
        if(!copy_from_user(&source,user_vector+i,sizeof(source))) return SRV_ERR_FAULT;
        if(!source) { *count_out=i;return 0; }
        if(i==capacity) return SRV_ERR_2BIG;
        int64_t error=copy_exec_string(source,storage[i],MAX_EXEC_STRING,SRV_ERR_2BIG);
        if(error) return error;
        values[i]=storage[i];
    }
    return SRV_ERR_2BIG;
}

static int64_t syscall_exec(const struct syscall_exec_request *user_request) {
    struct syscall_exec_request request;
    char path[MAX_PATH_LENGTH];
    char argv_storage[MAX_EXEC_ARGS][MAX_EXEC_STRING];
    char env_storage[MAX_EXEC_ENV][MAX_EXEC_STRING];
    const char *argv[MAX_EXEC_ARGS];
    const char *envp[MAX_EXEC_ENV];
    struct process_spawn_file_action file_actions[SRV_SPAWN_FILE_ACTION_MAX];
    uint64_t argc = 0;
    uint64_t envc = 0;
    uint64_t flags;
    int64_t result;

    if(!copy_from_user(&request,user_request,sizeof(request))) return SRV_ERR_FAULT;
    int64_t error=copy_exec_string(request.path,path,sizeof(path),SRV_ERR_NAMETOOLONG);
    if(!error) error=copy_user_string_vector(request.argv,argv_storage,argv,MAX_EXEC_ARGS,&argc);
    if(!error) error=copy_user_string_vector(request.envp,env_storage,envp,MAX_EXEC_ENV,&envc);
    if(error) return error;
    if(request.file_action_count>SRV_SPAWN_FILE_ACTION_MAX) return SRV_ERR_INVAL;
    if(request.file_action_count && !copy_from_user(file_actions,request.file_actions,
        request.file_action_count*sizeof(file_actions[0]))) return SRV_ERR_FAULT;
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
            request.stderr_fd,
            request.flags,
            request.signal_mask,
            request.signal_default);
    } else {
        result = process_spawn_exec(path,
            argc,
            argv,
            envc,
            envp,
            (request.flags & SRV_EXEC_BACKGROUND) != 0,
            request.stdin_fd,
            request.stdout_fd,
            request.stderr_fd,
            request.process_group,
            request.foreground != 0,
            request.file_action_count != 0 ? file_actions : NULL,
            request.file_action_count,
            request.flags,
            request.signal_mask,
            request.signal_default);
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
        length > MAX_SYSCALL_COPY) {
        return -1;
    }

    /* Native whole-file publication must not invalidate a live POSIX content
     * object. Callers can close their handles or use descriptor writes. */
    if(process_has_open_vfs_prefix(path)) return SRV_ERR_AGAIN;
    /* A zero-byte replacement is still one directory publication. Do not open
     * O_TRUNC first: the old contents survive until exFAT publishes this data. */
    if (length == 0) return exfat_write_file(path, NULL, 0) ? 0 : -1;
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
    if(process_has_open_vfs_prefix(path)) return SRV_ERR_AGAIN;
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
    struct syscall_stat copy = {0};
    struct vfs_metadata metadata;
    if (info == NULL || !copy_user_string(user_path, path, sizeof(path))) {
        return -1;
    }

    if (!vfs_stat(path, &metadata, &copy.size, &copy.type)) {
        return -1;
    }

    (void)process_open_file_size(path,&copy.size);
    copy.abi_version = SRV_ABI_VERSION;
    copy.struct_size = sizeof(copy);
    copy.inode = metadata.inode;
    copy.mode = metadata.mode;
    copy.nlink = metadata.nlink;
    copy.uid = metadata.uid;
    copy.gid = metadata.gid;
    copy.atime = metadata.atime;
    copy.mtime = metadata.mtime;
    copy.ctime = metadata.ctime;
    copy.blksize = 4096;
    copy.blocks = (copy.size + 511) / 512;
    return copy_abi_struct_to_user(info, &copy, sizeof(copy), SRV_ABI_VERSION) ? 0 : -1;
}

static int64_t syscall_statfs(const char *user_path, struct syscall_fsinfo *info) {
    char path[MAX_PATH_LENGTH];
    struct exfat_fsinfo fsinfo;
    struct syscall_fsinfo copy = {0};
    if (info == NULL || !copy_user_string(user_path, path, sizeof(path))) {
        return -1;
    }

    if (!exfat_statfs(path, &fsinfo)) {
        return -1;
    }

    copy.abi_version = SRV_ABI_VERSION;
    copy.struct_size = sizeof(copy);
    copy.block_size = fsinfo.block_size;
    copy.blocks = fsinfo.blocks;
    copy.blocks_free = fsinfo.blocks_free;
    copy.blocks_available = fsinfo.blocks_available;
    copy.files = fsinfo.files;
    copy.files_free = fsinfo.files_free;
    for (uint64_t i = 0; i < sizeof(copy.filesystem); i++) {
        copy.filesystem[i] = fsinfo.filesystem[i];
    }
    for (uint64_t i = 0; i < sizeof(copy.mountpoint); i++) {
        copy.mountpoint[i] = fsinfo.mountpoint[i];
    }
    return copy_abi_struct_to_user(info, &copy, sizeof(copy), SRV_ABI_VERSION) ? 0 : -1;
}

static int64_t syscall_chmod(const char *user_path, uint64_t mode) {
    char path[MAX_PATH_LENGTH];
    if (!copy_user_string(user_path, path, sizeof(path))) {
        return -1;
    }
    return vfs_chmod(path, mode) ? 0 : -1;
}

static int64_t syscall_utime(const char *user_path, uint64_t atime, uint64_t mtime) {
    char path[MAX_PATH_LENGTH];
    if (!copy_user_string(user_path, path, sizeof(path))) {
        return -1;
    }
    return vfs_utime(path, atime, mtime) ? 0 : -1;
}

static int64_t syscall_unlink(const char *user_path) {
    char path[MAX_PATH_LENGTH];
    if(!copy_user_string(user_path,path,sizeof(path))) return SRV_ERR_FAULT;
    return process_unlink_file(path);
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

/* The syscall gate serializes filesystem continuations on this single-core
 * kernel. No yielding call separates the VFS guard from entering the existing
 * exFAT mutation scope; owned I/O keeps other namespace mutations gated. */
static int64_t syscall_file_change(const struct srv_file_change *user_request) {
    struct srv_file_change q;char path[MAX_PATH_LENGTH],destination[MAX_PATH_LENGTH];
    if(abi_user_struct_size(user_request,sizeof(q),SRV_ABI_VERSION)!=sizeof(q) ||
        !copy_from_user(&q,user_request,sizeof(q)) || q.operation<SRV_FILE_RENAME || q.operation>SRV_FILE_DUPLICATE ||
        !q.expected_inode || q.expected_type>VFS_NODE_DIRECTORY || !copy_user_string(q.path,path,sizeof(path)) || path[0]!='/') return SRV_FILE_INVALID;
    if(q.operation!=SRV_FILE_DELETE && (!copy_user_string(q.destination,destination,sizeof(destination)) || destination[0]!='/')) return SRV_FILE_INVALID;
    if(q.operation==SRV_FILE_COPY && (q.expected_type!=VFS_NODE_FILE || q.length!=q.expected_size || q.length>MAX_SYSCALL_COPY)) return SRV_FILE_LIMIT;
    if(q.operation==SRV_FILE_DUPLICATE) {
        if(q.data || q.length) return SRV_FILE_INVALID;
        if(q.expected_type!=VFS_NODE_FILE || q.expected_size>SRV_FILE_DUPLICATE_MAX) return SRV_FILE_LIMIT;
    }
    const struct vfs_node *node=vfs_lookup(path);
    if(!node || node->metadata.inode!=q.expected_inode || node->size!=q.expected_size ||
        node->metadata.mtime!=q.expected_mtime || (uint64_t)node->type!=q.expected_type) return SRV_FILE_CHANGED;
    if(q.operation!=SRV_FILE_DELETE && vfs_lookup(destination)) return SRV_FILE_EXISTS;
    if(process_has_open_vfs_prefix(path) || (q.operation!=SRV_FILE_DELETE && process_has_open_vfs_prefix(destination))) return SRV_FILE_BUSY;
    if(q.operation==SRV_FILE_RENAME) return exfat_rename(path,destination)?0:-1;
    if(q.operation==SRV_FILE_DELETE) return (q.expected_type==VFS_NODE_DIRECTORY?exfat_delete_directory(path):exfat_delete_file(path))?0:-1;
    if(q.operation==SRV_FILE_DUPLICATE) {
        const uint8_t *snapshot=NULL;uint64_t length=0;
        bool read=vfs_read_all(node,&snapshot,&length);
        if(!read || length!=q.expected_size || (length && !snapshot)) {
            vfs_release_data(node,snapshot);return -1;
        }
        /* The source owns this immutable read buffer until release. The syscall
         * gate spans both operations, including owned I/O continuations, so no
         * other filesystem syscall can replace either guarded path in between. */
        bool ok=exfat_write_file(destination,snapshot,length);
        vfs_release_data(node,snapshot);return ok?0:-1;
    }
    uint8_t *data=q.length?kmalloc(q.length):NULL;
    if(q.length && (!data || !copy_from_user(data,q.data,q.length))) { kfree(data);return -1; }
    bool ok=exfat_write_file(destination,data,q.length);kfree(data);return ok?0:-1;
}

static int64_t syscall_seek(uint64_t fd, int64_t offset, uint64_t whence) {
    return process_file_seek(process_current(), fd, offset, whence);
}

static int64_t syscall_fstat(uint64_t fd, struct syscall_stat *info) {
    struct syscall_stat copy = {0};
    struct vfs_metadata metadata;
    if (info == NULL) {
        return -1;
    }
    if (process_file_stat(process_current(), fd, &metadata, &copy.size, &copy.type) < 0) {
        return -1;
    }
    copy.abi_version = SRV_ABI_VERSION;
    copy.struct_size = sizeof(copy);
    copy.inode = metadata.inode;
    copy.mode = metadata.mode;
    copy.nlink = metadata.nlink;
    copy.uid = metadata.uid;
    copy.gid = metadata.gid;
    copy.atime = metadata.atime;
    copy.mtime = metadata.mtime;
    copy.ctime = metadata.ctime;
    copy.blksize = 4096;
    copy.blocks = (copy.size + 511) / 512;
    return copy_abi_struct_to_user(info, &copy, sizeof(copy), SRV_ABI_VERSION) ? 0 : -1;
}

static int64_t syscall_fchmod(uint64_t fd, uint64_t mode) {
    return process_file_chmod(process_current(), fd, mode);
}

static int64_t syscall_futime(uint64_t fd, uint64_t atime, uint64_t mtime) {
    return process_file_utime(process_current(), fd, atime, mtime);
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

static int64_t syscall_mmap(uint64_t address,
    uint64_t length,
    uint64_t protection,
    uint64_t flags,
    int64_t fd,
    int64_t offset) {
    return process_mmap(process_current(), address, length, protection, flags, fd, offset);
}

static int64_t syscall_munmap(uint64_t address, uint64_t length) {
    return process_munmap(process_current(), address, length);
}

static int64_t syscall_mprotect(uint64_t address, uint64_t length, uint64_t protection) {
    return process_mprotect(process_current(), address, length, protection);
}

static int64_t syscall_msync(uint64_t address, uint64_t length, uint64_t flags) {
    return process_msync(process_current(), address, length, flags);
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

static int64_t syscall_pipe_pair(int32_t *fds_out) {
    uint64_t fds[2];
    int32_t copy[2];
    if (fds_out == NULL || !user_buffer_ok(fds_out, sizeof(copy), true)) {
        return -1;
    }
    if (process_file_pipe_pair(process_current(), fds) < 0) {
        return -1;
    }
    copy[0] = (int32_t)fds[0];
    copy[1] = (int32_t)fds[1];
    return copy_to_user(fds_out, copy, sizeof(copy)) ? 0 : -1;
}

static int64_t syscall_pty_create(int32_t *fds_out) {
    uint64_t fds[2];int32_t copy[2];
    if(!user_buffer_ok(fds_out,sizeof(copy),true) || process_file_pty_pair(process_current(),fds)<0) return -1;
    copy[0]=(int32_t)fds[0];copy[1]=(int32_t)fds[1];
    if(copy_to_user(fds_out,copy,sizeof(copy))) return 0;
    process_file_close(process_current(),fds[0]);process_file_close(process_current(),fds[1]);return -1;
}

static int64_t syscall_pipe_send_rights(uint64_t fd, const int32_t *user_fds, uint64_t count) {
    int32_t fds[SRV_RIGHTS_MAX];
    if (count == 0 || count > SRV_RIGHTS_MAX ||
        user_fds == NULL ||
        !user_buffer_ok((void *)user_fds, count * sizeof(int32_t), false)) {
        return -1;
    }
    if (!copy_from_user(fds, user_fds, count * sizeof(int32_t))) {
        return -1;
    }
    return process_file_send_rights(process_current(), fd, fds, count);
}

static int64_t syscall_pipe_recv_rights(uint64_t fd, int32_t *user_fds, uint64_t capacity, uint64_t *user_flags) {
    int32_t fds[SRV_RIGHTS_MAX];
    uint64_t flags = 0;
    if (capacity > SRV_RIGHTS_MAX ||
        (capacity != 0 && (user_fds == NULL || !user_buffer_ok(user_fds, capacity * sizeof(int32_t), true))) ||
        (user_flags != NULL && !user_buffer_ok(user_flags, sizeof(uint64_t), true))) {
        return -1;
    }
    int64_t count = process_file_recv_rights(process_current(), fd, fds, capacity, &flags);
    if (count < 0) {
        return -1;
    }
    if (count > 0 && !copy_to_user(user_fds, fds, (uint64_t)count * sizeof(int32_t))) {
        return -1;
    }
    if (user_flags != NULL && !copy_to_user(user_flags, &flags, sizeof(flags))) {
        return -1;
    }
    return count;
}

static int64_t syscall_unix_bind(const char *user_path) {
    char path[MAX_PATH_LENGTH];
    if (!copy_user_string(user_path, path, sizeof(path))) {
        return -1;
    }
    return process_unix_bind(process_current(), path);
}

static int64_t syscall_unix_listen(uint64_t fd, uint64_t backlog) {
    return process_unix_listen(process_current(), fd, backlog);
}

static int64_t syscall_unix_connect(const char *user_path) {
    char path[MAX_PATH_LENGTH];
    if (!copy_user_string(user_path, path, sizeof(path))) {
        return -1;
    }
    return process_unix_connect(process_current(), path);
}

static int64_t syscall_unix_accept(uint64_t fd) {
    return process_unix_accept(process_current(), fd);
}

static int64_t syscall_unix_unlink(const char *user_path) {
    char path[MAX_PATH_LENGTH];
    if (!copy_user_string(user_path, path, sizeof(path))) {
        return -1;
    }
    return process_unix_unlink(path);
}

static int64_t syscall_meminfo(struct srv_meminfo *info) {
    uint64_t total = pmm_ram_frames(), available = pmm_free_frames();
    struct srv_meminfo copy = {
        .abi_version = SRV_ABI_VERSION,
        .struct_size = sizeof(copy),
        .total_bytes = total * PMM_FRAME_SIZE,
        .free_bytes = available * PMM_FRAME_SIZE,
        .used_bytes = (total >= available ? total - available : 0) * PMM_FRAME_SIZE,
        .page_size = PMM_FRAME_SIZE,
    };
    if (info == NULL || !user_buffer_ok(info, sizeof(copy), true)) {
        return -1;
    }
    return copy_to_user(info, &copy, sizeof(copy)) ? 0 : -1;
}

static int64_t poll_once(struct process *process, struct syscall_pollfd *fds, uint64_t nfds) {
    uint64_t flags=irq_save();
    int64_t ready = 0;
    for (uint64_t i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].fd < 0) {
            continue;
        }
        uint16_t revents;
        if(process_fd_is_console(process,fds[i].fd,true))
            revents=(keyboard_buffered_count() || serial_has_char())?fds[i].events&SRV_POLLIN:0;
        else revents = process_file_poll(process, fds[i].fd, (uint16_t)fds[i].events);
        fds[i].revents = (int16_t)revents;
        if (revents != 0) {
            ready++;
        }
    }
    irq_restore(flags);return ready;
}

static bool poll_console_only(struct process *process,const struct syscall_pollfd *fds,uint64_t nfds) {
    for(uint64_t i=0;i<nfds;i++) if(fds[i].fd>=0 &&
        ((fds[i].events&~SRV_POLLIN) || !process_fd_is_console(process,fds[i].fd,true))) return false;
    return true;
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
        process_signal_pending_current() ||
        /* A sibling may redirect/replace a descriptor while we sleep. Wake
         * into the gate instead of examining a newly non-console handle
         * inside someone else's storage operation. */
        (scheduler_io_busy() && !poll_console_only(wait->process,wait->fds,wait->nfds)) ||
        poll_once(wait->process, wait->fds, wait->nfds) != 0;
}

static uint64_t poll_timeout_ticks(int64_t timeout_ms) {
    if (timeout_ms <= 0) {
        return 0;
    }
    return ((uint64_t)timeout_ms + 9) / 10;
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
    /* Admit only the copied, immutable console request during owned I/O. Never
     * classify a mutable userspace array and then read it again after yielding.
     * Everything else retains the original filesystem/process lifetime gate. */
    uint64_t start = timer_ticks();
    uint64_t timeout_ticks = poll_timeout_ticks(timeout_ms);
    uint64_t deadline_ticks = timeout_ms > 0 ? (timeout_ticks>UINT64_MAX-start?UINT64_MAX:start+timeout_ticks) : 0;
    struct syscall_poll_wait wait = {
        .process = process,
        .fds = fds,
        .nfds = nfds,
    };
#ifndef SRVROS_SYSCALL_POLL_HOST_TEST
    __asm__ volatile ("sti" : : : "memory");
#endif

    for (;;) {
        if (process_should_exit_current()) {
            return -1;
        }
        if (process_signal_pending_current()) {
            return copy_to_user(user_fds, fds, nfds * sizeof(*user_fds)) ? 0 : -1;
        }

        uint64_t flags=irq_save();
        if(!poll_console_only(process,fds,nfds)) {
            scheduler_io_set_user_safe(false);scheduler_io_wait();
        }
        int64_t ready = poll_once(process, fds, nfds);
        irq_restore(flags);
        if (ready != 0 || timeout_ms == 0) {
            return copy_to_user(user_fds, fds, nfds * sizeof(*user_fds)) ? ready : -1;
        }
        if (timeout_ms > 0 && timer_ticks() - start >= timeout_ticks) {
            return copy_to_user(user_fds, fds, nfds * sizeof(*user_fds)) ? 0 : -1;
        }
        uint64_t wait_deadline = deadline_ticks;
        if (timeout_ms < 0) {
            wait_deadline = timer_ticks() + 10;
        }
        if (!scheduler_wait_timeout(process_file_poll_wait_queue(), poll_wait_ready, &wait, wait_deadline)) {
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
    if (process_file_flush(process_current(), fd) < 0) return -1;
    return block_flush_all() ? 0 : -1;
}

static int64_t syscall_console_info(struct syscall_console_info *info) {
    struct syscall_console_info copy = {
        .abi_version = SRV_ABI_VERSION,
        .struct_size = sizeof(copy),
    };
    console_get_size(&copy.columns, &copy.rows);
    return copy_abi_struct_to_user(info, &copy, sizeof(copy), SRV_ABI_VERSION) ? 0 : -1;
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
static int64_t syscall_gui_keyboard_read(struct srv_key_event *out) {
    struct srv_key_event event;
    if (abi_user_struct_size(out,sizeof(event),SRV_KEY_EVENT_VERSION)!=sizeof(event) ||
        !user_buffer_ok(out,sizeof(event),true)) return -1;
    int64_t result=gui_keyboard_read(&event);
    if (result==1 && !copy_to_user(out,&event,sizeof(event))) return -1;
    return result;
}

static int64_t syscall_gfx_info(struct syscall_gfx_info *info) {
    struct console_display_info display;
    struct syscall_gfx_info copy = {
        .abi_version = SRV_ABI_VERSION,
        .struct_size = sizeof(copy),
        .scale_num = 1,
        .scale_den = 1,
    };

    if (!console_display_info(&display)) {
        return -1;
    }
    copy.width = display.width;
    copy.height = display.height;
    copy.pitch = display.pitch;
    copy.bpp = display.bpp;
    copy.memory_model = display.memory_model;
    copy.red_mask_size = display.red_mask_size;
    copy.red_mask_shift = display.red_mask_shift;
    copy.green_mask_size = display.green_mask_size;
    copy.green_mask_shift = display.green_mask_shift;
    copy.blue_mask_size = display.blue_mask_size;
    copy.blue_mask_shift = display.blue_mask_shift;
    copy.flags = intel_gfx_flags();
    copy.accel_backend = intel_gfx_backend();
    return copy_abi_struct_to_user(info, &copy, sizeof(copy), SRV_ABI_VERSION) ? 0 : -1;
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

static int64_t syscall_gfx_blit_copy(struct srv_gfx_blit_rect request, uint64_t scale) {
    uint64_t x = request.x;
    uint64_t y = request.y;
    uint64_t width = request.width;
    uint64_t height = request.height;
    uint64_t stride = request.stride;
    const uint32_t *pixels = request.pixels;

    if ((scale != 1 && scale != 2) || width == 0 || height == 0 || stride < width ||
        x > UINT64_MAX - 4096 * scale || y > UINT64_MAX - 4096 * scale ||
        width > 4096 || height > 4096 ||
        height - 1 > UINT64_MAX / stride ||
        (height - 1) * stride > UINT64_MAX - width ||
        ((height - 1) * stride + width) > UINT64_MAX / sizeof(uint32_t)) {
        return -1;
    }
    if (!user_buffer_ok(pixels, ((height - 1) * stride + width) * sizeof(uint32_t), false)) {
        return -1;
    }

    uint64_t row_bytes = width * sizeof(uint32_t);
    uint64_t chunk_rows = (64 * 1024) / row_bytes;
    if (chunk_rows == 0) {
        chunk_rows = 1;
    }
    if (chunk_rows > height) {
        chunk_rows = height;
    }
    uint32_t *copy = kmalloc((size_t)(chunk_rows * row_bytes));
    if (copy == NULL) {
        return -1;
    }

    for (uint64_t row = 0; row < height; row += chunk_rows) {
        uint64_t rows = height - row;
        if (rows > chunk_rows) {
            rows = chunk_rows;
        }
        for (uint64_t copy_row = 0; copy_row < rows; copy_row++) {
            const uint32_t *source = pixels + (row + copy_row) * stride;
            if (!copy_from_user(copy + copy_row * width, source, row_bytes)) {
                kfree(copy);
                return -1;
            }
        }

        uint64_t previous = vmm_current_address_space();
        uint64_t kernel = vmm_kernel_address_space();
        bool ok;
        if (kernel != 0 && previous != kernel) {
            vmm_switch_address_space(kernel);
        }
        ok = console_blit_rgb_scaled(x, y + row * scale, width, rows, copy, width, scale);
        if (kernel != 0 && previous != kernel) {
            vmm_switch_address_space(previous);
        }
        if (!ok) {
            kfree(copy);
            return -1;
        }
    }

    kfree(copy);
    return 0;
}

static int64_t syscall_gfx_blit_rect(const struct srv_gfx_blit_rect *user_request) {
    struct srv_gfx_blit_rect request;
    if (abi_user_struct_size(user_request, sizeof(request), SRV_ABI_VERSION) != sizeof(request) ||
        !copy_from_user(&request, user_request, sizeof(request))) return -1;
    return syscall_gfx_blit_copy(request, 1);
}

static int64_t syscall_gfx_blit_scaled_rect(const struct srv_gfx_blit_scaled_rect *user_request) {
    struct srv_gfx_blit_scaled_rect request;
    if (abi_user_struct_size(user_request, sizeof(request), SRV_ABI_VERSION) != sizeof(request) ||
        !copy_from_user(&request, user_request, sizeof(request))) return -1;
    return syscall_gfx_blit_copy(request.source, request.scale);
}

static int64_t syscall_gfx_console_mute(uint64_t muted) {
    struct process *process = process_current();
    if (process == NULL) {
        return -1;
    }
    console_set_framebuffer_muted_owner(process_pid(process), muted != 0);
    return 0;
}

static int64_t syscall_gfx_console_muted(void) {
    return console_framebuffer_muted() ? 1 : 0;
}

static int64_t syscall_gui_surface_create(struct srv_gui_surface_create *user_request) {
    struct srv_gui_surface_create request;
    struct process *process = process_current();
    if (process == NULL ||
        abi_user_struct_size(user_request, sizeof(request), SRV_ABI_VERSION) != sizeof(request) ||
        !copy_from_user(&request, user_request, sizeof(request))) {
        return -1;
    }

    int64_t surface_id = gui_surface_create(process_pid(process),
        request.width,
        request.height,
        request.flags);
    if (surface_id < 0) {
        return -1;
    }
    request.abi_version = SRV_ABI_VERSION;
    request.struct_size = sizeof(request);
    request.surface_id = (uint64_t)surface_id;
    return copy_to_user(user_request, &request, sizeof(request)) ? 0 : -1;
}

static int64_t syscall_gui_surface_destroy(uint64_t surface_id) {
    struct process *process = process_current();
    if (process == NULL) {
        return -1;
    }
    return gui_surface_destroy(process_pid(process), surface_id);
}

static int64_t syscall_gui_surface_blit(const struct srv_gui_surface_rect *user_request) {
    struct process *process = process_current();
    struct srv_gui_surface_rect request;
    if (process == NULL ||
        abi_user_struct_size(user_request, sizeof(request), SRV_ABI_VERSION) != sizeof(request) ||
        !copy_from_user(&request, user_request, sizeof(request))) {
        return -1;
    }
    if (request.width == 0 || request.height == 0 || request.stride < request.width ||
        request.width > 4096 || request.height > 4096 ||
        request.height - 1 > UINT64_MAX / request.stride ||
        (request.height - 1) * request.stride > UINT64_MAX - request.width ||
        ((request.height - 1) * request.stride + request.width) > UINT64_MAX / sizeof(uint32_t) ||
        !user_buffer_ok(request.pixels,
            ((request.height - 1) * request.stride + request.width) * sizeof(uint32_t),
            false)) {
        return -1;
    }

    uint64_t row_bytes = request.width * sizeof(uint32_t);
    uint64_t chunk_rows = (64 * 1024) / row_bytes;
    if (chunk_rows == 0) {
        chunk_rows = 1;
    }
    if (chunk_rows > request.height) {
        chunk_rows = request.height;
    }
    uint32_t *copy = kmalloc((size_t)(chunk_rows * row_bytes));
    if (copy == NULL) {
        return -1;
    }
    for (uint64_t row = 0; row < request.height; row += chunk_rows) {
        uint64_t rows = request.height - row;
        if (rows > chunk_rows) {
            rows = chunk_rows;
        }
        for (uint64_t copy_row = 0; copy_row < rows; copy_row++) {
            const uint32_t *source = request.pixels + (row + copy_row) * request.stride;
            if (!copy_from_user(copy + copy_row * request.width, source, row_bytes)) {
                kfree(copy);
                return -1;
            }
        }
        if (gui_surface_blit(process_pid(process),
                request.surface_id,
                request.x,
                request.y + row,
                request.width,
                rows,
                copy,
                request.width) != 0) {
            kfree(copy);
            return -1;
        }
    }
    kfree(copy);
    return 0;
}

static int64_t syscall_gui_surface_copy(const struct srv_gui_surface_rect *user_request) {
    struct process *process = process_current();
    struct srv_gui_surface_rect request;
    if (process == NULL ||
        abi_user_struct_size(user_request, sizeof(request), SRV_ABI_VERSION) != sizeof(request) ||
        !copy_from_user(&request, user_request, sizeof(request))) {
        return -1;
    }
    if (request.width == 0 || request.height == 0 || request.stride < request.width ||
        request.width > 4096 || request.height > 4096 ||
        request.height - 1 > UINT64_MAX / request.stride ||
        (request.height - 1) * request.stride > UINT64_MAX - request.width ||
        ((request.height - 1) * request.stride + request.width) > UINT64_MAX / sizeof(uint32_t) ||
        !user_buffer_ok(request.pixels,
            ((request.height - 1) * request.stride + request.width) * sizeof(uint32_t),
            true)) {
        return -1;
    }

    uint64_t row_bytes = request.width * sizeof(uint32_t);
    uint64_t chunk_rows = (64 * 1024) / row_bytes;
    if (chunk_rows == 0) {
        chunk_rows = 1;
    }
    if (chunk_rows > request.height) {
        chunk_rows = request.height;
    }
    uint32_t *copy = kmalloc((size_t)(chunk_rows * row_bytes));
    if (copy == NULL) {
        return -1;
    }
    for (uint64_t row = 0; row < request.height; row += chunk_rows) {
        uint64_t rows = request.height - row;
        if (rows > chunk_rows) {
            rows = chunk_rows;
        }
        if (gui_surface_copy(process_pid(process),
                request.surface_id,
                request.x,
                request.y + row,
                request.width,
                rows,
                copy,
                request.width) != 0) {
            kfree(copy);
            return -1;
        }
        for (uint64_t copy_row = 0; copy_row < rows; copy_row++) {
            uint32_t *target = request.pixels + (row + copy_row) * request.stride;
            if (!copy_to_user(target, copy + copy_row * request.width, row_bytes)) {
                kfree(copy);
                return -1;
            }
        }
    }
    kfree(copy);
    return 0;
}

static int64_t syscall_mouse_scan(struct syscall_mouse_event *event) {
    struct mouse_event kernel_event;
    struct syscall_mouse_event copy = {
        .abi_version = SRV_ABI_VERSION,
        .struct_size = sizeof(copy),
    };
    if (!abi_user_struct_size(event,sizeof(copy),SRV_ABI_VERSION)) return -1;
    if (!mouse_scan(&kernel_event)) {
        return 0;
    }

    copy.wheel_x = kernel_event.wheel_x;
    copy.wheel_y = kernel_event.wheel_y;
    copy.dx = kernel_event.dx;
    copy.dy = kernel_event.dy;
    copy.buttons = kernel_event.buttons;
    copy.flags = kernel_event.flags;
    copy.reserved = 0;
    return copy_abi_struct_to_user(event, &copy, sizeof(copy), SRV_ABI_VERSION) ? 1 : -1;
}

static int64_t syscall_gui_register(void) {
    return gui_register_server();
}

static int64_t syscall_gui_send(const struct gui_message *message) {
    struct gui_message copy;
    if (abi_user_struct_size(message, sizeof(copy), SRV_ABI_VERSION) != sizeof(copy) ||
        !copy_from_user(&copy, message, sizeof(copy))) {
        return -1;
    }
    return gui_send(&copy);
}

static int64_t syscall_gui_recv(struct gui_message *message) {
    struct gui_message copy;
    int64_t result;
    if (message == NULL) {
        return -1;
    }
    result = gui_recv(&copy);
    if (result > 0) {
        copy.abi_version = SRV_ABI_VERSION;
        copy.struct_size = sizeof(copy);
        if (!copy_abi_struct_to_user(message, &copy, sizeof(copy), SRV_ABI_VERSION)) {
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
    struct syscall_process_info copy = {
        .abi_version = SRV_ABI_VERSION,
        .struct_size = sizeof(copy),
    };
    uint64_t next = process_list_details(index, &pid, &name, &state, &copy.parent_pid, &copy.user_mapped_bytes);
    if (next == 0) {
        return 0;
    }
    copy.pid = pid;
    copy_kernel_string(copy.name, sizeof(copy.name), name);
    copy_kernel_string(copy.state, sizeof(copy.state), state);
    return copy_abi_struct_to_user(info, &copy, sizeof(copy), SRV_ABI_VERSION) ? (int64_t)next : -1;
}

static int64_t syscall_kill(int64_t pid, uint64_t signal) {
    if(!process_signal_is_supported(signal)) return SRV_ERR_INVAL;
    return process_signal_target(pid,signal)?0:SRV_ERR_SRCH;
}

static int64_t syscall_signal_config(uint64_t signal, uint64_t action) {
    if(action==SRV_SIGNAL_QUERY) return process_signal_action_current(signal);
    return process_signal_config_current(signal, action) ? 0 : -1;
}

static int64_t syscall_signal_poll(uint64_t *signal_out) {
    uint64_t signal = process_signal_poll_current();
    if (signal_out != NULL && !copy_to_user(signal_out, &signal, sizeof(signal))) {
        return -1;
    }
    return (int64_t)signal;
}

static int64_t syscall_signal_pending(uint64_t *mask_out) {
    uint64_t mask = process_signal_pending_mask_current();
    if (mask_out != NULL && !copy_to_user(mask_out, &mask, sizeof(mask))) {
        return -1;
    }
    return (int64_t)mask;
}

static int64_t syscall_signal_mask(uint64_t how, uint64_t set, uint64_t *oldset_out) {
    uint64_t oldset = 0;
    if (!process_signal_mask_current(how, set, oldset_out != NULL ? &oldset : NULL)) {
        return -1;
    }
    if (oldset_out != NULL && !copy_to_user(oldset_out, &oldset, sizeof(oldset))) {
        return -1;
    }
    return 0;
}

static int64_t syscall_signal_consume(uint64_t mask, uint64_t *signal_out) {
    uint64_t signal = process_signal_consume_current(mask);
    if (signal_out != NULL && !copy_to_user(signal_out, &signal, sizeof(signal))) {
        return -1;
    }
    return (int64_t)signal;
}

static int64_t syscall_proc_group(uint64_t pid, uint64_t group, uint64_t foreground) {
    if (pid != 0 && !process_set_group(pid, group)) {
        return -1;
    }
    if (foreground != 0) {
        process_set_foreground_group(group);
    }
    return 0;
}

static int64_t syscall_proc_control(uint64_t operation, uint64_t pid, uint64_t value) {
    switch (operation) {
    case SRV_PROC_GET_PGROUP:
        return (int64_t)process_group(pid);
    case SRV_PROC_SET_PGROUP:
        return process_set_group(pid, value) ? 0 : -1;
    case SRV_PROC_GET_SESSION:
        return (int64_t)process_session(pid);
    case SRV_PROC_SET_SESSION: {
        uint64_t session = 0;
        return process_create_session_current(&session) ? (int64_t)session : -1;
    }
    case SRV_PROC_GET_FOREGROUND_PGROUP:
        return (int64_t)process_foreground_group();
    case SRV_PROC_SET_FOREGROUND_PGROUP:
        process_set_foreground_group(value);
        return 0;
    default:
        return -1;
    }
}

static int64_t syscall_wait(uint64_t pid, uint64_t *status_out, uint64_t flags) {
    if(flags & ~(SRV_WAIT_NOHANG|SRV_WAIT_POSIX)) return SRV_ERR_INVAL;
    /* Validate before consuming the child: a bad output pointer must not lose
     * a waitable result. */
    if(status_out && !user_buffer_ok(status_out,sizeof(*status_out),true)) return SRV_ERR_FAULT;
    if((flags & SRV_WAIT_NOHANG) && scheduler_io_busy()) return 0;
    /* Reaping may free mappings/descriptors; never run that continuation under
     * another owner's I/O after the function enables interrupts. */
    scheduler_io_set_user_safe(false);
    uint64_t status = 0;
    if(!(flags & SRV_WAIT_NOHANG)) __asm__ volatile ("sti" : : : "memory");
    int64_t result=(flags & SRV_WAIT_POSIX)?
        process_wait_posix((int64_t)pid,&status,(flags & SRV_WAIT_NOHANG)!=0):
        process_wait(pid,&status,(flags & SRV_WAIT_NOHANG)!=0);
    if (result > 0 && status_out != NULL && !copy_to_user(status_out, &status, sizeof(status))) {
        return -1;
    }
    return result;
}

static int64_t syscall_proc_exit_status(uint64_t pid, uint64_t *status_out) {
    uint64_t status = 0;
    if (pid == 0 || status_out == NULL ||
        !user_buffer_ok(status_out, sizeof(*status_out), true) ||
        !process_exit_status(pid, &status)) {
        return -1;
    }
    return copy_to_user(status_out, &status, sizeof(status)) ? 0 : -1;
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

static int64_t syscall_net_connect(uint32_t remote_ip, uint16_t remote_port, uint64_t flags) {
    struct process *process = process_current();
    if (process == NULL) {
        return -1;
    }

    int64_t handle = net_connect(remote_ip, remote_port, (flags & SRV_FD_NONBLOCK) != 0);
    if (handle < 0) {
        return handle;
    }

    int64_t fd = process_handle_alloc(process, PROCESS_FILE_NET_CONNECTION, (uint64_t)handle);
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

static int64_t syscall_net_list(uint64_t index, struct srv_net_info *info) {
    struct srv_net_info copy;
    int64_t next = net_list(index, &copy);
    if (next <= 0) {
        return next;
    }
    return copy_net_struct_to_user(info, &copy, sizeof(copy)) ? next : -1;
}

static int64_t syscall_net_status_info(struct srv_net_status_info *info) {
    struct srv_net_status_info *copy = kmalloc(sizeof(*copy));
    if (copy == NULL) {
        return -1;
    }
    if (net_status_info(copy) < 0) {
        kfree(copy);
        return -1;
    }
    int64_t result = copy_net_struct_to_user(info, copy, sizeof(*copy)) ? 0 : -1;
    kfree(copy);
    return result;
}

static int64_t syscall_net_arp_list(uint64_t index, struct srv_arp_info *info) {
    struct srv_arp_info copy;
    int64_t next = net_arp_list(index, &copy);
    if (next <= 0) {
        return next;
    }
    return copy_net_struct_to_user(info, &copy, sizeof(copy)) ? next : -1;
}

static int64_t syscall_net_ping(uint32_t remote_ip,
    uint16_t sequence,
    uint64_t timeout_ticks,
    uint64_t *elapsed_ticks_out) {
    uint64_t elapsed_ticks = 0;
    int64_t result = net_ping(remote_ip, sequence, timeout_ticks, &elapsed_ticks);
    if (result < 0) {
        return -1;
    }
    if (elapsed_ticks_out != NULL && !copy_to_user(elapsed_ticks_out, &elapsed_ticks, sizeof(elapsed_ticks))) {
        return -1;
    }
    return 0;
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

static int64_t syscall_net_udp_open(void) {
    struct process *process = process_current();
    if (process == NULL) {
        return -1;
    }

    int64_t handle = net_udp_open();
    if (handle < 0) {
        return -1;
    }

    int64_t fd = process_handle_alloc(process, PROCESS_FILE_NET_UDP, (uint64_t)handle);
    if (fd < 0) {
        net_close((uint64_t)handle);
    }
    return fd;
}

static int64_t syscall_net_udp_bind(uint64_t fd, uint16_t port) {
    struct process *process = process_current();
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL || file->type != PROCESS_FILE_NET_UDP) {
        return -1;
    }
    return net_udp_bind(file->handle, port);
}

static int64_t syscall_net_udp_sendto(uint64_t fd,
    uint32_t remote_ip,
    uint16_t remote_port,
    const uint8_t *buffer,
    uint64_t length) {
    if (!user_buffer_ok(buffer, length, false)) {
        return -1;
    }

    struct process *process = process_current();
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL || file->type != PROCESS_FILE_NET_UDP) {
        return -1;
    }

    return net_udp_sendto(file->handle, remote_ip, remote_port, buffer, length);
}

static int64_t syscall_net_udp_recvfrom(uint64_t fd,
    uint8_t *buffer,
    uint64_t capacity,
    uint32_t *remote_ip_out,
    uint16_t *remote_port_out) {
    if (!user_buffer_ok(buffer, capacity, true) ||
        (remote_ip_out != NULL && !user_buffer_ok(remote_ip_out, sizeof(*remote_ip_out), true)) ||
        (remote_port_out != NULL && !user_buffer_ok(remote_port_out, sizeof(*remote_port_out), true))) {
        return -1;
    }

    struct process *process = process_current();
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL || file->type != PROCESS_FILE_NET_UDP) {
        return -1;
    }

    return net_udp_recvfrom(file->handle,
        buffer,
        capacity,
        remote_ip_out,
        remote_port_out,
        process_file_nonblocking(process, fd));
}

static int64_t syscall_net_name(uint64_t fd, uint32_t *ip_out, uint16_t *port_out, bool peer) {
    if ((ip_out != NULL && !user_buffer_ok(ip_out, sizeof(*ip_out), true)) ||
        (port_out != NULL && !user_buffer_ok(port_out, sizeof(*port_out), true))) {
        return -1;
    }

    struct process *process = process_current();
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL ||
        (file->type != PROCESS_FILE_NET_LISTENER &&
            file->type != PROCESS_FILE_NET_CONNECTION &&
            file->type != PROCESS_FILE_NET_UDP)) {
        return -1;
    }

    uint32_t ip = 0;
    uint16_t port = 0;
    int64_t result = peer
        ? net_peername(file->handle, &ip, &port)
        : net_sockname(file->handle, &ip, &port);
    if (result < 0) {
        return -1;
    }
    if (ip_out != NULL && !copy_to_user(ip_out, &ip, sizeof(ip))) {
        return -1;
    }
    if (port_out != NULL && !copy_to_user(port_out, &port, sizeof(port))) {
        return -1;
    }
    return 0;
}

static int64_t syscall_net_shutdown(uint64_t fd, int how) {
    struct process *process = process_current();
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL || file->type != PROCESS_FILE_NET_CONNECTION) {
        return -1;
    }
    return net_shutdown(file->handle, how);
}

static int64_t syscall_net_error(uint64_t fd) {
    struct process *process = process_current();
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL || file->type != PROCESS_FILE_NET_CONNECTION) {
        return -1;
    }
    return net_error(file->handle);
}

static int64_t syscall_net_peek(uint64_t fd, uint8_t *buffer, uint64_t capacity) {
    if (!user_buffer_ok(buffer, capacity, true)) {
        return -1;
    }

    struct process *process = process_current();
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL || file->type != PROCESS_FILE_NET_CONNECTION) {
        return -1;
    }
    return net_peek(file->handle, (char *)buffer, capacity, process_file_nonblocking(process, fd));
}

static int64_t syscall_getpid(void) {
    struct process *process = process_current();
    return process == NULL ? 0 : (int64_t)process_pid(process);
}

static int64_t syscall_getppid(void) {
    struct process *process = process_current();
    return process == NULL ? 0 : (int64_t)process_parent_pid(process);
}

static int64_t syscall_exepath(char *buffer, uint64_t capacity) {
    if (buffer == NULL || capacity == 0 || !user_buffer_ok(buffer, capacity, true)) {
        return -1;
    }
    const char *path = process_executable_path(process_current());
    uint64_t length = 0;
    while (path[length] != '\0') {
        length++;
    }
    if (length + 1 > capacity) {
        return -1;
    }
    return copy_to_user(buffer, path, length + 1) ? (int64_t)length : -1;
}

struct sleep_wait { uint64_t start,ticks; };
static bool sleep_finished(void *arg) {
    const struct sleep_wait *wait=arg;
    return timer_ticks()-wait->start>=wait->ticks || process_should_exit_current() || process_signal_pending_current();
}

static int64_t syscall_sleep_ticks(uint64_t ticks) {
    uint64_t start = timer_ticks();
    if(ticks>UINT64_MAX-start) return -1;
    struct sleep_wait wait={start,ticks};
#ifndef SRVROS_SYSCALL_SLEEP_HOST_TEST
    __asm__ volatile ("sti" : : : "memory");
#endif
    while (timer_ticks() - start < ticks) {
        if (process_should_exit_current()) {
            return -1;
        }
        if (process_signal_pending_current()) {
            return SRV_ERR_INTR;
        }
        /* Signals and cancellation already wake file-poll waiters. Reuse that
         * queue so a long sleep can be interrupted immediately; unrelated I/O
         * wakeups simply recheck the deadline without completing the sleep. */
        if(!scheduler_wait_timeout(process_file_poll_wait_queue(),sleep_finished,&wait,start+ticks))
            scheduler_yield();
    }
    return 0;
}

/* Bounded to one timer tick: signal/exit delivery cannot be stranded on the
 * private pointer queue. Readiness is rechecked atomically by the scheduler. */
static bool pointer_wait_ready(void *arg) {
    return mouse_pending() || sleep_finished(arg);
}
static int64_t syscall_pointer_wait(void) {
    uint64_t start=timer_ticks();
    struct sleep_wait wait={start,1};
#ifndef SRVROS_SYSCALL_SLEEP_HOST_TEST
    __asm__ volatile ("sti" : : : "memory");
#endif
    for(;;) {
        if(process_should_exit_current()) return -1;
        if(process_signal_pending_current()) return SRV_ERR_INTR;
        if(mouse_pending()) return 1;
        if(timer_ticks()-start>=1 || start==UINT64_MAX) return 0;
        if(!scheduler_wait_timeout(mouse_wait_queue(),pointer_wait_ready,&wait,start+1)) scheduler_yield();
    }
}

struct gui_wait { struct sleep_wait sleep; uint64_t server; bool compositor; };
static int64_t gui_wait_pending(const struct gui_wait *wait) {
    return wait->compositor?gui_server_ready():gui_client_ready(wait->server);
}
static bool gui_wait_ready(void *arg) {
    struct gui_wait *wait=arg;
    return gui_wait_pending(wait)!=0 || sleep_finished(&wait->sleep);
}
static int64_t syscall_gui_wait_common(uint64_t server,uint64_t ticks,bool compositor) {
    uint64_t start=timer_ticks();
    if((!compositor && !server) || ticks>(compositor?10u:100u) || ticks>UINT64_MAX-start) return -1;
    struct gui_wait wait={{start,ticks},server,compositor};
#ifndef SRVROS_SYSCALL_SLEEP_HOST_TEST
    __asm__ volatile ("sti" : : : "memory");
#endif
    for(;;) {
        if(process_should_exit_current()) return -1;
        if(process_signal_pending_current()) return SRV_ERR_INTR;
        int64_t ready=gui_wait_pending(&wait);
        if(ready) return ready;
        if(timer_ticks()-start>=ticks) return 0;
        /* Same queue as signals/exit and file readiness. GUI sends and server
         * cleanup wake it too. The scheduler rechecks with IRQs masked, closing
         * the race between the empty read and blocking. Unrelated wakes retry. */
        if(!scheduler_wait_timeout(process_file_poll_wait_queue(),gui_wait_ready,&wait,start+ticks))
            scheduler_yield();
    }
}

static int64_t syscall_gui_wait(uint64_t server,uint64_t ticks) {
    return syscall_gui_wait_common(server,ticks,false);
}
static int64_t syscall_gui_server_wait(uint64_t ticks) {
    return syscall_gui_wait_common(0,ticks,true);
}

static int64_t syscall_thread_create(uint64_t entry, uint64_t arg, uint64_t stack_top, uint64_t flags) {
    if (entry == 0 ||
        stack_top < 16 ||
        !user_buffer_ok((const void *)entry, 1, false) ||
        !user_buffer_ok((void *)(stack_top - 8), 8, true)) {
        return -1;
    }
    return process_thread_create(entry, arg, stack_top, flags);
}

static int64_t syscall_thread_join(uint64_t tid, uint64_t *value_out) {
    if (value_out != NULL && !user_buffer_ok(value_out, sizeof(*value_out), true)) {
        return -1;
    }

    uint64_t value = 0;
    int64_t result = process_thread_join(tid, &value);
    if (result < 0) {
        return result;
    }
    if (value_out != NULL && !copy_to_user(value_out, &value, sizeof(value))) {
        return -1;
    }
    return 0;
}

static int64_t syscall_futex_wait(uint32_t *address, uint32_t expected, uint64_t timeout_ticks) {
    if (!user_buffer_ok(address, sizeof(*address), false) ||
        (((uint64_t)(uintptr_t)address) & 3u) != 0) {
        return -1;
    }
    return process_futex_wait(address, expected, timeout_ticks);
}

static int64_t syscall_futex_wake(uint32_t *address, uint64_t max_count) {
    if (!user_buffer_ok(address, sizeof(*address), false) ||
        (((uint64_t)(uintptr_t)address) & 3u) != 0) {
        return -1;
    }
    return process_futex_wake(address, max_count);
}

static int64_t syscall_sync(void) {
    int64_t result = process_file_sync_all();
    if (!block_flush_all()) {
        result = -1;
    }
    return result;
}

/* Already-open read-only files retain shared in-memory contents. Their read
 * path only copies bytes and advances the shared cursor under IRQ exclusion;
 * it does not resolve a path, touch a block device, allocate, or close a ref.
 * Keep this narrow: no redirected stdio, writable handles, or unbounded copy. */
static bool io_snapshot_read(uint64_t fd,uint64_t length) {
    if(fd<3 || length>16384)return false;
    struct process_file *file=process_file_at(process_current(),fd);
    return file && file->type==PROCESS_FILE_VFS;
}

/* Only paths with no filesystem or process-lifetime mutations may
 * overlap owned I/O. Default-deny also covers future syscalls. Do not admit
 * poll by inspecting a mutable userspace fd array before the actual copy. Poll
 * enters only its copy/validation prelude here, then gates non-console work. */
bool syscall_io_safe(const struct isr_frame *frame) {
    switch (frame->rax) {
    case SYS_POLL: return true;
    case SYS_READ: return process_fd_is_console(process_current(),frame->rdi,true) || io_try_pipe_read(frame->rdi) || io_snapshot_read(frame->rdi,frame->rdx);
    case SYS_WRITE: return process_fd_is_console(process_current(),frame->rdi,false) || io_try_pipe_write(frame->rdi);
    case SYS_WAIT: return (frame->rdx & SRV_WAIT_NOHANG) && !(frame->rdx & ~(SRV_WAIT_NOHANG|SRV_WAIT_POSIX));
    case SYS_CONSOLE_INFO: case SYS_CONSOLE_CLEAR: case SYS_CONSOLE_GOTOXY:
    /* Bounded queues/mouse state use IRQ exclusion. Surface copies use private
     * chunks and IRQ-protected heap/surface state; no VFS or lifetime cleanup.
     * Process listing and memory counters are read-only snapshots.
     * Mapping/spawn/kill remain gated. */
    case SYS_GUI_SEND: case SYS_GUI_RECV: case SYS_GUI_WAIT: case SYS_GUI_SERVER_WAIT: case SYS_MOUSE_SCAN:
    case SYS_GUI_KEYBOARD_READ: case SYS_GUI_KEYBOARD_MODE: case SYS_GUI_POINTER_CONFIG: case SYS_GUI_BRIGHTNESS: case SYS_AUDIO: case SYS_DEVICE_LIST: case SYS_THERMAL_STATUS: case SYS_BATTERY_STATUS: case SYS_BATTERY_CONTROL: case SYS_AUDIO_STREAM: case SYS_SMC_STATUS:
    case SYS_GUI_SURFACE_BLIT: case SYS_GUI_SURFACE_COPY:
    case SYS_GUI_SURFACE_CREATE: case SYS_GUI_SURFACE_DESTROY:
    case SYS_GFX_BLIT_RECT: case SYS_GFX_BLIT_SCALED_RECT: case SYS_PROC_LIST: case SYS_MEMINFO: case SYS_SLEEP_TICKS:
    case SYS_POINTER_WAIT: case SYS_SCHEDULER_STATUS:
    case SYS_REALTIME_US:
    case SYS_MONOTONIC_US:
    case SYS_KEY_SCAN: case SYS_YIELD: case SYS_GETPID: case SYS_TICKS:
    case SYS_TTY_GETATTR: case SYS_IOCTL:
    /* stdio unlock wakes process-local waiters. It neither allocates futex
     * entries nor releases process/file resources; scheduler eligibility
     * still prevents a woken unsafe continuation from entering owned I/O. */
    case SYS_FUTEX_WAKE:
        return true;
    default: return false;
    }
}

static int64_t syscall_path_context(uint64_t operation,const char *input,char *out,uint64_t capacity) {
    char path[MAX_PATH_LENGTH],result[MAX_PATH_LENGTH];
    if(operation>SRV_PATH_RESOLVE) return SRV_ERR_INVAL;
    if(operation!=SRV_PATH_GETCWD) {
        int64_t error=copy_exec_string(input,path,sizeof(path),SRV_ERR_NAMETOOLONG);
        if(error) return error;
    }
    if(operation!=SRV_PATH_CHDIR && (!out || !capacity ||
        !user_buffer_ok(out,capacity>sizeof(result)?sizeof(result):capacity,true))) return SRV_ERR_FAULT;
    int64_t error=process_path_context(operation,path,result,capacity>sizeof(result)?sizeof(result):capacity);
    if(error || operation==SRV_PATH_CHDIR) return error;
    uint64_t size=0;while(result[size]) size++;size++;
    return copy_to_user(out,result,size)?0:SRV_ERR_FAULT;
}

static int64_t syscall_rlimit(uint64_t resource,const struct srv_rlimit *set,struct srv_rlimit *out,uint64_t size) {
    if(size!=sizeof(struct srv_rlimit) || (!set && !out) ||
       (set && !user_buffer_ok(set,size,false)) || (out && !user_buffer_ok(out,size,true))) return SRV_ERR_FAULT;
    struct srv_rlimit input={0},output;
    if(set) input=*set;
    int64_t result=process_rlimit(resource,set?&input:NULL,&output);
    if(!result && out) *out=output;
    return result;
}

static int64_t syscall_usage(int64_t who,struct srv_usage *out,uint64_t size) {
    if(size!=sizeof(*out) || !out || !user_buffer_ok(out,size,true)) return SRV_ERR_FAULT;
    struct srv_usage snapshot;
    int64_t result=process_usage(who,&snapshot);
    if(result==0) *out=snapshot;
    return result;
}

static int64_t syscall_scheduler_status(struct srv_scheduler_status *out,uint64_t size) {
    if(size!=sizeof(*out) || !user_buffer_ok(out,sizeof(*out),true)) return -1;
    struct scheduler_stats stats;scheduler_snapshot(&stats);
    struct srv_scheduler_status value={.version=SRV_SCHEDULER_VERSION,
        .monotonic_us=timer_monotonic_us(),.sampled_ticks=stats.sampled_ticks,
        .idle_ticks=stats.idle_ticks,.switches=stats.switches,.preemptions=stats.preemptions};
    return copy_to_user(out,&value,sizeof(value))?0:-1;
}

static int64_t syscall_battery_control(uint64_t operation,uint64_t epoch,struct srv_battery_control *out,uint64_t size) {
    if(operation>SRV_BATTERY_STOP || size!=sizeof(*out) || !user_buffer_ok(out,sizeof(*out),true))return -1;
    struct srv_battery_control value;
    int result=battery_control_request(process_group(process_pid(process_current())),operation,epoch,&value);
    return copy_to_user(out,&value,sizeof(value))?result:-1;
}

static int64_t syscall_battery(struct srv_battery_status *out,uint64_t size) {
    if(size!=sizeof(*out) || !user_buffer_ok(out,sizeof(*out),true))return -1;
    struct battery_service_status cached;battery_service_status(&cached);
    struct srv_battery_status value=battery_snapshot(&cached);
    return copy_to_user(out,&value,sizeof(value))?0:-1;
}

static int64_t syscall_smc(struct srv_smc_status *out,uint64_t size) {
    if(size!=sizeof(*out) || !user_buffer_ok(out,sizeof(*out),true)) return -1;
    struct srv_smc_status value;smc_status(&value);
    return copy_to_user(out,&value,sizeof(value))?0:-1;
}

static int64_t syscall_thermal(struct srv_thermal_status *out,uint64_t size) {
    if(size!=sizeof(*out) || !user_buffer_ok(out,sizeof(*out),true)) return -1;
    struct srv_thermal_status value;thermal_status(&value);
    return copy_to_user(out,&value,sizeof(value))?0:-1;
}

static int64_t syscall_device_list(uint64_t kind,uint64_t index,struct srv_device_info *out,uint64_t size) {
    if(size!=sizeof(*out) || !user_buffer_ok(out,sizeof(*out),true)) return -1;
    struct srv_device_info info;int64_t next=device_snapshot(kind,index,&info);
    if(next<0) return -1;
    return copy_to_user(out,&info,sizeof(info))?next:-1;
}

static int64_t syscall_audio_stream(uint64_t op,uint64_t handle,void *data,uint64_t length) {
    if(op<SRV_PCM_OPEN || op>SRV_PCM_CLOSE)return -1;
    if(op==SRV_PCM_WRITE) {
        if(!length || length>SRV_PCM_MAX_WRITE || (length&3) || !user_buffer_ok(data,length,false))return -1;
    } else if(op==SRV_PCM_QUERY) {
        if(length!=sizeof(struct srv_pcm_status) || !user_buffer_ok(data,length,true))return -1;
        struct srv_pcm_status value;
        int64_t result=audio_stream_request(process_pid(process_current()),(unsigned)op,handle,&value,sizeof(value));
        if(result<0)return result;
        return copy_to_user(data,&value,sizeof(value))?0:-1;
    } else if(data || length)return -1;
    return audio_stream_request(process_pid(process_current()),(unsigned)op,handle,data,length);
}
static int64_t syscall_audio(uint64_t operation,uint64_t value,struct srv_audio_status *out,uint64_t size) {
    if(size!=sizeof(*out) || !user_buffer_ok(out,sizeof(*out),true)) return -1;
    int64_t result=operation==SRV_AUDIO_QUERY?(value?-1:0):gui_audio_request(operation,value);
    struct srv_audio_status snapshot;audio_status(&snapshot);
    return copy_to_user(out,&snapshot,sizeof(snapshot))?result:-1;
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
    case SYS_PREAD:
        frame->rax = (uint64_t)(user_buffer_ok((void *)frame->rsi,frame->rdx,true)
            ? process_file_pread(process_current(),frame->rdi,(uint8_t *)frame->rsi,frame->rdx,(int64_t)frame->rcx)
            : SRV_ERR_FAULT);
        return;
    case SYS_PWRITE:
        frame->rax = (uint64_t)(user_buffer_ok((void *)frame->rsi,frame->rdx,false)
            ? process_file_pwrite(process_current(),frame->rdi,(const uint8_t *)frame->rsi,frame->rdx,(int64_t)frame->rcx)
            : SRV_ERR_FAULT);
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
    case SYS_NET_CONNECT:
        frame->rax = (uint64_t)syscall_net_connect((uint32_t)frame->rdi, (uint16_t)frame->rsi, frame->rdx);
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
    case SYS_NET_UDP_OPEN:
        frame->rax = (uint64_t)syscall_net_udp_open();
        return;
    case SYS_NET_UDP_BIND:
        frame->rax = (uint64_t)syscall_net_udp_bind(frame->rdi, (uint16_t)frame->rsi);
        return;
    case SYS_NET_UDP_SENDTO:
        frame->rax = (uint64_t)syscall_net_udp_sendto(frame->rdi,
            (uint32_t)frame->rsi,
            (uint16_t)frame->rdx,
            (const uint8_t *)frame->rcx,
            frame->r8);
        return;
    case SYS_NET_UDP_RECVFROM:
        frame->rax = (uint64_t)syscall_net_udp_recvfrom(frame->rdi,
            (uint8_t *)frame->rsi,
            frame->rdx,
            (uint32_t *)frame->rcx,
            (uint16_t *)frame->r8);
        return;
    case SYS_NET_SOCKNAME:
        frame->rax = (uint64_t)syscall_net_name(frame->rdi, (uint32_t *)frame->rsi, (uint16_t *)frame->rdx, false);
        return;
    case SYS_NET_PEERNAME:
        frame->rax = (uint64_t)syscall_net_name(frame->rdi, (uint32_t *)frame->rsi, (uint16_t *)frame->rdx, true);
        return;
    case SYS_NET_SHUTDOWN:
        frame->rax = (uint64_t)syscall_net_shutdown(frame->rdi, (int)frame->rsi);
        return;
    case SYS_NET_ERROR:
        frame->rax = (uint64_t)syscall_net_error(frame->rdi);
        return;
    case SYS_NET_PEEK:
        frame->rax = (uint64_t)syscall_net_peek(frame->rdi, (uint8_t *)frame->rsi, frame->rdx);
        return;
    case SYS_PIPE_SEND_RIGHTS:
        frame->rax = (uint64_t)syscall_pipe_send_rights(frame->rdi, (const int32_t *)frame->rsi, frame->rdx);
        return;
    case SYS_PIPE_RECV_RIGHTS:
        frame->rax = (uint64_t)syscall_pipe_recv_rights(frame->rdi,
            (int32_t *)frame->rsi,
            frame->rdx,
            (uint64_t *)frame->rcx);
        return;
    case SYS_UNIX_BIND:
        frame->rax = (uint64_t)syscall_unix_bind((const char *)frame->rdi);
        return;
    case SYS_UNIX_LISTEN:
        frame->rax = (uint64_t)syscall_unix_listen(frame->rdi, frame->rsi);
        return;
    case SYS_UNIX_CONNECT:
        frame->rax = (uint64_t)syscall_unix_connect((const char *)frame->rdi);
        return;
    case SYS_UNIX_ACCEPT:
        frame->rax = (uint64_t)syscall_unix_accept(frame->rdi);
        return;
    case SYS_UNIX_UNLINK:
        frame->rax = (uint64_t)syscall_unix_unlink((const char *)frame->rdi);
        return;
    case SYS_NET_LIST:
        frame->rax = (uint64_t)syscall_net_list(frame->rdi, (struct srv_net_info *)frame->rsi);
        return;
    case SYS_NET_STATUS_INFO:
        frame->rax = (uint64_t)syscall_net_status_info((struct srv_net_status_info *)frame->rdi);
        return;
    case SYS_NET_ARP_LIST:
        frame->rax = (uint64_t)syscall_net_arp_list(frame->rdi, (struct srv_arp_info *)frame->rsi);
        return;
    case SYS_NET_PING:
        frame->rax = (uint64_t)syscall_net_ping((uint32_t)frame->rdi,
            (uint16_t)frame->rsi,
            frame->rdx,
            (uint64_t *)frame->rcx);
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
    case SYS_GUI_SERVER_WAIT:
        frame->rax=(uint64_t)syscall_gui_server_wait(frame->rdi);
        return;
    case SYS_GUI_WAIT:
        frame->rax=(uint64_t)syscall_gui_wait(frame->rdi,frame->rsi);
        return;
    case SYS_GUI_REGISTER:
        frame->rax = (uint64_t)syscall_gui_register();
        return;
    case SYS_SCHEDULER_STATUS:
        frame->rax=(uint64_t)syscall_scheduler_status((struct srv_scheduler_status *)frame->rdi,frame->rsi);
        return;
    case SYS_BATTERY_CONTROL:
        frame->rax=(uint64_t)syscall_battery_control(frame->rdi,frame->rsi,(struct srv_battery_control *)frame->rdx,frame->rcx);
        return;
    case SYS_BATTERY_STATUS:
        frame->rax=(uint64_t)syscall_battery((struct srv_battery_status *)frame->rdi,frame->rsi);
        return;
    case SYS_SMC_STATUS:
        frame->rax=(uint64_t)syscall_smc((struct srv_smc_status *)frame->rdi,frame->rsi);
        return;
    case SYS_THERMAL_STATUS:
        frame->rax=(uint64_t)syscall_thermal((struct srv_thermal_status *)frame->rdi,frame->rsi);
        return;
    case SYS_DEVICE_LIST:
        frame->rax=(uint64_t)syscall_device_list(frame->rdi,frame->rsi,(struct srv_device_info *)frame->rdx,frame->rcx);
        return;
    case SYS_AUDIO_STREAM:
        frame->rax=(uint64_t)syscall_audio_stream(frame->rdi,frame->rsi,(void *)frame->rdx,frame->rcx);
        return;
    case SYS_AUDIO:
        frame->rax=(uint64_t)syscall_audio(frame->rdi,frame->rsi,(struct srv_audio_status *)frame->rdx,frame->rcx);
        return;
    case SYS_GUI_BRIGHTNESS:
        frame->rax=(uint64_t)gui_brightness(frame->rdi);
        break;
    case SYS_GUI_POINTER_CONFIG:
        frame->rax=(uint64_t)gui_pointer_config(frame->rdi,frame->rsi);
        break;
    case SYS_GUI_KEYBOARD_MODE:
        frame->rax=(uint64_t)gui_keyboard_mode(frame->rdi);
        return;
    case SYS_GUI_KEYBOARD_READ:
        frame->rax=(uint64_t)syscall_gui_keyboard_read((struct srv_key_event *)frame->rdi);
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
        frame->rax = (uint64_t)syscall_kill((int64_t)frame->rdi, frame->rsi);
        return;
    case SYS_SPAWN_BG_ARGS:
        frame->rax = (uint64_t)syscall_spawn_bg_args((const char *)frame->rdi, (const char *)frame->rsi);
        return;
    case SYS_STAT:
        frame->rax = (uint64_t)syscall_stat((const char *)frame->rdi, (struct syscall_stat *)frame->rsi);
        return;
    case SYS_STATFS:
        frame->rax = (uint64_t)syscall_statfs((const char *)frame->rdi, (struct syscall_fsinfo *)frame->rsi);
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
    case SYS_PROC_EXIT_STATUS:
        frame->rax = (uint64_t)syscall_proc_exit_status(frame->rdi, (uint64_t *)frame->rsi);
        return;
    case SYS_MKDIR:
        frame->rax = (uint64_t)syscall_mkdir((const char *)frame->rdi);
        return;
    case SYS_FILE_CHANGE:
        frame->rax=(uint64_t)syscall_file_change((const struct srv_file_change *)frame->rdi);
        break;
    case SYS_RENAME:
        frame->rax = (uint64_t)syscall_rename((const char *)frame->rdi, (const char *)frame->rsi);
        return;
    case SYS_RMDIR:
        frame->rax = (uint64_t)syscall_rmdir((const char *)frame->rdi);
        return;
    case SYS_GETPID:
        frame->rax = (uint64_t)syscall_getpid();
        return;
    case SYS_GETPPID:
        frame->rax = (uint64_t)syscall_getppid();
        return;
    case SYS_EXEPATH:
        frame->rax = (uint64_t)syscall_exepath((char *)frame->rdi, frame->rsi);
        return;
    case SYS_REALTIME_US:
        frame->rax = (uint64_t)timer_realtime_us();
        return;
    case SYS_MONOTONIC_US:
        frame->rax = timer_reference_ready() ? timer_monotonic_us() : (uint64_t)-1;
        return;
    case SYS_TICKS:
        frame->rax = timer_ticks();
        return;
    case SYS_POINTER_WAIT:
        frame->rax=(uint64_t)syscall_pointer_wait();
        break;
    case SYS_SLEEP_TICKS:
        frame->rax = (uint64_t)syscall_sleep_ticks(frame->rdi);
        return;
    case SYS_FSTAT:
        frame->rax = (uint64_t)syscall_fstat(frame->rdi, (struct syscall_stat *)frame->rsi);
        return;
    case SYS_SBRK:
        frame->rax = (uint64_t)syscall_sbrk((int64_t)frame->rdi, (uint64_t *)frame->rsi);
        return;
    case SYS_MMAP:
        frame->rax = (uint64_t)syscall_mmap(frame->rdi,
            frame->rsi,
            frame->rdx,
            frame->rcx,
            (int64_t)frame->r8,
            (int64_t)frame->r9);
        return;
    case SYS_MUNMAP:
        frame->rax = (uint64_t)syscall_munmap(frame->rdi, frame->rsi);
        return;
    case SYS_MPROTECT:
        frame->rax = (uint64_t)syscall_mprotect(frame->rdi, frame->rsi, frame->rdx);
        return;
    case SYS_MSYNC:
        frame->rax = (uint64_t)syscall_msync(frame->rdi, frame->rsi, frame->rdx);
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
    case SYS_PTY_CREATE:
        frame->rax = (uint64_t)syscall_pty_create((int32_t *)frame->rdi);
        return;
    case SYS_PIPE_PAIR:
        frame->rax = (uint64_t)syscall_pipe_pair((int32_t *)frame->rdi);
        return;
    case SYS_MEMINFO:
        frame->rax = (uint64_t)syscall_meminfo((struct srv_meminfo *)frame->rdi);
        return;
    case SYS_PROC_READY:
        process_mark_ready();
        frame->rax = 0;
        return;
    case SYS_RANDOM:
        frame->rax = (uint64_t)syscall_random((void *)frame->rdi, frame->rsi, frame->rdx);
        return;
    case SYS_UTIME:
        frame->rax = (uint64_t)syscall_utime((const char *)frame->rdi, frame->rsi, frame->rdx);
        return;
    case SYS_FUTIME:
        frame->rax = (uint64_t)syscall_futime(frame->rdi, frame->rsi, frame->rdx);
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
    case SYS_CHMOD:
        frame->rax = (uint64_t)syscall_chmod((const char *)frame->rdi, frame->rsi);
        return;
    case SYS_FCHMOD:
        frame->rax = (uint64_t)syscall_fchmod(frame->rdi, frame->rsi);
        return;
    case SYS_PROC_GROUP:
        frame->rax = (uint64_t)syscall_proc_group(frame->rdi, frame->rsi, frame->rdx);
        return;
    case SYS_PROC_CONTROL:
        frame->rax = (uint64_t)syscall_proc_control(frame->rdi, frame->rsi, frame->rdx);
        return;
    case SYS_THREAD_CREATE:
        frame->rax = (uint64_t)syscall_thread_create(frame->rdi, frame->rsi, frame->rdx, frame->rcx);
        return;
    case SYS_THREAD_EXIT:
        process_thread_exit(frame->rdi);
    case SYS_THREAD_JOIN:
        frame->rax = (uint64_t)syscall_thread_join(frame->rdi, (uint64_t *)frame->rsi);
        return;
    case SYS_THREAD_SELF:
        frame->rax = process_thread_self();
        return;
    case SYS_THREAD_DETACH:
        frame->rax = (uint64_t)process_thread_detach(frame->rdi);
        return;
    case SYS_PATH_CONTEXT:
        frame->rax=(uint64_t)syscall_path_context(frame->rdi,(const char *)frame->rsi,(char *)frame->rdx,frame->rcx);
        return;
    case SYS_FORK:
        __asm__ volatile ("sti" : : : "memory");
        frame->rax=(uint64_t)process_fork(frame);
        return;
    case SYS_RLIMIT:
        frame->rax=(uint64_t)syscall_rlimit(frame->rdi,(const struct srv_rlimit *)frame->rsi,(struct srv_rlimit *)frame->rdx,frame->rcx);
        return;
    case SYS_USAGE:
        frame->rax=(uint64_t)syscall_usage((int64_t)frame->rdi,(struct srv_usage *)frame->rsi,frame->rdx);
        return;
    case SYS_THREAD_SIGNAL:
        frame->rax = (uint64_t)process_thread_signal(frame->rdi,frame->rsi);
        return;
    case SYS_THREAD_STATUS:
        frame->rax = (uint64_t)process_thread_status(frame->rdi);
        return;
    case SYS_FUTEX_WAIT:
        frame->rax = (uint64_t)syscall_futex_wait((uint32_t *)frame->rdi, (uint32_t)frame->rsi, frame->rdx);
        return;
    case SYS_FUTEX_WAKE:
        frame->rax = (uint64_t)syscall_futex_wake((uint32_t *)frame->rdi, frame->rsi);
        return;
    case SYS_SYNC:
        frame->rax = (uint64_t)syscall_sync();
        return;
    case SYS_SIGNAL_CONFIG:
        frame->rax = (uint64_t)syscall_signal_config(frame->rdi, frame->rsi);
        return;
    case SYS_SIGNAL_POLL:
        frame->rax = (uint64_t)syscall_signal_poll((uint64_t *)frame->rdi);
        return;
    case SYS_SIGNAL_PENDING:
        frame->rax = (uint64_t)syscall_signal_pending((uint64_t *)frame->rdi);
        return;
    case SYS_SIGNAL_MASK:
        frame->rax = (uint64_t)syscall_signal_mask(frame->rdi, frame->rsi, (uint64_t *)frame->rdx);
        return;
    case SYS_SIGNAL_CONSUME:
        frame->rax = (uint64_t)syscall_signal_consume(frame->rdi, (uint64_t *)frame->rsi);
        return;
    case SYS_GFX_BLIT_RECT:
        frame->rax = (uint64_t)syscall_gfx_blit_rect((const struct srv_gfx_blit_rect *)frame->rdi);
        return;
    case SYS_GFX_BLIT_SCALED_RECT:
        frame->rax = (uint64_t)syscall_gfx_blit_scaled_rect((const struct srv_gfx_blit_scaled_rect *)frame->rdi);
        return;
    case SYS_GFX_CONSOLE_MUTE:
        frame->rax = (uint64_t)syscall_gfx_console_mute(frame->rdi);
        return;
    case SYS_GFX_CONSOLE_MUTED:
        frame->rax = (uint64_t)syscall_gfx_console_muted();
        return;
    case SYS_GUI_SURFACE_CREATE:
        frame->rax = (uint64_t)syscall_gui_surface_create((struct srv_gui_surface_create *)frame->rdi);
        return;
    case SYS_GUI_SURFACE_DESTROY:
        frame->rax = (uint64_t)syscall_gui_surface_destroy(frame->rdi);
        return;
    case SYS_GUI_SURFACE_BLIT:
        frame->rax = (uint64_t)syscall_gui_surface_blit((const struct srv_gui_surface_rect *)frame->rdi);
        return;
    case SYS_GUI_SURFACE_COPY:
        frame->rax = (uint64_t)syscall_gui_surface_copy((const struct srv_gui_surface_rect *)frame->rdi);
        return;
    default:
        frame->rax = (uint64_t)-1;
        return;
    }
}
