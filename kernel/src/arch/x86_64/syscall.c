#include <srvros/sync_timing.h>
#include <srvros/syscall.h>
#include <srvros/capture_write_timing.h>
#include <srvros/session_scope.h>
#include <srvros/file_stream_kernel.h>
#include <srvros/file_verify_kernel.h>
#include <srvros/exfat_stream.h>
#include <srvros/path_limits.h>
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
#ifdef SRVROS_SIGNAL_PUBLIC_GUEST_TEST
#include "../../process_signal.h"
#endif

#include <srvros/block.h>
#include <srvros/console.h>
#include <srvros/display_present.h>
#include <srvros/exfat.h>
#include <srvros/gui.h>
#include <srvros/heap.h>
#include <srvros/halt.h>
#include <srvros/keyboard.h>
#include <srvros/latency.h>
#include <srvros/intel_gfx.h>
#include <srvros/mouse.h>
#include <srvros/net.h>
#include <srvros/pmm.h>
#include <srvros/virtio_rng.h>
#include <srvros/random.h>
#include <srvros/process.h>
#include <srvros/pty.h>
#include <srvros/scheduler.h>
#include <srvros/service_window.h>
#include <srvros/serial.h>
#include <srvros/syscall_numbers.h>
#include <srvros/timer.h>
#include <srvros/vfs.h>
#include <srvros/vfs_namespace.h>
#include <srvros/vmm.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_PATH_LENGTH SRV_PATH_MAX
#include <srvros/vector_io.h>
#define MAX_SYSCALL_COPY (256 * 1024)
#define MAX_POLL_FDS 64
#include <srvros/exec_limits.h>
#define MAX_EXEC_ARGS SRV_EXEC_ARGS_MAX
#define MAX_EXEC_ENV SRV_EXEC_ENV_MAX

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
#if defined(SRVROS_SYSCALL_COPY_HOST_TEST)
    return syscall_copy_test_irq_save();
#elif defined(SRVROS_SYSCALL_POLL_HOST_TEST) || defined(SRVROS_SYSCALL_IO_HOST_TEST) || defined(SRVROS_SYSCALL_SLEEP_HOST_TEST)
    return 0;
#else
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
#endif
}

static void irq_restore(uint64_t flags) {
#if defined(SRVROS_SYSCALL_COPY_HOST_TEST)
    syscall_copy_test_irq_restore(flags);
#elif defined(SRVROS_SYSCALL_POLL_HOST_TEST) || defined(SRVROS_SYSCALL_IO_HOST_TEST) || defined(SRVROS_SYSCALL_SLEEP_HOST_TEST)
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

/* Keep each validation and its access indivisible with CPU0 mapping changes.
 * Release interrupts between pages unless the caller already masked them. A
 * later invalidation can leave a copied prefix, just as an interrupted user
 * transfer can; callers requiring transactional publication hold their own
 * exclusion around validation and the complete callback. */
static bool copy_user_bytes(void *kernel, void *user, uint64_t length, bool to_user) {
    if (!length) return true;
    uint64_t address = (uint64_t)(uintptr_t)user;
    if (!kernel || !user || length > UINT64_MAX - address) return false;
    uint8_t *bytes = kernel;
    uint64_t copied = 0;
    while (copied < length) {
        uint64_t current = address + copied;
        uint64_t count = 4096 - (current & 4095);
        if (count > length - copied) count = length - copied;
        uint64_t flags = irq_save();
        if (!user_buffer_ok((void *)(uintptr_t)current, count, to_user)) {
            irq_restore(flags);
            return false;
        }
        uint8_t *user_bytes = (uint8_t *)(uintptr_t)current;
        for (uint64_t i = 0; i < count; i++) {
            if (to_user) user_bytes[i] = bytes[copied + i];
            else bytes[copied + i] = user_bytes[i];
        }
        irq_restore(flags);
        copied += count;
    }
    return true;
}

static bool copy_from_user(void *destination, const void *source, uint64_t length) {
    return copy_user_bytes(destination, (void *)source, length, false);
}
#ifdef SRVROS_SIGNAL_PUBLIC_GUEST_TEST
bool syscall_signal_copy_input(void *out,uint64_t address,size_t length) {
    return length<=1024 && (!length || copy_from_user(out,(const void *)address,length));
}
#endif

static bool copy_to_user(void *destination, const void *source, uint64_t length) {
    return copy_user_bytes((void *)source, destination, length, true);
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
    /* Nonblocking: the VM entropy worker replenishes a private pool. Existing
     * hardware/software fallback semantics are retained when it is empty. */
    if (virtio_rng_take_u64(&hardware)) {
        irq_restore(flags);
        return hardware;
    }
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

static bool random_entropy_u64(uint64_t *value) {
    uint64_t flags = irq_save();
    bool available = virtio_rng_take_u64(value) || rdrand64(value);
    irq_restore(flags);
    return available;
}

static int64_t syscall_random(void *buffer, uint64_t length, uint64_t flags) {
    if ((flags != 0 && flags != SRV_RANDOM_REQUIRE_ENTROPY) ||
        length > MAX_SYSCALL_COPY || !user_buffer_ok(buffer, length, true)) {
        return -1;
    }

    uint8_t chunk[64];
    uint64_t copied = 0;
    if (flags == SRV_RANDOM_REQUIRE_ENTROPY) {
        while (copied < length) {
            uint64_t remaining = length - copied;
            uint64_t wanted = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
            uint64_t ready = 0;
            while (ready < wanted) {
                uint64_t value;
                if (!random_entropy_u64(&value)) {
                    break;
                }
                uint64_t count = wanted - ready;
                if (count > sizeof(value)) {
                    count = sizeof(value);
                }
                for (uint64_t i = 0; i < count; i++) {
                    chunk[ready + i] = (uint8_t)(value >> (i * 8));
                }
                ready += count;
            }
            if (ready == 0) {
                return copied ? (int64_t)copied : SRV_RANDOM_UNAVAILABLE;
            }
            if (!copy_to_user((uint8_t *)buffer + copied, chunk, ready)) {
                return -1;
            }
            copied += ready;
            if (ready < wanted) {
                return (int64_t)copied;
            }
        }
        return (int64_t)copied;
    }
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
struct console_fd_guard {struct process *process;uint64_t fd,generation;int reason;};
static int console_read_eligible(void *opaque) {
    struct console_fd_guard *g=opaque;
    if(!g->process || process_current()!=g->process || g->generation==UINT64_MAX ||
       process_fd_generation(g->process,g->fd)!=g->generation || !process_fd_is_console(g->process,g->fd,true))
        return g->reason=SRV_ERR_BADF;
    if(process_should_exit_current())return g->reason=SRV_ERR_INTR;
    if(process_signal_pending_current())return g->reason=SRV_ERR_INTR;
    return 0;
}
static bool console_write_eligible(void *opaque) {
    struct console_fd_guard *g=opaque;
    if(!g->process || process_current()!=g->process || g->generation==UINT64_MAX ||
       process_fd_generation(g->process,g->fd)!=g->generation || !process_fd_is_console(g->process,g->fd,false)) {
        g->reason=SRV_ERR_BADF;return false;
    }
    if(process_should_exit_current()) {g->reason=SRV_ERR_INTR;return false;}
    return true;
}
static int64_t console_write_checked(uint64_t fd,const char *buffer,uint64_t length,bool owned) {
    struct console_fd_guard guard={.process=process_current(),.fd=fd};
    guard.generation=process_fd_generation(guard.process,fd);uint64_t written=0;
    while(written<length) {
        char chunk[1024];uint64_t count=length-written;if(count>sizeof(chunk))count=sizeof(chunk);
        if(owned) {for(uint64_t i=0;i<count;i++)chunk[i]=buffer[written+i];}
        else if(!copy_from_user(chunk,buffer+written,count))return written?(int64_t)written:-1;
        /* Rendering/UART must run with IRQs enabled. The helper performs its
         * identity check atomically with each bounded bootlog publication. */
        irq_restore(UINT64_C(1)<<9);
        size_t accepted=console_write_n_checked(chunk,count,console_write_eligible,&guard);
        (void)irq_save();
        written+=accepted;
        if(accepted<count)return written?(int64_t)written:(guard.reason?guard.reason:SRV_ERR_IO);
    }
    return (int64_t)written;
}

static bool syscall_pipe_write_type(uint64_t type) {
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    return type == PROCESS_FILE_PIPE_WRITE;
#else
    return type == PROCESS_FILE_PIPE_WRITE || type == PROCESS_FILE_PIPE_DUPLEX;
#endif
}
static bool syscall_pipe_read_type(uint64_t type) {
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    return type == PROCESS_FILE_PIPE_READ;
#else
    return type == PROCESS_FILE_PIPE_READ || type == PROCESS_FILE_PIPE_DUPLEX;
#endif
}
static bool io_try_pipe_write(uint64_t fd) {
    struct process_file *f=process_file_at(process_current(),fd);
    return f && (syscall_pipe_write_type(f->type)
#ifndef SRVROS_SIGNAL_EXPERIMENTAL_INFO
        || f->type==PROCESS_FILE_PTY_MASTER || f->type==PROCESS_FILE_PTY_SLAVE
#endif
        ) &&
        process_file_nonblocking(process_current(),fd);
}
static bool io_defer_pipe_write(uint64_t fd,uint64_t length) {
    if(io_try_pipe_write(fd)) {
        if(process_file_pipe_io_safe(process_current(),fd,length,true))return false;
        /* Match the try-read contract: never enter descriptor/signal cleanup
         * while another process owns storage. Request a quiescent retry turn
         * so a continuous writer cannot starve worker request submission. */
        if(scheduler_io_busy()) { scheduler_io_request_turn();return true; }
        scheduler_io_set_user_safe(false);
    }
    return false;
}
struct network_user_io {
    struct process *process;
    uint64_t fd, generation, handle;
    enum process_file_type type;
    const struct srv_iovec *iov;
    uint64_t count;
    uint64_t *remaining;
    uint32_t *ip;
    uint16_t *port;
    struct srv_socket_peer *peer;
};
static bool network_current(void *context) {
    struct network_user_io *io = context;
    struct process_file *file = process_file_at(io->process, io->fd);
    return file && io->generation != UINT64_MAX &&
        process_fd_generation(io->process, io->fd) == io->generation &&
        file->type == io->type && file->handle == io->handle;
}
static bool network_init(struct network_user_io *io, uint64_t fd) {
    io->process = process_current(); io->fd = fd;
    struct process_file *file = process_file_at(io->process, fd);
    if (!file) return false;
    io->generation = process_fd_generation(io->process, fd);
    io->handle = file->handle; io->type = file->type;
    return network_current(io);
}
/* All output ranges are checked before publishing any part of a record.
 * The checked network call retains IRQ exclusion through its queue commit. */
static int64_t network_copyout(void *context, const uint8_t *data, uint64_t length,
    uint64_t remaining, uint32_t ip, uint16_t port) {
    struct network_user_io *io = context;
    if (!network_current(io)) return SRV_ERR_BADF;
    uint64_t capacity = 0;
    for (uint64_t i = 0; i < io->count; i++) {
        if (!user_buffer_ok(io->iov[i].base, io->iov[i].length, true)) return SRV_ERR_FAULT;
        capacity += io->iov[i].length;
    }
    if (length > capacity ||
        (io->remaining && !user_buffer_ok(io->remaining, sizeof(*io->remaining), true)) ||
        (io->ip && !user_buffer_ok(io->ip, sizeof(*io->ip), true)) ||
        (io->port && !user_buffer_ok(io->port, sizeof(*io->port), true)) ||
        (io->peer && !user_buffer_ok(io->peer, sizeof(*io->peer), true))) return SRV_ERR_FAULT;
    uint64_t copied = 0;
    for (uint64_t i = 0; i < io->count && copied < length; i++) {
        uint64_t n = io->iov[i].length;
        if (n > length - copied) n = length - copied;
        if (!copy_to_user(io->iov[i].base, data + copied, n)) return SRV_ERR_FAULT;
        copied += n;
    }
    struct srv_socket_peer peer = {.ip=ip, .port=port};
    if ((io->remaining && !copy_to_user(io->remaining, &remaining, sizeof(remaining))) ||
        (io->ip && !copy_to_user(io->ip, &ip, sizeof(ip))) ||
        (io->port && !copy_to_user(io->port, &port, sizeof(port))) ||
        (io->peer && !copy_to_user(io->peer, &peer, sizeof(peer)))) return SRV_ERR_FAULT;
    return 0;
}
static int64_t network_receive(uint64_t fd, struct network_user_io *io,
    uint64_t length, bool nonblock, bool peek, bool accept) {
    uint64_t flags = irq_save();
    int64_t result = SRV_ERR_BADF;
    if (!network_init(io, fd)) goto done;
    const struct net_transfer transfer = {io, network_current, network_copyout};
    if (accept && io->type == PROCESS_FILE_NET_LISTENER) {
        result = net_accept_checked(io->handle, length, nonblock, &transfer);
        if (result >= 0) {
            uint64_t handle = (uint64_t)result;
            result = process_handle_alloc(io->process, PROCESS_FILE_NET_CONNECTION, handle);
            if (result < 0) net_close(handle);
        }
    } else if (!accept && io->type == PROCESS_FILE_NET_UDP)
        result = net_udp_recv_checked(io->handle, length, nonblock, &transfer);
    else if (!accept && io->type == PROCESS_FILE_NET_CONNECTION)
        result = net_read_checked(io->handle, length, nonblock, peek, &transfer);
 done:
    irq_restore(flags);
    return result;
}
static int64_t network_send(uint64_t fd, const void *buffer, uint64_t length,
    bool nonblock, bool owned, bool udp, uint32_t ip, uint16_t port) {
    uint64_t flags = irq_save();
    struct network_user_io io = {0};
    int64_t result = SRV_ERR_BADF;
    uint8_t small[1536], *snapshot = small;
    if (!network_init(&io, fd) || io.type != (udp ? PROCESS_FILE_NET_UDP : PROCESS_FILE_NET_CONNECTION)) goto done;
    if (udp && length > SRV_NET_UDP_PAYLOAD_MAX) { result = SRV_ERR_INVAL; goto done; }
    if (!udp && length > MAX_SYSCALL_COPY) length = MAX_SYSCALL_COPY;
    if (!owned) {
        if (length > sizeof(small)) snapshot = kmalloc(length);
        if (!snapshot) { result = SRV_ERR_NOMEM; goto done; }
        if (!copy_from_user(snapshot, buffer, length)) { result = SRV_ERR_FAULT; goto done; }
        buffer = snapshot;
    }
    const struct net_transfer transfer = {.context=&io, .current=network_current};
    result = udp ? net_udp_send_checked(io.handle, ip, port, buffer, length, nonblock, &transfer) :
        net_respond_checked(io.handle, buffer, length, nonblock, &transfer);
 done:
    if (snapshot != small) kfree(snapshot);
    irq_restore(flags);
    return result;
}

static int64_t write_buffer(uint64_t fd, const char *buffer, uint64_t length, bool owned) {
    struct process_file *file=process_file_at(process_current(),fd);
    if(!file) return SRV_ERR_BADF;
    if(file->type==PROCESS_FILE_STDIO) {
        if(file->handle!=1 && file->handle!=2) return SRV_ERR_BADF;
        return console_write_checked(fd,buffer,length,owned);
    }
#ifndef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    if(file->type==PROCESS_FILE_VFS_WRITE || file->type==PROCESS_FILE_PTY_MASTER || file->type==PROCESS_FILE_PTY_SLAVE)
        return process_file_write(process_current(),fd,(const uint8_t *)buffer,length);
#endif
    if(syscall_pipe_write_type(file->type))
        return process_file_pipe_write(process_current(),fd,(const uint8_t *)buffer,length);
    if(file->type==PROCESS_FILE_NET_CONNECTION) {
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
        return SRV_ERR_NOTSUP;
#else
        return network_send(fd,buffer,length,process_file_nonblocking(process_current(),fd),owned,false,0,0);
#endif
    }
    return SRV_ERR_BADF;
}

static bool pipe_user_copyin(void *context, uint64_t offset, uint8_t *first,
    uint64_t first_length, uint8_t *second, uint64_t second_length) {
    const uint8_t *source = (const uint8_t *)context + offset;
    if (!user_buffer_ok(source, first_length + second_length, false)) return false;
    for (uint64_t i = 0; i < first_length; i++) first[i] = source[i];
    for (uint64_t i = 0; i < second_length; i++) second[i] = source[first_length + i];
    return true;
}

#ifndef SRVROS_SIGNAL_EXPERIMENTAL_INFO
static bool pty_kernel_copyin(void *context,uint64_t offset,uint8_t *first,uint64_t first_length,uint8_t *second,uint64_t second_length) {
    const uint8_t *source=(const uint8_t *)context+offset;
    for(uint64_t i=0;i<first_length;i++) first[i]=source[i];
    for(uint64_t i=0;i<second_length;i++) second[i]=source[first_length+i];
    return true;
}
#endif
/* Process file data operations are nonblocking and already execute with IRQs
 * masked internally. Include the mapping check in the same CPU0 exclusion so
 * the descriptor and mapping cannot change before their commit. */
static int64_t regular_user_io(uint64_t fd, void *buffer, uint64_t length,
    bool writing, bool positional, int64_t offset) {
    uint64_t flags = irq_save();
    int64_t result;
    struct process_file *file = process_file_at(process_current(), fd);
    if (!user_buffer_ok(buffer, length, !writing)) result = SRV_ERR_FAULT;
    else if (!positional && (!file || (file->type != PROCESS_FILE_VFS &&
             file->type != PROCESS_FILE_VFS_WRITE))) result = SRV_ERR_BADF;
    else if (positional) result = writing ?
        process_file_pwrite(process_current(), fd, buffer, length, offset) :
        process_file_pread(process_current(), fd, buffer, length, offset);
    else result = writing ? process_file_write(process_current(), fd, buffer, length) :
        process_file_read(process_current(), fd, buffer, length);
    irq_restore(flags);
    return result;
}

static int64_t syscall_write(uint64_t fd, const char *buffer, uint64_t length) {
    if(io_defer_pipe_write(fd,length)) return length?SRV_ERR_AGAIN:0;
    if(!user_buffer_ok(buffer,length,false)) return SRV_ERR_FAULT;
    struct process_file *file = process_file_at(process_current(), fd);
    if (file && (syscall_pipe_write_type(file->type))) {
        return process_file_pipe_write_checked(process_current(), fd, length, false,
            pipe_user_copyin, (void *)buffer);
    }
#ifndef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    if(file && (file->type==PROCESS_FILE_PTY_MASTER || file->type==PROCESS_FILE_PTY_SLAVE))
        return process_file_pty_write_checked(process_current(),fd,length,pipe_user_copyin,(void *)buffer);
    if (file && file->type == PROCESS_FILE_VFS_WRITE)
        return regular_user_io(fd, (void *)buffer, length, true, false, 0);
#endif
    return write_buffer(fd,buffer,length,false);
}

#if !defined(SRVROS_SIGNAL_EXPERIMENTAL_INFO) && !defined(SRVROS_SIGNAL_PUBLIC_GUEST_TEST)
struct reserved_append_input {const void *data;uint64_t requested;};
static bool reserved_append_copy(void *context,void *destination,uint64_t count,uint64_t requested) {
    const struct reserved_append_input *input=context;
    /* Match ordinary write precedence: validate the FULL original range even
     * when FSIZE permits only a positive short write. Every page access keeps
     * the caller's outer IF0. A late fault can touch only unpublished tail. */
    if(requested!=input->requested || !count || count>requested || requested>65536 ||
       !user_buffer_ok(input->data,requested,false))return false;
    return copy_from_user(destination,input->data,count);
}
#endif
bool syscall_reserved_append_candidate(const struct isr_frame *frame) {
#if defined(SRVROS_SIGNAL_EXPERIMENTAL_INFO) || defined(SRVROS_SIGNAL_PUBLIC_GUEST_TEST)
    (void)frame;return false;
#else
    return frame && (frame->cs&3)==3 && frame->rax==SYS_WRITE && frame->rdi>=3 &&
        frame->rdx && frame->rdx<=65536 &&
        process_file_reserved_append_entry_candidate(process_current(),frame->rdi,frame->rdx);
#endif
}
bool syscall_reserved_append_dispatch(struct isr_frame *frame) {
    if(!frame)return false;
    if((frame->cs&3)!=3 || frame->rax!=SYS_WRITE) {
        process_capture_write_invalidate();frame->rax=(uint64_t)SRV_ERR_INVAL;return false;
    }
    uint64_t fd=frame->rdi,requested=frame->rdx;
    const char *buffer=(const char *)frame->rsi;
    uint64_t handler_at=process_capture_write_clock();
#if !defined(SRVROS_SIGNAL_EXPERIMENTAL_INFO) && !defined(SRVROS_SIGNAL_PUBLIC_GUEST_TEST)
    if(fd>=3 && requested && requested<=65536) {
        struct reserved_append_input input={.data=buffer,.requested=requested};int64_t result;
        do {
            if(process_should_exit_current())break;
            if(process_file_try_reserved_append(process_current(),fd,requested,reserved_append_copy,&input,&result)) {
                process_capture_write_fast_result(requested,result);frame->rax=(uint64_t)result;
                process_capture_write_record(CAP_WRITE_HANDLER,handler_at);return true;
            }
        } while(process_file_wait_reserved_append(process_current(),fd,requested));
    }
#endif
    /* A full window or a lease revoked across a notified wait retains ordinary
     * admission. No private pointer or earlier eligibility survives this edge. */
    scheduler_io_set_user_safe(false);
    uint64_t wait_at=scheduler_io_active()?latency_clock():0;
    uint64_t gate_at=process_capture_write_clock();
    if(gate_at)process_capture_write_gate_wait();else scheduler_io_wait();
    process_capture_write_record(CAP_WRITE_GATE,gate_at);latency_gate(wait_at,SYS_WRITE);
    if(process_should_exit_current()) {
        scheduler_io_wait();process_exit(process_current_kill_status());
    }
    /* Exclude the observed ordinary gate from handler time. Prelude wait wall
     * remains explicit in the append-wait companion and whole raw-call span. */
    handler_at=process_capture_write_clock();
    frame->rax=(uint64_t)syscall_write(fd,buffer,requested);
    process_capture_write_record(CAP_WRITE_HANDLER,handler_at);return false;
}

static char tty_filter_input(char c) {
    if ((console_termios.iflag & SRV_TTY_IFLAG_ICRNL) != 0 && c == '\r') {
        return '\n';
    }
    return c;
}

struct tty_checked_read {
    struct console_fd_guard guard;
    char *buffer;uint64_t length,count;bool owned,canonical,finished;
    const struct srv_iovec *iov;uint64_t iov_count;
    volatile char *destination;
    char echo;bool echo_valid;uint64_t erase_echo;
};
static int tty_read_eligible(void *opaque) {
    struct tty_checked_read *r=opaque;int status=console_read_eligible(&r->guard);if(status)return status;
    if(r->count>=r->length)return SRV_ERR_FAULT;
    uint64_t offset=r->count,address=(uint64_t)(uintptr_t)r->buffer;
    if(r->iov) {
        if(r->iov_count>SRV_IOV_MAX)return SRV_ERR_FAULT;
        bool found=false;
        for(uint64_t i=0;i<r->iov_count;i++) {
            if(offset<r->iov[i].length) {address=(uint64_t)(uintptr_t)r->iov[i].base;found=true;break;}
            offset-=r->iov[i].length;
        }
        if(!found)return SRV_ERR_FAULT;
    }
    if(offset>UINT64_MAX-address)return SRV_ERR_FAULT;
    address+=offset;
    /* Initial syscall validation is separate. This callback runs per byte
     * under IF0: visit only the destination that this consume can publish. */
    if((r->iov || !r->owned) && !user_buffer_ok((void *)(uintptr_t)address,1,true))return SRV_ERR_FAULT;
    r->destination=(volatile char *)(uintptr_t)address;
    return 0;
}
/* Eligibility established this byte's destination with IRQs excluded; no call,
 * fault-recovery, scheduler, or fallible operation is permitted here. */
static void tty_read_commit(void *opaque,char input) {
    struct tty_checked_read *r=opaque;char c=tty_filter_input(input);r->echo=0;r->echo_valid=false;r->erase_echo=0;
    if(r->canonical) {
        char erase=console_termios.cc[SRV_TTY_VERASE]?(char)console_termios.cc[SRV_TTY_VERASE]:'\b';
        char eof=console_termios.cc[SRV_TTY_VEOF]?(char)console_termios.cc[SRV_TTY_VEOF]:4;
        char kill=console_termios.cc[SRV_TTY_VKILL]?(char)console_termios.cc[SRV_TTY_VKILL]:21;
        char eol=console_termios.cc[SRV_TTY_VEOL]?(char)console_termios.cc[SRV_TTY_VEOL]:'\n';
        if(c==eof) {r->finished=true;return;}
        if((c==erase || c=='\b') && r->count) {r->count--;r->erase_echo=1;return;}
        if(c==kill) {r->erase_echo=r->count;r->count=0;return;}
        if(c=='\n' || c==eol)r->finished=true;
    } else r->finished=true;
    *r->destination=c;
    r->count++;r->echo=c;r->echo_valid=true;
}
static int64_t tty_read_console_sink(uint64_t fd,char *buffer,uint64_t length,bool owned,
    const struct srv_iovec *iov,uint64_t iov_count) {
    if(!length)return 0;
    struct tty_checked_read r={.guard={.process=process_current(),.fd=fd},.buffer=buffer,.length=length,
        .owned=owned,.canonical=(console_termios.lflag&SRV_TTY_LFLAG_ICANON)!=0,.iov=iov,.iov_count=iov_count};
    r.guard.generation=process_fd_generation(r.guard.process,fd);
    irq_restore(UINT64_C(1)<<9);
    while(r.count<length && !r.finished) {
        char c;bool wait=r.canonical || console_termios.cc[SRV_TTY_VMIN]!=0;
        int got=keyboard_read_char_commit(&c,wait,tty_read_eligible,tty_read_commit,&r);
        if(got<0)return r.count?(int64_t)r.count:got;
        if(!got)return (int64_t)r.count;
        if(console_termios.lflag&SRV_TTY_LFLAG_ECHO) {
            for(uint64_t i=0;i<r.erase_echo;i++){console_putc('\b');console_putc(' ');console_putc('\b');}
            if(r.echo_valid)console_putc(r.echo);
        }
    }
    return (int64_t)r.count;
}
static int64_t tty_read_console(uint64_t fd,char *buffer,uint64_t length,bool owned) {
    return tty_read_console_sink(fd,buffer,length,owned,NULL,0);
}

/* Only explicit nonblocking pipe/PTY descriptors get the try-read path. No
 * redirected stdio guessing, network operations or regular-file reads. */
static bool io_try_pipe_read(uint64_t fd) {
    struct process_file *f=process_file_at(process_current(),fd);
    return f && (syscall_pipe_read_type(f->type)
#ifndef SRVROS_SIGNAL_EXPERIMENTAL_INFO
        || f->type==PROCESS_FILE_PTY_MASTER || f->type==PROCESS_FILE_PTY_SLAVE
#endif
        ) &&
        process_file_nonblocking(process_current(),fd);
}
static bool io_defer_pipe_read(uint64_t fd,uint64_t length) {
    if(io_try_pipe_read(fd)) {
        if(process_file_pipe_io_safe(process_current(),fd,length,false))return false;
        /* Replies need the same quiescent admission as requests and reaps.
         * Remain nonblocking and user-safe until the owner has unwound. */
        if(scheduler_io_busy()) { scheduler_io_request_turn();return true; }
        /* A read can discard transferred file rights and close/flush them.
         * Defer that work during I/O; retain ordinary eligibility otherwise. */
        scheduler_io_set_user_safe(false);
    }
    return false;
}
static int64_t read_buffer(uint64_t fd, char *buffer, uint64_t length, bool owned) {
    struct process_file *file = process_file_at(process_current(), fd);
    if (file == NULL) {
        return SRV_ERR_BADF;
    }
    if (file->type == PROCESS_FILE_STDIO && file->handle == 0) {
        return tty_read_console(fd,buffer,length,owned);
    }
    if (file->type == PROCESS_FILE_NET_CONNECTION) {
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
        return SRV_ERR_NOTSUP;
#else
        if (owned) return net_read(file->handle, buffer, length, process_file_nonblocking(process_current(), fd));
        struct srv_iovec iov = {.base=buffer, .length=length};
        struct network_user_io io = {.iov=&iov, .count=1};
        return network_receive(fd, &io, length, process_file_nonblocking(process_current(), fd), false, false);
#endif
    }
    if (syscall_pipe_read_type(file->type)) {
        return process_file_pipe_read(process_current(), fd, (uint8_t *)buffer, length);
    }
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    return SRV_ERR_NOTSUP;
#else
    if (file->type != PROCESS_FILE_VFS && file->type != PROCESS_FILE_VFS_WRITE &&
        file->type != PROCESS_FILE_PTY_MASTER && file->type != PROCESS_FILE_PTY_SLAVE) {
        return SRV_ERR_BADF;
    }

    return process_file_read(process_current(), fd, (uint8_t *)buffer, length);
#endif
}

struct pipe_user_iov {
    const struct srv_iovec *iov;
    uint64_t count;
};

/* Called with interrupts disabled, before pipe bytes or rights are consumed. */
static bool pipe_user_copyout(void *context, const uint8_t *first, uint64_t first_length,
    const uint8_t *second, uint64_t second_length) {
    struct pipe_user_iov *out = context;
    uint64_t total = first_length + second_length, offset = 0;
    for (uint64_t i = 0; i < out->count && offset < total; i++) {
        uint64_t n = out->iov[i].length;
        if (n > total - offset) n = total - offset;
        if (!user_buffer_ok(out->iov[i].base, n, true)) return false;
        offset += n;
    }
    if (offset != total) return false;
    offset = 0;
    for (uint64_t i = 0; i < out->count && offset < total; i++) {
        uint64_t n = out->iov[i].length;
        if (n > total - offset) n = total - offset;
        uint64_t a = offset < first_length ? first_length - offset : 0;
        if (a > n) a = n;
        if (a && !copy_to_user(out->iov[i].base, first + offset, a)) return false;
        if (n > a && !copy_to_user((uint8_t *)out->iov[i].base + a,
            second + (offset + a - first_length), n - a)) return false;
        offset += n;
    }
    return true;
}

static int64_t syscall_read(uint64_t fd, char *buffer, uint64_t length) {
    if(io_defer_pipe_read(fd,length)) return length?SRV_ERR_AGAIN:0;
    if(!user_buffer_ok(buffer,length,true)) return SRV_ERR_FAULT;
    struct process_file *file = process_file_at(process_current(), fd);
    if (file && (syscall_pipe_read_type(file->type))) {
        struct srv_iovec iov = { .base = buffer, .length = length };
        struct pipe_user_iov out = { &iov, 1 };
        return process_file_pipe_read_checked(process_current(), fd, length, false, pipe_user_copyout, &out);
    }
#ifndef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    if(file && (file->type==PROCESS_FILE_PTY_MASTER || file->type==PROCESS_FILE_PTY_SLAVE)) {
        struct srv_iovec iov={.base=buffer,.length=length};
        struct pipe_user_iov out={&iov,1};
        return process_file_pty_read_checked(process_current(),fd,length,pipe_user_copyout,&out);
    }
    if (file && (file->type == PROCESS_FILE_VFS || file->type == PROCESS_FILE_VFS_WRITE))
        return regular_user_io(fd, buffer, length, false, false, 0);
#endif
    return read_buffer(fd,buffer,length,false);
}

/* A vector is one transfer, not one potentially blocking transfer per element.
 * Snapshot vector metadata and gather write data before any wait. Pipe reads
 * retain destination addresses and revalidate them at commit. Large requests
 * may return a bounded prefix, which the caller must handle like any short I/O. */
static int64_t syscall_vector_io_prepared(uint64_t fd, const struct srv_iovec *user_iov,
    uint64_t count, uint64_t operation, int64_t offset,const struct srv_iovec *prepared) {
    if(count>SRV_IOV_MAX || operation>SRV_VECTOR_PWRITE ||
       (operation>=SRV_VECTOR_PREAD && offset<0)) return SRV_ERR_INVAL;
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    if(operation!=SRV_VECTOR_READ && operation!=SRV_VECTOR_WRITE)return SRV_ERR_NOTSUP;
#endif
    struct srv_iovec iov[SRV_IOV_MAX];
    if(prepared) { for(uint64_t i=0;i<count;i++) iov[i]=prepared[i]; }
    else if(!copy_from_user(iov,user_iov,count*sizeof(*iov))) return SRV_ERR_FAULT;
    bool writing=operation==SRV_VECTOR_WRITE || operation==SRV_VECTOR_PWRITE;
    uint64_t total=0;
    for(uint64_t i=0;i<count;i++) {
        if(iov[i].length>(uint64_t)INT64_MAX-total) return SRV_ERR_INVAL;
        total+=iov[i].length;
    }
    for(uint64_t i=0;i<count;i++)
        if(!user_buffer_ok(iov[i].base,iov[i].length,!writing)) return SRV_ERR_FAULT;
    uint64_t length=total>MAX_SYSCALL_COPY?MAX_SYSCALL_COPY:total;
    struct process_file *file = process_file_at(process_current(), fd);
    if(operation==SRV_VECTOR_READ && file && file->type==PROCESS_FILE_STDIO && file->handle==0)
        return tty_read_console_sink(fd,NULL,length,false,iov,count);
    if (operation == SRV_VECTOR_READ && file &&
        (syscall_pipe_read_type(file->type))) {
        struct pipe_user_iov out = { iov, count };
        return process_file_pipe_read_checked(process_current(), fd, length, false, pipe_user_copyout, &out);
    }
#ifndef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    if(operation==SRV_VECTOR_READ && file && (file->type==PROCESS_FILE_PTY_MASTER || file->type==PROCESS_FILE_PTY_SLAVE)) {
        struct pipe_user_iov out={iov,count};
        return process_file_pty_read_checked(process_current(),fd,length,pipe_user_copyout,&out);
    }
    if (operation == SRV_VECTOR_READ && file && file->type == PROCESS_FILE_NET_CONNECTION) {
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
        return SRV_ERR_NOTSUP;
#else
        struct network_user_io io = {.iov=iov, .count=count};
        return network_receive(fd, &io, length, process_file_nonblocking(process_current(), fd), false, false);
#endif
    }
#endif
    uint8_t small[4096];
    uint8_t *buffer=length>sizeof(small)?kmalloc(length):small;
    if(!buffer) return SRV_ERR_NOMEM;
    uint64_t copied=0;
    if(writing) for(uint64_t i=0;i<count && copied<length;i++) {
        uint64_t size=iov[i].length;
        if(size>length-copied) size=length-copied;
        if(!copy_from_user(buffer+copied,iov[i].base,size)) {
            if(buffer!=small) kfree(buffer);
            return SRV_ERR_FAULT;
        }
        copied+=size;
    }
    int64_t result;
#ifndef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    if(operation==SRV_VECTOR_PREAD)
        result=process_file_pread(process_current(),fd,buffer,length,offset);
    else if(operation==SRV_VECTOR_PWRITE)
        result=process_file_pwrite(process_current(),fd,buffer,length,offset);
    else if(writing && file && (file->type==PROCESS_FILE_PTY_MASTER || file->type==PROCESS_FILE_PTY_SLAVE))
        result=process_file_pty_write_checked(process_current(),fd,length,pty_kernel_copyin,buffer);
    else
#endif
    if(writing) result=write_buffer(fd,(char *)buffer,length,true);
    else result=read_buffer(fd,(char *)buffer,length,true);
    /* Console/network waits may return with interrupts enabled. Revalidation
     * and copying must exclude a sibling unmapping the destination. */
    uint64_t irq=irq_save();
    if(result>0 && !writing) {
        copied=0;
        for(uint64_t i=0;i<count && copied<(uint64_t)result;i++) {
            uint64_t size=iov[i].length;
            if(size>(uint64_t)result-copied) size=(uint64_t)result-copied;
            if(!copy_to_user(iov[i].base,buffer+copied,size)) {
                result=copied?(int64_t)copied:SRV_ERR_FAULT;
                break;
            }
            copied+=size;
        }
    }
    if(buffer!=small) kfree(buffer);
    irq_restore(irq);
    return result;
}

static int64_t syscall_vector_io(uint64_t fd,const struct srv_iovec *iov,uint64_t count,uint64_t operation,int64_t offset) {
    return syscall_vector_io_prepared(fd,iov,count,operation,offset,NULL);
}

static bool syscall_tty_fd_ok(uint64_t fd) {
    struct process *process=process_current();
    return process_tty_model(process,fd) || process_fd_is_console(process,fd,true) ||
        process_fd_is_console(process,fd,false);
}

static int64_t syscall_tty_getattr(uint64_t fd, struct srv_termios *termios) {
    if(!process_file_at(process_current(),fd)) return SRV_ERR_BADF;
    if(!syscall_tty_fd_ok(fd)) return SRV_ERR_NOTTY;
    if(!user_buffer_ok(termios,sizeof(*termios),true)) return SRV_ERR_FAULT;
    struct pty_model *terminal=process_tty_model(process_current(),fd);
    return copy_to_user(termios, terminal?&terminal->attributes:&console_termios, sizeof(console_termios)) ? 0 : SRV_ERR_FAULT;
}

static int64_t syscall_tty_setattr(uint64_t fd, uint64_t actions, const struct srv_termios *termios) {
    struct srv_termios copy;
    if(!process_file_at(process_current(),fd)) return SRV_ERR_BADF;
    if(!syscall_tty_fd_ok(fd)) return SRV_ERR_NOTTY;
    if(actions>SRV_TCSAFLUSH) return SRV_ERR_INVAL;
    if(!copy_from_user(&copy,termios,sizeof(copy))) return SRV_ERR_FAULT;
    uint64_t terminal=process_tty_handle(process_current(),fd);
    if(terminal) return process_file_pty_setattr(process_current(),fd,&copy,(unsigned)actions);
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

    uint64_t address = (uint64_t)(uintptr_t)source;
    for (uint64_t i = 0; i < capacity; i++) {
        if (i > UINT64_MAX - address ||
            !copy_from_user(&destination[i], (const void *)(uintptr_t)(address + i), 1)) {
            return false;
        }
        if (destination[i] == '\0') {
            return true;
        }
    }

    destination[capacity - 1] = '\0';
    return false;
}

static int64_t copy_exec_string(const char *,char *,uint64_t,int64_t);
/* The consuming syscall owns admission: libc's earlier PATH_RESOLVE is not
 * authority to reuse an alias or cached backing path after a namespace change. */
static int64_t copy_storage_path(const char *input,char *out,uint64_t capacity,bool directory) {
    char logical[MAX_PATH_LENGTH],normalized[MAX_PATH_LENGTH];
    int64_t error=copy_exec_string(input,logical,sizeof(logical),SRV_ERR_NAMETOOLONG);
    if(error) return error;
    if(!directory) return process_resolve_storage_path(logical,out,capacity);
    error=process_path_context(SRV_PATH_RESOLVE_DIRECTORY,logical,normalized,sizeof(normalized));
    if(error) return error;
    return vfs_namespace_system_path(normalized,out,capacity);
}

/* The scalar request and path are copied once; only the caller-owned immutable
 * expected byte region is sampled in bounded, independently checked chunks. */
static bool verify_expected_copy(void *context,void *destination,uint64_t offset,size_t length) {
    const uint8_t *expected=context;
    return copy_from_user(destination,expected+offset,length);
}
static int64_t syscall_file_verify(const struct srv_file_verify_request *user) {
    struct srv_file_verify_request q;char path[MAX_PATH_LENGTH];
    if(!copy_from_user(&q,user,sizeof(q)))return SRV_ERR_FAULT;
    if(q.abi_version!=SRV_ABI_VERSION || q.struct_size!=sizeof(q) || q.reserved ||
       !q.expected_inode || q.length>SRV_FILE_VERIFY_MAX || (q.length && !q.data))return SRV_ERR_INVAL;
    if(q.length && (q.length>UINTPTR_MAX-(uintptr_t)q.data || !user_buffer_ok(q.data,q.length,false)))return SRV_ERR_FAULT;
    int64_t result=copy_storage_path(q.path,path,sizeof(path),false);
    if(result)return result;
    return file_verify(path,&q,verify_expected_copy,(void *)q.data);
}

/* SYS_FILE_STREAM is admitted as one namespace operation. Its user request,
 * bounded payload snapshot and final copyout all remain inside that window. */
static int64_t syscall_file_stream(const struct srv_file_stream_request *user) {
    struct srv_file_stream_request request;
    if(!copy_from_user(&request,user,sizeof(request)))return SRV_ERR_FAULT;
    if(request.version!=SRV_FILE_STREAM_VERSION || request.size!=sizeof(request) ||
       request.flags || request.reserved[0] || request.reserved[1])return SRV_ERR_INVAL;
    uint64_t owner=process_stream_identity(process_current());if(!owner)return SRV_ERR_INVAL;
    unsigned op=request.operation;
    if(op==SRV_FILE_STREAM_OPEN_READ || op==SRV_FILE_STREAM_CREATE_APPEND) {
        if(request.handle || !request.path || request.buffer || request.length || request.offset ||
           (op==SRV_FILE_STREAM_OPEN_READ && request.mode) || (request.mode&~07777ull))return SRV_ERR_INVAL;
        char path[MAX_PATH_LENGTH];int64_t result=copy_storage_path((const char *)(uintptr_t)request.path,path,sizeof(path),false);
        if(result<0)return result;
        return file_stream_open(owner,path,op==SRV_FILE_STREAM_CREATE_APPEND,
            op==SRV_FILE_STREAM_CREATE_APPEND?process_creation_mode(request.mode):0);
    }
    if(!request.handle || request.path || request.mode)return SRV_ERR_INVAL;
    if(op==SRV_FILE_STREAM_PREAD || op==SRV_FILE_STREAM_APPEND) {
        if(request.length>SRV_FILE_STREAM_CHUNK || (!request.buffer && request.length) ||
           (op==SRV_FILE_STREAM_APPEND && request.offset) || request.offset>UINT64_MAX-request.length)return SRV_ERR_INVAL;
        uint64_t length=request.length;
        if(op==SRV_FILE_STREAM_APPEND) {
            int64_t admitted=file_stream_append_count(owner,request.handle,length);
            if(admitted<0)return admitted;
            if((uint64_t)admitted>length)return SRV_ERR_IO;
            length=(uint64_t)admitted;
        }
        void *output=(void *)(uintptr_t)request.buffer;
        if(!user_buffer_ok(output,length,op==SRV_FILE_STREAM_PREAD))return SRV_ERR_FAULT;
        if(!length)return op==SRV_FILE_STREAM_PREAD?
            file_stream_pread(owner,request.handle,NULL,0,request.offset):0;
        uint8_t *chunk=kmalloc(length);if(!chunk)return SRV_ERR_NOMEM;
        int64_t result;
        if(op==SRV_FILE_STREAM_APPEND) {
            result=copy_from_user(chunk,output,length)?
                file_stream_append(owner,request.handle,chunk,length):SRV_ERR_FAULT;
        } else {
            result=file_stream_pread(owner,request.handle,chunk,length,request.offset);
            if(result>(int64_t)length)result=SRV_ERR_IO;
            if(result>0 && !copy_to_user(output,chunk,(uint64_t)result))result=SRV_ERR_FAULT;
        }
        kfree(chunk);return result;
    }
    if(op==SRV_FILE_STREAM_STAT) {
        if(!request.buffer || request.length!=sizeof(struct srv_file_stream_stat) || request.offset)return SRV_ERR_INVAL;
        void *output=(void *)(uintptr_t)request.buffer;
        if(!user_buffer_ok(output,request.length,true))return SRV_ERR_FAULT;
        struct srv_file_stream_stat info;int64_t result=file_stream_stat(owner,request.handle,&info);
        return result<0?result:copy_to_user(output,&info,sizeof(info))?0:SRV_ERR_FAULT;
    }
    if(request.buffer || request.length || request.offset)return SRV_ERR_INVAL;
    if(op==SRV_FILE_STREAM_SYNC)return file_stream_sync(owner,request.handle);
    if(op==SRV_FILE_STREAM_CLOSE)return file_stream_close(owner,request.handle);
    return SRV_ERR_INVAL;
}

static int64_t syscall_open(const char *user_path) {
    char path[MAX_PATH_LENGTH];
    int64_t error=copy_storage_path(user_path,path,sizeof(path),false);
    if(error) return error;

    if(exfat_stream_busy(path,false))return SRV_ERR_BUSY;
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
    return process_file_open_flags(process_current(),path,flags);
}

static int64_t syscall_close(uint64_t fd) {
    int64_t result;
    if(process_file_try_close_pipe(process_current(),fd,&result))return result;
    scheduler_io_set_user_safe(false);scheduler_io_wait();
    return process_file_close(process_current(), fd);
}

static int64_t syscall_list(uint64_t index,char *path_buffer,uint64_t path_capacity,uint64_t *size_out) {
    if(!path_buffer || !size_out || !path_capacity ||
        !user_buffer_ok(path_buffer,path_capacity,true) ||
        !user_buffer_ok(size_out,sizeof(*size_out),true)) return SRV_ERR_FAULT;
    char path[SRV_PATH_MAX];const struct vfs_node *node;
    int status=vfs_namespace_system_list(index,path,sizeof(path),&node);
    if(status<=0) return status;
    uint64_t length=0;while(path[length])length++;
    if(length>=path_capacity) { length=path_capacity-1;path[length]=0; }
    uint64_t size=node->size;(void)process_open_file_size(node->path,&size);
    if(!copy_to_user(path_buffer,path,length+1) || !copy_to_user(size_out,&size,sizeof(size))) return SRV_ERR_FAULT;
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
    char *storage, uint64_t *used, uint64_t *budget,
    const char *values[],uint64_t capacity,uint64_t *count_out) {
    *count_out=0;
    if(!user_vector) return 0;
    for(uint64_t i=0;i<=capacity;i++) {
        const char *source=NULL;
        if(!copy_from_user(&source,user_vector+i,sizeof(source))) return SRV_ERR_FAULT;
        if(!source) { *count_out=i;return 0; }
        if(i==capacity || *budget<=sizeof(uint64_t)) return SRV_ERR_2BIG;
        *budget-=sizeof(uint64_t);
        values[i]=storage+*used;
        for(;;) {
            if(!*budget) return SRV_ERR_2BIG;
            char c;
            if(!copy_from_user(&c,source++,1)) return SRV_ERR_FAULT;
            storage[(*used)++]=c;(*budget)--;
            if(!c) break;
        }
    }
    return SRV_ERR_2BIG;
}

static int64_t syscall_exec(const struct syscall_exec_request *user_request,uint64_t scope_token) {
    struct syscall_exec_request request;
    char path[MAX_PATH_LENGTH];
    char vector_storage[SRV_EXEC_BYTES_MAX];
    uint64_t used=0,budget=SRV_EXEC_BYTES_MAX-2*sizeof(uint64_t);
    const char *argv[MAX_EXEC_ARGS];
    const char *envp[MAX_EXEC_ENV];
    struct process_spawn_file_action file_actions[SRV_SPAWN_FILE_ACTION_MAX];
    char action_paths[SRV_SPAWN_FILE_ACTION_MAX][MAX_PATH_LENGTH];
    uint64_t argc = 0;
    uint64_t envc = 0;
    uint64_t flags;
    int64_t result;

    if(!copy_from_user(&request,user_request,sizeof(request))) return SRV_ERR_FAULT;
    if(scope_token && (!(request.flags&SRV_EXEC_BACKGROUND) || (request.flags&SRV_EXEC_REPLACE))) return SRV_ERR_INVAL;
    if(request.flags&~(SRV_EXEC_BACKGROUND|SRV_EXEC_REPLACE|SRV_EXEC_SET_SIGNAL_DEFAULT|SRV_EXEC_SET_SIGNAL_MASK|SRV_EXEC_INHERIT_FILES)) return SRV_ERR_INVAL;
    if((request.flags&SRV_EXEC_REPLACE) && (request.file_action_count || (request.flags&SRV_EXEC_INHERIT_FILES))) return SRV_ERR_INVAL;
    if((request.flags&SRV_EXEC_INHERIT_FILES) && (request.stdin_fd!=-1 || request.stdout_fd!=-1 || request.stderr_fd!=-1)) return SRV_ERR_INVAL;
    int64_t error=copy_exec_string(request.path,path,sizeof(path),SRV_ERR_NAMETOOLONG);
    if(!error) error=copy_user_string_vector(request.argv,vector_storage,&used,&budget,argv,MAX_EXEC_ARGS,&argc);
    if(!error) error=copy_user_string_vector(request.envp,vector_storage,&used,&budget,envp,MAX_EXEC_ENV,&envc);
    if(error) return error;
    if(request.file_action_count>SRV_SPAWN_FILE_ACTION_MAX) return SRV_ERR_INVAL;
    if(request.file_action_count && !copy_from_user(file_actions,request.file_actions,
        request.file_action_count*sizeof(file_actions[0]))) return SRV_ERR_FAULT;
    for(uint64_t i=0;i<request.file_action_count;i++) if(file_actions[i].type==SRV_SPAWN_FILE_ACTION_OPEN) {
        error=copy_exec_string(file_actions[i].path,action_paths[i],sizeof(action_paths[i]),SRV_ERR_NAMETOOLONG);
        if(error) return error;
        file_actions[i].path=action_paths[i];
    }
    if (argc == 0) {
        uint64_t path_bytes=1;while(path[path_bytes-1]) path_bytes++;
        if(path_bytes+sizeof(uint64_t)>budget) return SRV_ERR_2BIG;
        argv[argc++] = path;
    }

    uint64_t request_mask=(scope_token?1ull:0)|(request.flags!=SRV_EXEC_BACKGROUND?2ull:0)|
        ((request.stdin_fd!=-1 || request.stdout_fd!=-1 || request.stderr_fd!=-1)?4ull:0)|
        (request.process_group?8ull:0)|(request.foreground?16ull:0)|(request.file_action_count?32ull:0);
    if(!request_mask) {
        if(process_try_spawn_private(path,argc,argv,envc,envp,&result))return result;
    } else scheduler_launch_trace_exec(LAUNCH_TRACE_REQUEST,request_mask,0,0,0);
    scheduler_io_set_user_safe(false);scheduler_io_wait();
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
        result = process_spawn_exec_scoped(scope_token,path,
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

static bool scope_publish(void *destination,const void *source,uint64_t size) {
    uint64_t flags=irq_save();
    bool ok=user_buffer_ok(destination,size,true) && copy_to_user(destination,source,size);
    irq_restore(flags);return ok;
}

static int64_t syscall_session_scope(uint64_t op,uint64_t token,void *payload) {
    if(op==SRV_SCOPE_CREATE) {
        uint64_t created=0;if(token || !payload) return SRV_ERR_INVAL;
        int64_t result=process_scope_create(&created);if(result) return result;
        if(!scope_publish(payload,&created,sizeof(created))) {
            (void)process_scope_control(SRV_SCOPE_REVOKE,created,NULL);
            (void)process_scope_control(SRV_SCOPE_RELEASE,created,NULL);
            return SRV_ERR_FAULT;
        }
        return 0;
    }
    if(op==SRV_SCOPE_EXEC) {
        struct srv_scope_exec request;
        if(!payload || !copy_from_user(&request,payload,sizeof(request))) return SRV_ERR_FAULT;
        if(request.version!=SRV_SCOPE_VERSION || request.size!=sizeof(request) || request.reserved) return SRV_ERR_INVAL;
        int64_t result=process_scope_reserve(token);if(result) return result;
        result=syscall_exec((const struct syscall_exec_request *)request.request,token);
        process_scope_cancel_reservation(token);return result;
    }
    if(op==SRV_SCOPE_STATUS) {
        struct srv_scope_status status;
        if(!payload || !copy_from_user(&status,payload,sizeof(status))) return SRV_ERR_FAULT;
        if(status.version!=SRV_SCOPE_VERSION || status.size!=sizeof(status) || status.reserved) return SRV_ERR_INVAL;
        int64_t result=process_scope_control(op,token,&status);
        if(result) return result;
        return scope_publish(payload,&status,sizeof(status))?0:SRV_ERR_FAULT;
    }
    if(payload) return SRV_ERR_INVAL;
    if(op==SRV_SCOPE_REAP_RETIRED) return process_scope_reap(token);
    return process_scope_control(op,token,NULL);
}

static int64_t syscall_fs_write(const char *user_path, const uint8_t *buffer, uint64_t length) {
    char path[MAX_PATH_LENGTH];
    uint8_t *copy;
    int64_t result;
    if(length > MAX_SYSCALL_COPY) return SRV_ERR_FBIG;
    int64_t error=copy_storage_path(user_path,path,sizeof(path),false);
    if(error) return error;

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

    if(length > MAX_SYSCALL_COPY) return SRV_ERR_FBIG;
    int64_t error=copy_storage_path(user_path,path,sizeof(path),false);
    if(error) return error;
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
    if(!info) return SRV_ERR_FAULT;
    int64_t error=copy_storage_path(user_path,path,sizeof(path),false);
    if(error) return error;

    if (!vfs_stat(path, &metadata, &copy.size, &copy.type)) {
        return SRV_ERR_NOENT;
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
    return copy_abi_struct_to_user(info, &copy, sizeof(copy), SRV_ABI_VERSION) ? 0 : SRV_ERR_FAULT;
}

static int64_t syscall_statfs(const char *user_path, struct syscall_fsinfo *info) {
    char path[MAX_PATH_LENGTH];
    struct exfat_fsinfo fsinfo;
    struct syscall_fsinfo copy = {0};
    if(!info) return SRV_ERR_FAULT;
    int64_t error=copy_storage_path(user_path,path,sizeof(path),false);
    if(error) return error;
    if(!vfs_lookup(path)) return SRV_ERR_NOENT;

    if (!exfat_statfs(path, &fsinfo)) {
        return SRV_ERR_IO;
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
    return copy_abi_struct_to_user(info, &copy, sizeof(copy), SRV_ABI_VERSION) ? 0 : SRV_ERR_FAULT;
}

static int64_t syscall_chmod(const char *user_path, uint64_t mode) {
    char path[MAX_PATH_LENGTH];
    int64_t error=copy_storage_path(user_path,path,sizeof(path),false);
    if(error) return error;
    if(exfat_stream_busy(path,false))return SRV_ERR_BUSY;
    if(!vfs_lookup(path)) return SRV_ERR_NOENT;
    process_revoke_reserved_append_path(path);
    return vfs_chmod(path, mode) ? 0 : SRV_ERR_IO;
}

static int64_t syscall_utime(const char *user_path, uint64_t atime, uint64_t mtime) {
    char path[MAX_PATH_LENGTH];
    int64_t error=copy_storage_path(user_path,path,sizeof(path),false);
    if(error) return error;
    if(exfat_stream_busy(path,false))return SRV_ERR_BUSY;
    return process_path_utime(path,atime,mtime);
}

static int64_t syscall_unlink(const char *user_path) {
    char path[MAX_PATH_LENGTH];
    int64_t error=copy_storage_path(user_path,path,sizeof(path),false);
    if(error) return error;
    return process_unlink_file(path);
}

static int64_t syscall_mkdir(const char *user_path,uint64_t mode) {
    char path[MAX_PATH_LENGTH];
    int64_t error=copy_storage_path(user_path,path,sizeof(path),true);
    if(error) return error;
    if(vfs_lookup(path)) return SRV_ERR_EXIST;
    return exfat_create_directory_mode(path,mode==UINT64_MAX?mode:process_creation_mode(mode)) ? 0 : SRV_ERR_IO;
}

static int64_t syscall_rmdir(const char *user_path) {
    char path[MAX_PATH_LENGTH];
    int64_t error=copy_storage_path(user_path,path,sizeof(path),false);
    if(error) return error;
    if(vfs_namespace_system_protects(path)) return SRV_ERR_BUSY;
    const struct vfs_node *directory=vfs_lookup(path);
    if(!directory) return SRV_ERR_NOENT;
    if(directory->type!=VFS_NODE_DIRECTORY) return SRV_ERR_NOTDIR;
    if(process_has_open_vfs_prefix(path)) return SRV_ERR_BUSY;
    uint64_t length=0;while(path[length]) length++;
    for(uint64_t i=0;;i++) {
        const struct vfs_node *node=vfs_node_at(i);if(!node) break;
        uint64_t j=0;while(j<length && path[j]==node->path[j]) j++;
        if(j==length && node->path[j]=='/') return SRV_ERR_NOTEMPTY;
    }
    return exfat_delete_directory(path) ? 0 : SRV_ERR_IO;
}

static int64_t syscall_rename(const char *user_old_path, const char *user_new_path) {
    char old_path[MAX_PATH_LENGTH];
    char new_path[MAX_PATH_LENGTH];
    int64_t error=copy_storage_path(user_old_path,old_path,sizeof(old_path),false);
    if(error) return error;
    error=copy_storage_path(user_new_path,new_path,sizeof(new_path),false);
    if(error) return error;
    return process_rename_file(old_path,new_path);
}

/* The syscall gate serializes filesystem continuations on this single-core
 * kernel. No yielding call separates the VFS guard from entering the existing
 * exFAT mutation scope; owned I/O keeps other namespace mutations gated. */
static int64_t syscall_file_change(const struct srv_file_change *user_request) {
    struct srv_file_change q;char path[MAX_PATH_LENGTH],destination[MAX_PATH_LENGTH];
    if(abi_user_struct_size(user_request,sizeof(q),SRV_ABI_VERSION)!=sizeof(q) ||
        !copy_from_user(&q,user_request,sizeof(q)) || q.operation<SRV_FILE_RENAME || q.operation>SRV_FILE_DUPLICATE ||
        !q.expected_inode || q.expected_type>VFS_NODE_DIRECTORY || copy_storage_path(q.path,path,sizeof(path),false)) return SRV_FILE_INVALID;
    if(q.operation!=SRV_FILE_DELETE && (copy_storage_path(q.destination,destination,sizeof(destination),false))) return SRV_FILE_INVALID;
    if(q.operation==SRV_FILE_COPY && (q.expected_type!=VFS_NODE_FILE || q.length!=q.expected_size || q.length>MAX_SYSCALL_COPY)) return SRV_FILE_LIMIT;
    if(q.operation==SRV_FILE_DUPLICATE) {
        if(q.data || q.length) return SRV_FILE_INVALID;
        if(q.expected_type!=VFS_NODE_FILE || q.expected_size>SRV_FILE_DUPLICATE_MAX) return SRV_FILE_LIMIT;
    }
    if((q.operation==SRV_FILE_RENAME || q.operation==SRV_FILE_DELETE) &&
       (vfs_namespace_system_protects(path) || (q.operation==SRV_FILE_RENAME && vfs_namespace_system_protects(destination)))) return SRV_FILE_BUSY;
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
    int64_t result;
    if(process_try_mmap_private(process_current(),address,length,protection,flags,fd,offset,&result))return result;
    uint64_t wait_at=scheduler_io_active()?latency_clock():0;
    scheduler_io_set_user_safe(false);scheduler_io_wait();latency_gate(wait_at,SYS_MMAP);
    result=process_should_exit_current()?-1:process_mmap(process_current(),address,length,protection,flags,fd,offset);
    uint64_t irq=irq_save();scheduler_io_set_user_safe(true);scheduler_io_handoff();scheduler_io_set_user_safe(false);irq_restore(irq);
    return result;
}

static int64_t syscall_munmap(uint64_t address, uint64_t length) {
    int64_t result;
    if(process_try_munmap_private(process_current(),address,length,&result))return result;
    scheduler_io_set_user_safe(false);scheduler_io_wait();
    result=process_munmap(process_current(), address, length);
    /* Dispatch classified the validation prelude as safe. After a gated
     * fallback unwinds, retain the ordinary unsafe syscall's fair handoff. */
    uint64_t irq=irq_save();
    scheduler_io_set_user_safe(true);scheduler_io_handoff();scheduler_io_set_user_safe(false);
    irq_restore(irq);return result;
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

static int64_t syscall_pipe_flags(int32_t *fds_out,uint64_t flags,bool duplex) {
    if(flags&~(SRV_FD_NONBLOCK|SRV_FD_CLOEXEC)) return SRV_ERR_INVAL;
    uint64_t fds[2];
    int32_t copy[2];
    uint64_t irq=irq_save();
    if (fds_out == NULL || !user_buffer_ok(fds_out, sizeof(copy), true)) {
        irq_restore(irq);return SRV_ERR_FAULT;
    }
    int64_t result=duplex?process_file_pipe_pair(process_current(),fds):process_file_pipe(process_current(),fds);
    if (result < 0) {
        irq_restore(irq);return result;
    }
    for(unsigned i=0;i<2;i++) {
        (void)process_file_set_flags(process_current(),fds[i],flags&SRV_FD_NONBLOCK);
        (void)process_file_set_fd_flags(process_current(),fds[i],flags&SRV_FD_CLOEXEC);
    }
    copy[0] = (int32_t)fds[0];
    copy[1] = (int32_t)fds[1];
    if(!copy_to_user(fds_out,copy,sizeof(copy))) {
        (void)process_file_close(process_current(),fds[0]);
        (void)process_file_close(process_current(),fds[1]);
        result=SRV_ERR_FAULT;
    }
    irq_restore(irq);return result;
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

struct pipe_rights_copyout {
    int32_t *fds;
    uint64_t *flags;
    uint64_t capacity;
};

static bool pipe_rights_copyout(void *context, const int32_t *fds, uint64_t count, uint64_t flags) {
    struct pipe_rights_copyout *out = context;
    if (count > out->capacity ||
        (out->capacity && !user_buffer_ok(out->fds, out->capacity * sizeof(*fds), true)) ||
        (out->flags && !user_buffer_ok(out->flags, sizeof(flags), true))) return false;
    return (!count || copy_to_user(out->fds, fds, count * sizeof(*fds))) &&
        (!out->flags || copy_to_user(out->flags, &flags, sizeof(flags)));
}

static int64_t syscall_pipe_recv_rights(uint64_t fd, int32_t *user_fds, uint64_t capacity, uint64_t *user_flags) {
    if (capacity > SRV_RIGHTS_MAX ||
        (capacity && !user_buffer_ok(user_fds, capacity * sizeof(*user_fds), true)) ||
        (user_flags && !user_buffer_ok(user_flags, sizeof(*user_flags), true))) return -1;
    struct pipe_rights_copyout out = {user_fds, user_flags, capacity};
    int64_t count = process_file_recv_rights_checked(process_current(), fd, capacity,
        pipe_rights_copyout, &out);
    return count < 0 ? -1 : count; /* Preserve the legacy raw syscall ABI. */
}

/* Snapshot each userspace input while mappings cannot change on this CPU. */
static bool pipe_message_copy_from_user(void *destination, const void *source, uint64_t length) {
    uint64_t irq = irq_save();
    bool copied = copy_from_user(destination, source, length);
    irq_restore(irq);
    return copied;
}

struct pipe_message_copyout {
    struct srv_pipe_message *message;
    struct srv_pipe_message_layout layout;
    struct srv_iovec iov[SRV_IOV_MAX];
    uint64_t capacity;
};

/* Check the same snapshotted destinations before waiting and again under IRQ
 * exclusion before committing data and rights. */
static bool pipe_message_output_valid(const struct pipe_message_copyout *out) {
    if (!user_buffer_ok(out->message, sizeof(*out->message), true)) return false;
    if (!user_buffer_ok(out->layout.control, out->layout.control_length, true) ||
        !user_buffer_ok(out->layout.header, out->layout.header_length, true)) return false;
    uint64_t remaining = out->capacity;
    for (uint64_t i = 0; i < out->layout.iov_count && remaining; i++) {
        uint64_t count = out->iov[i].length < remaining ? out->iov[i].length : remaining;
        if (!user_buffer_ok(out->iov[i].base, count, true)) return false;
        remaining -= count;
    }
    return remaining == 0;
}

/* Called with interrupts excluded, before consuming any data or rights. */
static bool pipe_message_copyout(void *context, const void *data, uint64_t length,
    const struct srv_pipe_message *message) {
    struct pipe_message_copyout *out = context;
    if (length > out->capacity || !pipe_message_output_valid(out)) return false;
    uint64_t copied = 0;
    for (uint64_t i = 0; i < out->layout.iov_count && copied < length; i++) {
        uint64_t count = out->iov[i].length < length - copied ? out->iov[i].length : length - copied;
        if (!copy_to_user(out->iov[i].base, (const uint8_t *)data + copied, count)) return false;
        copied += count;
    }
    if (copied != length) return false;
    return copy_to_user(out->message, message, sizeof(*message));
}

#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
static bool signal_experimental_rights_allowed(const struct srv_pipe_message *message) {
    if(!message || message->count>SRV_RIGHTS_MAX)return false;
    for(uint32_t i=0;i<message->count;i++)
        if(message->fds[i]>=0 && !process_signal_experimental_fd_allowed((uint64_t)message->fds[i]))return false;
    return true;
}
#endif
struct signal_pipe_snapshot {
    struct srv_pipe_message message;
    struct srv_pipe_message_layout layout;
    struct srv_iovec iov[SRV_IOV_MAX];
};
static int64_t syscall_pipe_message_prepared(uint64_t fd, const struct srv_pipe_message_layout *layout,
    struct srv_pipe_message *user_message, uint64_t operation,const struct signal_pipe_snapshot *prepared) {
    struct srv_pipe_message message;
    uint8_t buffer[SRV_PIPE_MESSAGE_MAX];
    struct pipe_message_copyout out = {0};
    if (operation != SRV_PIPE_MESSAGE_SEND && operation != SRV_PIPE_MESSAGE_RECV) return SRV_ERR_INVAL;
    bool receiving = operation == SRV_PIPE_MESSAGE_RECV;
    if(prepared)message=prepared->message;
    else if (!pipe_message_copy_from_user(&message, user_message, sizeof(message))) return SRV_ERR_FAULT;
    if (message.count > SRV_RIGHTS_MAX) return SRV_ERR_INVAL;
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    if(!receiving && !signal_experimental_rights_allowed(&message))return SRV_ERR_NOTSUP;
#endif
    uint32_t valid_flags = receiving ? (SRV_PIPE_MSG_DONTWAIT | SRV_PIPE_MSG_CLOEXEC | SRV_PIPE_MSG_PEEK) :
        (SRV_PIPE_MSG_DONTWAIT | SRV_PIPE_MSG_NOSIGNAL);
    if (message.flags & ~valid_flags) return SRV_ERR_INVAL;
    if(prepared)out.layout=prepared->layout;
    else if (!pipe_message_copy_from_user(&out.layout, layout, sizeof(out.layout))) return SRV_ERR_FAULT;
    if (out.layout.iov_count > SRV_IOV_MAX ||
        (receiving && out.layout.header_length > SRV_PIPE_MESSAGE_AUX_MAX)) return SRV_ERR_INVAL;
    if (out.layout.control_length > SRV_PIPE_MESSAGE_AUX_MAX)
        out.layout.control_length = SRV_PIPE_MESSAGE_AUX_MAX;
    if(prepared) { for(uint64_t i=0;i<out.layout.iov_count;i++) out.iov[i]=prepared->iov[i]; }
    else if (!pipe_message_copy_from_user(out.iov, out.layout.iov, out.layout.iov_count * sizeof(out.iov[0])))
        return SRV_ERR_FAULT;
    uint64_t length = 0;
    for (uint64_t i = 0; i < out.layout.iov_count; i++) {
        uint64_t room = SRV_PIPE_MESSAGE_MAX - length;
        length += out.iov[i].length < room ? out.iov[i].length : room;
    }
    if (!receiving) {
        uint64_t copied = 0;
        for (uint64_t i = 0; i < out.layout.iov_count && copied < length; i++) {
            uint64_t count = out.iov[i].length < length - copied ? out.iov[i].length : length - copied;
            if (!pipe_message_copy_from_user(buffer + copied, out.iov[i].base, count)) return SRV_ERR_FAULT;
            copied += count;
        }
        return process_file_send_message(process_current(), fd, buffer, length, &message);
    }
    out.message = user_message;
    out.capacity = length;
    /* Repeat these checks after waiting, before committing rights or data. */
    if (!pipe_message_output_valid(&out)) return SRV_ERR_FAULT;
    return process_file_recv_message_checked(process_current(), fd, buffer, length,
        &message, pipe_message_copyout, &out);
}

static int64_t syscall_pipe_message(uint64_t fd,const struct srv_pipe_message_layout *layout,
    struct srv_pipe_message *message,uint64_t operation) {
    return syscall_pipe_message_prepared(fd,layout,message,operation,NULL);
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

static uint16_t poll_one_ready(struct process *process,int64_t fd,uint16_t events) {
    if(process_fd_is_console(process,fd,true))
        return (keyboard_buffered_count() || serial_has_char())?events&SRV_POLLIN:0;
    return process_file_poll(process,fd,events);
}

static int64_t poll_once(struct process *process, struct syscall_pollfd *fds, uint64_t nfds) {
    uint64_t flags=irq_save();
    int64_t ready = 0;
    for (uint64_t i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].fd < 0) {
            continue;
        }
        uint16_t revents=poll_one_ready(process,fds[i].fd,(uint16_t)fds[i].events);
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

struct poll_pipe_identity { uint64_t generation,handle; };
/* Metadata-only observation of the same simple endpoints already admitted by
 * bounded nonblocking byte I/O. The caller holds IF0 through classification,
 * revalidation and observation; no descriptor release or rights work occurs. */
static bool poll_pipe_endpoint(struct process *process,const struct syscall_pollfd *fd,
    struct process_file **out) {
    struct process_file *file=process_file_at(process,(uint64_t)fd->fd);
    if(!file)return false;
    bool writing=file->type==PROCESS_FILE_PIPE_WRITE;
    if((!writing && file->type!=PROCESS_FILE_PIPE_READ) ||
       fd->events!=(writing?SRV_POLLOUT:SRV_POLLIN) ||
       !process_file_pipe_io_safe(process,(uint64_t)fd->fd,0,writing))return false;
    *out=file;return true;
}
static bool poll_pipe_capture(struct process *process,const struct syscall_pollfd *fds,uint64_t nfds,
    struct poll_pipe_identity *identities) {
    bool any=false;
    for(uint64_t i=0;i<nfds;i++)if(fds[i].fd>=0) {
        struct process_file *file;
        if(!poll_pipe_endpoint(process,&fds[i],&file))return false;
        uint64_t generation=process_fd_generation(process,(uint64_t)fds[i].fd);
        if(generation==UINT64_MAX)return false;
        identities[i]=(struct poll_pipe_identity){generation,file->handle};any=true;
    }
    return any;
}
static bool poll_pipes_current(struct process *process,const struct syscall_pollfd *fds,uint64_t nfds,
    const struct poll_pipe_identity *identities) {
    for(uint64_t i=0;i<nfds;i++)if(fds[i].fd>=0) {
        /* Identity precedes endpoint-specific lookup: never follow a newly
         * installed descriptor family under a foreign storage owner. */
        if(process_fd_generation(process,(uint64_t)fds[i].fd)!=identities[i].generation)return false;
        struct process_file *file=process_file_at(process,(uint64_t)fds[i].fd);
        if(!file || file->handle!=identities[i].handle)return false;
        if(!poll_pipe_endpoint(process,&fds[i],&file))return false;
    }
    return true;
}

struct syscall_poll_wait {
    struct process *process;
    struct syscall_pollfd *fds;
    uint64_t nfds;
    const struct poll_pipe_identity *pipes;
    bool pipe_request;
};

static bool poll_wait_ready(void *arg) {
    struct syscall_poll_wait *wait = arg;
    if(wait && wait->pipe_request) {
        /* scheduler_wait checks under IF0 before enrollment. A safety/identity
         * change wakes into the original gate, without polling a replacement. */
        return process_should_exit_current() || process_signal_pending_current() ||
            !poll_pipes_current(wait->process,wait->fds,wait->nfds,wait->pipes) ||
            poll_once(wait->process,wait->fds,wait->nfds)!=0;
    }
    return wait == NULL ||
        process_should_exit_current() ||
        process_signal_pending_current() ||
        /* A sibling may redirect/replace a descriptor while we sleep. Wake
         * into the gate instead of examining a newly non-console handle
         * inside someone else's storage operation. */
        (scheduler_io_busy() && !poll_console_only(wait->process,wait->fds,wait->nfds)) ||
        poll_once(wait->process, wait->fds, wait->nfds) != 0;
}

static uint64_t poll_deadline_us(int64_t timeout_ms,uint64_t now) {
    if(timeout_ms<=0) return 0;
    uint64_t milliseconds=(uint64_t)timeout_ms;
    if(milliseconds>(UINT64_MAX-now)/1000) return UINT64_MAX;
    return now+milliseconds*1000;
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
    /* Admit only copied, immutable console or strict simple-pipe metadata
     * requests during owned I/O. Never
     * classify a mutable userspace array and then read it again after yielding.
     * Everything else retains the original filesystem/process lifetime gate. */
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    for(uint64_t i=0;i<nfds;i++)if(fds[i].fd>=0 && !process_signal_experimental_fd_allowed((uint64_t)fds[i].fd))return SRV_ERR_NOTSUP;
#endif
    uint64_t deadline_us=poll_deadline_us(timeout_ms,timer_monotonic_us());
    struct poll_pipe_identity pipes[MAX_POLL_FDS];
    struct syscall_poll_wait wait = {
        .process = process,
        .fds = fds,
        .nfds = nfds,
        .pipes = pipes,
    };
    uint64_t initial_flags=irq_save();
    wait.pipe_request=poll_pipe_capture(process,fds,nfds,pipes);
    irq_restore(initial_flags);
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
        bool pipe_safe=wait.pipe_request && poll_pipes_current(process,fds,nfds,pipes);
        /* Revocation is permanent for this request. Even if the old number
         * becomes a simple pipe again, it cannot reacquire this observation. */
        if(wait.pipe_request && !pipe_safe)wait.pipe_request=false;
        if(!pipe_safe && !poll_console_only(process,fds,nfds)) {
            scheduler_io_set_user_safe(false);scheduler_io_wait();
        }
        int64_t ready = poll_once(process, fds, nfds);
        irq_restore(flags);
        if (ready != 0 || timeout_ms == 0) {
            return copy_to_user(user_fds, fds, nfds * sizeof(*user_fds)) ? ready : -1;
        }
        uint64_t now_us=timer_monotonic_us();
        if(timeout_ms>0 && now_us>=deadline_us)
            return copy_to_user(user_fds,fds,nfds*sizeof(*user_fds))?0:-1;
        /* Scheduler wakeups are tick-granular, but the completion contract is
         * elapsed monotonic time. A tick boundary may arrive early; recheck the
         * absolute deadline and block again instead of returning early/spinning. */
        uint64_t remaining=timeout_ms>0?deadline_us-now_us:100000;
        uint64_t ticks=remaining/10000+(remaining%10000!=0);
        uint64_t now_ticks=timer_ticks();
        uint64_t wait_deadline=ticks>UINT64_MAX-now_ticks?UINT64_MAX:now_ticks+ticks;
        if (!scheduler_wait_timeout(process_file_poll_wait_queue(), poll_wait_ready, &wait, wait_deadline)) {
            scheduler_yield();
        }
    }
}

#ifdef SRVROS_SIGNAL_PUBLIC_GUEST_TEST
static int64_t poll_once_expected(struct process *process, struct syscall_pollfd *fds, uint64_t nfds,const uint64_t *generations) {
    uint64_t flags=irq_save();
    int64_t ready = 0;
    for (uint64_t i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].fd < 0) {
            continue;
        }
        uint16_t revents;
        if(generations && (generations[i]==UINT64_MAX || process_fd_generation(process,(uint64_t)fds[i].fd)!=generations[i]))
            revents=SRV_POLLNVAL;
        else revents=poll_one_ready(process,fds[i].fd,(uint16_t)fds[i].events);
        fds[i].revents = (int16_t)revents;
        if (revents != 0) {
            ready++;
        }
    }
    irq_restore(flags);return ready;
}

struct signal_poll_wait {
    struct process *process;
    struct syscall_pollfd *fds;
    uint64_t nfds;
    const uint64_t *generations;
};

static bool signal_poll_wait_ready(void *arg) {
    struct signal_poll_wait *wait = arg;
    return wait == NULL ||
        process_should_exit_current() ||
        process_signal_pending_current() ||
        /* A sibling may redirect/replace a descriptor while we sleep. Wake
         * into the gate instead of examining a newly non-console handle
         * inside someone else's storage operation. */
        (scheduler_io_busy() && !poll_console_only(wait->process,wait->fds,wait->nfds)) ||
        poll_once_expected(wait->process, wait->fds, wait->nfds,wait->generations) != 0;
}

/* prepared/generations are trusted same-entry snapshots. NULL keeps raw
 * syscall semantics, including legacy pending-signal handling. */
static int64_t syscall_poll_prepared(struct syscall_pollfd *user_fds,uint64_t nfds,int64_t timeout_ms,
    const struct syscall_pollfd *prepared,const uint64_t *generations) {
    if (nfds > MAX_POLL_FDS || (nfds && !user_buffer_ok(user_fds,nfds*sizeof(*user_fds),true))) return -1;
    struct syscall_pollfd fds[MAX_POLL_FDS];
    if(prepared) {for(uint64_t i=0;i<nfds;i++)fds[i]=prepared[i];}
    else if(nfds && !copy_from_user(fds,user_fds,nfds*sizeof(*fds)))return -1;
    struct process *process=process_current();
    uint64_t deadline_us=poll_deadline_us(timeout_ms,timer_monotonic_us());
    struct signal_poll_wait wait={.process=process,.fds=fds,.nfds=nfds,.generations=generations};
#ifndef SRVROS_SYSCALL_POLL_HOST_TEST
    __asm__ volatile ("sti" : : : "memory");
#endif
    for(;;) {
        if(process_should_exit_current())return -1;
        if(!prepared && process_signal_pending_current())
            return copy_to_user(user_fds,fds,nfds*sizeof(*fds))?0:-1;
        uint64_t flags=irq_save();
        if(!poll_console_only(process,fds,nfds)) {scheduler_io_set_user_safe(false);scheduler_io_wait();}
        int64_t ready=poll_once_expected(process,fds,nfds,generations);
        uint64_t now_us=timer_monotonic_us();
        bool timeout=timeout_ms==0 || (timeout_ms>0 && now_us>=deadline_us);
#ifdef SRVROS_SIGNAL_PUBLIC_GUEST_TEST
        bool interrupted=prepared && process_signal_info_pending_current();
        bool legacy=prepared && !interrupted && process_signal_pending_current();
#else
        bool interrupted=false,legacy=false;
#endif
        if(ready!=0 || timeout || legacy) {
            bool copied=copy_to_user(user_fds,fds,nfds*sizeof(*fds));
            irq_restore(flags);return copied?ready:-1;
        }
        irq_restore(flags);
        if(interrupted)return SRV_ERR_INTR;
        uint64_t remaining=timeout_ms>0?deadline_us-now_us:100000;
        uint64_t ticks=remaining/10000+(remaining%10000!=0),now_ticks=timer_ticks();
        uint64_t wait_deadline=ticks>UINT64_MAX-now_ticks?UINT64_MAX:now_ticks+ticks;
        if(!scheduler_wait_timeout(process_file_poll_wait_queue(),signal_poll_wait_ready,&wait,wait_deadline))scheduler_yield();
    }
}

#endif

static int64_t syscall_fcntl(uint64_t fd, uint64_t command, uint64_t arg) {
    if(command==SRV_F_RESERVE_APPEND_RAM) {
#if defined(SRVROS_SIGNAL_EXPERIMENTAL_INFO) || defined(SRVROS_SIGNAL_PUBLIC_GUEST_TEST)
        return SRV_ERR_NOTSUP;
#else
        return process_file_reserve_append_ram(process_current(),fd,arg);
#endif
    }
    if(command==SRV_F_DUPFD || command==SRV_F_DUPFD_CLOEXEC)
        return process_file_dup_min(process_current(),fd,arg,command==SRV_F_DUPFD_CLOEXEC);
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
    struct process_file *file=process_file_at(process_current(),fd);
    if(!file) return SRV_ERR_BADF;
    if(file->type!=PROCESS_FILE_VFS && file->type!=PROCESS_FILE_VFS_WRITE) return SRV_ERR_INVAL;
    int64_t result=process_file_flush(process_current(),fd);
    if(result<0) return result==-1?SRV_ERR_IO:result;
    /* File writeback is synchronous: exFAT success includes data, metadata and
     * clean-marker barriers on its backing volume. Clean descriptions have no
     * deferred writeback, and unlinked RAM contents have no remaining disk file.
     * An unrelated offline disk must not veto this file's acknowledgment.
     * Keep system-wide barriers in SYS_SYNC; any future deferred backend needs
     * an explicit file-scoped durability contract here. */
    return 0;
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

    if (!display_present_info_available() || !console_display_info(&display)) {
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
    copy.flags = intel_gfx_flags() | (display_present_available() ? 0x30ull : 0) |
        (display_present_active() ? 0x40ull : 0);
    copy.accel_backend = intel_gfx_backend();
    return copy_abi_struct_to_user(info, &copy, sizeof(copy), SRV_ABI_VERSION) ? 0 : -1;
}

struct syscall_gfx_mode { uint64_t abi_version, struct_size, width, height; };
static int64_t syscall_gfx_mode_count(void) {
    struct console_display_info display;
    if (!display_present_info_available() || !console_display_info(&display)) return -1;
    return display_present_available() ? (int64_t)display_mode_count() : 1;
}
static int64_t syscall_gfx_mode_get(uint64_t index, struct syscall_gfx_mode *out) {
    struct console_display_info display;
    struct syscall_gfx_mode mode = {.abi_version=SRV_ABI_VERSION,.struct_size=sizeof(mode)};
    if (!display_present_info_available() || !console_display_info(&display)) return -1;
    if (display_present_available()) {
        if (!display_mode_get(index,&mode.width,&mode.height)) return -1;
    } else {
        if (index) return -1;
        mode.width=display.width; mode.height=display.height;
    }
    if (abi_user_struct_size(out,sizeof(mode),SRV_ABI_VERSION)!=sizeof(mode)) return -1;
    return copy_abi_struct_to_user(out,&mode,sizeof(mode),SRV_ABI_VERSION) ? 0 : -1;
}
static int64_t syscall_gfx_mode_set(uint64_t index) {
    struct process *p=process_current();
    uint64_t pid=p ? process_pid(p) : 0;
    if (!pid || !gui_server_is_owner(pid) || !console_framebuffer_muted_by(pid)) return -1;
    return display_mode_set(pid,index) ? 0 : -1;
}

static bool gfx_write_allowed(void) {
    struct process *p = process_current();
    return display_present_can_write(p ? process_pid(p) : 0);
}
static int64_t syscall_gfx_present(unsigned operation) {
    struct process *p = process_current();
    uint64_t pid = p ? process_pid(p) : 0;
    if (!pid) return -1;
    if (operation == 0 && (!gui_server_is_owner(pid) || !console_framebuffer_muted_by(pid))) return -1;
    bool ok = operation == 0 ? display_present_begin(pid) :
        operation == 1 ? display_present_frame(pid) : display_present_end(pid);
    return ok ? 0 : -1;
}

static int64_t syscall_gfx_put_pixel(uint64_t x, uint64_t y, uint32_t rgb) {
    if (!gfx_write_allowed()) return -1;
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
    if (!gfx_write_allowed()) return -1;
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

static int64_t syscall_gfx_blit_copy_impl(struct srv_gfx_blit_rect request, uint64_t scale,struct service_window *window) {
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
        if (!gfx_write_allowed()) { kfree(copy); return -1; }
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
        service_window_checkpoint(window);
    }

    kfree(copy);
    return 0;
}

static int64_t syscall_gfx_blit_copy(struct srv_gfx_blit_rect request,uint64_t scale) {
    struct service_window window=service_window_begin();
    int64_t result=syscall_gfx_blit_copy_impl(request,scale,&window);
    service_window_end(window);return result;
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

static int64_t syscall_gui_surface_create_impl(struct srv_gui_surface_create *user_request,struct service_window *window) {
    (void)window;
    struct srv_gui_surface_create request;
    struct process *process = process_current();
    if (process == NULL || !user_buffer_ok(user_request,sizeof(request),true) ||
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
    if(copy_to_user(user_request,&request,sizeof(request)))return 0;
    gui_surface_destroy(process_pid(process),(uint64_t)surface_id);
    return -1;
}

static int64_t syscall_gui_surface_create(struct srv_gui_surface_create *user_request) {
    struct service_window window=service_window_begin();
    int64_t result=syscall_gui_surface_create_impl(user_request,&window);
    service_window_end(window);return result;
}

static int64_t syscall_gui_surface_destroy(uint64_t surface_id) {
    struct process *process = process_current();
    if (process == NULL) {
        return -1;
    }
    return gui_surface_destroy(process_pid(process), surface_id);
}

static int64_t syscall_gui_surface_blit_impl(const struct srv_gui_surface_rect *user_request,struct service_window *window) {
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
        service_window_checkpoint(window);
    }
    kfree(copy);
    return 0;
}

static int64_t syscall_gui_surface_blit(const struct srv_gui_surface_rect *user_request) {
    struct service_window window=service_window_begin();
    int64_t result=syscall_gui_surface_blit_impl(user_request,&window);
    service_window_end(window);return result;
}

static int64_t syscall_gui_surface_copy_impl(const struct srv_gui_surface_rect *user_request,struct service_window *window) {
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
        service_window_checkpoint(window);
    }
    kfree(copy);
    return 0;
}

static int64_t syscall_gui_surface_copy(const struct srv_gui_surface_rect *user_request) {
    struct service_window window=service_window_begin();
    int64_t result=syscall_gui_surface_copy_impl(user_request,&window);
    service_window_end(window);return result;
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

static bool io_defer_wait(uint64_t flags) {
    if((flags & SRV_WAIT_NOHANG) && scheduler_io_busy()) {
        scheduler_io_request_turn();
        return true;
    }
    return false;
}
static bool syscall_wait_copy(void *context,uint64_t status) {
    uint64_t *out=context;
    if(!out)return true;
    /* Process invokes this under IRQ0 before claiming the child. Validate the
     * complete object before the first byte and keep exclusion through copy. */
    if(!user_buffer_ok(out,sizeof(*out),true))return false;
    for(size_t i=0;i<sizeof(status);i++)((volatile uint8_t *)out)[i]=((const uint8_t *)&status)[i];
    return true;
}
static int64_t syscall_wait(uint64_t pid,uint64_t *status_out,uint64_t flags) {
    if(flags&~(SRV_WAIT_NOHANG|SRV_WAIT_POSIX))return SRV_ERR_INVAL;
    if(status_out && !user_buffer_ok(status_out,sizeof(*status_out),true))return SRV_ERR_FAULT;
    int64_t result=0;
    if(!(flags&SRV_WAIT_POSIX) && process_try_wait_retired_checked(pid,syscall_wait_copy,status_out,&result))return result;
    if(io_defer_wait(flags))return 0;
    scheduler_io_set_user_safe(false);scheduler_io_wait();
#ifndef SRVROS_SYSCALL_IO_HOST_TEST
    if(!(flags&SRV_WAIT_NOHANG))__asm__ volatile("sti":::"memory");
#endif
    result=process_wait_checked((int64_t)pid,flags,syscall_wait_copy,status_out);
    uint64_t irq=irq_save();scheduler_io_set_user_safe(true);scheduler_io_handoff();scheduler_io_set_user_safe(false);
    irq_restore(irq);return result;
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

    struct srv_iovec iov = {.base=buffer, .length=capacity};
    struct network_user_io io = {.iov=&iov, .count=1, .remaining=length_out};
    return network_receive(listener_fd, &io, capacity,
        process_file_nonblocking(process, listener_fd), false, true);
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

    return network_send(fd, buffer, length, false, false, true, remote_ip, remote_port);
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

    struct srv_iovec iov = {.base=buffer, .length=capacity};
    struct network_user_io io = {.iov=&iov, .count=1, .ip=remote_ip_out, .port=remote_port_out};
    return network_receive(fd, &io, capacity, process_file_nonblocking(process, fd), false, false);
}

/* All operations are intrinsically nonblocking. Ordinary syscall admission
 * stays in force; this is not a GUI/I/O-owner bypass. */
static int64_t syscall_socket_try_io(uint64_t fd, uint64_t operation, void *buffer,
    uint64_t length, struct srv_socket_peer *user_peer) {
    if (operation < SRV_SOCKET_RECV || operation > SRV_SOCKET_UDP_SEND) return SRV_ERR_INVAL;
    bool writing = operation == SRV_SOCKET_SEND || operation == SRV_SOCKET_UDP_SEND;
    if (!user_buffer_ok(buffer, length, !writing)) return SRV_ERR_FAULT;
    struct srv_socket_peer peer = {0};
    if (operation == SRV_SOCKET_UDP_SEND) {
        if (!copy_from_user(&peer, user_peer, sizeof(peer))) return SRV_ERR_FAULT;
        if (peer.reserved) return SRV_ERR_INVAL;
    } else if (operation == SRV_SOCKET_UDP_RECV && user_peer &&
        !user_buffer_ok(user_peer, sizeof(*user_peer), true)) return SRV_ERR_FAULT;
    struct process *process = process_current();
    struct process_file *file = process_file_at(process, fd);
    if (!file) return SRV_ERR_BADF;
    if (operation == SRV_SOCKET_UDP_SEND || operation == SRV_SOCKET_UDP_RECV) {
        if (file->type != PROCESS_FILE_NET_UDP) return SRV_ERR_BADF;
        if (operation == SRV_SOCKET_UDP_SEND)
            return network_send(fd, buffer, length, true, false, true, peer.ip, peer.port);
        struct srv_iovec iov = {.base=buffer, .length=length};
        struct network_user_io io = {.iov=&iov, .count=1, .peer=user_peer};
        return network_receive(fd, &io, length, true, false, false);
    }
    if (file->type == PROCESS_FILE_NET_CONNECTION) {
        if (operation == SRV_SOCKET_SEND) return network_send(fd, buffer, length, true, false, false, 0, 0);
        struct srv_iovec iov = {.base=buffer, .length=length};
        struct network_user_io io = {.iov=&iov, .count=1};
        return network_receive(fd, &io, length, true, operation == SRV_SOCKET_PEEK, false);
    }
    if (operation == SRV_SOCKET_PEEK) return SRV_ERR_NOTSUP;
    if (operation == SRV_SOCKET_SEND &&
        (file->type == PROCESS_FILE_PIPE_WRITE || file->type == PROCESS_FILE_PIPE_DUPLEX))
        return process_file_pipe_write_checked(process, fd, length, true, pipe_user_copyin, buffer);
    if (operation == SRV_SOCKET_RECV &&
        (file->type == PROCESS_FILE_PIPE_READ || file->type == PROCESS_FILE_PIPE_DUPLEX))
    {
        struct srv_iovec iov = { .base = buffer, .length = length };
        struct pipe_user_iov out = { &iov, 1 };
        return process_file_pipe_read_checked(process, fd, length, true, pipe_user_copyout, &out);
    }
    return SRV_ERR_BADF;
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
    struct srv_iovec iov = {.base=buffer, .length=capacity};
    struct network_user_io io = {.iov=&iov, .count=1};
    return network_receive(fd, &io, capacity, process_file_nonblocking(process, fd), true, false);
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

struct signal_sleep_elapsed { uint64_t us,ticks;bool mono_valid; };
static int64_t syscall_sleep_ticks_measured(uint64_t ticks,struct signal_sleep_elapsed *elapsed) {
    uint64_t start=timer_ticks();
    if(ticks>UINT64_MAX-start)return -1;
    bool mono=elapsed && timer_reference_ready();uint64_t start_us=mono?timer_monotonic_us():0;
    struct sleep_wait wait={start,ticks};int64_t result=0;
#ifndef SRVROS_SYSCALL_SLEEP_HOST_TEST
    __asm__ volatile ("sti" : : : "memory");
#endif
    for(;;) {
        uint64_t irq=irq_save();uint64_t now=timer_ticks();
        if(now-start>=ticks)result=0;
        else if(process_should_exit_current())result=-1;
        else if(process_signal_pending_current())result=SRV_ERR_INTR;
        else {irq_restore(irq);if(!scheduler_wait_timeout(process_file_poll_wait_queue(),sleep_finished,&wait,start+ticks))scheduler_yield();continue;}
        if(elapsed) {
            elapsed->ticks=now-start;elapsed->mono_valid=mono;
            uint64_t end=mono?timer_monotonic_us():0;
            if(mono && end>=start_us)elapsed->us=end-start_us;
            else {elapsed->mono_valid=false;elapsed->us=0;}
        }
        irq_restore(irq);return result;
    }
}
static int64_t syscall_sleep_ticks(uint64_t ticks) {return syscall_sleep_ticks_measured(ticks,NULL);}

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

/* All snapshots stay on the waiting thread's kernel stack. The scheduler only
 * calls this predicate synchronously on that thread, with IRQs masked, before
 * registering it on the common GUI/file/signal wake queue. */
struct gui_io_wait {
    struct process *process;
    struct srv_gui_wait request;
    uint64_t generations[SRV_GUI_WAIT_FDS],expected_epoch;
    bool deferred;
};
static bool gui_io_wait_ready(void *arg) {
    struct gui_io_wait *wait=arg;
    struct srv_gui_wait *r=&wait->request;
    uint64_t irq=irq_save();
    r->ready=gui_client_wait_state(r->server,wait->expected_epoch,&r->cancel_epoch);
    bool deferred=scheduler_io_busy();
    for(unsigned i=0;i<r->count;i++) {
        r->fds[i].revents=process_file_watch_poll(wait->process,r->fds[i].fd,
            r->fds[i].events,wait->generations[i],deferred);
        if(r->fds[i].revents)r->ready|=SRV_GUI_FD_READY;
    }
    if(!r->ready && timer_monotonic_us()>=r->deadline_us)r->ready=SRV_GUI_DEADLINE;
    bool ready=r->ready || process_should_exit_current() || process_signal_pending_current();
    irq_restore(irq);return ready;
}
static bool gui_io_wait_pending(void *arg) {
    struct gui_io_wait *wait=arg;
    /* Ownership can change between calculating the sleep bound and atomic
     * registration. Return to the loop and recompute its one-tick defer bound. */
    return scheduler_io_busy()!=wait->deferred || gui_io_wait_ready(arg);
}
static int64_t syscall_gui_wait_io(struct srv_gui_wait *user) {
    struct gui_io_wait wait={.process=process_current()};
    struct srv_gui_wait *r=&wait.request;
    if(!user_buffer_ok(user,sizeof(*user),true) || !copy_from_user(r,user,sizeof(*r)) ||
       r->abi_version!=SRV_GUI_WAIT_VERSION || r->struct_size!=sizeof(*r) ||
       !r->server || r->count>SRV_GUI_WAIT_FDS)return -1;
    uint64_t now=timer_monotonic_us();
    if(r->deadline_us>now && r->deadline_us-now>SRV_GUI_WAIT_MAX_US)return -1;
    wait.expected_epoch=r->cancel_epoch;
    for(unsigned i=0;i<r->count;i++) {
        if(r->fds[i].reserved || process_file_watch_capture(wait.process,r->fds[i].fd,
            r->fds[i].events,&wait.generations[i]))return -1;
    }
#ifndef SRVROS_SYSCALL_POLL_HOST_TEST
    __asm__ volatile ("sti" : : : "memory");
#endif
    for(;;) {
        if(process_should_exit_current())return -1;
        if(process_signal_pending_current())return SRV_ERR_INTR;
        if(gui_io_wait_ready(&wait)) {
            if(process_should_exit_current())return -1;
            if(process_signal_pending_current())return SRV_ERR_INTR;
            return copy_to_user(user,r,sizeof(*r))?0:-1;
        }
        now=timer_monotonic_us();
        if(now>=r->deadline_us)continue;
        uint64_t remaining=r->deadline_us-now;
        uint64_t ticks=remaining/10000+(remaining%10000!=0);
        /* Try pipe/PTY I/O is deferred while storage owns the kernel. Continue
         * servicing GUI now; recheck deferred endpoints in at most one tick.
         * Never enter the general filesystem gate from this metadata wait. */
        wait.deferred=scheduler_io_busy();
        if(wait.deferred && ticks>1)ticks=1;
        uint64_t start=timer_ticks();
        uint64_t end=ticks>UINT64_MAX-start?UINT64_MAX:start+ticks;
        if(!scheduler_wait_timeout(process_file_poll_wait_queue(),gui_io_wait_pending,&wait,end))scheduler_yield();
    }
}

static int64_t syscall_gui_wait(uint64_t server,uint64_t ticks) {
    return syscall_gui_wait_common(server,ticks,false);
}
static int64_t syscall_gui_server_wait(uint64_t ticks) {
    return syscall_gui_wait_common(0,ticks,true);
}

static int64_t syscall_thread_create(uint64_t entry, uint64_t arg, uint64_t stack_top, uint64_t flags) {
    int64_t result;
    if(process_try_thread_create_private(entry,arg,stack_top,flags,&result))return result;
    uint64_t wait_at=scheduler_io_active()?latency_clock():0;
    scheduler_io_set_user_safe(false);scheduler_io_wait();latency_gate(wait_at,SYS_THREAD_CREATE);
    if (process_should_exit_current() || entry == 0 ||
        stack_top < 16 ||
        !user_buffer_ok((const void *)entry, 1, false) ||
        !user_buffer_ok((void *)(stack_top - 8), 8, true)) {
        result=-1;
    } else result=process_thread_create(entry,arg,stack_top,flags);
    uint64_t irq=irq_save();scheduler_io_set_user_safe(true);scheduler_io_handoff();scheduler_io_set_user_safe(false);irq_restore(irq);
    return result;
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
    bool timed=sync_timing_enabled() && sync_timing_begin(process_pid(process_current()));
    int64_t result = process_file_sync_all();
    if(timed)sync_timing_files_done(result>=0);
    bool flushed=block_flush_all();
    if(timed)sync_timing_end(flushed);
    if (!flushed) result = -1;
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

/* Only independently synchronized paths with no filesystem or process-lifetime
 * teardown may overlap owned I/O. Default-deny also covers future syscalls. Do not admit
 * poll by inspecting a mutable userspace fd array before the actual copy. Poll
 * enters only its copy/validation prelude here, then gates requests other
 * than validated console or strict simple-pipe metadata observation.
 * Close likewise admits only an atomic simple-pipe attempt before gating all
 * other descriptor cleanup; queued rights may recursively flush files.
 * Exec/exit likewise admit only copied-request or resource-proof preludes;
 * a foreign owner is never borrowed or released. Memory/thread calls admit
 * only a validated bounded atomic attempt. Generic
 * allocation, thread creation, mapping and retirement retain gated fallback. */
void syscall_gate_trace(const struct isr_frame *frame) {
    if(!scheduler_launch_trace_enabled() || (frame->cs&3)!=3)return;
    if(frame->rax==SYS_EXEC)scheduler_launch_trace_entry();
    else if(frame->rax==SYS_FCNTL) {
        uint64_t flags=irq_save();
        const struct process_file *f=process_file_at(process_current(),frame->rdi);
        scheduler_launch_trace_fcntl(frame->rdi,frame->rsi,frame->rdx,f?(uint64_t)f->type:0,f?f->flags:0);
        irq_restore(flags);
    }
}
bool syscall_io_safe(const struct isr_frame *frame) {
    switch (frame->rax) {
    case SYS_POLL: case SYS_CLOSE: case SYS_MUNMAP: case SYS_MMAP: case SYS_THREAD_CREATE:
    case SYS_EXEC: case SYS_EXIT: case SYS_THREAD_EXIT: return true; /* Validated prelude, gated fallback. */
    case SYS_READ: return process_fd_is_console(process_current(),frame->rdi,true) || io_try_pipe_read(frame->rdi) || io_snapshot_read(frame->rdi,frame->rdx);
    case SYS_WRITE: return process_fd_is_console(process_current(),frame->rdi,false) || io_try_pipe_write(frame->rdi);
    case SYS_WAIT: return true; /* Validated retired-child claim; gated fallback. */
    case SYS_CONSOLE_INFO: case SYS_CONSOLE_CLEAR: case SYS_CONSOLE_GOTOXY:
    /* Bounded queues/mouse state use IRQ exclusion. Surface copies use private
     * chunks and IRQ-protected heap/surface state; no VFS or lifetime cleanup.
     * Process listing and memory counters are read-only snapshots.
     * General process spawn/kill remain gated. The lifecycle/memory/thread preludes above
     * require ownership/bounds proofs; all refusals regain ordinary admission. */
    case SYS_GUI_WAIT_IO: case SYS_GUI_CANCEL_WAIT:
    case SYS_GUI_SEND: case SYS_GUI_RECV: case SYS_GUI_WAIT: case SYS_GUI_SERVER_WAIT: case SYS_MOUSE_SCAN:
    case SYS_GUI_KEYBOARD_READ: case SYS_GUI_KEYBOARD_MODE: case SYS_GUI_POINTER_CONFIG: case SYS_GUI_BRIGHTNESS: case SYS_AUDIO: case SYS_DEVICE_LIST: case SYS_THERMAL_STATUS: case SYS_BATTERY_STATUS: case SYS_BATTERY_CONTROL: case SYS_AUDIO_STREAM: case SYS_SMC_STATUS:
    case SYS_GUI_SURFACE_BLIT: case SYS_GUI_SURFACE_COPY:
    case SYS_GUI_SURFACE_CREATE: case SYS_GUI_SURFACE_DESTROY:
    case SYS_GFX_BLIT_RECT: case SYS_GFX_BLIT_SCALED_RECT: case SYS_PROC_LIST: case SYS_MEMINFO: case SYS_SLEEP_TICKS:
    case SYS_POINTER_WAIT: case SYS_SCHEDULER_STATUS: case SYS_SCHED_TRACE:
    case SYS_REALTIME_US:
    case SYS_MONOTONIC_US:
    case SYS_KEY_SCAN: case SYS_YIELD: case SYS_GETPID: case SYS_TICKS:
    case SYS_TTY_GETATTR: case SYS_IOCTL:
    /* Process-local synchronization uses IRQ-protected fixed table entries;
     * no heap/VFS/device work. Scheduler eligibility still excludes unsafe
     * continuations from a foreign owner's I/O. */
    case SYS_FUTEX_WAIT: case SYS_FUTEX_WAKE:
    /* Wait reserves a fixed process-scoped table entry under IRQ exclusion.
     * Each four-byte read revalidates its mapping; active-wait pins and unmap
     * invalidation protect the parked continuation. No allocation/VFS/device
     * callback occurs. Exit on cancellation still regains ordinary admission
     * in the IDT epilogue; this does not admit process teardown under I/O. */
    /* Current scheduler context -> immutable thread ID; no allocation,
     * pointer copy, table mutation, status/reap or lifetime operation. */
    case SYS_THREAD_SELF:
    /* Bounded current-process/thread signal metadata. Configuration changes
     * fixed masks under IRQ exclusion; mask updates may mark cancellation but
     * never run a handler, release a file or tear down a process. IDT regains
     * ordinary admission before acting on that cancellation. */
    case SYS_SIGNAL_CONFIG: case SYS_SIGNAL_PENDING: case SYS_SIGNAL_MASK:
        return true;
    default: return false;
    }
}

static int64_t syscall_directory(uint64_t operation,uint64_t fd,void *out,uint64_t capacity) {
    if(operation==SRV_DIRECTORY_READ_INFO) {
        struct srv_dirent_info entries[SRV_DIRECTORY_BATCH_MAX];
        if(!capacity || capacity>sizeof(entries) || capacity%sizeof(entries[0]))return SRV_ERR_INVAL;
        if(!out || !user_buffer_ok(out,capacity,true))return SRV_ERR_FAULT;
        int64_t count=process_directory(operation,fd,entries,capacity);
        if(count<=0)return count;
        return copy_to_user(out,entries,(uint64_t)count*sizeof(entries[0]))?count:SRV_ERR_FAULT;
    }
    if(operation==SRV_DIRECTORY_CHDIR) return process_directory(operation,fd,NULL,0);
    union { struct srv_dirent entry; char path[MAX_PATH_LENGTH]; } result;
    uint64_t size=operation==SRV_DIRECTORY_READ?sizeof(result.entry):MAX_PATH_LENGTH;
    if(operation>SRV_DIRECTORY_CHDIR || (operation==SRV_DIRECTORY_READ && capacity!=size)) return SRV_ERR_INVAL;
    if(capacity<size) size=capacity;
    if(!out || !size || !user_buffer_ok(out,size,true)) return SRV_ERR_FAULT;
    int64_t status=process_directory(operation,fd,&result,size);
    if(status<0 || (operation==SRV_DIRECTORY_READ && !status)) return status;
    if(operation==SRV_DIRECTORY_PATH) { size=0;while(result.path[size]) size++;size++; }
    return copy_to_user(out,&result,size)?status:SRV_ERR_FAULT;
}

static int64_t syscall_path_context(uint64_t operation,const char *input,char *out,uint64_t capacity) {
    char path[MAX_PATH_LENGTH],result[MAX_PATH_LENGTH];
    if(operation>SRV_PATH_RESOLVE_DIRECTORY) return SRV_ERR_INVAL;
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

static int64_t syscall_sched_trace(uint64_t operation,struct srv_sched_trace *out,uint64_t size) {
    if(operation>SRV_SCHED_TRACE_END || size!=sizeof(*out) || !out || !user_buffer_ok(out,size,true))return -1;
    struct srv_sched_trace result={0};
    if(!scheduler_trace((unsigned)operation,&result))return -1;
    return copy_to_user(out,&result,sizeof(result))?0:-1;
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
        if(!length || length>SRV_PCM_MAX_WRITE || (length&3))return -1;
        uint64_t flags=irq_save();
        int64_t result=user_buffer_ok(data,length,false) ?
            audio_stream_request(process_pid(process_current()),(unsigned)op,handle,data,length) : -1;
        irq_restore(flags);
        return result;
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

#include "signal_io_backend.inc"

#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
static bool signal_experimental_raw_allowed(const struct isr_frame *frame) {
    /* This exact test image admits local runtime qualification only. Reject
     * every external/namespace socket entry before any backend effect. */
    switch(frame->rax) {
    case SYS_SESSION_SCOPE:case SYS_FILE_STREAM:case SYS_FILE_VERIFY:return false;
    case SYS_NET_LISTEN:case SYS_NET_ACCEPT:case SYS_NET_DHCP:case SYS_NET_STATUS:case SYS_NET_DNS:
    case SYS_NET_CONNECT:case SYS_NET_UDP_OPEN:case SYS_NET_UDP_BIND:case SYS_NET_UDP_SENDTO:
    case SYS_NET_UDP_RECVFROM:case SYS_NET_SOCKNAME:case SYS_NET_PEERNAME:case SYS_NET_SHUTDOWN:
    case SYS_NET_ERROR:case SYS_NET_LIST:case SYS_NET_STATUS_INFO:case SYS_NET_ARP_LIST:case SYS_NET_PING:
    case SYS_NET_PEEK:case SYS_UNIX_BIND:case SYS_UNIX_LISTEN:case SYS_UNIX_CONNECT:case SYS_UNIX_ACCEPT:
    case SYS_UNIX_UNLINK:case SYS_SOCKET_TRY_IO:return false;
    case SYS_PREAD:case SYS_PWRITE:case SYS_PIPE_MESSAGE:
    case SYS_PIPE_SEND_RIGHTS:case SYS_PIPE_RECV_RIGHTS:case SYS_PIPE_PAIR:case SYS_PIPE_PAIR_FLAGS:return false;
    case SYS_READ:case SYS_WRITE:case SYS_VECTOR_IO:return process_signal_experimental_fd_allowed(frame->rdi);
    case SYS_FCNTL:return (frame->rsi==SRV_F_GETFD || frame->rsi==SRV_F_GETFL) &&
        process_signal_experimental_fd_allowed(frame->rdi);
    default:return true;
    }
}
#endif
static void syscall_dispatch_impl(struct isr_frame *frame) {
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    if(!signal_experimental_raw_allowed(frame)){frame->rax=(uint64_t)SRV_ERR_NOTSUP;return;}
#endif
    uint64_t io_args[6]={frame->rdi,frame->rsi,frame->rdx,frame->rcx,frame->r8,frame->r9};
    int64_t io_result;
    if(signal_io_raw_backend(frame->rax,io_args,&io_result)) {frame->rax=(uint64_t)io_result;return;}
    switch (frame->rax) {
#ifdef SRVROS_SIGNAL_PUBLIC_GUEST_TEST
    case SYS_SIGNAL_CONTROL:
    case SYS_SIGNAL_IO: {
        bool control=frame->rax==SYS_SIGNAL_CONTROL;
        uint64_t storage[16]={0};uint8_t *wire=(uint8_t *)storage;
        size_t length=control?sizeof(struct srv_signal_control_v1):sizeof(struct srv_signal_io_v1);
        if(frame->rsi!=length || frame->r8 || frame->r9) {frame->rax=(uint64_t)SRV_ERR_INVAL;return;}
        if(!copy_from_user(wire,(const void *)frame->rdi,length)){frame->rax=(uint64_t)SRV_ERR_FAULT;return;}
        uint64_t operation;
        if(control)operation=((const struct srv_signal_control_v1 *)wire)->operation;
        else operation=((const struct srv_signal_io_v1 *)wire)->operation;
        if(!control && operation==SRV_SIGNAL_IO_CALL) {
            if(frame->rdx || frame->rcx) {frame->rax=(uint64_t)SRV_ERR_INVAL;return;}
            frame->rax=(uint64_t)signal_io_call(frame,(const struct srv_signal_io_v1 *)wire);return;
        }
        frame->rax=(uint64_t)process_signal_public_stage(frame,
            control?SIGNAL_REQUEST_CONTROL:SIGNAL_REQUEST_IO,operation,wire,frame->rdx,frame->rcx);
        return;
    }
    case SYS_SIGNAL_RETURN: {
        if(frame->rdx || frame->rcx || frame->r8 || frame->r9){
            (void)process_record_user_fault(SRV_SIGNAL_SEGV);frame->rax=(uint64_t)SRV_ERR_INVAL;return;
        }
        uint64_t wire[16]={frame->rdi,frame->rsi};
        frame->rax=(uint64_t)process_signal_public_stage(frame,SIGNAL_REQUEST_RETURN,1,
            (const uint8_t *)wire,0,0);
        if((int64_t)frame->rax<0)(void)process_record_user_fault(SRV_SIGNAL_SEGV);
        return;
    }
    case SYS_SIGNAL_SUSPEND: {
        if(frame->rcx || frame->r8 || frame->r9){frame->rax=(uint64_t)SRV_ERR_INVAL;return;}
        uint64_t wire[16]={frame->rdi,frame->rsi,frame->rdx};
        frame->rax=(uint64_t)process_signal_public_stage(frame,SIGNAL_REQUEST_SUSPEND,frame->rdi,
            (const uint8_t *)wire,0,0);
        return;
    }
#endif

    case SYS_FILE_VERIFY:
        frame->rax=(uint64_t)syscall_file_verify((const struct srv_file_verify_request *)frame->rdi);
        return;
    case SYS_FILE_STREAM:
        frame->rax=(uint64_t)syscall_file_stream((const struct srv_file_stream_request *)frame->rdi);
        return;
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
        frame->rax = (uint64_t)regular_user_io(frame->rdi,(void *)frame->rsi,
            frame->rdx,false,true,(int64_t)frame->rcx);
        return;
    case SYS_VECTOR_IO:
        frame->rax=(uint64_t)syscall_vector_io(frame->rdi,(const struct srv_iovec *)frame->rsi,
            frame->rdx,frame->rcx,(int64_t)frame->r8);
        return;
    case SYS_PWRITE:
        frame->rax = (uint64_t)regular_user_io(frame->rdi,(void *)frame->rsi,
            frame->rdx,true,true,(int64_t)frame->rcx);
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
    case SYS_SOCKET_TRY_IO:
        frame->rax = (uint64_t)syscall_socket_try_io(frame->rdi, frame->rsi, (void *)frame->rdx,
            frame->rcx, (struct srv_socket_peer *)frame->r8);
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
    case SYS_PIPE_MESSAGE:
        frame->rax = (uint64_t)syscall_pipe_message(frame->rdi,
            (const struct srv_pipe_message_layout *)frame->rsi,
            (struct srv_pipe_message *)frame->rdx, frame->rcx);
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
    case SYS_GFX_PRESENT_BEGIN:
        frame->rax = (uint64_t)syscall_gfx_present(0); break;
    case SYS_GFX_PRESENT:
        frame->rax = (uint64_t)syscall_gfx_present(1); break;
    case SYS_GFX_PRESENT_END:
        frame->rax = (uint64_t)syscall_gfx_present(2); break;
    case SYS_GFX_MODE_COUNT:
        frame->rax = (uint64_t)syscall_gfx_mode_count(); break;
    case SYS_GFX_MODE_GET:
        frame->rax = (uint64_t)syscall_gfx_mode_get(frame->rdi,(struct syscall_gfx_mode *)frame->rsi); break;
    case SYS_GFX_MODE_SET:
        frame->rax = (uint64_t)syscall_gfx_mode_set(frame->rdi); break;
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
    case SYS_GUI_WAIT_IO:
        frame->rax=(uint64_t)syscall_gui_wait_io((struct srv_gui_wait *)frame->rdi);
        return;
    case SYS_GUI_CANCEL_WAIT:
        frame->rax=(uint64_t)gui_cancel_wait();
        return;
    case SYS_GUI_WAIT:
        frame->rax=(uint64_t)syscall_gui_wait(frame->rdi,frame->rsi);
        return;
    case SYS_GUI_REGISTER:
        frame->rax = (uint64_t)syscall_gui_register();
        return;
    case SYS_SCHED_TRACE:
        frame->rax=(uint64_t)syscall_sched_trace(frame->rdi,(struct srv_sched_trace *)frame->rsi,frame->rdx);
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
        frame->rax = (uint64_t)syscall_mkdir((const char *)frame->rdi,UINT64_MAX);
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
    case SYS_DUP3:
        frame->rax=(uint64_t)process_file_dup3(process_current(),frame->rdi,frame->rsi,frame->rdx);
        return;
    case SYS_PIPE:
        frame->rax = (uint64_t)syscall_pipe_flags((int32_t *)frame->rdi,0,false);
        return;
    case SYS_PIPE2:
        frame->rax = (uint64_t)syscall_pipe_flags((int32_t *)frame->rdi,frame->rsi,false);
        return;
    case SYS_PTY_CREATE:
        frame->rax = (uint64_t)syscall_pty_create((int32_t *)frame->rdi);
        return;
    case SYS_PIPE_PAIR_FLAGS:
        frame->rax=(uint64_t)syscall_pipe_flags((int32_t *)frame->rdi,frame->rsi,true);
        break;
    case SYS_PIPE_PAIR:
        frame->rax = (uint64_t)syscall_pipe_flags((int32_t *)frame->rdi,0,true);
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
    case SYS_SESSION_SCOPE:
        frame->rax = (uint64_t)syscall_session_scope(frame->rdi,frame->rsi,(void *)frame->rdx);
        return;
    case SYS_EXEC:
        frame->rax = (uint64_t)syscall_exec((const struct syscall_exec_request *)frame->rdi,0);
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
        frame->rax = 0;
        return;
    case SYS_THREAD_JOIN:
        frame->rax = (uint64_t)syscall_thread_join(frame->rdi, (uint64_t *)frame->rsi);
        return;
    case SYS_THREAD_SELF:
        frame->rax = process_thread_self();
        return;
    case SYS_THREAD_DETACH:
        frame->rax = (uint64_t)process_thread_detach(frame->rdi);
        return;
    case SYS_UMASK:
        frame->rax=(uint64_t)process_umask(frame->rdi);return;
    case SYS_MKDIR_MODE:
        frame->rax=(uint64_t)syscall_mkdir((const char *)frame->rdi,frame->rsi&07777);return;
    case SYS_DIRECTORY:
        frame->rax=(uint64_t)syscall_directory(frame->rdi,frame->rsi,(void *)frame->rdx,frame->rcx);
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

/* One owner span covers pathname snapshots, namespace translation, guards,
 * backend I/O and publication. Spawn/exec retain their existing image-specific
 * scopes: wrapping their entire lifetime would defeat private-image admission.
 * This does not change syscall_io_safe or make filesystem operations I/O-safe. */
static bool namespace_syscall(uint64_t number) {
    switch(number) {
    case SYS_FILE_VERIFY: case SYS_FILE_STREAM: case SYS_OPEN: case SYS_OPEN_MODE: case SYS_LIST:
    case SYS_FS_WRITE: case SYS_FS_APPEND: case SYS_STAT: case SYS_STATFS:
    case SYS_CHMOD: case SYS_UTIME: case SYS_UNLINK: case SYS_MKDIR:
    case SYS_MKDIR_MODE: case SYS_RMDIR: case SYS_RENAME: case SYS_FILE_CHANGE:
    case SYS_PATH_CONTEXT: case SYS_DIRECTORY: return true;
    default: return false;
    }
}
void syscall_dispatch(struct isr_frame *frame) {
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    /* Unsupported extensions must not wait for namespace I/O ownership. */
    if(frame->rax==SYS_SESSION_SCOPE || frame->rax==SYS_FILE_STREAM || frame->rax==SYS_FILE_VERIFY) {
        frame->rax=(uint64_t)SRV_ERR_NOTSUP;return;
    }
#endif
    if(!namespace_syscall(frame->rax)) { syscall_dispatch_impl(frame);return; }
    uint64_t flags=irq_save();
    scheduler_io_set_user_safe(false);scheduler_io_wait();
    bool held=scheduler_io_begin();
    if(!held) frame->rax=(uint64_t)SRV_ERR_IO;
    else { syscall_dispatch_impl(frame);scheduler_io_end(); }
    irq_restore(flags);
}
