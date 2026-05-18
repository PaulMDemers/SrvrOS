#include <srvros/console.h>
#include <srvros/exfat.h>
#include <srvros/fpu.h>
#include <srvros/heap.h>
#include <srvros/net.h>
#include <srvros/pmm.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <srvros/syscall_numbers.h>
#include <srvros/timer.h>
#include <srvros/vfs.h>
#include <srvros/vmm.h>

#include <stddef.h>
#include <stdint.h>

#define EI_NIDENT 16
#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EV_CURRENT 1
#define ET_EXEC 2
#define EM_X86_64 62
#define PT_LOAD 1
#define PF_X 1
#define PF_W 2
#define PAGE_SIZE 4096ull
#define USER_STACK_TOP 0x1000000ull
#define USER_STACK_PAGES 16
#define USER_HEAP_LIMIT (USER_STACK_TOP - (USER_STACK_PAGES * PAGE_SIZE) - PAGE_SIZE)
#define USER_MMAP_BASE 0x10000000ull
#define USER_MMAP_LIMIT 0x40000000ull
#define PROCESS_MAX_MAPPINGS 4096
#define PROCESS_MAX_MMAP_REGIONS 64
#define PROCESS_NAME_MAX 64
#define PROCESS_KERNEL_STACK_SIZE 131072
#define PROCESS_KERNEL_STACK_MIN_PHYSICAL 0x100000ull
#define PROCESS_USER_PAGE_MIN_PHYSICAL 0x4000000ull
#define PROCESS_MAX_ARGS 16
#define PROCESS_ARGS_MAX 256
#define PROCESS_PATH_MAX 160
#define PROCESS_WRITE_BUFFER_MAX (1024 * 1024)
#define PROCESS_MAX_WRITE_FILES 32
#define PROCESS_MAX_READ_FILES 64
#define PROCESS_MAX_PIPES 32
#define PROCESS_MAX_FILE_LOCKS 64
#define PROCESS_PIPE_CAPACITY 4096
#define PROCESS_STDIO_CLOSED_FD (-2)
#define PROCESS_MAX_USER_THREADS 8
#define PROCESS_MAX_FUTEXES 64

struct process_context {
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rsp;
    uint64_t rip;
};

struct process_mapping {
    uint64_t virtual_address;
    uint64_t physical_address;
    uint64_t flags;
};

struct process_mmap_region {
    bool used;
    uint64_t base;
    uint64_t length;
};

struct process;

struct process_user_thread {
    bool used;
    bool active;
    bool detached;
    bool joined;
    uint64_t tid;
    uint64_t entry;
    uint64_t arg;
    uint64_t stack_top;
    uint64_t return_value;
    struct process *owner;
    struct process_context context;
    struct fpu_state fpu;
};

struct process_pipe {
    bool used;
    uint64_t refs;
    uint64_t read_refs;
    uint64_t write_refs;
    uint64_t read_pos;
    uint64_t write_pos;
    uint64_t size;
    uint8_t buffer[PROCESS_PIPE_CAPACITY];
};

struct process_write_file {
    bool used;
    uint64_t refs;
    const uint8_t *data;
    uint64_t size;
    uint64_t offset;
    uint64_t capacity;
    bool dirty;
    bool failed;
    char path[PROCESS_PATH_MAX];
};

struct process_read_file {
    bool used;
    uint64_t refs;
    const struct vfs_node *node;
    const uint8_t *data;
    uint64_t size;
    uint64_t offset;
    char path[PROCESS_PATH_MAX];
};

struct process_file_lock {
    bool used;
    uint64_t pid;
    uint64_t start;
    uint64_t end;
    int16_t type;
    char path[PROCESS_PATH_MAX];
};

struct process {
    bool active;
    bool allocated;
    bool detached;
    bool killed;
    bool exiting;
    bool reapable;
    bool quiet;
    uint64_t kill_signal;
    uint64_t process_group;
    uint64_t pid;
    uint64_t exit_status;
    uint64_t entry;
    uint64_t stack_top;
    uint64_t argc;
    uint64_t argv;
    uint64_t envp;
    uint64_t address_space;
    uint64_t heap_base;
    uint64_t program_break;
    uint64_t mapped_break;
    uint64_t heap_limit;
    uint8_t *kernel_stack;
    uint64_t kernel_stack_physical;
    uint64_t kernel_stack_frames;
    uint64_t kernel_stack_top;
    bool stdout_redirect;
    bool stdout_append;
    bool stdout_written;
    bool stdin_redirect;
    int64_t stdin_fd;
    int64_t stdout_fd;
    bool stderr_redirect;
    int64_t stderr_fd;
    char stdout_path[PROCESS_PATH_MAX];
    char name[PROCESS_NAME_MAX];
    struct process_context context;
    struct fpu_state fpu;
    struct process_file files[PROCESS_MAX_OPEN_FILES];
    struct process_mapping mappings[PROCESS_MAX_MAPPINGS];
    uint64_t mapping_count;
    struct process_mmap_region mmap_regions[PROCESS_MAX_MMAP_REGIONS];
    uint64_t mmap_next;
    struct process_user_thread user_threads[PROCESS_MAX_USER_THREADS];
    uint64_t next_thread_id;
    struct scheduler_wait_queue thread_wait_queue;
};

struct elf64_header {
    uint8_t ident[EI_NIDENT];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed));

struct elf64_program_header {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} __attribute__((packed));

struct process_futex {
    bool used;
    struct process *process;
    uint64_t address;
    struct scheduler_wait_queue wait_queue;
};

void usermode_enter(uint64_t entry, uint64_t stack_top, uint64_t argc, uint64_t argv, uint64_t envp) __attribute__((noreturn));
uint64_t process_context_save(struct process_context *context) __attribute__((returns_twice));
void process_context_restore(struct process_context *context, uint64_t value) __attribute__((noreturn));
uint64_t gdt_default_kernel_stack_top(void);
void gdt_set_kernel_stack(uint64_t stack_top);

static struct process processes[PROCESS_MAX_PROCESSES];
static struct process_write_file write_files[PROCESS_MAX_WRITE_FILES];
static struct process_read_file read_files[PROCESS_MAX_READ_FILES];
static struct process_pipe pipes[PROCESS_MAX_PIPES];
static struct process_file_lock file_locks[PROCESS_MAX_FILE_LOCKS];
static struct process_futex futexes[PROCESS_MAX_FUTEXES];
static struct process *loading_process;
static uint64_t next_pid = 1;
static struct scheduler_wait_queue process_wait_queue;
static struct scheduler_wait_queue pipe_wait_queue;
static struct scheduler_wait_queue fd_poll_wait_queue;
static struct scheduler_wait_queue file_lock_wait_queue;
static uint64_t foreground_process_group;

static void set_process_name(struct process *process, const char *path);
static struct process_read_file *read_file_at(uint64_t handle);
static struct process_write_file *write_file_at(uint64_t handle);
static void background_process_thread(void *arg);
static void process_user_thread_start(void *arg);
static void process_futex_wake_process(struct process *process);
static bool process_configure_stdio_fds(struct process *child,
    struct process *parent,
    int64_t stdin_fd,
    int64_t stdout_fd,
    int64_t stderr_fd);
static bool process_apply_stdio_fds(struct process *process,
    int64_t stdin_fd,
    int64_t stdout_fd,
    int64_t stderr_fd);

static uint64_t process_irq_save(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void process_irq_restore(uint64_t flags) {
    if ((flags & (1ull << 9)) != 0) {
        __asm__ volatile ("sti" : : : "memory");
    }
}
static int64_t process_file_dup_into(struct process *process, struct process_file *source, uint64_t target_index);
static bool process_apply_spawn_file_actions(struct process *child,
    struct process *parent,
    const struct process_spawn_file_action *actions,
    uint64_t action_count);

static struct process_user_thread *process_current_user_thread(void) {
    return (struct process_user_thread *)scheduler_current_user_thread_context();
}

static bool copy_process_path(char *destination, uint64_t capacity, const char *source) {
    if (destination == NULL || capacity == 0 || source == NULL || source[0] == '\0') {
        return false;
    }

    uint64_t i = 0;
    while (source[i] != '\0' && i + 1 < capacity) {
        destination[i] = source[i];
        i++;
    }
    if (source[i] != '\0') {
        return false;
    }
    destination[i] = '\0';
    return true;
}

static bool path_under_prefix(const char *path, const char *prefix) {
    if (path == NULL || prefix == NULL || prefix[0] == '\0') {
        return false;
    }

    uint64_t i = 0;
    while (prefix[i] != '\0') {
        if (path[i] != prefix[i]) {
            return false;
        }
        i++;
    }
    return path[i] == '\0' || path[i] == '/';
}

static bool process_path_equal(const char *a, const char *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    uint64_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return false;
        }
        i++;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static void clear_process(struct process *process) {
    uint8_t *bytes = (uint8_t *)process;
    for (uint64_t i = 0; i < sizeof(*process); i++) {
        bytes[i] = 0;
    }
}

static struct process *alloc_process(const char *path, bool detached) {
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        if (!processes[i].allocated) {
            clear_process(&processes[i]);
            processes[i].allocated = true;
            processes[i].active = true;
            processes[i].detached = detached;
            processes[i].pid = next_pid++;
            if (next_pid == 0) {
                next_pid = 1;
            }
            processes[i].stack_top = USER_STACK_TOP;
            processes[i].kernel_stack_top = gdt_default_kernel_stack_top();
            processes[i].next_thread_id = 2;
            fpu_init_state(&processes[i].fpu);
            set_process_name(&processes[i], path);
            return &processes[i];
        }
    }

    return NULL;
}

static void release_process(struct process *process) {
    if (process == NULL) {
        return;
    }

    if (process->kernel_stack_physical != 0 && process->kernel_stack_frames != 0) {
        pmm_free_frame_range(process->kernel_stack_physical, process->kernel_stack_frames);
        process->kernel_stack_physical = 0;
        process->kernel_stack_frames = 0;
        process->kernel_stack = NULL;
    }

    clear_process(process);
}

static void release_process_runtime(struct process *process) {
    if (process == NULL) {
        return;
    }

    if (process->kernel_stack_physical != 0 && process->kernel_stack_frames != 0) {
        pmm_free_frame_range(process->kernel_stack_physical, process->kernel_stack_frames);
        process->kernel_stack_physical = 0;
        process->kernel_stack_frames = 0;
        process->kernel_stack = NULL;
    }
}

static bool allocate_kernel_stack(struct process *process) {
    if (process == NULL) {
        return false;
    }

    process->kernel_stack_frames = PROCESS_KERNEL_STACK_SIZE / PAGE_SIZE;
    process->kernel_stack_physical = pmm_alloc_frames_above_tagged(
        process->kernel_stack_frames,
        PROCESS_KERNEL_STACK_MIN_PHYSICAL,
        PMM_FRAME_KERNEL_STACK);
    if (process->kernel_stack_physical == 0) {
        process->kernel_stack_frames = 0;
        return false;
    }

    process->kernel_stack = pmm_phys_to_virt(process->kernel_stack_physical);
    process->kernel_stack_top = ((uint64_t)process->kernel_stack + PROCESS_KERNEL_STACK_SIZE) & ~0xfull;
    return true;
}

static void set_process_name(struct process *process, const char *path) {
    uint64_t i = 0;
    if (path == NULL) {
        process->name[0] = '\0';
        return;
    }

    for (; i + 1 < PROCESS_NAME_MAX && path[i] != '\0'; i++) {
        process->name[i] = path[i];
    }
    process->name[i] = '\0';
}

static uint64_t page_down(uint64_t value) {
    return value & ~(PAGE_SIZE - 1);
}

static uint64_t page_up(uint64_t value) {
    return (value + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static void zero_page(void *page) {
    uint8_t *bytes = page;
    for (uint64_t i = 0; i < PAGE_SIZE; i++) {
        bytes[i] = 0;
    }
}

static void copy_bytes(uint8_t *destination, const uint8_t *source, uint64_t length) {
    for (uint64_t i = 0; i < length; i++) {
        destination[i] = source[i];
    }
}

static bool user_space_write(struct process *process, uint64_t virtual_address,
    const void *source, uint64_t length) {
    const uint8_t *bytes = source;
    for (uint64_t copied = 0; copied < length;) {
        uint64_t physical_address;
        if (!vmm_virt_to_phys_in_space(process->address_space,
                virtual_address + copied,
                &physical_address)) {
            return false;
        }

        uint64_t page_offset = (virtual_address + copied) & (PAGE_SIZE - 1);
        uint64_t chunk = PAGE_SIZE - page_offset;
        if (chunk > length - copied) {
            chunk = length - copied;
        }

        copy_bytes((uint8_t *)pmm_phys_to_virt(physical_address - page_offset) + page_offset,
            bytes + copied,
            chunk);
        copied += chunk;
    }
    return true;
}

static uint64_t local_strlen_limit(const char *text, uint64_t capacity) {
    uint64_t length = 0;
    if (text == NULL) {
        return 0;
    }
    while (length < capacity && text[length] != '\0') {
        length++;
    }
    return length;
}

static uint64_t parse_argument(char *args, uint64_t cursor, char **value_out) {
    char quote = '\0';
    uint64_t out = cursor;

    *value_out = &args[cursor];
    while (args[cursor] != '\0') {
        char c = args[cursor++];
        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
                continue;
            }
            if (c == '\\' && args[cursor] != '\0') {
                c = args[cursor++];
            }
            args[out++] = c;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '\\' && args[cursor] != '\0') {
            args[out++] = args[cursor++];
            continue;
        }
        if (c == ' ' || c == '\t') {
            break;
        }
        args[out++] = c;
    }
    args[out] = '\0';
    return cursor;
}

static bool setup_process_arguments(struct process *process, const char *path, const char *args) {
    uint64_t argc = 0;
    uint64_t argv[PROCESS_MAX_ARGS + 1];
    const char *values[PROCESS_MAX_ARGS];
    uint64_t lengths[PROCESS_MAX_ARGS];
    char args_copy[PROCESS_ARGS_MAX];
    uint64_t args_length = local_strlen_limit(args, sizeof(args_copy) - 1);
    uint64_t cursor;
    uint64_t stack_cursor;

    if (process == NULL || process->address_space == 0 || path == NULL) {
        return false;
    }

    values[argc] = path;
    lengths[argc++] = local_strlen_limit(path, PROCESS_PATH_MAX - 1);

    for (uint64_t i = 0; i < args_length; i++) {
        args_copy[i] = args[i];
    }
    args_copy[args_length] = '\0';

    cursor = 0;
    while (args_copy[cursor] != '\0' && argc < PROCESS_MAX_ARGS) {
        while (args_copy[cursor] == ' ' || args_copy[cursor] == '\t') {
            cursor++;
        }
        if (args_copy[cursor] == '\0') {
            break;
        }

        char *value;
        cursor = parse_argument(args_copy, cursor, &value);
        values[argc] = value;
        lengths[argc] = local_strlen_limit(values[argc], PROCESS_ARGS_MAX);
        argc++;
    }

    stack_cursor = process->stack_top;
    for (uint64_t i = argc; i > 0; i--) {
        uint64_t index = i - 1;
        stack_cursor -= lengths[index] + 1;
        if (stack_cursor < process->stack_top - USER_STACK_PAGES * PAGE_SIZE) {
            return false;
        }
        if (!user_space_write(process, stack_cursor, values[index], lengths[index] + 1)) {
            return false;
        }
        argv[index] = stack_cursor;
    }

    stack_cursor &= ~0xfull;
    stack_cursor -= sizeof(uint64_t) * (argc + 1);
    if (stack_cursor < process->stack_top - USER_STACK_PAGES * PAGE_SIZE) {
        return false;
    }
    argv[argc] = 0;
    if (!user_space_write(process, stack_cursor, argv, sizeof(uint64_t) * (argc + 1))) {
        return false;
    }

    process->argc = argc;
    process->argv = stack_cursor;
    process->stack_top = stack_cursor & ~0xfull;
    return true;
}

static bool setup_process_vectors(struct process *process,
    uint64_t argc,
    const char *const *argv_values,
    uint64_t envc,
    const char *const *env_values) {
    uint64_t argv[PROCESS_MAX_ARGS + 1];
    uint64_t envp[PROCESS_MAX_ARGS + 1];
    uint64_t argv_lengths[PROCESS_MAX_ARGS];
    uint64_t env_lengths[PROCESS_MAX_ARGS];
    uint64_t stack_cursor;

    if (process == NULL || process->address_space == 0 || argv_values == NULL) {
        return false;
    }
    if (argc == 0 || argc > PROCESS_MAX_ARGS || envc > PROCESS_MAX_ARGS) {
        return false;
    }

    for (uint64_t i = 0; i < argc; i++) {
        if (argv_values[i] == NULL) {
            return false;
        }
        argv_lengths[i] = local_strlen_limit(argv_values[i], PROCESS_ARGS_MAX);
    }
    for (uint64_t i = 0; i < envc; i++) {
        if (env_values == NULL || env_values[i] == NULL) {
            return false;
        }
        env_lengths[i] = local_strlen_limit(env_values[i], PROCESS_ARGS_MAX);
    }

    stack_cursor = process->stack_top;
    for (uint64_t i = envc; i > 0; i--) {
        uint64_t index = i - 1;
        stack_cursor -= env_lengths[index] + 1;
        if (stack_cursor < process->stack_top - USER_STACK_PAGES * PAGE_SIZE) {
            return false;
        }
        if (!user_space_write(process, stack_cursor, env_values[index], env_lengths[index] + 1)) {
            return false;
        }
        envp[index] = stack_cursor;
    }
    for (uint64_t i = argc; i > 0; i--) {
        uint64_t index = i - 1;
        stack_cursor -= argv_lengths[index] + 1;
        if (stack_cursor < process->stack_top - USER_STACK_PAGES * PAGE_SIZE) {
            return false;
        }
        if (!user_space_write(process, stack_cursor, argv_values[index], argv_lengths[index] + 1)) {
            return false;
        }
        argv[index] = stack_cursor;
    }

    stack_cursor &= ~0xfull;
    stack_cursor -= sizeof(uint64_t) * (envc + 1);
    if (stack_cursor < process->stack_top - USER_STACK_PAGES * PAGE_SIZE) {
        return false;
    }
    envp[envc] = 0;
    if (!user_space_write(process, stack_cursor, envp, sizeof(uint64_t) * (envc + 1))) {
        return false;
    }
    process->envp = stack_cursor;

    stack_cursor &= ~0xfull;
    stack_cursor -= sizeof(uint64_t) * (argc + 1);
    if (stack_cursor < process->stack_top - USER_STACK_PAGES * PAGE_SIZE) {
        return false;
    }
    argv[argc] = 0;
    if (!user_space_write(process, stack_cursor, argv, sizeof(uint64_t) * (argc + 1))) {
        return false;
    }
    process->argc = argc;
    process->argv = stack_cursor;
    process->stack_top = stack_cursor & ~0xfull;
    return true;
}

static bool validate_elf(const uint8_t *data, uint64_t size, const struct elf64_header **header) {
    if (data == NULL || size < sizeof(struct elf64_header)) {
        return false;
    }

    const struct elf64_header *candidate = (const struct elf64_header *)data;
    if (candidate->ident[0] != ELFMAG0 ||
        candidate->ident[1] != ELFMAG1 ||
        candidate->ident[2] != ELFMAG2 ||
        candidate->ident[3] != ELFMAG3 ||
        candidate->ident[4] != ELFCLASS64 ||
        candidate->ident[5] != ELFDATA2LSB ||
        candidate->ident[6] != EV_CURRENT ||
        candidate->version != EV_CURRENT ||
        candidate->type != ET_EXEC ||
        candidate->machine != EM_X86_64 ||
        candidate->phentsize != sizeof(struct elf64_program_header)) {
        return false;
    }

    if (candidate->phoff > size ||
        (candidate->phnum != 0 && candidate->phentsize > (size - candidate->phoff) / candidate->phnum)) {
        return false;
    }

    *header = candidate;
    return true;
}

static bool map_process_page(struct process *process, uint64_t virtual_page, uint64_t flags) {
    if (process == NULL || process->mapping_count >= PROCESS_MAX_MAPPINGS) {
        console_printf("run: map page rejected process=%x count=%u limit=%u virt=%x\n",
            (uint64_t)process,
            process != NULL ? process->mapping_count : 0,
            (uint64_t)PROCESS_MAX_MAPPINGS,
            virtual_page);
        return false;
    }

    uint64_t physical = pmm_alloc_frames_above_tagged(1,
        PROCESS_USER_PAGE_MIN_PHYSICAL,
        PMM_FRAME_USER);
    if (physical == 0) {
        console_printf("run: map page pmm exhausted virt=%x\n", virtual_page);
        return false;
    }

    zero_page(pmm_phys_to_virt(physical));
    if (!vmm_map_page_in_space(process->address_space, virtual_page, physical, flags)) {
        console_printf("run: map page failed as=%x virt=%x phys=%x flags=%x count=%u\n",
            process->address_space,
            virtual_page,
            physical,
            flags,
            process->mapping_count);
        pmm_free_frame(physical);
        return false;
    }

    process->mappings[process->mapping_count++] = (struct process_mapping) {
        .virtual_address = virtual_page,
        .physical_address = physical,
        .flags = flags,
    };

    return true;
}

static bool map_segment_page(uint64_t virtual_page, uint64_t flags) {
    return map_process_page(loading_process, virtual_page, flags);
}

static int64_t process_mapping_index(struct process *process, uint64_t virtual_page) {
    if (process == NULL) {
        return -1;
    }
    for (uint64_t i = 0; i < process->mapping_count; i++) {
        if (process->mappings[i].virtual_address == virtual_page) {
            return (int64_t)i;
        }
    }
    return -1;
}

static bool process_unmap_tracked_page(struct process *process, uint64_t virtual_page) {
    int64_t index = process_mapping_index(process, virtual_page);
    if (index < 0) {
        return false;
    }

    uint64_t physical = 0;
    if (vmm_unmap_page_in_space(process->address_space, virtual_page, &physical)) {
        pmm_free_frame(physical);
    } else if (process->mappings[index].physical_address != 0) {
        pmm_free_frame(process->mappings[index].physical_address);
    }

    uint64_t unsigned_index = (uint64_t)index;
    process->mappings[unsigned_index] = process->mappings[--process->mapping_count];
    return true;
}

static void cleanup_process_mappings(struct process *process) {
    if (process == NULL) {
        return;
    }

    for (uint64_t i = 0; i < process->mapping_count; i++) {
        uint64_t physical = 0;
        if (vmm_unmap_page_in_space(process->address_space,
                process->mappings[i].virtual_address,
                &physical)) {
            pmm_free_frame(physical);
        } else if (process->mappings[i].physical_address != 0) {
            pmm_free_frame(process->mappings[i].physical_address);
        }
    }
    process->mapping_count = 0;
    for (uint64_t i = 0; i < PROCESS_MAX_MMAP_REGIONS; i++) {
        process->mmap_regions[i].used = false;
    }
    process->mmap_next = USER_MMAP_BASE;
}

void process_refresh_mappings(struct process *process) {
    if (process == NULL || process->address_space == 0) {
        return;
    }

    vmm_refresh_kernel_mappings(process->address_space);
    for (uint64_t i = 0; i < process->mapping_count; i++) {
        if (process->mappings[i].physical_address == 0) {
            continue;
        }
        (void)vmm_ensure_page_in_space(process->address_space,
            process->mappings[i].virtual_address,
            process->mappings[i].physical_address,
            process->mappings[i].flags);
    }
}

static void cleanup_process_files(struct process *process) {
    if (process == NULL) {
        return;
    }

    for (uint64_t i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        if (!process->files[i].used) {
            continue;
        }
        (void)process_file_close(process, i + 3);
    }
}

static bool file_is_exec_stdio_redirect(struct process *process, uint64_t fd) {
    return process != NULL &&
        ((process->stdin_redirect && process->stdin_fd == (int64_t)fd) ||
            (process->stdout_redirect && process->stdout_fd == (int64_t)fd) ||
            (process->stderr_redirect && process->stderr_fd == (int64_t)fd));
}

static void close_cloexec_files(struct process *process) {
    if (process == NULL) {
        return;
    }
    for (uint64_t i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        uint64_t fd = i + 3;
        if (process->files[i].used &&
            (process->files[i].flags & SRV_FD_CLOEXEC) != 0 &&
            !file_is_exec_stdio_redirect(process, fd)) {
            (void)process_file_close(process, fd);
        }
    }
}

static bool load_segment(const uint8_t *file, uint64_t file_size, const struct elf64_program_header *program) {
    if (program->memsz < program->filesz ||
        program->offset > file_size ||
        program->filesz > file_size - program->offset) {
        console_printf("run: bad segment off=%x filesz=%x memsz=%x file=%x\n",
            program->offset,
            program->filesz,
            program->memsz,
            file_size);
        return false;
    }

    uint64_t start = page_down(program->vaddr);
    uint64_t end = page_up(program->vaddr + program->memsz);
    uint64_t flags = VMM_PAGE_USER;
    if ((program->flags & PF_W) != 0) {
        flags |= VMM_PAGE_WRITABLE;
    }
    if ((program->flags & PF_X) == 0) {
        flags |= VMM_PAGE_NO_EXECUTE;
    }

    for (uint64_t page = start; page < end; page += PAGE_SIZE) {
        if (!map_segment_page(page, flags)) {
            return false;
        }
    }

    for (uint64_t copied = 0; copied < program->filesz;) {
        uint64_t virtual_address = program->vaddr + copied;
        uint64_t physical_address;
        if (!vmm_virt_to_phys_in_space(loading_process->address_space, virtual_address, &physical_address)) {
            return false;
        }

        uint64_t page_offset = virtual_address & (PAGE_SIZE - 1);
        uint64_t chunk = PAGE_SIZE - page_offset;
        if (chunk > program->filesz - copied) {
            chunk = program->filesz - copied;
        }

        copy_bytes((uint8_t *)pmm_phys_to_virt(physical_address - page_offset) + page_offset,
            file + program->offset + copied,
            chunk);
        copied += chunk;
    }

    return true;
}

static bool map_user_stack(uint64_t stack_top) {
    uint64_t bottom = stack_top - (USER_STACK_PAGES * PAGE_SIZE);
    for (uint64_t page = bottom; page < stack_top; page += PAGE_SIZE) {
        if (!map_segment_page(page, VMM_PAGE_USER | VMM_PAGE_WRITABLE | VMM_PAGE_NO_EXECUTE)) {
            return false;
        }
    }
    return true;
}

static bool load_elf_image(const char *path, struct process *process, uint64_t stack_top, uint64_t *entry_out) {
    if (process == NULL || process->address_space != 0) {
        return false;
    }
    if (!vmm_create_address_space(&process->address_space)) {
        console_write("run: failed to create address space\n");
        return false;
    }

    loading_process = process;

    const struct vfs_node *node = vfs_lookup(path);
    const uint8_t *data;
    uint64_t size;
    const struct elf64_header *header;

    if (node == NULL || !vfs_read_all(node, &data, &size)) {
        console_printf("run: not found: %s\n", path);
        vmm_destroy_address_space(process->address_space);
        process->address_space = 0;
        loading_process = NULL;
        return false;
    }

    if (!validate_elf(data, size, &header)) {
        console_printf("run: invalid executable: %s\n", path);
        vfs_release_data(node, data);
        vmm_destroy_address_space(process->address_space);
        process->address_space = 0;
        loading_process = NULL;
        return false;
    }

    const struct elf64_program_header *programs =
        (const struct elf64_program_header *)(data + header->phoff);

    uint64_t image_end = 0;
    for (uint64_t i = 0; i < header->phnum; i++) {
        if (programs[i].type == PT_LOAD) {
            uint64_t end = programs[i].vaddr + programs[i].memsz;
            if (end > image_end) {
                image_end = end;
            }
        }
    }

    for (uint64_t i = 0; i < header->phnum; i++) {
        if (programs[i].type == PT_LOAD && !load_segment(data, size, &programs[i])) {
            console_printf("run: failed to load segment %u\n", i);
            vfs_release_data(node, data);
            cleanup_process_mappings(process);
            vmm_destroy_address_space(process->address_space);
            process->address_space = 0;
            loading_process = NULL;
            return false;
        }
    }

    if (!map_user_stack(stack_top)) {
        console_write("run: failed to map user stack\n");
        vfs_release_data(node, data);
        cleanup_process_mappings(process);
        vmm_destroy_address_space(process->address_space);
        process->address_space = 0;
        loading_process = NULL;
        return false;
    }

    *entry_out = header->entry;
    process->entry = header->entry;
    process->stack_top = stack_top;
    process->heap_base = page_up(image_end);
    process->program_break = process->heap_base;
    process->mapped_break = process->heap_base;
    process->heap_limit = USER_HEAP_LIMIT;
    loading_process = NULL;
    vfs_release_data(node, data);
    return true;
}

static const char *process_state(const struct process *process) {
    if (process == NULL || !process->allocated) {
        return "unused";
    }
    if (process->active) {
        if (process->killed) {
            return "stopping";
        }
        return process->detached ? "background" : "foreground";
    }
    if (process->reapable) {
        return "exited";
    }
    return "exited";
}

static void cleanup_process_address_space(struct process *process) {
    if (process == NULL) {
        return;
    }

    cleanup_process_mappings(process);
    if (process->address_space != 0) {
        vmm_destroy_address_space(process->address_space);
        process->address_space = 0;
    }
}

static bool enter_process(struct process *process, const char *label) {
    uint64_t restored = process_context_save(&process->context);
    if (restored == 0) {
        scheduler_set_user_context(process, process->address_space, process->kernel_stack_top);
        if (!process->quiet) {
            console_printf("%s: entering pid=%u %s entry=%x stack=%x\n",
                label,
                process->pid,
                process->name,
                process->entry,
                process->stack_top);
        }
        usermode_enter(process->entry, process->stack_top, process->argc, process->argv, process->envp);
    }

    scheduler_clear_user_context();
    __asm__ volatile ("sti" : : : "memory");
    return process->exit_status == 0;
}

bool process_run_elf(const char *path) {
    return process_run_elf_args(path, "");
}

bool process_run_elf_args(const char *path, const char *args) {
    if (path == NULL || path[0] == '\0') {
        return false;
    }

    struct process *process = alloc_process(path, false);
    if (process == NULL) {
        console_write("run: process table full\n");
        return false;
    }

    if (!allocate_kernel_stack(process)) {
        release_process(process);
        return false;
    }

    if (!load_elf_image(path, process, USER_STACK_TOP, &process->entry) ||
        !setup_process_arguments(process, path, args != NULL ? args : "")) {
        cleanup_process_address_space(process);
        release_process(process);
        return false;
    }

    bool ok = enter_process(process, "run");
    console_printf("run: pid=%u exited status=%u\n", process->pid, process->exit_status);
    release_process(process);
    return ok;
}

int64_t process_spawn_elf(const char *path) {
    return process_spawn_elf_args(path, "");
}

int64_t process_spawn_elf_args(const char *path, const char *args) {
    struct process *parent = process_current();
    if (parent == NULL || path == NULL || path[0] == '\0') {
        return -1;
    }

    struct process *child = alloc_process(path, false);
    if (child == NULL) {
        return -1;
    }
    if (!allocate_kernel_stack(child)) {
        release_process(child);
        return -1;
    }

    if (!load_elf_image(path, child, USER_STACK_TOP, &child->entry) ||
        !setup_process_arguments(child, path, args)) {
        cleanup_process_address_space(child);
        release_process(child);
        scheduler_set_user_context(parent, parent->address_space, parent->kernel_stack_top);
        return -1;
    }

    uint64_t restored = process_context_save(&child->context);
    if (restored == 0) {
        scheduler_set_user_context(child, child->address_space, child->kernel_stack_top);
        console_printf("spawn: entering pid=%u %s entry=%x stack=%x\n",
            child->pid,
            path,
            child->entry,
            child->stack_top);
        usermode_enter(child->entry, child->stack_top, child->argc, child->argv, child->envp);
    }

    int64_t status = (int64_t)child->exit_status;
    scheduler_set_user_context(parent, parent->address_space, parent->kernel_stack_top);
    __asm__ volatile ("sti" : : : "memory");
    release_process(child);
    return status;
}

int64_t process_spawn_elf_args_redirect(const char *path,
    const char *args,
    const char *stdout_path,
    bool append) {
    struct process *parent = process_current();
    if (parent == NULL || path == NULL) {
        return -1;
    }

    struct process *child = alloc_process(path, false);
    if (child == NULL) {
        return -1;
    }

    if (!allocate_kernel_stack(child)) {
        release_process(child);
        return -1;
    }

    if (!load_elf_image(path, child, USER_STACK_TOP, &child->entry) ||
        !setup_process_arguments(child, path, args) ||
        !process_set_stdout_redirect(child, stdout_path, append)) {
        cleanup_process_address_space(child);
        release_process(child);
        scheduler_set_user_context(parent, parent->address_space, parent->kernel_stack_top);
        return -1;
    }

    uint64_t restored = process_context_save(&child->context);
    if (restored == 0) {
        scheduler_set_user_context(child, child->address_space, child->kernel_stack_top);
        console_printf("spawn: entering pid=%u %s entry=%x stack=%x\n",
            child->pid,
            path,
            child->entry,
            child->stack_top);
        usermode_enter(child->entry, child->stack_top, child->argc, child->argv, child->envp);
    }

    int64_t status = (int64_t)child->exit_status;
    scheduler_set_user_context(parent, parent->address_space, parent->kernel_stack_top);
    __asm__ volatile ("sti" : : : "memory");
    release_process(child);
    return status;
}

int64_t process_spawn_exec(const char *path,
    uint64_t argc,
    const char *const *argv,
    uint64_t envc,
    const char *const *envp,
    bool detached,
    int64_t stdin_fd,
    int64_t stdout_fd,
    int64_t stderr_fd,
    uint64_t process_group,
    bool foreground,
    const struct process_spawn_file_action *file_actions,
    uint64_t file_action_count) {
    struct process *parent = process_current();
    if (parent == NULL || path == NULL || path[0] == '\0') {
        return -1;
    }

    struct process *child = alloc_process(path, detached);
    if (child == NULL) {
        console_write("exec: process table full\n");
        return -1;
    }
    if (!allocate_kernel_stack(child)) {
        console_write("exec: kernel stack alloc failed\n");
        release_process(child);
        return -1;
    }

    if (!load_elf_image(path, child, USER_STACK_TOP, &child->entry) ||
        !setup_process_vectors(child, argc, argv, envc, envp) ||
        !process_configure_stdio_fds(child, parent, stdin_fd, stdout_fd, stderr_fd) ||
        !process_apply_spawn_file_actions(child, parent, file_actions, file_action_count)) {
        cleanup_process_address_space(child);
        cleanup_process_files(child);
        release_process(child);
        scheduler_set_user_context(parent, parent->address_space, parent->kernel_stack_top);
        return -1;
    }

    if (process_group == UINT64_MAX) {
        child->process_group = child->pid;
    } else {
        child->process_group = process_group;
    }
    if (foreground && child->process_group != 0) {
        foreground_process_group = child->process_group;
    }

    if (detached) {
        uint64_t pid = child->pid;
        child->quiet = true;
        if (!scheduler_spawn("user", background_process_thread, child)) {
            console_write("exec: scheduler thread table full\n");
            cleanup_process_address_space(child);
            cleanup_process_files(child);
            release_process(child);
            return -1;
        }
        return (int64_t)pid;
    }

    uint64_t restored = process_context_save(&child->context);
    if (restored == 0) {
        scheduler_set_user_context(child, child->address_space, child->kernel_stack_top);
        console_printf("spawn: entering pid=%u %s entry=%x stack=%x\n",
            child->pid,
            path,
            child->entry,
            child->stack_top);
        usermode_enter(child->entry, child->stack_top, child->argc, child->argv, child->envp);
    }

    int64_t status = (int64_t)child->exit_status;
    scheduler_set_user_context(parent, parent->address_space, parent->kernel_stack_top);
    __asm__ volatile ("sti" : : : "memory");
    release_process(child);
    return status;
}

static uint64_t load_process_u64(const uint64_t *value) {
    const volatile uint64_t *source = value;
    return *source;
}

static void move_process_image(struct process *destination, struct process *source) {
    destination->entry = load_process_u64(&source->entry);
    destination->stack_top = load_process_u64(&source->stack_top);
    destination->argc = load_process_u64(&source->argc);
    destination->argv = load_process_u64(&source->argv);
    destination->envp = load_process_u64(&source->envp);
    destination->address_space = load_process_u64(&source->address_space);
    destination->heap_base = load_process_u64(&source->heap_base);
    destination->program_break = load_process_u64(&source->program_break);
    destination->mapped_break = load_process_u64(&source->mapped_break);
    destination->heap_limit = load_process_u64(&source->heap_limit);
    destination->mapping_count = load_process_u64(&source->mapping_count);
    for (uint64_t i = 0; i < source->mapping_count; i++) {
        destination->mappings[i].virtual_address = load_process_u64(&source->mappings[i].virtual_address);
        destination->mappings[i].physical_address = load_process_u64(&source->mappings[i].physical_address);
        destination->mappings[i].flags = load_process_u64(&source->mappings[i].flags);
    }

    source->address_space = 0;
    source->mapping_count = 0;
}

int64_t process_exec_replace(const char *path,
    uint64_t argc,
    const char *const *argv,
    uint64_t envc,
    const char *const *envp,
    int64_t stdin_fd,
    int64_t stdout_fd,
    int64_t stderr_fd) {
    struct process *process = process_current();
    if (process == NULL || path == NULL || path[0] == '\0') {
        return -1;
    }

    struct process *image = kcalloc(1, sizeof(*image));
    if (image == NULL) {
        return -1;
    }
    image->stack_top = USER_STACK_TOP;

    if (!load_elf_image(path, image, USER_STACK_TOP, &image->entry) ||
        !setup_process_vectors(image, argc, argv, envc, envp)) {
        cleanup_process_address_space(image);
        kfree(image);
        return -1;
    }
    if (!process_apply_stdio_fds(process, stdin_fd, stdout_fd, stderr_fd)) {
        cleanup_process_address_space(image);
        kfree(image);
        return -1;
    }

    scheduler_clear_user_context();
    cleanup_process_address_space(process);
    move_process_image(process, image);
    set_process_name(process, path);
    fpu_init_state(&process->fpu);
    kfree(image);

    close_cloexec_files(process);

    scheduler_set_user_context(process, process->address_space, process->kernel_stack_top);
    console_printf("exec: entering pid=%u %s entry=%x stack=%x\n",
        process->pid,
        path,
        process->entry,
        process->stack_top);
    usermode_enter(process->entry, process->stack_top, process->argc, process->argv, process->envp);
}

static void background_process_thread(void *arg) {
    struct process *process = arg;
    if (process == NULL) {
        return;
    }

    (void)enter_process(process, "bg");
    if (!process->quiet) {
        console_printf("bg: pid=%u exited status=%u\n", process->pid, process->exit_status);
    }
    release_process_runtime(process);
    process->reapable = true;
    scheduler_wake_all(&process_wait_queue);
}

bool process_start_background(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return false;
    }

    struct process *process = alloc_process(path, true);
    if (process == NULL) {
        console_write("bg: process table full\n");
        return false;
    }

    if (!allocate_kernel_stack(process)) {
        release_process(process);
        return false;
    }

    if (!load_elf_image(path, process, USER_STACK_TOP, &process->entry) ||
        !setup_process_arguments(process, path, "")) {
        cleanup_process_address_space(process);
        release_process(process);
        return false;
    }

    if (!scheduler_spawn("user", background_process_thread, process)) {
        cleanup_process_address_space(process);
        release_process(process);
        return false;
    }

    console_printf("bg: started pid=%u %s\n", process->pid, path);
    return true;
}

static bool process_fd_available(struct process *process, int64_t fd) {
    return fd < 3 || process_file_at(process, (uint64_t)fd) != NULL;
}

static bool process_apply_stdio_fds(struct process *process,
    int64_t stdin_fd,
    int64_t stdout_fd,
    int64_t stderr_fd) {
    if (process == NULL) {
        return false;
    }
    if (stdin_fd == PROCESS_STDIO_CLOSED_FD) {
        process->stdin_redirect = true;
        process->stdin_fd = -1;
    } else if (stdin_fd >= 0 && stdin_fd != 0) {
        if (!process_fd_available(process, stdin_fd)) {
            return false;
        }
        process->stdin_redirect = true;
        process->stdin_fd = stdin_fd;
    }
    if (stdout_fd == PROCESS_STDIO_CLOSED_FD) {
        process->stdout_redirect = true;
        process->stdout_fd = -1;
    } else if (stdout_fd >= 0 && stdout_fd != 1) {
        if (!process_fd_available(process, stdout_fd)) {
            return false;
        }
        process->stdout_redirect = true;
        process->stdout_fd = stdout_fd;
    }
    if (stderr_fd == PROCESS_STDIO_CLOSED_FD) {
        process->stderr_redirect = true;
        process->stderr_fd = -1;
    } else if (stderr_fd >= 0 && stderr_fd != 2) {
        if (!process_fd_available(process, stderr_fd)) {
            return false;
        }
        process->stderr_redirect = true;
        process->stderr_fd = stderr_fd;
    }
    return true;
}

static int64_t process_clone_fd_from(struct process *target, struct process *source_process, int64_t source_fd) {
    struct process_file stdio_source;
    struct process_file *source = NULL;
    if (target == NULL || source_process == NULL || source_fd < 0) {
        return -1;
    }

    if (source_fd < 3) {
        if (source_fd == 0 && source_process->stdin_redirect) {
            source_fd = source_process->stdin_fd;
        } else if (source_fd == 1 && source_process->stdout_redirect) {
            source_fd = source_process->stdout_fd;
        } else if (source_fd == 2 && source_process->stderr_redirect) {
            source_fd = source_process->stderr_fd;
        }
    }
    if (source_fd < 0) {
        return -1;
    }

    if (source_fd < 3) {
        uint8_t *bytes = (uint8_t *)&stdio_source;
        for (uint64_t i = 0; i < sizeof(stdio_source); i++) {
            bytes[i] = 0;
        }
        stdio_source.used = true;
        stdio_source.type = PROCESS_FILE_STDIO;
        stdio_source.handle = (uint64_t)source_fd;
        source = &stdio_source;
    } else {
        source = process_file_at(source_process, (uint64_t)source_fd);
    }
    if (source == NULL) {
        return -1;
    }

    for (uint64_t i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        if (!target->files[i].used) {
            return process_file_dup_into(target, source, i);
        }
    }
    return -1;
}

static struct process_file *process_fd_source(struct process *process,
    int64_t fd,
    struct process_file *stdio_source,
    bool resolve_stdio_redirect) {
    if (process == NULL || fd < 0) {
        return NULL;
    }
    if (resolve_stdio_redirect && fd < 3) {
        if (fd == 0 && process->stdin_redirect) {
            fd = process->stdin_fd;
        } else if (fd == 1 && process->stdout_redirect) {
            fd = process->stdout_fd;
        } else if (fd == 2 && process->stderr_redirect) {
            fd = process->stderr_fd;
        }
    }
    if (fd < 0) {
        return NULL;
    }
    if (fd < 3) {
        uint8_t *bytes = (uint8_t *)stdio_source;
        for (uint64_t i = 0; i < sizeof(*stdio_source); i++) {
            bytes[i] = 0;
        }
        stdio_source->used = true;
        stdio_source->type = PROCESS_FILE_STDIO;
        stdio_source->handle = (uint64_t)fd;
        return stdio_source;
    }
    return process_file_at(process, (uint64_t)fd);
}

static bool process_spawn_dup2_action(struct process *child,
    struct process *parent,
    int64_t old_fd,
    int64_t new_fd) {
    if (child == NULL ||
        parent == NULL ||
        old_fd < 0 ||
        new_fd < 3 ||
        new_fd >= 3 + PROCESS_MAX_OPEN_FILES) {
        return false;
    }
    if (old_fd == new_fd && process_file_at(child, (uint64_t)new_fd) != NULL) {
        return true;
    }

    struct process_file parent_stdio;
    struct process_file *source = old_fd >= 3 ? process_file_at(child, (uint64_t)old_fd) : NULL;
    if (source == NULL) {
        source = process_fd_source(parent, old_fd, &parent_stdio, true);
    }
    if (source == NULL) {
        return false;
    }

    struct process_file source_copy = *source;
    uint64_t target_index = (uint64_t)new_fd - 3;
    if (child->files[target_index].used && process_file_close(child, (uint64_t)new_fd) < 0) {
        return false;
    }
    return process_file_dup_into(child, &source_copy, target_index) == new_fd;
}

static bool process_apply_spawn_file_actions(struct process *child,
    struct process *parent,
    const struct process_spawn_file_action *actions,
    uint64_t action_count) {
    if (action_count == 0) {
        return true;
    }
    if (child == NULL ||
        parent == NULL ||
        actions == NULL ||
        action_count > SRV_SPAWN_FILE_ACTION_MAX) {
        return false;
    }
    for (uint64_t i = 0; i < action_count; i++) {
        const struct process_spawn_file_action *action = &actions[i];
        if (action->type == SRV_SPAWN_FILE_ACTION_CLOSE) {
            if (action->fd >= 3 && action->fd < 3 + PROCESS_MAX_OPEN_FILES &&
                process_file_at(child, (uint64_t)action->fd) != NULL &&
                process_file_close(child, (uint64_t)action->fd) < 0) {
                return false;
            }
        } else if (action->type == SRV_SPAWN_FILE_ACTION_DUP2) {
            if (!process_spawn_dup2_action(child, parent, action->fd, action->new_fd)) {
                return false;
            }
        } else {
            return false;
        }
    }
    return true;
}

static bool process_inherit_stdio_redirect(struct process *child,
    struct process *parent,
    bool parent_redirect,
    int64_t parent_fd,
    bool *child_redirect,
    int64_t *child_fd) {
    if (!parent_redirect) {
        return true;
    }
    if (parent_fd < 0) {
        *child_redirect = true;
        *child_fd = -1;
        return true;
    }
    int64_t fd = process_clone_fd_from(child, parent, parent_fd);
    if (fd < 0) {
        return false;
    }
    *child_redirect = true;
    *child_fd = fd;
    return true;
}

static bool process_configure_stdio_fds(struct process *child,
    struct process *parent,
    int64_t stdin_fd,
    int64_t stdout_fd,
    int64_t stderr_fd) {
    if (stdin_fd == PROCESS_STDIO_CLOSED_FD) {
        child->stdin_redirect = true;
        child->stdin_fd = -1;
    } else if (stdin_fd >= 0 && stdin_fd != 0) {
        int64_t fd = process_clone_fd_from(child, parent, stdin_fd);
        if (fd < 0) {
            return false;
        }
        child->stdin_redirect = true;
        child->stdin_fd = fd;
    } else if (stdin_fd < 0 &&
        !process_inherit_stdio_redirect(child,
            parent,
            parent->stdin_redirect,
            parent->stdin_fd,
            &child->stdin_redirect,
            &child->stdin_fd)) {
        return false;
    }

    if (stdout_fd == PROCESS_STDIO_CLOSED_FD) {
        child->stdout_redirect = true;
        child->stdout_fd = -1;
    } else if (stdout_fd >= 0 && stdout_fd != 1) {
        int64_t fd = process_clone_fd_from(child, parent, stdout_fd);
        if (fd < 0) {
            return false;
        }
        child->stdout_redirect = true;
        child->stdout_fd = fd;
    } else if (stdout_fd < 0 &&
        !process_inherit_stdio_redirect(child,
            parent,
            parent->stdout_redirect,
            parent->stdout_fd,
            &child->stdout_redirect,
            &child->stdout_fd)) {
        return false;
    }

    if (stderr_fd == PROCESS_STDIO_CLOSED_FD) {
        child->stderr_redirect = true;
        child->stderr_fd = -1;
    } else if (stderr_fd >= 0 && stderr_fd != 2) {
        int64_t fd = process_clone_fd_from(child, parent, stderr_fd);
        if (fd < 0) {
            return false;
        }
        child->stderr_redirect = true;
        child->stderr_fd = fd;
    } else if (stderr_fd < 0 &&
        !process_inherit_stdio_redirect(child,
            parent,
            parent->stderr_redirect,
            parent->stderr_fd,
            &child->stderr_redirect,
            &child->stderr_fd)) {
        return false;
    }

    return true;
}

static int64_t process_spawn_background_elf_args_quiet_flag(const char *path, const char *args, bool quiet) {
    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    struct process *process = alloc_process(path, true);
    if (process == NULL) {
        return -1;
    }

    if (!allocate_kernel_stack(process)) {
        release_process(process);
        return -1;
    }

    if (!load_elf_image(path, process, USER_STACK_TOP, &process->entry) ||
        !setup_process_arguments(process, path, args != NULL ? args : "")) {
        cleanup_process_address_space(process);
        release_process(process);
        return -1;
    }

    uint64_t pid = process->pid;
    process->quiet = quiet;
    if (!scheduler_spawn("user", background_process_thread, process)) {
        cleanup_process_address_space(process);
        release_process(process);
        return -1;
    }

    return (int64_t)pid;
}

int64_t process_spawn_background_elf_args(const char *path, const char *args) {
    return process_spawn_background_elf_args_quiet_flag(path, args, false);
}

int64_t process_spawn_background_elf_args_quiet(const char *path, const char *args) {
    return process_spawn_background_elf_args_quiet_flag(path, args, true);
}

int64_t process_spawn_background_elf_args_fds(const char *path,
    const char *args,
    int64_t stdin_fd,
    int64_t stdout_fd) {
    struct process *parent = process_current();
    if (parent == NULL || path == NULL || path[0] == '\0') {
        return -1;
    }

    struct process *process = alloc_process(path, true);
    if (process == NULL) {
        return -1;
    }

    if (!allocate_kernel_stack(process)) {
        release_process(process);
        return -1;
    }

    if (!load_elf_image(path, process, USER_STACK_TOP, &process->entry) ||
        !setup_process_arguments(process, path, args != NULL ? args : "") ||
        !process_configure_stdio_fds(process, parent, stdin_fd, stdout_fd, -1)) {
        cleanup_process_address_space(process);
        cleanup_process_files(process);
        release_process(process);
        return -1;
    }

    uint64_t pid = process->pid;
    process->quiet = true;
    if (!scheduler_spawn("user", background_process_thread, process)) {
        cleanup_process_address_space(process);
        cleanup_process_files(process);
        release_process(process);
        return -1;
    }

    return (int64_t)pid;
}

int64_t process_spawn_background_elf(const char *path) {
    return process_spawn_background_elf_args(path, "");
}

static struct process *find_reapable_process(uint64_t pid) {
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        if (!processes[i].allocated || !processes[i].detached || !processes[i].reapable) {
            continue;
        }
        if (pid == 0 || processes[i].pid == pid) {
            return &processes[i];
        }
    }
    return NULL;
}

static bool has_wait_target(uint64_t pid) {
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        if (!processes[i].allocated || !processes[i].detached) {
            continue;
        }
        if (pid == 0 || processes[i].pid == pid) {
            return true;
        }
    }
    return false;
}

static bool process_wait_ready(void *arg) {
    uint64_t pid = arg != NULL ? *(uint64_t *)arg : 0;
    return find_reapable_process(pid) != NULL || !has_wait_target(pid);
}

int64_t process_wait(uint64_t pid, uint64_t *status_out, bool nohang) {
    for (;;) {
        struct process *process = find_reapable_process(pid);
        if (process != NULL) {
            uint64_t waited_pid = process->pid;
            uint64_t status = process->exit_status;
            if (status_out != NULL) {
                *status_out = status;
            }
            release_process(process);
            return (int64_t)waited_pid;
        }

        if (!has_wait_target(pid)) {
            return -1;
        }
        if (nohang) {
            return 0;
        }

        if (!scheduler_wait(&process_wait_queue, process_wait_ready, &pid)) {
            scheduler_yield();
        }
    }
}

uint64_t process_list(uint64_t start_index,
    uint64_t *pid_out,
    const char **name_out,
    const char **state_out) {
    for (uint64_t i = start_index; i < PROCESS_MAX_PROCESSES; i++) {
        if (processes[i].allocated) {
            if (pid_out != NULL) {
                *pid_out = processes[i].pid;
            }
            if (name_out != NULL) {
                *name_out = processes[i].name;
            }
            if (state_out != NULL) {
                *state_out = process_state(&processes[i]);
            }
            return i + 1;
        }
    }
    return 0;
}

bool process_kill_pid(uint64_t pid) {
    return process_signal_pid(pid, SRV_SIGNAL_TERM);
}

bool process_signal_pid(uint64_t pid, uint64_t signal) {
    if (signal == 0) {
        signal = SRV_SIGNAL_TERM;
    }
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        if (processes[i].allocated && processes[i].active && processes[i].pid == pid) {
            processes[i].killed = true;
            processes[i].kill_signal = signal;
            net_process_wake(&processes[i]);
            scheduler_wake_all(&pipe_wait_queue);
            process_file_poll_wake();
            process_futex_wake_process(&processes[i]);
            return true;
        }
    }
    return false;
}

bool process_set_group(uint64_t pid, uint64_t group) {
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        if (processes[i].allocated && processes[i].pid == pid) {
            processes[i].process_group = group;
            return true;
        }
    }
    return false;
}

void process_set_foreground_group(uint64_t group) {
    foreground_process_group = group;
}

static bool is_root_interactive_shell(const struct process *process) {
    return process != NULL &&
        process->pid == 1 &&
        (process_path_equal(process->name, "/fat/bin/sh") ||
            process_path_equal(process->name, "/sh"));
}

bool process_interrupt_foreground(uint64_t signal) {
    bool signaled = false;
    if (foreground_process_group != 0) {
        for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
            struct process *candidate = &processes[i];
            if (!candidate->allocated ||
                !candidate->active ||
                candidate->killed ||
                candidate->process_group != foreground_process_group) {
                continue;
            }
            signaled = process_signal_pid(candidate->pid, signal) || signaled;
        }
        if (signaled) {
            return true;
        }
    }

    struct process *victim = NULL;
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        struct process *candidate = &processes[i];
        if (!candidate->allocated ||
            !candidate->active ||
            candidate->detached ||
            candidate->killed ||
            is_root_interactive_shell(candidate)) {
            continue;
        }
        if (victim == NULL || candidate->pid > victim->pid) {
            victim = candidate;
        }
    }
    if (victim == NULL) {
        return false;
    }
    return process_signal_pid(victim->pid, signal);
}

static struct process *first_background_process(void) {
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        if (processes[i].allocated && processes[i].active && processes[i].detached) {
            return &processes[i];
        }
    }
    return NULL;
}

bool process_background_active(void) {
    return first_background_process() != NULL;
}

bool process_background_killed(void) {
    struct process *process = first_background_process();
    return process != NULL && process->killed;
}

const char *process_background_name(void) {
    struct process *process = first_background_process();
    return process != NULL ? process->name : "";
}

uint64_t process_background_status(void) {
    struct process *process = first_background_process();
    return process != NULL ? process->exit_status : 0;
}

bool process_kill_background(void) {
    struct process *process = first_background_process();
    if (process == NULL) {
        return false;
    }

    return process_signal_pid(process->pid, SRV_SIGNAL_TERM);
}

bool process_should_exit_current(void) {
    struct process *process = process_current();
    return process != NULL && (process->killed || process->exiting);
}

uint64_t process_current_kill_status(void) {
    struct process *process = process_current();
    if (process != NULL && process->exiting) {
        return process->exit_status;
    }
    uint64_t signal = process != NULL && process->kill_signal != 0 ?
        process->kill_signal :
        SRV_SIGNAL_TERM;
    return 128 + signal;
}

bool process_has_open_vfs_prefix(const char *prefix) {
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        if (!processes[i].allocated) {
            continue;
        }
        for (uint64_t fd = 0; fd < PROCESS_MAX_OPEN_FILES; fd++) {
            struct process_file *file = &processes[i].files[fd];
            if (!file->used) {
                continue;
            }
            if (file->type == PROCESS_FILE_VFS) {
                if (path_under_prefix(file->path, prefix)) {
                    return true;
                }
            } else if (file->type == PROCESS_FILE_VFS_WRITE) {
                struct process_write_file *write = write_file_at(file->handle);
                if (write != NULL && path_under_prefix(write->path, prefix)) {
                    return true;
                }
            }
        }
    }
    return false;
}

struct process *process_current(void) {
    return (struct process *)scheduler_current_user_context();
}

static struct process_user_thread *process_find_user_thread(struct process *process, uint64_t tid) {
    if (process == NULL || tid < 2) {
        return NULL;
    }
    for (uint64_t i = 0; i < PROCESS_MAX_USER_THREADS; i++) {
        struct process_user_thread *thread = &process->user_threads[i];
        if (thread->used && thread->tid == tid) {
            return thread;
        }
    }
    return NULL;
}

static bool process_user_thread_done(void *arg) {
    struct process_user_thread *thread = arg;
    return thread == NULL || !thread->used || !thread->active;
}

static void process_user_thread_start(void *arg) {
    struct process_user_thread *thread = arg;
    if (thread == NULL || thread->owner == NULL || !thread->used) {
        return;
    }

    struct process *process = thread->owner;
    scheduler_set_user_context_fpu(process,
        process->address_space,
        0,
        &thread->fpu);
    scheduler_set_user_thread_context(thread);
    usermode_enter(thread->entry, thread->stack_top, thread->arg, 0, 0);
}

int64_t process_thread_create(uint64_t entry, uint64_t arg, uint64_t stack_top, uint64_t flags) {
    struct process *process = process_current();
    if (process == NULL || !process->active || entry == 0 || stack_top < PAGE_SIZE) {
        return -1;
    }
    if ((flags & ~SRV_THREAD_DETACHED) != 0) {
        return -1;
    }

    struct process_user_thread *thread = NULL;
    for (uint64_t i = 0; i < PROCESS_MAX_USER_THREADS; i++) {
        if (!process->user_threads[i].used) {
            thread = &process->user_threads[i];
            break;
        }
    }
    if (thread == NULL) {
        return -1;
    }

    uint64_t tid = process->next_thread_id++;
    if (process->next_thread_id < 2) {
        process->next_thread_id = 2;
    }

    *thread = (struct process_user_thread) {
        .used = true,
        .active = true,
        .detached = (flags & SRV_THREAD_DETACHED) != 0,
        .joined = false,
        .tid = tid,
        .entry = entry,
        .arg = arg,
        .stack_top = stack_top & ~0xfull,
        .return_value = 0,
        .owner = process,
    };
    fpu_init_state(&thread->fpu);

    if (!scheduler_spawn("uthread", process_user_thread_start, thread)) {
        thread->used = false;
        thread->active = false;
        return -1;
    }
    return (int64_t)tid;
}

void process_thread_exit(uint64_t value) {
    struct process_user_thread *thread = process_current_user_thread();
    if (thread == NULL || thread->owner == NULL) {
        process_exit((uint64_t)(value != 0));
    }

    __asm__ volatile ("cli" : : : "memory");
    thread->return_value = value;
    thread->active = false;
    scheduler_wake_all(&thread->owner->thread_wait_queue);
    scheduler_clear_user_context();
    if (thread->detached) {
        thread->used = false;
    }
    __asm__ volatile ("sti" : : : "memory");
    scheduler_exit_current();
}

int64_t process_thread_join(uint64_t tid, uint64_t *value_out) {
    struct process *process = process_current();
    struct process_user_thread *current_thread = process_current_user_thread();
    struct process_user_thread *thread = process_find_user_thread(process, tid);
    if (thread == NULL ||
        thread->detached ||
        thread->joined ||
        (current_thread != NULL && current_thread == thread)) {
        return -1;
    }

    __asm__ volatile ("sti" : : : "memory");
    while (thread->active) {
        if (!scheduler_wait(&process->thread_wait_queue, process_user_thread_done, thread)) {
            scheduler_yield();
        }
    }

    if (value_out != NULL) {
        *value_out = thread->return_value;
    }
    thread->joined = true;
    thread->used = false;
    return 0;
}

int64_t process_thread_detach(uint64_t tid) {
    struct process *process = process_current();
    struct process_user_thread *thread = process_find_user_thread(process, tid);
    if (thread == NULL || thread->joined || thread->detached) {
        return -1;
    }

    thread->detached = true;
    if (!thread->active) {
        thread->used = false;
    }
    return 0;
}

int64_t process_thread_status(uint64_t tid) {
    struct process *process = process_current();
    struct process_user_thread *thread = process_find_user_thread(process, tid);
    if (thread == NULL) {
        return -1;
    }
    return thread->active ? 1 : 0;
}

uint64_t process_thread_self(void) {
    struct process_user_thread *thread = process_current_user_thread();
    return thread != NULL && thread->used ? thread->tid : 1;
}

static struct process_futex *process_futex_find(struct process *process, uint64_t address, bool create) {
    struct process_futex *free_entry = NULL;
    for (uint64_t i = 0; i < PROCESS_MAX_FUTEXES; i++) {
        struct process_futex *entry = &futexes[i];
        if (entry->used && entry->process == process && entry->address == address) {
            return entry;
        }
        if ((!entry->used || entry->wait_queue.waiters == 0) && free_entry == NULL) {
            free_entry = entry;
        }
    }
    if (!create || free_entry == NULL) {
        return NULL;
    }

    *free_entry = (struct process_futex) {
        .used = true,
        .process = process,
        .address = address,
        .wait_queue = {0},
    };
    return free_entry;
}

struct process_futex_wait {
    uint32_t *address;
    uint32_t expected;
};

static bool process_futex_wait_ready(void *arg) {
    struct process_futex_wait *wait = arg;
    return process_should_exit_current() ||
        __atomic_load_n(wait->address, __ATOMIC_ACQUIRE) != wait->expected;
}

int64_t process_futex_wait(uint32_t *address, uint32_t expected, uint64_t timeout_ticks) {
    struct process *process = process_current();
    if (process == NULL || address == NULL || (((uint64_t)(uintptr_t)address) & 3u) != 0) {
        return -1;
    }
    if (__atomic_load_n(address, __ATOMIC_ACQUIRE) != expected) {
        return -11;
    }

    uint64_t flags = process_irq_save();
    struct process_futex *futex = process_futex_find(process, (uint64_t)(uintptr_t)address, true);
    process_irq_restore(flags);
    if (futex == NULL) {
        return -11;
    }

    uint64_t start_ticks = timer_ticks();
    uint64_t deadline_ticks = timeout_ticks == 0 ? 0 : start_ticks + timeout_ticks;
    if (deadline_ticks == 0 && timeout_ticks != 0) {
        deadline_ticks = UINT64_MAX;
    }
    struct process_futex_wait wait = {
        .address = address,
        .expected = expected,
    };
    __asm__ volatile ("sti" : : : "memory");
    for (;;) {
        if (process_should_exit_current()) {
            return -1;
        }
        if (__atomic_load_n(address, __ATOMIC_ACQUIRE) != expected) {
            return 0;
        }
        if (timeout_ticks != 0 && timer_ticks() - start_ticks >= timeout_ticks) {
            return -110;
        }
        if (!scheduler_wait_timeout(&futex->wait_queue, process_futex_wait_ready, &wait, deadline_ticks)) {
            scheduler_yield();
        }
    }
}

int64_t process_futex_wake(uint32_t *address, uint64_t max_count) {
    struct process *process = process_current();
    if (process == NULL || address == NULL || (((uint64_t)(uintptr_t)address) & 3u) != 0) {
        return -1;
    }

    uint64_t flags = process_irq_save();
    struct process_futex *futex = process_futex_find(process, (uint64_t)(uintptr_t)address, false);
    process_irq_restore(flags);
    if (futex == NULL) {
        return 0;
    }

    return (int64_t)scheduler_wake_count(&futex->wait_queue, max_count);
}

static void process_futex_wake_process(struct process *process) {
    if (process == NULL) {
        return;
    }
    uint64_t flags = process_irq_save();
    for (uint64_t i = 0; i < PROCESS_MAX_FUTEXES; i++) {
        if (futexes[i].used && futexes[i].process == process) {
            scheduler_wake_all(&futexes[i].wait_queue);
            futexes[i].used = false;
            futexes[i].process = NULL;
            futexes[i].address = 0;
        }
    }
    process_irq_restore(flags);
}

uint64_t process_pid(const struct process *process) {
    return process != NULL ? process->pid : 0;
}

const char *process_name(const struct process *process) {
    return process != NULL ? process->name : "";
}

bool process_current_quiet(void) {
    struct process *process = process_current();
    return process != NULL && process->quiet;
}

bool process_set_stdout_redirect(struct process *process, const char *path, bool append) {
    if (process == NULL || path == NULL || path[0] == '\0') {
        return false;
    }

    uint64_t flags = SRV_OPEN_WRITE | SRV_OPEN_CREATE | (append ? SRV_OPEN_APPEND : SRV_OPEN_TRUNC);
    int64_t fd = process_file_open_write(process, path, flags);
    if (fd < 0 || !copy_process_path(process->stdout_path, sizeof(process->stdout_path), path)) {
        if (fd >= 0) {
            (void)process_file_close(process, (uint64_t)fd);
        }
        return false;
    }
    process->stdout_redirect = true;
    process->stdout_append = append;
    process->stdout_written = false;
    process->stdout_fd = fd;
    return true;
}

int64_t process_stdin_read(struct process *process, uint8_t *buffer, uint64_t length) {
    if (process == NULL || !process->stdin_redirect) {
        return -2;
    }
    if (buffer == NULL && length != 0) {
        return -1;
    }
    if (length == 0) {
        return 0;
    }

    struct process_file *file = process_file_at(process, (uint64_t)process->stdin_fd);
    if (file == NULL) {
        return -1;
    }
    if (file->type == PROCESS_FILE_PIPE_READ) {
        return process_file_pipe_read(process, (uint64_t)process->stdin_fd, buffer, length);
    }
    if (file->type == PROCESS_FILE_STDIO && file->handle == 0) {
        return -2;
    }
    return process_file_read(process, (uint64_t)process->stdin_fd, buffer, length);
}

int64_t process_output_write(struct process *process, uint64_t fd, const uint8_t *buffer, uint64_t length) {
    bool redirected;
    int64_t target_fd;
    if (process == NULL || (fd != 1 && fd != 2)) {
        return -2;
    }
    redirected = fd == 2 ? process->stderr_redirect : process->stdout_redirect;
    target_fd = fd == 2 ? process->stderr_fd : process->stdout_fd;
    if (!redirected) {
        return -2;
    }
    if (buffer == NULL) {
        return -1;
    }
    if (length == 0) {
        return 0;
    }

    struct process_file *file = process_file_at(process, (uint64_t)target_fd);
    if (file == NULL) {
        return -1;
    }
    if (file->type == PROCESS_FILE_STDIO && (file->handle == 1 || file->handle == 2)) {
        return -2;
    }

    int64_t written = file->type == PROCESS_FILE_PIPE_WRITE ?
        process_file_pipe_write(process, (uint64_t)target_fd, buffer, length) :
        process_file_write(process, (uint64_t)target_fd, buffer, length);
    if (written < 0) {
        return written;
    }
    if (file->type == PROCESS_FILE_VFS_WRITE && process_file_flush(process, (uint64_t)target_fd) < 0) {
        return -1;
    }
    process->stdout_written = true;
    return written;
}

struct process_file *process_file_at(struct process *process, uint64_t fd) {
    if (process == NULL || fd < 3 || fd >= 3 + PROCESS_MAX_OPEN_FILES) {
        return NULL;
    }

    struct process_file *file = &process->files[fd - 3];
    return file->used ? file : NULL;
}

int64_t process_file_alloc(struct process *process,
    const struct vfs_node *node,
    const uint8_t *data,
    uint64_t size) {
    if (process == NULL || node == NULL || data == NULL) {
        return -1;
    }

    uint64_t free_index = PROCESS_MAX_OPEN_FILES;
    for (uint64_t i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        if (!process->files[i].used) {
            free_index = i;
            break;
        }
    }
    if (free_index == PROCESS_MAX_OPEN_FILES) {
        vfs_release_data(node, data);
        return -1;
    }

    uint64_t handle = 0;
    for (uint64_t i = 0; i < PROCESS_MAX_READ_FILES; i++) {
        if (!read_files[i].used) {
            handle = i + 1;
            break;
        }
    }
    if (handle == 0) {
        vfs_release_data(node, data);
        return -1;
    }

    struct process_read_file *read = &read_files[handle - 1];
    uint8_t *read_bytes = (uint8_t *)read;
    for (uint64_t i = 0; i < sizeof(*read); i++) {
        read_bytes[i] = 0;
    }
    read->used = true;
    read->refs = 1;
    read->node = node;
    read->data = data;
    read->size = size;
    read->offset = 0;
    (void)copy_process_path(read->path, sizeof(read->path), node->path);

    struct process_file *file = &process->files[free_index];
    uint8_t *bytes = (uint8_t *)file;
    for (uint64_t i = 0; i < sizeof(*file); i++) {
        bytes[i] = 0;
    }
    file->used = true;
    file->type = PROCESS_FILE_VFS;
    file->node = node;
    file->data = data;
    file->size = size;
    file->handle = handle;
    file->flags = 0;
    (void)copy_process_path(file->path, sizeof(file->path), node->path);
    return (int64_t)(free_index + 3);
}

static struct process_read_file *read_file_at(uint64_t handle) {
    if (handle == 0 || handle > PROCESS_MAX_READ_FILES) {
        return NULL;
    }
    struct process_read_file *file = &read_files[handle - 1];
    return file->used ? file : NULL;
}

static void read_file_ref(uint64_t handle) {
    struct process_read_file *file = read_file_at(handle);
    if (file != NULL) {
        file->refs++;
    }
}

static void read_file_close(uint64_t handle) {
    struct process_read_file *file = read_file_at(handle);
    if (file == NULL) {
        return;
    }
    if (file->refs > 0) {
        file->refs--;
    }
    if (file->refs > 0) {
        return;
    }
    if (file->node != NULL && file->data != NULL) {
        vfs_release_data(file->node, file->data);
    }
    uint8_t *bytes = (uint8_t *)file;
    for (uint64_t i = 0; i < sizeof(*file); i++) {
        bytes[i] = 0;
    }
}

int64_t process_file_read(struct process *process, uint64_t fd, uint8_t *buffer, uint64_t length) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL ||
        (file->type != PROCESS_FILE_VFS && file->type != PROCESS_FILE_VFS_WRITE) ||
        (buffer == NULL && length != 0)) {
        return -1;
    }

    uint64_t size = file->size;
    uint64_t offset = file->offset;
    const uint8_t *data = file->data;
    struct process_read_file *read = NULL;
    struct process_write_file *write = NULL;
    if (file->type == PROCESS_FILE_VFS) {
        read = read_file_at(file->handle);
        if (read == NULL) {
            return -1;
        }
        size = read->size;
        offset = read->offset;
        data = read->data;
    } else if (file->type == PROCESS_FILE_VFS_WRITE) {
        write = write_file_at(file->handle);
        if (write == NULL) {
            return -1;
        }
        size = write->size;
        offset = write->offset;
        data = write->data;
    }

    uint64_t remaining = offset <= size ? size - offset : 0;
    uint64_t count = length < remaining ? length : remaining;
    for (uint64_t i = 0; i < count; i++) {
        buffer[i] = data[offset + i];
    }
    if (write != NULL) {
        write->offset += count;
    } else if (read != NULL) {
        read->offset += count;
    } else {
        file->offset += count;
    }
    return (int64_t)count;
}

static struct process_write_file *write_file_at(uint64_t handle) {
    if (handle == 0 || handle > PROCESS_MAX_WRITE_FILES) {
        return NULL;
    }
    struct process_write_file *file = &write_files[handle - 1];
    return file->used ? file : NULL;
}

static bool file_lock_ranges_overlap(uint64_t a_start, uint64_t a_end, uint64_t b_start, uint64_t b_end) {
    return a_start < b_end && b_start < a_end;
}

static bool file_lock_type_valid(int16_t type) {
    return type == SRV_F_RDLCK || type == SRV_F_WRLCK || type == SRV_F_UNLCK;
}

static bool file_lock_path_for_fd(struct process_file *file, const char **path_out) {
    if (file == NULL || path_out == NULL) {
        return false;
    }
    if (file->type == PROCESS_FILE_VFS && file->path[0] != '\0') {
        *path_out = file->path;
        return true;
    }
    if (file->type == PROCESS_FILE_VFS_WRITE) {
        struct process_write_file *write = write_file_at(file->handle);
        if (write != NULL && write->path[0] != '\0') {
            *path_out = write->path;
            return true;
        }
    }
    return false;
}

static bool file_lock_range_for_fd(struct process_file *file,
    const struct srv_flock *request,
    uint64_t *start_out,
    uint64_t *end_out) {
    if (file == NULL || request == NULL || start_out == NULL || end_out == NULL ||
        request->start < 0 || request->len < 0) {
        return false;
    }

    uint64_t base = 0;
    if (request->whence == 0) {
        base = 0;
    } else if (request->whence == 1) {
        if (file->type == PROCESS_FILE_VFS) {
            struct process_read_file *read = read_file_at(file->handle);
            if (read == NULL) {
                return false;
            }
            base = read->offset;
        } else if (file->type == PROCESS_FILE_VFS_WRITE) {
            struct process_write_file *write = write_file_at(file->handle);
            if (write == NULL) {
                return false;
            }
            base = write->offset;
        } else {
            base = file->offset;
        }
    } else if (request->whence == 2) {
        if (file->type == PROCESS_FILE_VFS) {
            struct process_read_file *read = read_file_at(file->handle);
            if (read == NULL) {
                return false;
            }
            base = read->size;
        } else if (file->type == PROCESS_FILE_VFS_WRITE) {
            struct process_write_file *write = write_file_at(file->handle);
            if (write == NULL) {
                return false;
            }
            base = write->size;
        } else {
            base = file->size;
        }
    } else {
        return false;
    }

    uint64_t start = base + (uint64_t)request->start;
    if (start < base) {
        return false;
    }
    uint64_t end = request->len == 0 ? UINT64_MAX : start + (uint64_t)request->len;
    if (end <= start) {
        return false;
    }
    *start_out = start;
    *end_out = end;
    return true;
}

static struct process_file_lock *file_lock_conflict(const char *path,
    uint64_t pid,
    uint64_t start,
    uint64_t end,
    int16_t type) {
    for (uint64_t i = 0; i < PROCESS_MAX_FILE_LOCKS; i++) {
        struct process_file_lock *lock = &file_locks[i];
        if (!lock->used ||
            lock->pid == pid ||
            !process_path_equal(lock->path, path) ||
            !file_lock_ranges_overlap(lock->start, lock->end, start, end)) {
            continue;
        }
        if (type == SRV_F_WRLCK || lock->type == SRV_F_WRLCK) {
            return lock;
        }
    }
    return NULL;
}

static void file_lock_release_overlap(uint64_t pid, const char *path, uint64_t start, uint64_t end) {
    for (uint64_t i = 0; i < PROCESS_MAX_FILE_LOCKS; i++) {
        struct process_file_lock *lock = &file_locks[i];
        if (lock->used &&
            lock->pid == pid &&
            process_path_equal(lock->path, path) &&
            file_lock_ranges_overlap(lock->start, lock->end, start, end)) {
            lock->used = false;
        }
    }
}

static void file_lock_release_path(uint64_t pid, const char *path) {
    if (path == NULL || path[0] == '\0') {
        return;
    }
    for (uint64_t i = 0; i < PROCESS_MAX_FILE_LOCKS; i++) {
        if (file_locks[i].used &&
            file_locks[i].pid == pid &&
            process_path_equal(file_locks[i].path, path)) {
            file_locks[i].used = false;
        }
    }
    scheduler_wake_all(&file_lock_wait_queue);
}

static int64_t file_lock_add(uint64_t pid, const char *path, uint64_t start, uint64_t end, int16_t type) {
    for (uint64_t i = 0; i < PROCESS_MAX_FILE_LOCKS; i++) {
        if (!file_locks[i].used) {
            struct process_file_lock *lock = &file_locks[i];
            lock->used = true;
            lock->pid = pid;
            lock->start = start;
            lock->end = end;
            lock->type = type;
            if (!copy_process_path(lock->path, sizeof(lock->path), path)) {
                lock->used = false;
                return -1;
            }
            return 0;
        }
    }
    return -1;
}

static uint64_t write_file_alloc(void) {
    for (uint64_t i = 0; i < PROCESS_MAX_WRITE_FILES; i++) {
        if (write_files[i].used) {
            continue;
        }
        struct process_write_file *file = &write_files[i];
        uint8_t *bytes = (uint8_t *)file;
        for (uint64_t j = 0; j < sizeof(*file); j++) {
            bytes[j] = 0;
        }
        file->used = true;
        file->refs = 1;
        return i + 1;
    }
    return 0;
}

static void write_file_ref(uint64_t handle) {
    struct process_write_file *file = write_file_at(handle);
    if (file != NULL) {
        file->refs++;
    }
}

static bool ensure_write_capacity(struct process_write_file *file, uint64_t needed) {
    if (file == NULL) {
        return false;
    }
    if (needed <= file->capacity) {
        return true;
    }

    uint64_t capacity = file->capacity == 0 ? 4096 : file->capacity;
    while (capacity < needed) {
        uint64_t next = capacity * 2;
        if (next <= capacity) {
            return false;
        }
        capacity = next;
    }

    uint8_t *next_data = kmalloc(capacity);
    if (next_data == NULL) {
        return false;
    }

    for (uint64_t i = 0; i < file->size; i++) {
        next_data[i] = file->data[i];
    }
    for (uint64_t i = file->size; i < capacity; i++) {
        next_data[i] = 0;
    }

    if (file->data != NULL) {
        kfree((void *)file->data);
    }
    file->data = next_data;
    file->capacity = capacity;
    return true;
}

int64_t process_file_open_write(struct process *process, const char *path, uint64_t flags) {
    if (process == NULL ||
        path == NULL ||
        (flags & SRV_OPEN_WRITE) == 0 ||
        ((flags & SRV_OPEN_TRUNC) != 0 && (flags & SRV_OPEN_APPEND) != 0)) {
        return -1;
    }

    const struct vfs_node *node = vfs_lookup(path);
    const uint8_t *old_data = NULL;
    uint64_t old_size = 0;
    bool exists = node != NULL;
    bool append = (flags & SRV_OPEN_APPEND) != 0;
    bool trunc = (flags & SRV_OPEN_TRUNC) != 0;
    bool ensure_empty = trunc || (!exists && (flags & SRV_OPEN_CREATE) != 0);

    if (!exists && (flags & SRV_OPEN_CREATE) == 0) {
        return -1;
    }
    if (exists && !trunc && !append && (flags & SRV_OPEN_READ) == 0) {
        append = true;
    }

    if (exists && !trunc && !vfs_read_all(node, &old_data, &old_size)) {
        return -1;
    }

    uint64_t free_index = PROCESS_MAX_OPEN_FILES;
    for (uint64_t i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        if (process->files[i].used) {
            continue;
        }
        free_index = i;
        break;
    }
    if (free_index == PROCESS_MAX_OPEN_FILES) {
        if (old_data != NULL) {
            vfs_release_data(node, old_data);
        }
        return -1;
    }

    uint64_t handle = write_file_alloc();
    if (handle == 0) {
        if (old_data != NULL) {
            vfs_release_data(node, old_data);
        }
        return -1;
    }

    struct process_write_file *write = write_file_at(handle);
    if (write == NULL ||
        !copy_process_path(write->path, sizeof(write->path), path) ||
        (old_size != 0 && !ensure_write_capacity(write, old_size))) {
        if (old_data != NULL) {
            vfs_release_data(node, old_data);
        }
        if (write != NULL && write->data != NULL) {
            kfree((void *)write->data);
        }
        if (write != NULL) {
            write->used = false;
        }
        return -1;
    }

    for (uint64_t j = 0; j < old_size; j++) {
        ((uint8_t *)write->data)[j] = old_data[j];
    }
    write->size = trunc ? 0 : old_size;
    write->offset = append ? old_size : 0;
    write->dirty = false;
    if (old_data != NULL) {
        vfs_release_data(node, old_data);
    }
    if (ensure_empty && !exfat_write_file(path, NULL, 0)) {
        if (write->data != NULL) {
            kfree((void *)write->data);
        }
        uint8_t *write_bytes = (uint8_t *)write;
        for (uint64_t j = 0; j < sizeof(*write); j++) {
            write_bytes[j] = 0;
        }
        return -1;
    }

    struct process_file *file = &process->files[free_index];
    uint8_t *bytes = (uint8_t *)file;
    for (uint64_t j = 0; j < sizeof(*file); j++) {
        bytes[j] = 0;
    }
    file->used = true;
    file->type = PROCESS_FILE_VFS_WRITE;
    file->handle = handle;
    file->flags = (flags & SRV_OPEN_NONBLOCK) != 0 ? SRV_FD_NONBLOCK : 0;
    (void)copy_process_path(file->path, sizeof(file->path), path);
    return (int64_t)(free_index + 3);
}

int64_t process_file_write(struct process *process, uint64_t fd, const uint8_t *buffer, uint64_t length) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL || file->type != PROCESS_FILE_VFS_WRITE || (buffer == NULL && length != 0)) {
        return -1;
    }
    struct process_write_file *write = write_file_at(file->handle);
    if (write == NULL) {
        return -1;
    }
    if (length == 0) {
        return 0;
    }

    uint64_t needed = write->offset + length;
    if (needed < write->offset || needed > PROCESS_WRITE_BUFFER_MAX || !ensure_write_capacity(write, needed)) {
        write->failed = true;
        return -1;
    }

    for (uint64_t i = write->size; i < write->offset; i++) {
        ((uint8_t *)write->data)[i] = 0;
    }
    for (uint64_t i = 0; i < length; i++) {
        ((uint8_t *)write->data)[write->offset + i] = buffer[i];
    }

    write->offset += length;
    if (write->offset > write->size) {
        write->size = write->offset;
    }
    write->dirty = true;
    return (int64_t)length;
}

bool process_file_nonblocking(struct process *process, uint64_t fd) {
    struct process_file *file = process_file_at(process, fd);
    return file != NULL && (file->flags & SRV_FD_NONBLOCK) != 0;
}

static struct process_pipe *pipe_at(uint64_t handle) {
    if (handle == 0 || handle > PROCESS_MAX_PIPES) {
        return NULL;
    }
    struct process_pipe *pipe = &pipes[handle - 1];
    return pipe->used ? pipe : NULL;
}

static uint64_t pipe_alloc(void) {
    for (uint64_t i = 0; i < PROCESS_MAX_PIPES; i++) {
        if (pipes[i].used) {
            continue;
        }
        struct process_pipe *pipe = &pipes[i];
        uint8_t *bytes = (uint8_t *)pipe;
        for (uint64_t j = 0; j < sizeof(*pipe); j++) {
            bytes[j] = 0;
        }
        pipe->used = true;
        pipe->refs = 2;
        pipe->read_refs = 1;
        pipe->write_refs = 1;
        return i + 1;
    }
    return 0;
}

static void pipe_ref(uint64_t handle, bool write_end) {
    struct process_pipe *pipe = pipe_at(handle);
    if (pipe == NULL) {
        return;
    }
    pipe->refs++;
    if (write_end) {
        pipe->write_refs++;
    } else {
        pipe->read_refs++;
    }
}

static void pipe_close(uint64_t handle, bool write_end) {
    struct process_pipe *pipe = pipe_at(handle);
    if (pipe == NULL) {
        return;
    }
    if (pipe->refs > 0) {
        pipe->refs--;
    }
    if (write_end && pipe->write_refs > 0) {
        pipe->write_refs--;
    } else if (!write_end && pipe->read_refs > 0) {
        pipe->read_refs--;
    }
    if (pipe->refs == 0) {
        pipe->used = false;
    }
    scheduler_wake_all(&pipe_wait_queue);
    process_file_poll_wake();
}

static int64_t write_file_close(uint64_t handle) {
    struct process_write_file *write = write_file_at(handle);
    if (write == NULL) {
        return -1;
    }

    if (write->refs > 0) {
        write->refs--;
    }
    if (write->refs > 0) {
        return 0;
    }

    int64_t result = 0;
    if (process_file_flush(NULL, handle) < 0) {
        result = -1;
    }

    if (write->data != NULL) {
        kfree((void *)write->data);
    }
    uint8_t *bytes = (uint8_t *)write;
    for (uint64_t i = 0; i < sizeof(*write); i++) {
        bytes[i] = 0;
    }
    return result;
}

int64_t process_file_truncate(struct process *process, uint64_t fd, uint64_t length) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL || file->type != PROCESS_FILE_VFS_WRITE || length > PROCESS_WRITE_BUFFER_MAX) {
        return -1;
    }

    struct process_write_file *write = write_file_at(file->handle);
    if (write == NULL) {
        return -1;
    }
    if (length > write->capacity && !ensure_write_capacity(write, length)) {
        write->failed = true;
        return -1;
    }
    for (uint64_t i = write->size; i < length; i++) {
        ((uint8_t *)write->data)[i] = 0;
    }
    write->size = length;
    write->dirty = true;
    return 0;
}

int64_t process_file_flush(struct process *process, uint64_t fd) {
    struct process_write_file *write = NULL;
    if (process == NULL) {
        write = write_file_at(fd);
    } else {
        struct process_file *file = process_file_at(process, fd);
        if (file == NULL) {
            return -1;
        }
        if (file->type == PROCESS_FILE_VFS ||
            file->type == PROCESS_FILE_STDIO ||
            file->type == PROCESS_FILE_PIPE_READ ||
            file->type == PROCESS_FILE_PIPE_WRITE) {
            return 0;
        }
        if (file->type != PROCESS_FILE_VFS_WRITE) {
            return -1;
        }
        write = write_file_at(file->handle);
    }
    if (write == NULL) {
        return -1;
    }
    if (write->failed) {
        return -1;
    }
    if (!write->dirty) {
        return 0;
    }

    bool ok = exfat_write_file(write->path, write->data, write->size);
    if (!ok && vfs_lookup(write->path) != NULL) {
        ok = exfat_delete_file(write->path) &&
            exfat_write_file(write->path, write->data, write->size);
    }
    if (!ok) {
        write->failed = true;
        return -1;
    }
    write->dirty = false;
    return 0;
}

static bool pipe_read_ready(void *arg) {
    struct process_pipe *pipe = arg;
    return process_should_exit_current() ||
        pipe == NULL ||
        !pipe->used ||
        pipe->size > 0 ||
        pipe->write_refs == 0;
}

static bool pipe_write_ready(void *arg) {
    struct process_pipe *pipe = arg;
    return process_should_exit_current() ||
        pipe == NULL ||
        !pipe->used ||
        pipe->read_refs == 0 ||
        pipe->size < PROCESS_PIPE_CAPACITY;
}

int64_t process_file_pipe(struct process *process, uint64_t fds_out[2]) {
    if (process == NULL || fds_out == NULL) {
        return -1;
    }

    uint64_t read_index = PROCESS_MAX_OPEN_FILES;
    uint64_t write_index = PROCESS_MAX_OPEN_FILES;
    for (uint64_t i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        if (process->files[i].used) {
            continue;
        }
        if (read_index == PROCESS_MAX_OPEN_FILES) {
            read_index = i;
        } else {
            write_index = i;
            break;
        }
    }
    if (write_index == PROCESS_MAX_OPEN_FILES) {
        return -1;
    }

    uint64_t handle = pipe_alloc();
    if (handle == 0) {
        return -1;
    }

    struct process_file *read_file = &process->files[read_index];
    struct process_file *write_file = &process->files[write_index];
    uint8_t *read_bytes = (uint8_t *)read_file;
    uint8_t *write_bytes = (uint8_t *)write_file;
    for (uint64_t i = 0; i < sizeof(*read_file); i++) {
        read_bytes[i] = 0;
        write_bytes[i] = 0;
    }
    read_file->used = true;
    read_file->type = PROCESS_FILE_PIPE_READ;
    read_file->handle = handle;
    write_file->used = true;
    write_file->type = PROCESS_FILE_PIPE_WRITE;
    write_file->handle = handle;
    fds_out[0] = read_index + 3;
    fds_out[1] = write_index + 3;
    return 0;
}

int64_t process_file_pipe_read(struct process *process, uint64_t fd, uint8_t *buffer, uint64_t length) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL || file->type != PROCESS_FILE_PIPE_READ || (buffer == NULL && length != 0)) {
        return -1;
    }
    if (length == 0) {
        return 0;
    }
    struct process_pipe *pipe = pipe_at(file->handle);
    if (pipe == NULL) {
        return -1;
    }
    while (pipe->size == 0 && pipe->write_refs > 0) {
        if (process_should_exit_current()) {
            return -1;
        }
        if ((file->flags & SRV_FD_NONBLOCK) != 0) {
            return SRV_ERR_AGAIN;
        }
        if (!scheduler_wait(&pipe_wait_queue, pipe_read_ready, pipe)) {
            break;
        }
    }
    uint64_t count = length < pipe->size ? length : pipe->size;
    for (uint64_t i = 0; i < count; i++) {
        buffer[i] = pipe->buffer[pipe->read_pos];
        pipe->read_pos = (pipe->read_pos + 1) % PROCESS_PIPE_CAPACITY;
    }
    pipe->size -= count;
    if (count > 0) {
        scheduler_wake_all(&pipe_wait_queue);
        process_file_poll_wake();
    }
    return (int64_t)count;
}

int64_t process_file_pipe_write(struct process *process, uint64_t fd, const uint8_t *buffer, uint64_t length) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL || file->type != PROCESS_FILE_PIPE_WRITE || (buffer == NULL && length != 0)) {
        return -1;
    }
    struct process_pipe *pipe = pipe_at(file->handle);
    if (pipe == NULL || pipe->read_refs == 0) {
        return -1;
    }

    uint64_t done = 0;
    while (done < length) {
        while (pipe->size == PROCESS_PIPE_CAPACITY && pipe->read_refs > 0) {
            if (process_should_exit_current()) {
                return done > 0 ? (int64_t)done : -1;
            }
            if ((file->flags & SRV_FD_NONBLOCK) != 0) {
                return done > 0 ? (int64_t)done : SRV_ERR_AGAIN;
            }
            if (!scheduler_wait(&pipe_wait_queue, pipe_write_ready, pipe)) {
                break;
            }
        }
        if (!pipe->used || pipe->read_refs == 0) {
            return done > 0 ? (int64_t)done : -1;
        }

        uint64_t space = PROCESS_PIPE_CAPACITY - pipe->size;
        if (space == 0) {
            return done > 0 ? (int64_t)done : 0;
        }
        uint64_t remaining = length - done;
        uint64_t count = remaining < space ? remaining : space;
        for (uint64_t i = 0; i < count; i++) {
            pipe->buffer[pipe->write_pos] = buffer[done + i];
            pipe->write_pos = (pipe->write_pos + 1) % PROCESS_PIPE_CAPACITY;
        }
        pipe->size += count;
        done += count;
        scheduler_wake_all(&pipe_wait_queue);
        process_file_poll_wake();
    }
    return (int64_t)done;
}

struct scheduler_wait_queue *process_file_poll_wait_queue(void) {
    return &fd_poll_wait_queue;
}

void process_file_poll_wake(void) {
    scheduler_wake_all(&fd_poll_wait_queue);
}

uint16_t process_file_poll(struct process *process, int64_t fd, uint16_t events) {
    if (process == NULL) {
        return SRV_POLLNVAL;
    }

    if (fd == 0) {
        return (events & SRV_POLLIN) != 0 ? SRV_POLLIN : 0;
    }
    if (fd == 1 || fd == 2) {
        return (events & SRV_POLLOUT) != 0 ? SRV_POLLOUT : 0;
    }

    struct process_file *file = process_file_at(process, (uint64_t)fd);
    if (file == NULL) {
        return SRV_POLLNVAL;
    }

    if (file->type == PROCESS_FILE_STDIO) {
        if (file->handle == 0) {
            return (events & SRV_POLLIN) != 0 ? SRV_POLLIN : 0;
        }
        if (file->handle == 1 || file->handle == 2) {
            return (events & SRV_POLLOUT) != 0 ? SRV_POLLOUT : 0;
        }
        return SRV_POLLNVAL;
    }

    if (file->type == PROCESS_FILE_VFS) {
        uint16_t revents = 0;
        if ((events & SRV_POLLIN) != 0) {
            revents |= SRV_POLLIN;
        }
        return revents;
    }

    if (file->type == PROCESS_FILE_VFS_WRITE) {
        struct process_write_file *write = write_file_at(file->handle);
        if (write == NULL) {
            return SRV_POLLNVAL;
        }
        uint16_t revents = 0;
        if ((events & SRV_POLLIN) != 0) {
            revents |= SRV_POLLIN;
        }
        if ((events & SRV_POLLOUT) != 0 && !write->failed) {
            revents |= SRV_POLLOUT;
        }
        if (write->failed) {
            revents |= SRV_POLLERR;
        }
        return revents;
    }

    if (file->type == PROCESS_FILE_PIPE_READ || file->type == PROCESS_FILE_PIPE_WRITE) {
        struct process_pipe *pipe = pipe_at(file->handle);
        if (pipe == NULL) {
            return SRV_POLLNVAL;
        }

        uint16_t revents = 0;
        if (file->type == PROCESS_FILE_PIPE_READ) {
            if ((events & SRV_POLLIN) != 0 && pipe->size > 0) {
                revents |= SRV_POLLIN;
            }
            if (pipe->write_refs == 0) {
                revents |= SRV_POLLHUP;
            }
            return revents;
        }

        if ((events & SRV_POLLOUT) != 0 && pipe->read_refs != 0 && pipe->size < PROCESS_PIPE_CAPACITY) {
            revents |= SRV_POLLOUT;
        }
        if (pipe->read_refs == 0) {
            revents |= SRV_POLLERR;
        }
        return revents;
    }

    if (file->type == PROCESS_FILE_NET_LISTENER ||
        file->type == PROCESS_FILE_NET_CONNECTION ||
        file->type == PROCESS_FILE_NET_UDP) {
        return net_poll_events(file->handle, events);
    }

    return SRV_POLLNVAL;
}

int64_t process_handle_alloc(struct process *process,
    enum process_file_type type,
    uint64_t handle) {
    if (process == NULL ||
        (type != PROCESS_FILE_NET_LISTENER &&
            type != PROCESS_FILE_NET_CONNECTION &&
            type != PROCESS_FILE_NET_UDP) ||
        handle == 0) {
        return -1;
    }

    for (uint64_t i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        if (!process->files[i].used) {
            struct process_file *file = &process->files[i];
            uint8_t *bytes = (uint8_t *)file;
            for (uint64_t j = 0; j < sizeof(*file); j++) {
                bytes[j] = 0;
            }
            file->used = true;
            file->type = type;
            file->handle = handle;
            return (int64_t)(i + 3);
        }
    }

    return -1;
}

static int64_t process_file_dup_into(struct process *process, struct process_file *source, uint64_t target_index) {
    if (process == NULL || source == NULL || target_index >= PROCESS_MAX_OPEN_FILES) {
        return -1;
    }

    struct process_file *target = &process->files[target_index];
    if (source->type == PROCESS_FILE_STDIO) {
        uint64_t handle = source->handle;
        uint8_t *bytes = (uint8_t *)target;
        for (uint64_t i = 0; i < sizeof(*target); i++) {
            bytes[i] = 0;
        }
        target->used = true;
        target->type = PROCESS_FILE_STDIO;
        target->handle = handle;
        target->flags = source->flags;
        return (int64_t)(target_index + 3);
    }

    if (source->type == PROCESS_FILE_PIPE_READ || source->type == PROCESS_FILE_PIPE_WRITE) {
        uint64_t handle = source->handle;
        bool write_end = source->type == PROCESS_FILE_PIPE_WRITE;
        if (pipe_at(handle) == NULL) {
            return -1;
        }
        uint8_t *bytes = (uint8_t *)target;
        for (uint64_t i = 0; i < sizeof(*target); i++) {
            bytes[i] = 0;
        }
        pipe_ref(handle, write_end);
        target->used = true;
        target->type = source->type;
        target->handle = handle;
        target->flags = source->flags;
        return (int64_t)(target_index + 3);
    }

    if (source->type == PROCESS_FILE_VFS_WRITE) {
        uint64_t handle = source->handle;
        if (write_file_at(handle) == NULL) {
            return -1;
        }
        uint8_t *bytes = (uint8_t *)target;
        for (uint64_t i = 0; i < sizeof(*target); i++) {
            bytes[i] = 0;
        }
        write_file_ref(handle);
        target->used = true;
        target->type = PROCESS_FILE_VFS_WRITE;
        target->handle = handle;
        target->flags = source->flags;
        return (int64_t)(target_index + 3);
    }

    if (source->type != PROCESS_FILE_VFS || source->handle == 0 || source->path[0] == '\0') {
        return -1;
    }
    struct process_read_file *read = read_file_at(source->handle);
    if (read == NULL) {
        return -1;
    }
    read_file_ref(source->handle);

    uint8_t *bytes = (uint8_t *)target;
    for (uint64_t i = 0; i < sizeof(*target); i++) {
        bytes[i] = 0;
    }
    target->used = true;
    target->type = PROCESS_FILE_VFS;
    target->node = read->node;
    target->data = read->data;
    target->size = read->size;
    target->handle = source->handle;
    target->flags = source->flags;
    (void)copy_process_path(target->path, sizeof(target->path), source->path);
    return (int64_t)(target_index + 3);
}

int64_t process_file_dup(struct process *process, uint64_t old_fd) {
    struct process_file stdio_source;
    struct process_file *source = NULL;
    if (old_fd < 3) {
        uint8_t *bytes = (uint8_t *)&stdio_source;
        for (uint64_t i = 0; i < sizeof(stdio_source); i++) {
            bytes[i] = 0;
        }
        stdio_source.used = true;
        stdio_source.type = PROCESS_FILE_STDIO;
        stdio_source.handle = old_fd;
        source = &stdio_source;
    } else {
        source = process_file_at(process, old_fd);
    }
    if (process == NULL || source == NULL) {
        return -1;
    }

    for (uint64_t i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        if (!process->files[i].used) {
            return process_file_dup_into(process, source, i);
        }
    }
    return -1;
}

int64_t process_file_dup2(struct process *process, uint64_t old_fd, uint64_t new_fd) {
    if (process == NULL) {
        return -1;
    }
    if (old_fd == new_fd) {
        return old_fd < 3 || process_file_at(process, old_fd) != NULL ? (int64_t)new_fd : -1;
    }
    if (new_fd < 3 || new_fd >= 3 + PROCESS_MAX_OPEN_FILES) {
        return -1;
    }

    struct process_file stdio_source;
    struct process_file *source = NULL;
    if (old_fd < 3) {
        uint8_t *bytes = (uint8_t *)&stdio_source;
        for (uint64_t i = 0; i < sizeof(stdio_source); i++) {
            bytes[i] = 0;
        }
        stdio_source.used = true;
        stdio_source.type = PROCESS_FILE_STDIO;
        stdio_source.handle = old_fd;
        source = &stdio_source;
    } else {
        source = process_file_at(process, old_fd);
    }
    if (source == NULL) {
        return -1;
    }

    uint64_t target_index = new_fd - 3;
    if (process->files[target_index].used) {
        if (process_file_close(process, new_fd) < 0) {
            return -1;
        }
    }
    return process_file_dup_into(process, source, target_index);
}

int64_t process_file_get_flags(struct process *process, uint64_t fd) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL) {
        return -1;
    }
    return (int64_t)file->flags;
}

int64_t process_file_set_flags(struct process *process, uint64_t fd, uint64_t flags) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL) {
        return -1;
    }
    file->flags = (file->flags & SRV_FD_CLOEXEC) | (flags & SRV_FD_NONBLOCK);
    return 0;
}

int64_t process_file_get_fd_flags(struct process *process, uint64_t fd) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL) {
        return fd < 3 ? 0 : -1;
    }
    return (file->flags & SRV_FD_CLOEXEC) != 0 ? SRV_FD_CLOEXEC : 0;
}

int64_t process_file_set_fd_flags(struct process *process, uint64_t fd, uint64_t flags) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL) {
        return fd < 3 ? 0 : -1;
    }
    file->flags = (file->flags & ~SRV_FD_CLOEXEC) | (flags & SRV_FD_CLOEXEC);
    return 0;
}

int64_t process_file_get_lock(struct process *process, uint64_t fd, struct srv_flock *lock) {
    struct process_file *file = process_file_at(process, fd);
    const char *path = NULL;
    uint64_t start = 0;
    uint64_t end = 0;
    if (process == NULL ||
        lock == NULL ||
        file == NULL ||
        !file_lock_type_valid(lock->type) ||
        !file_lock_path_for_fd(file, &path) ||
        !file_lock_range_for_fd(file, lock, &start, &end)) {
        return -1;
    }

    if (lock->type == SRV_F_UNLCK) {
        lock->pid = 0;
        return 0;
    }

    struct process_file_lock *conflict = file_lock_conflict(path, process->pid, start, end, lock->type);
    if (conflict == NULL) {
        lock->type = SRV_F_UNLCK;
        lock->pid = 0;
        return 0;
    }

    lock->type = conflict->type;
    lock->whence = 0;
    lock->start = (int64_t)conflict->start;
    lock->len = conflict->end == UINT64_MAX ? 0 : (int64_t)(conflict->end - conflict->start);
    lock->pid = (int64_t)conflict->pid;
    return 0;
}

int64_t process_file_set_lock(struct process *process, uint64_t fd, const struct srv_flock *lock, bool wait) {
    struct process_file *file = process_file_at(process, fd);
    const char *path = NULL;
    uint64_t start = 0;
    uint64_t end = 0;
    if (process == NULL ||
        lock == NULL ||
        file == NULL ||
        !file_lock_type_valid(lock->type) ||
        !file_lock_path_for_fd(file, &path) ||
        !file_lock_range_for_fd(file, lock, &start, &end)) {
        return -1;
    }
    if (lock->type == SRV_F_WRLCK && file->type != PROCESS_FILE_VFS_WRITE) {
        return -1;
    }

    for (;;) {
        if (lock->type == SRV_F_UNLCK) {
            file_lock_release_overlap(process->pid, path, start, end);
            scheduler_wake_all(&file_lock_wait_queue);
            return 0;
        }

        if (file_lock_conflict(path, process->pid, start, end, lock->type) == NULL) {
            file_lock_release_overlap(process->pid, path, start, end);
            int64_t result = file_lock_add(process->pid, path, start, end, lock->type);
            if (result == 0) {
                scheduler_wake_all(&file_lock_wait_queue);
            }
            return result;
        }

        if (!wait) {
            return SRV_ERR_AGAIN;
        }
        scheduler_yield();
    }
}

int64_t process_file_close(struct process *process, uint64_t fd) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL) {
        return -1;
    }

    int64_t result = 0;
    const char *lock_path = NULL;
    if (process != NULL && file_lock_path_for_fd(file, &lock_path)) {
        file_lock_release_path(process->pid, lock_path);
    }
    if ((file->type == PROCESS_FILE_NET_LISTENER ||
            file->type == PROCESS_FILE_NET_CONNECTION ||
            file->type == PROCESS_FILE_NET_UDP) &&
        net_close(file->handle) < 0) {
        result = -1;
    }

    if (file->type == PROCESS_FILE_VFS) {
        read_file_close(file->handle);
    } else if (file->type == PROCESS_FILE_PIPE_READ) {
        pipe_close(file->handle, false);
    } else if (file->type == PROCESS_FILE_PIPE_WRITE) {
        pipe_close(file->handle, true);
    } else if (file->type == PROCESS_FILE_VFS_WRITE) {
        if (write_file_close(file->handle) < 0) {
            result = -1;
        }
    }

    uint8_t *bytes = (uint8_t *)file;
    for (uint64_t i = 0; i < sizeof(*file); i++) {
        bytes[i] = 0;
    }
    return result;
}

int64_t process_file_seek(struct process *process, uint64_t fd, int64_t offset, uint64_t whence) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL ||
        (file->type != PROCESS_FILE_VFS && file->type != PROCESS_FILE_VFS_WRITE)) {
        return -1;
    }

    struct process_write_file *write = NULL;
    struct process_read_file *read = NULL;
    uint64_t current_offset = file->offset;
    uint64_t current_size = file->size;
    if (file->type == PROCESS_FILE_VFS) {
        read = read_file_at(file->handle);
        if (read == NULL) {
            return -1;
        }
        current_offset = read->offset;
        current_size = read->size;
    } else if (file->type == PROCESS_FILE_VFS_WRITE) {
        write = write_file_at(file->handle);
        if (write == NULL) {
            return -1;
        }
        current_offset = write->offset;
        current_size = write->size;
    }

    int64_t base;
    if (whence == 0) {
        base = 0;
    } else if (whence == 1) {
        base = (int64_t)current_offset;
    } else if (whence == 2) {
        base = (int64_t)current_size;
    } else {
        return -1;
    }

    int64_t target = base + offset;
    if (target < 0 ||
        (file->type == PROCESS_FILE_VFS && (uint64_t)target > current_size)) {
        return -1;
    }

    if (write != NULL) {
        write->offset = (uint64_t)target;
        return (int64_t)write->offset;
    }
    if (read != NULL) {
        read->offset = (uint64_t)target;
        return (int64_t)read->offset;
    }
    file->offset = (uint64_t)target;
    return (int64_t)file->offset;
}

int64_t process_file_stat(struct process *process,
    uint64_t fd,
    struct vfs_metadata *metadata_out,
    uint64_t *size_out,
    uint64_t *type_out) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL || size_out == NULL || type_out == NULL) {
        return -1;
    }

    if (file->type == PROCESS_FILE_VFS || file->type == PROCESS_FILE_VFS_WRITE) {
        *type_out = 0;
        if (file->type == PROCESS_FILE_VFS_WRITE) {
            struct process_write_file *write = write_file_at(file->handle);
            if (write == NULL) {
                return -1;
            }
            *size_out = write->size;
            if (!vfs_stat(write->path, metadata_out, NULL, type_out)) {
                if (metadata_out != NULL) {
                    *metadata_out = (struct vfs_metadata) {
                        .inode = 0,
                        .mode = VFS_MODE_IFREG | 0644,
                        .nlink = 1,
                    };
                }
            }
            return 0;
        }
        struct process_read_file *read = read_file_at(file->handle);
        if (read == NULL) {
            return -1;
        }
        *size_out = read->size;
        if (read->node != NULL) {
            *type_out = (uint64_t)read->node->type;
            if (metadata_out != NULL) {
                *metadata_out = read->node->metadata;
            }
        }
        return 0;
    }

    *size_out = 0;
    *type_out = 0;
    if (metadata_out != NULL) {
        *metadata_out = (struct vfs_metadata) {
            .inode = 0,
            .mode = VFS_MODE_IFREG | 0600,
            .nlink = 1,
        };
    }
    return 0;
}

int64_t process_file_chmod(struct process *process, uint64_t fd, uint64_t mode) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL) {
        return -1;
    }
    if (file->type == PROCESS_FILE_VFS && file->path[0] != '\0') {
        return vfs_chmod(file->path, mode) ? 0 : -1;
    }
    if (file->type == PROCESS_FILE_VFS_WRITE) {
        struct process_write_file *write = write_file_at(file->handle);
        if (write != NULL && write->path[0] != '\0') {
            return vfs_chmod(write->path, mode) ? 0 : -1;
        }
    }
    return -1;
}

int64_t process_sbrk(struct process *process, int64_t increment, uint64_t *previous_out) {
    if (process == NULL || previous_out == NULL || process->address_space == 0) {
        return -1;
    }

    uint64_t previous = process->program_break;
    if (increment == 0) {
        *previous_out = previous;
        return 0;
    }

    uint64_t next;
    if (increment > 0) {
        uint64_t amount = (uint64_t)increment;
        if (amount > process->heap_limit - previous) {
            return -1;
        }
        next = previous + amount;
        uint64_t target_mapped = page_up(next);
        for (uint64_t page = process->mapped_break; page < target_mapped; page += PAGE_SIZE) {
            if (!map_process_page(process, page, VMM_PAGE_USER | VMM_PAGE_WRITABLE | VMM_PAGE_NO_EXECUTE)) {
                return -1;
            }
        }
        process->mapped_break = target_mapped;
    } else {
        uint64_t amount = (uint64_t)(-increment);
        if (amount > previous - process->heap_base) {
            return -1;
        }
        next = previous - amount;
    }

    process->program_break = next;
    *previous_out = previous;
    return 0;
}

static bool mmap_range_valid(uint64_t base, uint64_t length) {
    if (length == 0 || (base % PAGE_SIZE) != 0 || base < PAGE_SIZE) {
        return false;
    }
    if (base < USER_MMAP_BASE || base >= USER_MMAP_LIMIT) {
        return false;
    }
    if (length > USER_MMAP_LIMIT - base) {
        return false;
    }
    return true;
}

static bool mmap_range_free(struct process *process, uint64_t base, uint64_t length) {
    for (uint64_t page = base; page < base + length; page += PAGE_SIZE) {
        if (process_mapping_index(process, page) >= 0) {
            return false;
        }
    }
    return true;
}

static int64_t mmap_region_index_for_page(struct process *process, uint64_t page) {
    if (process == NULL) {
        return -1;
    }
    for (uint64_t i = 0; i < PROCESS_MAX_MMAP_REGIONS; i++) {
        if (!process->mmap_regions[i].used) {
            continue;
        }
        uint64_t start = process->mmap_regions[i].base;
        uint64_t end = start + process->mmap_regions[i].length;
        if (page >= start && page < end) {
            return (int64_t)i;
        }
    }
    return -1;
}

static struct process_mmap_region *mmap_free_region_slot(struct process *process) {
    for (uint64_t i = 0; i < PROCESS_MAX_MMAP_REGIONS; i++) {
        if (!process->mmap_regions[i].used) {
            return &process->mmap_regions[i];
        }
    }
    return NULL;
}

static bool mmap_record_region(struct process *process, uint64_t base, uint64_t length) {
    struct process_mmap_region *region = mmap_free_region_slot(process);
    if (region == NULL) {
        return false;
    }
    *region = (struct process_mmap_region) {
        .used = true,
        .base = base,
        .length = length,
    };
    return true;
}

static int64_t mmap_find_free_range(struct process *process, uint64_t length, uint64_t hint) {
    uint64_t start = USER_MMAP_BASE;
    if (hint >= USER_MMAP_BASE && hint < USER_MMAP_LIMIT) {
        start = page_up(hint);
    } else if (process->mmap_next >= USER_MMAP_BASE && process->mmap_next < USER_MMAP_LIMIT) {
        start = page_up(process->mmap_next);
    }

    for (uint64_t pass = 0; pass < 2; pass++) {
        uint64_t cursor = pass == 0 ? start : USER_MMAP_BASE;
        while (cursor < USER_MMAP_LIMIT && length <= USER_MMAP_LIMIT - cursor) {
            if (mmap_range_free(process, cursor, length)) {
                return (int64_t)cursor;
            }
            cursor += length;
        }
    }
    return -1;
}

static bool mmap_can_split_for_unmap(struct process *process, uint64_t base, uint64_t length) {
    uint64_t end = base + length;
    uint64_t needed_slots = 0;
    uint64_t free_slots = 0;
    for (uint64_t i = 0; i < PROCESS_MAX_MMAP_REGIONS; i++) {
        struct process_mmap_region *region = &process->mmap_regions[i];
        if (!region->used) {
            free_slots++;
            continue;
        }
        uint64_t region_start = region->base;
        uint64_t region_end = region_start + region->length;
        if (base <= region_start || end >= region_end || end <= region_start || base >= region_end) {
            continue;
        }
        needed_slots++;
    }
    return needed_slots <= free_slots;
}

static void mmap_forget_range(struct process *process, uint64_t base, uint64_t length) {
    uint64_t end = base + length;
    for (uint64_t i = 0; i < PROCESS_MAX_MMAP_REGIONS; i++) {
        struct process_mmap_region *region = &process->mmap_regions[i];
        if (!region->used) {
            continue;
        }
        uint64_t region_start = region->base;
        uint64_t region_end = region_start + region->length;
        uint64_t overlap_start = base > region_start ? base : region_start;
        uint64_t overlap_end = end < region_end ? end : region_end;
        if (overlap_start >= overlap_end) {
            continue;
        }
        if (overlap_start == region_start && overlap_end == region_end) {
            region->used = false;
            continue;
        }
        if (overlap_start == region_start) {
            region->base = overlap_end;
            region->length = region_end - overlap_end;
            continue;
        }
        if (overlap_end == region_end) {
            region->length = overlap_start - region_start;
            continue;
        }

        uint64_t tail_base = overlap_end;
        uint64_t tail_length = region_end - overlap_end;
        region->length = overlap_start - region_start;
        (void)mmap_record_region(process, tail_base, tail_length);
    }
}

static bool mmap_file_source(struct process *process,
    int64_t fd,
    const uint8_t **data_out,
    uint64_t *size_out) {
    if (data_out == NULL || size_out == NULL || fd < 0) {
        return false;
    }
    struct process_file *file = process_file_at(process, (uint64_t)fd);
    if (file == NULL) {
        return false;
    }
    if (file->type == PROCESS_FILE_VFS) {
        struct process_read_file *read = read_file_at(file->handle);
        if (read == NULL) {
            return false;
        }
        *data_out = read->data;
        *size_out = read->size;
        return true;
    }
    if (file->type == PROCESS_FILE_VFS_WRITE) {
        struct process_write_file *write = write_file_at(file->handle);
        if (write == NULL) {
            return false;
        }
        *data_out = write->data;
        *size_out = write->size;
        return true;
    }
    return false;
}

static bool mmap_protection_valid(uint64_t protection) {
    return (protection & ~(SRV_PROT_READ | SRV_PROT_WRITE | SRV_PROT_EXEC)) == 0;
}

static uint64_t mmap_page_flags_for_protection(uint64_t protection) {
    uint64_t page_flags = VMM_PAGE_NO_EXECUTE;
    if (protection != SRV_PROT_NONE) {
        page_flags |= VMM_PAGE_USER;
    }
    if ((protection & SRV_PROT_WRITE) != 0) {
        page_flags |= VMM_PAGE_WRITABLE;
    }
    if ((protection & SRV_PROT_EXEC) != 0) {
        page_flags &= ~VMM_PAGE_NO_EXECUTE;
    }
    return page_flags;
}

static bool mmap_apply_protection(struct process *process,
    uint64_t page,
    uint64_t page_flags) {
    int64_t index = process_mapping_index(process, page);
    if (index < 0) {
        return false;
    }
    if (!vmm_protect_page_in_space(process->address_space, page, page_flags)) {
        return false;
    }
    process->mappings[index].flags = page_flags;
    return true;
}

int64_t process_mmap(struct process *process,
    uint64_t address,
    uint64_t length,
    uint64_t protection,
    uint64_t flags,
    int64_t fd,
    int64_t offset) {
    if (process == NULL || process->address_space == 0 || length == 0) {
        return -1;
    }
    if ((flags & ~(SRV_MAP_ANONYMOUS | SRV_MAP_PRIVATE | SRV_MAP_SHARED | SRV_MAP_FIXED)) != 0 ||
        (flags & SRV_MAP_PRIVATE) == 0 ||
        (flags & SRV_MAP_SHARED) != 0) {
        return -1;
    }
    if (!mmap_protection_valid(protection)) {
        return -1;
    }

    const bool anonymous = (flags & SRV_MAP_ANONYMOUS) != 0;
    const uint8_t *file_data = NULL;
    uint64_t file_size = 0;
    if (anonymous) {
        if (fd != -1 || offset != 0) {
            return -1;
        }
    } else {
        if (fd < 0 || offset < 0 || (((uint64_t)offset) % PAGE_SIZE) != 0 ||
            !mmap_file_source(process, fd, &file_data, &file_size)) {
            return -1;
        }
    }

    uint64_t mapped_length = page_up(length);
    if (mapped_length < length) {
        return -1;
    }

    uint64_t base = 0;
    if ((flags & SRV_MAP_FIXED) != 0) {
        base = address;
        if (!mmap_range_valid(base, mapped_length) ||
            !mmap_range_free(process, base, mapped_length)) {
            return -1;
        }
    } else {
        int64_t found = mmap_find_free_range(process, mapped_length, address);
        if (found < 0) {
            return -1;
        }
        base = (uint64_t)found;
    }

    uint64_t page_flags = mmap_page_flags_for_protection(protection);
    uint64_t initial_page_flags = page_flags;
    if (protection == SRV_PROT_NONE) {
        initial_page_flags = VMM_PAGE_USER | VMM_PAGE_NO_EXECUTE;
    }

    uint64_t mapped = 0;
    for (; mapped < mapped_length; mapped += PAGE_SIZE) {
        if (!map_process_page(process, base + mapped, initial_page_flags)) {
            for (uint64_t rollback = 0; rollback < mapped; rollback += PAGE_SIZE) {
                (void)process_unmap_tracked_page(process, base + rollback);
            }
            return -1;
        }
    }

    if (!anonymous && (uint64_t)offset < file_size) {
        uint64_t available = file_size - (uint64_t)offset;
        uint64_t copy_length = length < available ? length : available;
        if (copy_length != 0 &&
            !user_space_write(process, base, file_data + (uint64_t)offset, copy_length)) {
            for (uint64_t rollback = 0; rollback < mapped_length; rollback += PAGE_SIZE) {
                (void)process_unmap_tracked_page(process, base + rollback);
            }
            return -1;
        }
    }

    if (protection == SRV_PROT_NONE) {
        for (uint64_t protected = 0; protected < mapped_length; protected += PAGE_SIZE) {
            if (!mmap_apply_protection(process, base + protected, page_flags)) {
                for (uint64_t rollback = 0; rollback < mapped_length; rollback += PAGE_SIZE) {
                    (void)process_unmap_tracked_page(process, base + rollback);
                }
                return -1;
            }
        }
    }

    if (!mmap_record_region(process, base, mapped_length)) {
        for (uint64_t rollback = 0; rollback < mapped_length; rollback += PAGE_SIZE) {
            (void)process_unmap_tracked_page(process, base + rollback);
        }
        return -1;
    }

    uint64_t next = base + mapped_length;
    process->mmap_next = next < USER_MMAP_LIMIT ? next : USER_MMAP_BASE;
    return (int64_t)base;
}

int64_t process_munmap(struct process *process, uint64_t address, uint64_t length) {
    if (process == NULL || process->address_space == 0 || length == 0 || (address % PAGE_SIZE) != 0) {
        return -1;
    }
    uint64_t mapped_length = page_up(length);
    if (mapped_length < length || !mmap_range_valid(address, mapped_length)) {
        return -1;
    }
    if (!mmap_can_split_for_unmap(process, address, mapped_length)) {
        return -1;
    }
    for (uint64_t page = address; page < address + mapped_length; page += PAGE_SIZE) {
        if (mmap_region_index_for_page(process, page) < 0 ||
            process_mapping_index(process, page) < 0) {
            return -1;
        }
    }
    for (uint64_t page = address; page < address + mapped_length; page += PAGE_SIZE) {
        (void)process_unmap_tracked_page(process, page);
    }
    mmap_forget_range(process, address, mapped_length);
    if (address < process->mmap_next) {
        process->mmap_next = address;
    }
    return 0;
}

int64_t process_mprotect(struct process *process,
    uint64_t address,
    uint64_t length,
    uint64_t protection) {
    if (process == NULL || process->address_space == 0 || length == 0 ||
        (address % PAGE_SIZE) != 0 ||
        !mmap_protection_valid(protection)) {
        return -1;
    }
    uint64_t mapped_length = page_up(length);
    if (mapped_length < length || !mmap_range_valid(address, mapped_length)) {
        return -1;
    }
    for (uint64_t page = address; page < address + mapped_length; page += PAGE_SIZE) {
        if (mmap_region_index_for_page(process, page) < 0 ||
            process_mapping_index(process, page) < 0) {
            return -1;
        }
    }

    uint64_t page_flags = mmap_page_flags_for_protection(protection);
    for (uint64_t page = address; page < address + mapped_length; page += PAGE_SIZE) {
        if (!mmap_apply_protection(process, page, page_flags)) {
            return -1;
        }
    }
    return 0;
}

int64_t process_msync(struct process *process,
    uint64_t address,
    uint64_t length,
    uint64_t flags) {
    if (process == NULL || process->address_space == 0 || length == 0 ||
        (address % PAGE_SIZE) != 0 ||
        (flags & ~(SRV_MS_ASYNC | SRV_MS_SYNC | SRV_MS_INVALIDATE)) != 0 ||
        ((flags & SRV_MS_ASYNC) != 0 && (flags & SRV_MS_SYNC) != 0)) {
        return -1;
    }
    uint64_t mapped_length = page_up(length);
    if (mapped_length < length || !mmap_range_valid(address, mapped_length)) {
        return -1;
    }
    for (uint64_t page = address; page < address + mapped_length; page += PAGE_SIZE) {
        if (mmap_region_index_for_page(process, page) < 0 ||
            process_mapping_index(process, page) < 0) {
            return -1;
        }
    }
    return 0;
}

void process_exit(uint64_t status) {
    struct process *process = process_current();
    if (process == NULL || !process->active) {
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    struct process_user_thread *thread = process_current_user_thread();
    if (thread != NULL) {
        __asm__ volatile ("cli" : : : "memory");
        process->exit_status = status;
        process->exiting = true;
        thread->active = false;
        net_process_wake(process);
        scheduler_wake_all(&pipe_wait_queue);
        scheduler_wake_all(&process->thread_wait_queue);
        process_file_poll_wake();
        process_futex_wake_process(process);
        scheduler_clear_user_context();
        __asm__ volatile ("sti" : : : "memory");
        scheduler_exit_current();
    }

    __asm__ volatile ("cli" : : : "memory");
    process->exit_status = status;
    scheduler_kill_user_threads(process, NULL);
    process_futex_wake_process(process);
    for (uint64_t i = 0; i < PROCESS_MAX_USER_THREADS; i++) {
        process->user_threads[i].used = false;
        process->user_threads[i].active = false;
    }
    net_process_cleanup(process);
    cleanup_process_files(process);
    process->active = false;
    scheduler_clear_user_context();
    cleanup_process_address_space(process);

    process_context_restore(&process->context, 1);
}

void process_fpu_save(void *process_ptr) {
    struct process *process = process_ptr;
    if (process == NULL || !process->allocated) {
        return;
    }
    fpu_save(&process->fpu);
}

struct fpu_state *process_fpu_state(void *process_ptr) {
    struct process *process = process_ptr;
    if (process == NULL || !process->allocated) {
        return NULL;
    }
    return &process->fpu;
}

void process_fpu_restore(void *process_ptr) {
    struct process *process = process_ptr;
    if (process == NULL || !process->allocated) {
        return;
    }
    fpu_restore(&process->fpu);
}
