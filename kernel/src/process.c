#include <srvros/console.h>
#include <srvros/exfat.h>
#include <srvros/heap.h>
#include <srvros/net.h>
#include <srvros/pmm.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <srvros/syscall_numbers.h>
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
#define PROCESS_MAX_MAPPINGS 4096
#define PROCESS_NAME_MAX 64
#define PROCESS_KERNEL_STACK_SIZE 131072
#define PROCESS_KERNEL_STACK_MIN_PHYSICAL 0x100000ull
#define PROCESS_USER_PAGE_MIN_PHYSICAL 0x4000000ull
#define PROCESS_MAX_ARGS 16
#define PROCESS_ARGS_MAX 256
#define PROCESS_PATH_MAX 160
#define PROCESS_WRITE_BUFFER_MAX (256 * 1024)

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

struct process {
    bool active;
    bool allocated;
    bool detached;
    bool killed;
    bool reapable;
    uint64_t pid;
    uint64_t exit_status;
    uint64_t entry;
    uint64_t stack_top;
    uint64_t argc;
    uint64_t argv;
    uint64_t address_space;
    uint8_t *kernel_stack;
    uint64_t kernel_stack_physical;
    uint64_t kernel_stack_frames;
    uint64_t kernel_stack_top;
    bool stdout_redirect;
    bool stdout_append;
    bool stdout_written;
    int64_t stdout_fd;
    char stdout_path[PROCESS_PATH_MAX];
    char name[PROCESS_NAME_MAX];
    struct process_context context;
    struct process_file files[PROCESS_MAX_OPEN_FILES];
    struct process_mapping mappings[PROCESS_MAX_MAPPINGS];
    uint64_t mapping_count;
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

void usermode_enter(uint64_t entry, uint64_t stack_top, uint64_t argc, uint64_t argv) __attribute__((noreturn));
uint64_t process_context_save(struct process_context *context) __attribute__((returns_twice));
void process_context_restore(struct process_context *context, uint64_t value) __attribute__((noreturn));
uint64_t gdt_default_kernel_stack_top(void);
void gdt_set_kernel_stack(uint64_t stack_top);

static struct process processes[PROCESS_MAX_PROCESSES];
static struct process *loading_process;
static uint64_t next_pid = 1;
static struct scheduler_wait_queue process_wait_queue;

static void set_process_name(struct process *process, const char *path);

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

static bool map_segment_page(uint64_t virtual_page, uint64_t flags) {
    struct process *process = loading_process;
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
    loading_process = NULL;
    vfs_release_data(node, data);
    return true;
}

static const char *process_state(const struct process *process) {
    if (process == NULL || !process->allocated) {
        return "unused";
    }
    if (process->killed) {
        return "stopping";
    }
    if (process->active) {
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
        console_printf("%s: entering pid=%u %s entry=%x stack=%x\n",
            label,
            process->pid,
            process->name,
            process->entry,
            process->stack_top);
        usermode_enter(process->entry, process->stack_top, process->argc, process->argv);
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
        usermode_enter(child->entry, child->stack_top, child->argc, child->argv);
    }

    int64_t status = (int64_t)child->exit_status;
    scheduler_set_user_context(parent, parent->address_space, parent->kernel_stack_top);
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
        usermode_enter(child->entry, child->stack_top, child->argc, child->argv);
    }

    int64_t status = (int64_t)child->exit_status;
    scheduler_set_user_context(parent, parent->address_space, parent->kernel_stack_top);
    release_process(child);
    return status;
}

static void background_process_thread(void *arg) {
    struct process *process = arg;
    if (process == NULL) {
        return;
    }

    (void)enter_process(process, "bg");
    console_printf("bg: pid=%u exited status=%u\n", process->pid, process->exit_status);
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

int64_t process_spawn_background_elf_args(const char *path, const char *args) {
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
    if (!scheduler_spawn("user", background_process_thread, process)) {
        cleanup_process_address_space(process);
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
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        if (processes[i].allocated && processes[i].active && processes[i].pid == pid) {
            processes[i].killed = true;
            net_process_wake(&processes[i]);
            return true;
        }
    }
    return false;
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

    process->killed = true;
    net_process_wake(process);
    return true;
}

bool process_should_exit_current(void) {
    struct process *process = process_current();
    return process != NULL && process->killed;
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
                if (file->node != NULL && path_under_prefix(file->node->path, prefix)) {
                    return true;
                }
            } else if (file->type == PROCESS_FILE_VFS_WRITE) {
                if (path_under_prefix(file->path, prefix)) {
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

uint64_t process_pid(const struct process *process) {
    return process != NULL ? process->pid : 0;
}

const char *process_name(const struct process *process) {
    return process != NULL ? process->name : "";
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

int64_t process_stdout_write(struct process *process, const uint8_t *buffer, uint64_t length) {
    if (process == NULL || !process->stdout_redirect) {
        return -2;
    }
    if (buffer == NULL) {
        return -1;
    }
    if (length == 0) {
        return 0;
    }

    int64_t written = process_file_write(process, (uint64_t)process->stdout_fd, buffer, length);
    if (written < 0) {
        return written;
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

    for (uint64_t i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        if (!process->files[i].used) {
            struct process_file *file = &process->files[i];
            file->used = true;
            file->type = PROCESS_FILE_VFS;
            file->node = node;
            file->data = data;
            file->size = size;
            file->offset = 0;
            return (int64_t)(i + 3);
        }
    }

    return -1;
}

static bool ensure_write_capacity(struct process_file *file, uint64_t needed) {
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

    if (!exists && (flags & SRV_OPEN_CREATE) == 0) {
        return -1;
    }
    if (exists && !trunc && !append) {
        append = true;
    }

    if (exists && !trunc && !vfs_read_all(node, &old_data, &old_size)) {
        return -1;
    }

    for (uint64_t i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        if (process->files[i].used) {
            continue;
        }

        struct process_file *file = &process->files[i];
        uint8_t *bytes = (uint8_t *)file;
        for (uint64_t j = 0; j < sizeof(*file); j++) {
            bytes[j] = 0;
        }

        file->used = true;
        file->type = PROCESS_FILE_VFS_WRITE;
        file->size = 0;
        file->offset = 0;
        file->dirty = trunc;
        if (!copy_process_path(file->path, sizeof(file->path), path) ||
            (old_size != 0 && !ensure_write_capacity(file, old_size))) {
            file->used = false;
            if (old_data != NULL) {
                vfs_release_data(node, old_data);
            }
            return -1;
        }
        for (uint64_t j = 0; j < old_size; j++) {
            ((uint8_t *)file->data)[j] = old_data[j];
        }
        file->size = trunc ? 0 : old_size;
        file->offset = append ? old_size : 0;
        if (old_data != NULL) {
            vfs_release_data(node, old_data);
        }
        return (int64_t)(i + 3);
    }

    if (old_data != NULL) {
        vfs_release_data(node, old_data);
    }
    return -1;
}

int64_t process_file_write(struct process *process, uint64_t fd, const uint8_t *buffer, uint64_t length) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL || file->type != PROCESS_FILE_VFS_WRITE || (buffer == NULL && length != 0)) {
        return -1;
    }
    if (length == 0) {
        return 0;
    }

    uint64_t needed = file->offset + length;
    if (needed < file->offset || needed > PROCESS_WRITE_BUFFER_MAX || !ensure_write_capacity(file, needed)) {
        file->failed = true;
        return -1;
    }

    for (uint64_t i = file->size; i < file->offset; i++) {
        ((uint8_t *)file->data)[i] = 0;
    }
    for (uint64_t i = 0; i < length; i++) {
        ((uint8_t *)file->data)[file->offset + i] = buffer[i];
    }

    file->offset += length;
    if (file->offset > file->size) {
        file->size = file->offset;
    }
    file->dirty = true;
    return (int64_t)length;
}

int64_t process_handle_alloc(struct process *process,
    enum process_file_type type,
    uint64_t handle) {
    if (process == NULL ||
        (type != PROCESS_FILE_NET_LISTENER && type != PROCESS_FILE_NET_CONNECTION) ||
        handle == 0) {
        return -1;
    }

    for (uint64_t i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        if (!process->files[i].used) {
            struct process_file *file = &process->files[i];
            file->used = true;
            file->type = type;
            file->handle = handle;
            return (int64_t)(i + 3);
        }
    }

    return -1;
}

int64_t process_file_close(struct process *process, uint64_t fd) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL) {
        return -1;
    }

    int64_t result = 0;
    if ((file->type == PROCESS_FILE_NET_LISTENER ||
            file->type == PROCESS_FILE_NET_CONNECTION) &&
        net_close(file->handle) < 0) {
        result = -1;
    }

    if (file->type == PROCESS_FILE_VFS) {
        vfs_release_data(file->node, file->data);
    } else if (file->type == PROCESS_FILE_VFS_WRITE) {
        if (file->failed) {
            result = -1;
        } else if (file->dirty) {
            bool ok = false;
            if (file->size == 0) {
                ok = vfs_lookup(file->path) == NULL || exfat_delete_file(file->path);
            } else {
                ok = exfat_write_file(file->path, file->data, file->size);
                if (!ok && vfs_lookup(file->path) != NULL) {
                    ok = exfat_delete_file(file->path) &&
                        exfat_write_file(file->path, file->data, file->size);
                }
            }
            if (!ok) {
                result = -1;
            }
        }
        if (file->data != NULL) {
            kfree((void *)file->data);
        }
    }

    uint8_t *bytes = (uint8_t *)file;
    for (uint64_t i = 0; i < sizeof(*file); i++) {
        bytes[i] = 0;
    }
    return result;
}

int64_t process_file_seek(struct process *process, uint64_t fd, uint64_t offset) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL ||
        (file->type != PROCESS_FILE_VFS && file->type != PROCESS_FILE_VFS_WRITE) ||
        (file->type == PROCESS_FILE_VFS && offset > file->size)) {
        return -1;
    }

    file->offset = offset;
    return (int64_t)file->offset;
}

void process_exit(uint64_t status) {
    struct process *process = process_current();
    if (process == NULL || !process->active) {
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    process->exit_status = status;
    net_process_cleanup(process);
    cleanup_process_files(process);
    process->active = false;
    vmm_switch_address_space(vmm_kernel_address_space());
    cleanup_process_address_space(process);

    process_context_restore(&process->context, 1);
}
