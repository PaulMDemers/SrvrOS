#include <srvros/session_scope.h>
#include <srvros/sync_timing.h>
#include "capture_write_timing_state.h"
#include <srvros/path_limits.h>
#include <srvros/audio.h>
#include "arch/x86_64/isr_frame.h"
#include "arch/x86_64/user_frame_copy.h"
#include "arch/x86_64/fpu_trap.h"
#include "process_signal.h"
#include <srvros/console.h>
#include <srvros/exfat.h>
#include <srvros/fpu.h>
#include <srvros/gui.h>
#include <srvros/heap.h>
#include <srvros/latency.h>
#include <srvros/keyboard.h>
#include <srvros/net.h>
#include <srvros/pmm.h>
#include <srvros/process.h>
#include <srvros/file_stream_kernel.h>
#include <srvros/exfat_stream.h>
#include <srvros/pty.h>
#include <srvros/scheduler.h>
#include <srvros/syscall_numbers.h>
#include <srvros/timer.h>
#include <srvros/vfs.h>
#include <srvros/vfs_namespace.h>
#include <srvros/vmm.h>

#include <stddef.h>
#include <stdint.h>

_Static_assert(sizeof(struct isr_frame)==176 && offsetof(struct isr_frame,rip)==136, "usermode_resume frame layout");

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
#define PT_TLS 7
#define PF_X 1
#define PF_W 2
#define PAGE_SIZE 4096ull
#define USER_STACK_TOP 0x1000000ull
#define USER_STACK_PAGES 512
#define USER_STACK_GUARD_PAGES 1
#define USER_HEAP_LIMIT (USER_STACK_TOP - (USER_STACK_PAGES * PAGE_SIZE) - PAGE_SIZE)
#define USER_MMAP_BASE 0x10000000ull
#define USER_MMAP_LIMIT 0x70000000ull
#define USER_DYNAMIC_STACK_TOP (USER_MMAP_BASE - PAGE_SIZE)
#define USER_TLS_BASE 0x70000000ull
#define USER_TLS_LIMIT 0x74000000ull
#define USER_TLS_TCB_SIZE 16
#define PROCESS_TLS_TEMPLATE_MAX 4096
#define PROCESS_MAX_MAPPINGS 65536
#define PROCESS_MAX_MMAP_REGIONS 512
#define PROCESS_NAME_MAX 64
#define PROCESS_KERNEL_STACK_SIZE 131072
#define PROCESS_KERNEL_STACK_MIN_PHYSICAL 0x100000ull
#define PROCESS_USER_PAGE_MIN_PHYSICAL 0x4000000ull
#include <srvros/exec_limits.h>
#define PROCESS_MAX_ARGS SRV_EXEC_ARGS_MAX
#define PROCESS_MAX_ENV SRV_EXEC_ENV_MAX
#define PROCESS_ARGS_MAX 256
#define PROCESS_PATH_MAX SRV_PATH_MAX
#define PROCESS_WRITE_BUFFER_MAX (16 * 1024 * 1024)
#define PROCESS_MAX_WRITE_FILES 64
#define PROCESS_MAX_READ_FILES 128
#define PROCESS_MAX_PIPES 64
#define PROCESS_PIPE_RIGHTS_QUEUE 8
#define PROCESS_MAX_UNIX_LISTENERS 16
#define PROCESS_UNIX_ACCEPT_QUEUE 16
#define PROCESS_MAX_FILE_LOCKS 64
/* A desktop worker can transfer one bounded 64 KiB batch per UI turn.
 * Fixed storage is 4 MiB across the 64-pipe pool; no allocation in I/O paths. */
#define PROCESS_PIPE_CAPACITY (64 * 1024)
#define PROCESS_STDIO_CLOSED_FD (-2)
#define PROCESS_MAX_USER_THREADS 8
#define PROCESS_MAX_FUTEXES 64
#define PROCESS_EXIT_STATUS_CACHE 128

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
    bool anonymous;
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
    uint64_t tls_base;
    uint64_t tls_thread_pointer;
    uint64_t return_value;
    uint64_t signal_mask;
    uint64_t pending_signals;
    struct signal_thread_state signal_state;
    uint64_t user_us,system_us;
    struct process *owner;
    struct process_context context;
    struct fpu_state fpu;
};

struct process_pipe_rights {
    bool used;
    uint64_t offset;
    uint64_t count;
    struct process_file files[SRV_RIGHTS_MAX];
};

struct process_pipe {
    bool used;
    uint64_t refs;
    uint64_t read_refs;
    uint64_t write_refs;
    uint64_t read_flags,write_flags;
    uint64_t read_pos;
    uint64_t write_pos;
    uint64_t size;
    uint8_t buffer[PROCESS_PIPE_CAPACITY];
    struct process_pipe_rights rights[PROCESS_PIPE_RIGHTS_QUEUE];
};
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
static bool process_experimental_pipe_simple(const struct process_pipe *pipe) {
    if(!pipe || !pipe->used)return false;
    for(unsigned i=0;i<PROCESS_PIPE_RIGHTS_QUEUE;i++)if(pipe->rights[i].used)return false;
    return true;
}
#endif

struct process_unix_pending {
    bool used;
    struct process_file file;
};

struct process_unix_listener {
    uint64_t refs,status_flags;
    bool used;
    bool bound;
    bool listening;
    uint64_t backlog;
    char path[SRV_UNIX_PATH_MAX];
    struct process_unix_pending queue[PROCESS_UNIX_ACCEPT_QUEUE];
};

/* One content object per live pathname; unlinked objects retain their identity
 * until last close. Offsets/status belong to separate open descriptions. */
struct process_file_content {
    uint64_t refs,diagnostic_identity;
    const struct vfs_node *node;
    const uint8_t *data;
    uint64_t size,capacity;
    uint64_t dirty_generation,committed_size;
    struct {
        bool active;
        const uint8_t *data;
        uint64_t size,generation,append_calls,append_bytes;
    } writeback;
    bool owned,dirty,failed,unlinked;
    struct vfs_metadata orphan_metadata;
    char path[PROCESS_PATH_MAX];
};
struct process_write_file {
    bool used;
    uint64_t refs,open_flags,offset;
    struct process_file_content *content;
    bool fresh_exclusive;
    struct {
        bool valid;
        struct process *owner;
        struct process_file_content *content;
        const uint8_t *data;
        uint64_t image_generation,fd,fd_generation,capacity,limit,open_flags;
    } append;
    char path[PROCESS_PATH_MAX];
};
struct process_read_file {
    bool used,directory;
    uint64_t refs;
    const struct vfs_node *node;
    struct process_file_content *content;
    uint64_t offset,status_flags;
    char path[PROCESS_PATH_MAX];
};

struct process_file_lock {
    bool used;
    uint64_t pid;
    uint64_t start;
    uint64_t end;
    int16_t type;
    struct process_file_content *content;
};

struct process {
    bool active;
    bool allocated;
    bool detached;
    bool killed;
    bool exiting;
    bool reapable;
    bool quiet;
    bool startup_ready;
    uint64_t kill_signal;
    uint64_t signal_catch_mask;
    uint64_t signal_ignore_mask;
    uint64_t pending_signals;
    uint64_t main_signal_mask;
    uint64_t main_pending_signals;
    struct signal_action_descriptor usr1_action;
    uint64_t usr1_action_generation;
    struct signal_thread_state main_signal_state;
    bool main_thread_exited;
    bool main_thread_joined, main_thread_detached, main_thread_join_claimed;
    uint64_t main_thread_return_value;
    uint64_t user_us,system_us,main_user_us,main_system_us;
    uint64_t child_user_us,child_system_us,child_peak_bytes,peak_mapped_bytes;
    struct srv_rlimit fsize_limit;
    uint64_t creation_mask;
    char cwd[PROCESS_PATH_MAX];
    char cwd_storage[PROCESS_PATH_MAX];
    uint64_t process_group;
    uint64_t lifetime; /* Immutable across exec, job control and orphaning. */
    uint64_t owned_user_scope;
    uint64_t session_id;
    uint64_t controlling_tty;
    uint64_t pid;
    uint64_t stream_instance; /* Never copied into a child or recycled slot. */
    uint64_t parent_pid;
    uint64_t exit_status;
    uint64_t entry;
    uint64_t stack_top;
    uint64_t argc;
    uint64_t argv;
    uint64_t envp;
    uint64_t address_space;
    uint64_t image_generation;
    uint64_t heap_base;
    uint64_t program_break;
    uint64_t mapped_break;
    uint64_t heap_limit;
    uint64_t tls_filesz;
    uint64_t tls_memsz;
    uint64_t tls_align;
    uint64_t tls_base;
    uint64_t tls_thread_pointer;
    uint8_t tls_template[PROCESS_TLS_TEMPLATE_MAX];
    uint8_t *kernel_stack;
    uint64_t kernel_stack_physical;
    uint64_t kernel_stack_frames;
    uint64_t kernel_stack_top;
    char name[PROCESS_NAME_MAX];
    char executable_path[PROCESS_PATH_MAX];
    bool fork_resume;
    struct isr_frame fork_frame;
    struct process_context context;
    struct fpu_state fpu;
    struct process_file files[PROCESS_MAX_OPEN_FILES];
    uint64_t file_generations[PROCESS_MAX_OPEN_FILES];
    /* Full-capacity image-owned backing, acquired before its first user page.
     * Only [0,mapping_count) is published. Empty slots own none. */
    struct process_mapping *mappings;
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
    uint64_t active_waits; /* Includes runnable and not-yet-enqueued wait calls. */
    bool invalidated; /* A removed/revoked mapping must not bind a replacement. */
};

struct process_exit_record {
    bool used;
    uint64_t pid;
    uint64_t status;
    uint64_t sequence;
};

void usermode_enter(uint64_t entry,
    uint64_t stack_top,
    uint64_t argc,
    uint64_t argv,
    uint64_t envp,
    uint64_t fs_base) __attribute__((noreturn));
void usermode_resume(const struct isr_frame *frame,uint64_t fs_base) __attribute__((noreturn));
static bool process_signal_mask_current_locked(uint64_t how, uint64_t set, uint64_t *oldset_out);
uint64_t process_context_save(struct process_context *context) __attribute__((returns_twice));
void process_context_restore(struct process_context *context, uint64_t value) __attribute__((noreturn));
uint64_t gdt_default_kernel_stack_top(void);
void gdt_set_kernel_stack(uint64_t stack_top);

static struct process processes[PROCESS_MAX_PROCESSES];
static uint64_t system_reaper_pid;
static struct process_file_content file_contents[PROCESS_MAX_WRITE_FILES+PROCESS_MAX_READ_FILES];
static struct process_write_file write_files[PROCESS_MAX_WRITE_FILES];
static struct process_read_file read_files[PROCESS_MAX_READ_FILES];
static struct process_pipe pipes[PROCESS_MAX_PIPES];
static struct process_unix_listener unix_listeners[PROCESS_MAX_UNIX_LISTENERS];
static struct process_file_lock file_locks[PROCESS_MAX_FILE_LOCKS];
static struct process_futex futexes[PROCESS_MAX_FUTEXES];
static struct process_exit_record exit_records[PROCESS_EXIT_STATUS_CACHE];
/* Reserve slots while their large mapping arrays are initialized or cleared.
 * This keeps preemptible work outside the short publication critical section. */
static bool process_slots_busy[PROCESS_MAX_PROCESSES];
/* CPU0 process management: short IRQ-masked publication, preemptible loading
 * and destruction. Members include reservations and final resource release. */
struct process_lifetime {
    uint64_t token, members, peak;
    bool revoked, root_reserved, user, reservation;
    struct process *owner;
    uint64_t nonzero_exits, first_exit_status;
};
static struct process_lifetime process_lifetimes[PROCESS_MAX_PROCESSES];
static uint64_t next_lifetime = 1;
static uint64_t next_image_generation;
static uint64_t next_pid = 1;
static uint64_t next_stream_instance = 1;
static uint64_t next_exit_record_sequence;
static struct scheduler_wait_queue process_wait_queue;
static struct scheduler_wait_queue pipe_wait_queue;
static struct scheduler_wait_queue unix_wait_queue;
static struct scheduler_wait_queue fd_poll_wait_queue;
static struct scheduler_wait_queue file_lock_wait_queue;
static uint64_t foreground_process_group;

static void set_process_name(struct process *process, const char *path);
static struct process_read_file *read_file_at(uint64_t handle);
static struct process_write_file *write_file_at(uint64_t handle);
static void background_process_thread(void *arg);
static void process_user_thread_start(void *arg);
static void process_futex_wake_process(struct process *process);
static void process_futex_cleanup_process(struct process *process);
static void process_futex_invalidate_page(struct process *process, uint64_t page);
static bool mmap_range_free(struct process *process, uint64_t base, uint64_t length);
static bool process_configure_stdio_fds(struct process *child,
    struct process *parent,
    int64_t stdin_fd,
    int64_t stdout_fd,
    int64_t stderr_fd);
static bool process_apply_stdio_fds(struct process *process,
    int64_t stdin_fd,
    int64_t stdout_fd,
    int64_t stderr_fd);

/* These privileged transitions are not host fixture operations. Keep native
 * instructions exact; unexpected host entry must fail rather than emulate it. */
#ifdef SRVROS_PROCESS_HOST_TEST
#define PROCESS_ARCH_STI() __builtin_trap()
#define PROCESS_ARCH_HALT() __builtin_trap()
#else
#define PROCESS_ARCH_STI() __asm__ volatile ("sti" : : : "memory")
#define PROCESS_ARCH_HALT() __asm__ volatile ("hlt")
#endif

static uint64_t process_irq_save(void) {
#ifdef SRVROS_PROCESS_IRQ_HOST_TEST
    return process_test_irq_save();
#elif defined(SRVROS_PROCESS_HOST_TEST)
    return 0;
#else
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
#endif
}

static void process_irq_restore(uint64_t flags) {
#ifdef SRVROS_PROCESS_IRQ_HOST_TEST
    process_test_irq_restore(flags);
#elif defined(SRVROS_PROCESS_HOST_TEST)
    (void)flags;
#else
    if ((flags & (1ull << 9)) != 0) {
        __asm__ volatile ("sti" : : : "memory");
    }
#endif
}

/* Fixed embedded private storage; retirement never follows user-backed links.
 * Called before an image/stack/slot can become stale, with IRQs excluded. */
static void process_signal_clear_records(struct process *process) {
    process->main_signal_state=(struct signal_thread_state){0};
    for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++)
        process->user_threads[i].signal_state=(struct signal_thread_state){0};
}
static bool process_signal_records_active(const struct process *process) {
    if(process->main_signal_state.stack.depth) return true;
    for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++)
        if(process->user_threads[i].signal_state.stack.depth) return true;
    return false;
}
static void process_signal_retire_image(struct process *process) {
    process_signal_clear_records(process);
    process->signal_catch_mask &= ~(1ull<<SRV_SIGNAL_USR1);
    process->usr1_action=(struct signal_action_descriptor){
        .mode=(process->signal_ignore_mask&(1ull<<SRV_SIGNAL_USR1)) ? SIGNAL_ACTION_IGNORE : SIGNAL_ACTION_DEFAULT};
    process->usr1_action_generation=0; /* New committed image has a different identity. */
}

/* IDs are never recycled, including after a failed staged image. */
static int64_t process_prepare_image(struct process *image) {
    uint64_t irq=process_irq_save();
    if (!image || image->image_generation || !image->address_space ||
        !vmm_prepare_user_kernel_mappings(image->address_space)) {
        process_irq_restore(irq);return SRV_ERR_IO;
    }
    if (next_image_generation == UINT64_MAX) {
        process_irq_restore(irq);return SRV_ERR_AGAIN;
    }
    image->image_generation=++next_image_generation;
    process_irq_restore(irq);return 0;
}

bool process_resolve_user_binding(void *candidate, void *worker,
    uint64_t generation, uint64_t root, struct process_user_binding *out) {
    uint64_t flags=0;
#ifndef SRVROS_PROCESS_HOST_TEST
    __asm__ volatile ("pushfq; popq %0" : "=r"(flags) : : "memory");
#elif defined(SRVROS_PROCESS_IRQ_HOST_TEST)
    flags=process_test_rflags();
#endif
    if ((flags & (1ull << 9)) || !out || !candidate || !generation ||
        generation > next_image_generation || !root ||
        (root & ~0x000ffffffffff000ull) || root == vmm_kernel_address_space()) return false;
    struct process *process=NULL;
    unsigned slot=0;
    for (unsigned i=0;i<PROCESS_MAX_PROCESSES;i++) {
        if (candidate == &processes[i]) { process=&processes[i];slot=i;break; }
    }
    if (!process || process_slots_busy[slot] || !process->allocated || !process->active ||
        process->reapable || process->image_generation != generation ||
        process->address_space != root) return false;
    struct process_user_thread *thread=NULL;
    unsigned tls_slot=0;
    if (worker) {
        for (unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++)
            if (worker == &process->user_threads[i]) {thread=&process->user_threads[i];tls_slot=i+1;break;}
        if (!thread || !thread->used || !thread->active || thread->owner != process || thread->tid < 2)
            return false;
    }
    struct fpu_state *fpu=thread ? &thread->fpu : &process->fpu;
    if (((uintptr_t)fpu & 15) || fpu->active_user_trap ||
        (fpu_available() && !fpu->initialized)) return false;
    uint64_t base=thread ? thread->tls_base : process->tls_base;
    uint64_t fs=thread ? thread->tls_thread_pointer : process->tls_thread_pointer;
    if (!process->tls_memsz) {
        if (process->tls_filesz || base || fs) return false;
    } else {
        uint64_t align=process->tls_align;
        if (process->tls_filesz > process->tls_memsz || process->tls_memsz > PROCESS_TLS_TEMPLATE_MAX ||
            !align || align > PAGE_SIZE || (align & (align-1))) return false;
        uint64_t block=(process->tls_memsz+align-1)&~(align-1);
        uint64_t length=(block+USER_TLS_TCB_SIZE+PAGE_SIZE-1)&~(PAGE_SIZE-1);
        if (!length || length > (USER_TLS_LIMIT-USER_TLS_BASE)/(PROCESS_MAX_USER_THREADS+1) ||
            base != USER_TLS_BASE+tls_slot*length || fs != base+block ||
            base+length > USER_TLS_LIMIT) return false;
    }
    uint64_t top=0;
    if (!thread) {
        uint64_t physical=process->kernel_stack_physical;
        uint64_t stack=(uint64_t)process->kernel_stack;
        if (!physical || (physical & ~0x000ffffffffff000ull) ||
            physical > 0x0010000000000000ull-PROCESS_KERNEL_STACK_SIZE ||
            process->kernel_stack_frames != PROCESS_KERNEL_STACK_SIZE/PAGE_SIZE ||
            !stack || (stack & (PAGE_SIZE-1)) || process->kernel_stack != pmm_phys_to_virt(physical) ||
            stack > UINT64_MAX-PROCESS_KERNEL_STACK_SIZE) return false;
        top=stack+PROCESS_KERNEL_STACK_SIZE;
        if ((stack >> 47) != 0x1ffff || (top >> 47) != 0x1ffff ||
            (top & 15) || process->kernel_stack_top != top) return false;
    }
    *out=(struct process_user_binding){.user_fpu=fpu,.fs_base=fs,
        .kernel_stack_top=top,.kernel_stack_base=thread?0:(uint64_t)process->kernel_stack,
        .kernel_stack_physical=thread?0:process->kernel_stack_physical,
        .kernel_stack_frames=thread?0:process->kernel_stack_frames,
        .new_worker_allowed=!process->killed && !process->exiting};
    return true;
}

bool process_admit_user_worker(void *candidate, void *worker, uint64_t generation,
    uint64_t root, struct process_user_binding *out) {
    struct process_user_binding binding;
    if (!worker || !out || !process_resolve_user_binding(candidate,worker,generation,root,&binding))
        return false;
    /* The resolver established static-slot membership under the same exclusion. */
    struct process *process=candidate;
    if (process->killed || process->exiting) return false;
    *out=binding;
    return true;
}

struct process_signal_owner {
    struct process *process;
    struct process_user_thread *worker;
    struct signal_thread_state *state;
    struct signal_state_identity identity;
    uint64_t root;
};
struct signal_default_decision {
    uint64_t *pending;
    uint64_t bit,signal;
};
/* Optional prospective current-thread view; other live threads are unchanged. */
struct signal_default_view {
    struct process_user_thread *worker;
    uint64_t mask,local_pending,shared_pending;
};
static struct signal_default_decision process_prepare_signal_default_view(
    struct process *,uint64_t,const struct signal_default_view *);
static void process_commit_signal_default(struct process *,struct signal_default_decision);
static bool process_signal_observe_owner(struct process_signal_owner *out,bool entry) {
    struct scheduler_user_return_observation observed;
    struct process_user_binding binding;
    if(!out || !(entry?scheduler_observe_user_entry(&observed):scheduler_observe_user_return(&observed)) ||
        !process_resolve_user_binding(observed.process,observed.user_thread,
            observed.image_generation,observed.address_space,&binding) ||
        !binding.new_worker_allowed || binding.user_fpu!=observed.user_fpu ||
        binding.fs_base!=observed.user_fs_base ||
        (observed.user_thread ? binding.kernel_stack_top!=0 :
            binding.kernel_stack_top!=observed.kernel_stack_top)) return false;
    /* Resolver established process and worker membership before dereference. */
    struct process *process=observed.process;
    struct process_user_thread *worker=observed.user_thread;
    if((!worker && process->main_thread_exited) ||
        (worker && worker->tid>(uint64_t)INT64_MAX) || !observed.thread_identity) return false;
    *out=(struct process_signal_owner){.process=process,.worker=worker,
        .state=worker?&worker->signal_state:&process->main_signal_state,
        .identity={observed.image_generation,worker?worker->tid:1,observed.thread_identity},
        .root=observed.address_space};
    return true;
}
static bool process_signal_current_owner(struct process_signal_owner *out) {
    return process_signal_observe_owner(out,false);
}

bool process_signal_record_push(const struct signal_context_record *record,struct signal_context_range range) {
    uint64_t irq=process_irq_save();struct process_signal_owner owner;
    bool result=false;
    if(process_signal_current_owner(&owner)) {
        struct signal_thread_state next=*owner.state;
        if(signal_thread_state_bind(&next,owner.identity) &&
            signal_thread_state_push(&next,owner.identity,record,range)) {
            *owner.state=next;result=true;
        }
    }
    process_irq_restore(irq);return result;
}
bool process_signal_record_pop(uint64_t token,uint64_t frame_identity,
    struct signal_context_record *record,struct signal_context_range *range) {
    uint64_t irq=process_irq_save();struct process_signal_owner owner;
    bool result=process_signal_current_owner(&owner) &&
        signal_thread_state_pop(owner.state,owner.identity,token,frame_identity,record,range);
    process_irq_restore(irq);return result;
}

struct process_frame_copy_plan {
    uint64_t root;
    struct user_frame_stack stack;
    struct user_frame_page pages[2];
    size_t page_count;
    struct fpu_state *user_fpu,*kernel_fpu;
};
static bool process_prepare_frame_copy(const struct isr_frame *frame,
    const struct user_frame_source *source,uint64_t destination,size_t length,
    struct process_frame_copy_plan *out) {
    if (!out || !frame || !source || !destination || !length || length>USER_FRAME_COPY_LIMIT ||
        destination >= (1ull<<47) || length > (1ull<<47)-destination) return false;
    struct scheduler_user_return_observation observed;
    struct process_user_binding binding;
    if (!scheduler_observe_user_return(&observed) || !observed.kernel_fpu ||
        !process_resolve_user_binding(observed.process,observed.user_thread,
            observed.image_generation,observed.address_space,&binding) ||
        binding.user_fpu!=observed.user_fpu || binding.fs_base!=observed.user_fs_base ||
        !vmm_user_frame_copy_policy_current(observed.address_space) ||
        !fpu_validate_user_trap(frame)) return false;
    /* Resolver established membership; its unwind allowance is intentionally
     * narrower here: an exiting image cannot publish a future signal frame. */
    struct process *process=observed.process;
    if (!binding.new_worker_allowed || (!observed.user_thread && process->main_thread_exited) ||
        (process->mapping_count>PROCESS_MAX_MAPPINGS || (process->mapping_count && !process->mappings))) return false;
    struct user_frame_stack stack;
    if (observed.user_thread) {
        struct scheduler_user_worker_stack worker;
        if (binding.kernel_stack_top || binding.kernel_stack_base || binding.kernel_stack_physical || binding.kernel_stack_frames ||
            !scheduler_observe_user_worker_stack(&worker) || worker.thread_identity!=observed.thread_identity ||
            worker.top!=observed.kernel_stack_top) return false;
        stack=(struct user_frame_stack){worker.base,worker.physical,worker.frames};
    } else {
        if (binding.kernel_stack_top!=observed.kernel_stack_top) return false;
        stack=(struct user_frame_stack){binding.kernel_stack_base,binding.kernel_stack_physical,binding.kernel_stack_frames};
    }
    uint64_t first=destination & ~(uint64_t)(PAGE_SIZE-1);
    uint64_t last=(destination+length-1) & ~(uint64_t)(PAGE_SIZE-1);
    size_t page_count=first==last?1:2;
    struct user_frame_page pages[2]={{0}};
    const uint64_t allowed=VMM_PAGE_PRESENT|VMM_PAGE_USER|VMM_PAGE_WRITABLE|VMM_PAGE_NO_EXECUTE;
    for(size_t page=0;page<page_count;page++) {
        uint64_t address=first+page*PAGE_SIZE;unsigned matches=0;
        for(uint64_t i=0;i<process->mapping_count;i++) {
            const struct process_mapping *mapping=&process->mappings[i];
            if(mapping->virtual_address!=address) continue;
            if(++matches!=1 || !mapping->physical_address || (mapping->physical_address & (PAGE_SIZE-1)) ||
               (mapping->flags & ~allowed) ||
               (mapping->flags & (VMM_PAGE_USER|VMM_PAGE_WRITABLE))!=(VMM_PAGE_USER|VMM_PAGE_WRITABLE)) return false;
            pages[page]=(struct user_frame_page){address,mapping->physical_address,mapping->flags};
        }
        if(matches!=1) return false;
    }
    for(size_t page=0;page<page_count;page++) {
        unsigned owners=0;
        for(uint64_t i=0;i<process->mapping_count;i++)
            if(process->mappings[i].physical_address==pages[page].physical_address && ++owners>1) return false;
        if(owners!=1) return false;
    }
    *out=(struct process_frame_copy_plan){.root=observed.address_space,.stack=stack,
        .pages={pages[0],pages[1]},.page_count=page_count,.user_fpu=observed.user_fpu,.kernel_fpu=observed.kernel_fpu};
    return true;
}
bool process_copy_user_frame_current(const struct isr_frame *frame,
    const struct user_frame_source *source,uint64_t destination,size_t length) {
    struct process_frame_copy_plan plan;
    if(!process_prepare_frame_copy(frame,source,destination,length,&plan)) return false;
    return vmm_copy_user_frame_current(plan.root,destination,source,length,
        &plan.stack,plan.pages,plan.page_count,frame,plan.user_fpu,plan.kernel_fpu);
}

_Static_assert(sizeof(struct process_signal_private_info)==64,"private signal information layout");
_Static_assert(8+sizeof(struct signal_context_snapshot)+sizeof(struct process_signal_private_info)==PROCESS_SIGNAL_PRIVATE_FRAME_BYTES,"complete private signal frame");
_Static_assert(sizeof(struct signal_thread_state)<=4096,"bounded complete private-state exclusion");
_Static_assert(offsetof(struct process,usr1_action_generation)==offsetof(struct process,usr1_action)+sizeof(struct signal_action_descriptor),"descriptor generation adjacency");
_Static_assert(offsetof(struct process,main_signal_state)-offsetof(struct process,signal_catch_mask)<=4096,"bounded action and main metadata exclusion");
_Static_assert(offsetof(struct process,main_signal_state)<=4096,"complete process prefix exclusion");
_Static_assert(PROCESS_MAX_USER_THREADS+2<=USER_FRAME_EXTENDED_EXCLUSION_MAX,"RESET complete metadata exclusions");
_Static_assert(offsetof(struct process_user_thread,pending_signals)==offsetof(struct process_user_thread,signal_mask)+8,"worker pending-mask adjacency");

/* First caller is an opt-in test kernel only. All objects below are private
 * resident local/static storage in this single uninterrupted IF0 continuation.
 * Final publication uses no fallible helper after the admitted user copy. */
enum process_signal_delivery_result process_signal_deliver_private(struct isr_frame *frame) {
    if(!frame || frame->cs!=0x23 || frame->ss!=0x1b) return PROCESS_SIGNAL_FATAL;
    struct process_signal_owner owner;
    if(!process_signal_current_owner(&owner)) return PROCESS_SIGNAL_DEFERRED;
    struct process *process=owner.process;
    struct process_user_thread *worker=owner.worker;
    struct signal_action_descriptor action=process->usr1_action;
    if(action.mode!=SIGNAL_ACTION_INFO) return PROCESS_SIGNAL_NONE;
    uint64_t bit=1ull<<SRV_SIGNAL_USR1;
    uint64_t *mask=worker?&worker->signal_mask:&process->main_signal_mask;
    uint64_t *local=worker?&worker->pending_signals:&process->main_pending_signals;
    if((*mask&bit) || !((*local|process->pending_signals)&bit)) return PROCESS_SIGNAL_NONE;
    uint64_t *selected=(*local&bit)?local:&process->pending_signals;
    struct signal_thread_state next=*owner.state;
    if(!signal_thread_state_bind(&next,owner.identity)) return PROCESS_SIGNAL_FATAL;
    if(next.stack.depth==SIGNAL_CONTEXT_MAX_DEPTH) return PROCESS_SIGNAL_DEFERRED;
    if(next.stack.last_token==UINT64_MAX || !signal_action_descriptor_valid(&action) ||
        !vmm_user_exec_current(action.handler,owner.root) ||
        !vmm_user_exec_current(action.trampoline,owner.root)) return PROCESS_SIGNAL_FATAL;
    bool reset=(action.flags&SIGNAL_ACTION_RESET)!=0;
    if(reset && process->usr1_action_generation==UINT64_MAX) return PROCESS_SIGNAL_FATAL;
    uint64_t action_generation=process->usr1_action_generation+(reset?1:0);
    uint64_t handler_mask=(*mask|action.mask|bit)&~((1ull<<9)|(1ull<<19));
    struct signal_default_decision default_decision={0};
    if(reset) {
        struct signal_default_view view={.worker=worker,.mask=handler_mask,
            .local_pending=*local&~(selected==local?bit:0),
            .shared_pending=process->pending_signals&~(selected==&process->pending_signals?bit:0)};
        default_decision=process_prepare_signal_default_view(process,SRV_SIGNAL_USR1,&view);
    }
    uint64_t token=next.stack.last_token+1;
    struct signal_context_plan public;
    if(!signal_context_plan_frame(&public,frame->rsp,PROCESS_SIGNAL_PRIVATE_FRAME_BYTES,
            next.ranges,next.stack.depth)) return PROCESS_SIGNAL_FATAL;
    struct fpu_user_snapshot saved;
    struct signal_context_snapshot snapshot;
    if(!fpu_snapshot_user_trap(frame,&saved) ||
        !signal_context_capture(&snapshot,frame,saved.bytes,0,token,*mask)) return PROCESS_SIGNAL_FATAL;
    struct signal_context_record record={.frame=*frame,.mask=*mask,
        .image_generation=owner.identity.image_generation,.thread_generation=owner.identity.scheduler_identity,
        .token=token,.frame_identity=public.entry_rsp};
    for(size_t i=0;i<sizeof(record.fpu);i++) record.fpu[i]=saved.bytes[i];
    if(!signal_io_mark_interrupted(&next,owner.identity))return PROCESS_SIGNAL_FATAL;
    struct signal_context_range record_range={public.entry_rsp,public.end};
    struct signal_wait_slot *wait=next.stack.depth<4?&next.waits[next.stack.depth]:NULL;
    bool associate=wait && wait->ticket && ((wait->phase_flags&255)==SIGNAL_WAIT_RETURNING ||
        (wait->phase_flags&255)==SIGNAL_WAIT_LEGACY_READY);
    if(associate?!signal_wait_push_record(&next,owner.identity,wait->ticket,&record,record_range):
        !signal_thread_state_push(&next,owner.identity,&record,record_range))return PROCESS_SIGNAL_FATAL;
    struct process_signal_private_info info={.size=sizeof(info),.version=1,
        .signo=SRV_SIGNAL_USR1,.valid=PROCESS_SIGNAL_INFO_SIGNO|PROCESS_SIGNAL_INFO_TARGET_TID,
        .target_tid=owner.identity.public_tid};
    struct user_frame_source wire={0};
    for(size_t i=0;i<sizeof(snapshot);i++) wire.bytes[8+i]=((const uint8_t *)&snapshot)[i];
    for(size_t i=0;i<sizeof(info);i++) wire.bytes[8+sizeof(snapshot)+i]=((const uint8_t *)&info)[i];
    struct fpu_user_trap_commit commit;
    if(!fpu_prepare_user_trap_commit(frame,NULL,&commit)) return PROCESS_SIGNAL_FATAL;
    struct process_frame_copy_plan copy;
    if(!process_prepare_frame_copy(frame,&wire,public.entry_rsp,PROCESS_SIGNAL_PRIVATE_FRAME_BYTES,&copy))
        return PROCESS_SIGNAL_FATAL;
    if(reset) {
        struct user_frame_exclusion excluded[PROCESS_MAX_USER_THREADS+2]={
            {(uint64_t)owner.state,sizeof(*owner.state)},
            {(uint64_t)process,offsetof(struct process,main_signal_state)}};
        for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++)
            excluded[i+2]=(struct user_frame_exclusion){(uint64_t)&process->user_threads[i].signal_mask,16};
        if(!vmm_copy_user_frame_extended_current(copy.root,public.entry_rsp,&wire,PROCESS_SIGNAL_PRIVATE_FRAME_BYTES,
                &copy.stack,copy.pages,copy.page_count,frame,copy.user_fpu,copy.kernel_fpu,
                excluded,PROCESS_MAX_USER_THREADS+2)) return PROCESS_SIGNAL_FATAL;
    } else {
        struct user_frame_exclusion excluded[USER_FRAME_EXCLUSION_MAX]={
            {(uint64_t)owner.state,sizeof(*owner.state)},
            {(uint64_t)&process->signal_catch_mask,offsetof(struct process,main_signal_state)-offsetof(struct process,signal_catch_mask)},
            {worker?(uint64_t)&worker->signal_mask:0,worker?16:0}};
        if(!vmm_copy_user_frame_excluding_current(copy.root,public.entry_rsp,&wire,PROCESS_SIGNAL_PRIVATE_FRAME_BYTES,
                &copy.stack,copy.pages,copy.page_count,frame,copy.user_fpu,copy.kernel_fpu,
                excluded,worker?3:2)) return PROCESS_SIGNAL_FATAL;
    }

    /* Successful-copy suffix: only prepared resident stores and FPU commit.
     * Preserve a separately queued bit in the unchosen pending source. */
    *owner.state=next;
    *selected &= ~bit;
    *mask=handler_mask;
    if(reset) {
        process->usr1_action=(struct signal_action_descriptor){0};
        process->usr1_action_generation=action_generation;
        process->signal_catch_mask &= ~bit;
        process->signal_ignore_mask &= ~bit;
    }
    process_commit_signal_default(process,default_decision);
    frame->rdi=SRV_SIGNAL_USR1;
    frame->rsi=public.snapshot+sizeof(snapshot);
    frame->rdx=public.snapshot;frame->rcx=action.handler;
    frame->r8=token;frame->r9=public.entry_rsp;
    frame->rip=action.trampoline;frame->rsp=public.entry_rsp;
    frame->cs=0x23;frame->ss=0x1b;frame->rflags=0x202;
    fpu_commit_user_trap_prepared(&commit);
    return default_decision.pending?PROCESS_SIGNAL_DELIVERED_EXIT_USR1:PROCESS_SIGNAL_DELIVERED;
}

uint64_t process_signal_return_private(struct isr_frame *frame,uint64_t token,uint64_t frame_identity) {
    struct process_signal_owner owner;
    if(!frame || frame->cs!=0x23 || frame->ss!=0x1b || !process_signal_current_owner(&owner)) return false;
    struct signal_thread_state next=*owner.state;
    struct signal_context_record record;
    struct signal_context_range range;
    bool associated=next.stack.depth && next.stack.depth<=4 && next.stack.records[next.stack.depth-1].wait_ticket;
    bool popped=associated?signal_wait_pop_record(&next,owner.identity,token,frame_identity,&record,&range):
        signal_thread_state_pop(&next,owner.identity,token,frame_identity,&record,&range);
    if(!popped ||
        range.start!=frame_identity || range.end-range.start!=PROCESS_SIGNAL_PRIVATE_FRAME_BYTES) return false;
    uint64_t *local=owner.worker?&owner.worker->pending_signals:&owner.process->main_pending_signals;
    uint64_t pending_local=*local,pending_shared=owner.process->pending_signals;
    /* Match existing mask-unblock/default handling, staged before any commit.
     * Local priority consumes only the selected copy of a fatal bit. */
    uint64_t unblocked=(pending_local|pending_shared)&~record.mask&
        ~(owner.process->signal_catch_mask|owner.process->signal_ignore_mask);
    uint64_t ignored_defaults=unblocked&((1ull<<SRV_SIGNAL_CHLD)|(1ull<<SRV_SIGNAL_WINCH));
    pending_local &= ~ignored_defaults;pending_shared &= ~ignored_defaults;
    unblocked &= ~ignored_defaults;
    uint64_t fatal_signal=0;
    for(uint64_t signal=1;signal<64;signal++) if(unblocked&(1ull<<signal)) {
        fatal_signal=signal;
        if(pending_local&(1ull<<signal)) pending_local &= ~(1ull<<signal);
        else pending_shared &= ~(1ull<<signal);
        break;
    }
    struct fpu_user_snapshot saved;
    for(size_t i=0;i<sizeof(saved.bytes);i++) saved.bytes[i]=record.fpu[i];
    struct fpu_user_trap_commit commit;
    if(!fpu_prepare_user_trap_commit(frame,&saved,&commit)) return false;
    /* No user bytes or fallible operation participate in restoration. */
    *owner.state=next;
    *local=pending_local;owner.process->pending_signals=pending_shared;
    if(owner.worker) owner.worker->signal_mask=record.mask;
    else owner.process->main_signal_mask=record.mask;
    *frame=record.frame;
    if(fatal_signal) {owner.process->killed=true;owner.process->kill_signal=fatal_signal;}
    fpu_commit_user_trap_prepared(&commit);
    return fatal_signal?128+fatal_signal:PROCESS_SIGNAL_RETURN_RESTORED;
}

struct process_latency_span { uint64_t start;bool entry_masked; };
static struct process_latency_span process_latency_begin(void) {
#ifdef SRVROS_PROCESS_HOST_TEST
    return (struct process_latency_span){0};
#else
    uint64_t flags;__asm__ volatile("pushfq; popq %0":"=r"(flags)::"memory");
    return (struct process_latency_span){latency_clock(),!(flags&(1ull<<9))};
#endif
}
static void process_latency_end(enum latency_work_site site,struct process_latency_span span) {
#ifdef SRVROS_PROCESS_HOST_TEST
    (void)site;(void)span;
#else
    latency_work_record(site,span.start,span.entry_masked);
#endif
}
/* Legacy host fixtures have no stream manager. The dedicated stream/process
 * fixture enables these production hooks and supplies its backend seams. */
static bool process_stream_busy(const char *path,bool subtree) {
#if defined(SRVROS_PROCESS_HOST_TEST) && !defined(SRVROS_STREAM_PROCESS_HOST_TEST)
    (void)path;(void)subtree;return false;
#else
    return exfat_stream_busy(path,subtree);
#endif
}

/* These are serialized service windows, not general preemption of a mutable
 * process/VFS operation. Only the existing audited GUI and kernel services may
 * run; siblings and unsafe entries wait behind the inherited owner gate. */
struct process_service_scope { uint64_t flags;bool held; };
static struct process_service_scope process_service_begin(void) {
#if defined(SRVROS_PROCESS_HOST_TEST) && !defined(SRVROS_PROCESS_SERVICE_HOST_TEST)
    return (struct process_service_scope){0};
#else
    struct process_service_scope scope={.flags=process_irq_save()};
    scope.held=scheduler_io_begin();return scope;
#endif
}
static void process_service_end(struct process_service_scope scope) {
#if defined(SRVROS_PROCESS_HOST_TEST) && !defined(SRVROS_PROCESS_SERVICE_HOST_TEST)
    (void)scope;
#else
    if(scope.held)scheduler_io_end();
    process_irq_restore(scope.flags);
#endif
}
static bool process_stream_cleanup(uint64_t owner) {
    if(!owner)return true;
#if defined(SRVROS_PROCESS_HOST_TEST) && !defined(SRVROS_STREAM_PROCESS_HOST_TEST)
    (void)owner;return true;
#elif defined(SRVROS_PROCESS_HOST_TEST) && !defined(SRVROS_PROCESS_SERVICE_HOST_TEST)
    return file_stream_cleanup(owner);
#else
    struct process_service_scope scope=process_service_begin();
    if(!scope.held) { process_service_end(scope);return false; }
    bool okay=file_stream_cleanup(owner);process_service_end(scope);return okay;
#endif
}
static void process_service_checkpoint(void) {
#if !defined(SRVROS_PROCESS_HOST_TEST) || defined(SRVROS_PROCESS_SERVICE_HOST_TEST)
    scheduler_io_checkpoint();
#endif
}

static void process_record_exit_status_locked(uint64_t pid, uint64_t status) {
    struct process_exit_record *oldest = &exit_records[0];
    for (uint64_t i = 0; i < PROCESS_EXIT_STATUS_CACHE; i++) {
        struct process_exit_record *record = &exit_records[i];
        if (record->used && record->pid == pid) {
            record->status = status;
            record->sequence = ++next_exit_record_sequence;
            return;
        }
        if (!record->used || record->sequence < oldest->sequence) {
            oldest = record;
        }
    }
    oldest->used = true;
    oldest->pid = pid;
    oldest->status = status;
    oldest->sequence = ++next_exit_record_sequence;
}

static void process_record_exit_status(uint64_t pid, uint64_t status) {
    uint64_t flags = process_irq_save();
    process_record_exit_status_locked(pid, status);
    process_irq_restore(flags);
}

bool process_exit_status(uint64_t pid, uint64_t *status_out) {
    bool found = false;
    uint64_t status = 0;
    uint64_t flags = process_irq_save();
    for (uint64_t i = 0; i < PROCESS_EXIT_STATUS_CACHE; i++) {
        if (exit_records[i].used && exit_records[i].pid == pid) {
            status = exit_records[i].status;
            found = true;
            break;
        }
    }
    process_irq_restore(flags);
    if (found && status_out != NULL) {
        *status_out = status;
    }
    return found;
}
static bool process_file_clone_entry(struct process_file *target, const struct process_file *source);
static int64_t process_path_context_for(struct process *,uint64_t,const char *,char *,uint64_t);
static int64_t process_inherit_files(struct process *,struct process *);
static int64_t process_posix_spawn_actions(struct process *,const struct process_spawn_file_action *,uint64_t);
static int64_t process_file_close_entry(struct process *process, struct process_file *file);
static void process_file_release_entry(struct process_file *file);
static void pipe_rights_collect(void);
static int64_t process_file_dup_into(struct process *process, const struct process_file *source, uint64_t target_index);
static bool process_apply_spawn_file_actions(struct process *child,
    struct process *parent,
    const struct process_spawn_file_action *actions,
    uint64_t action_count);

void process_account_cpu(void *raw_process,void *raw_thread,bool user,uint64_t elapsed) {
    struct process *process=raw_process;
    struct process_user_thread *thread=raw_thread;
    if(!process || !process->allocated) return;
    if(user) process->user_us+=elapsed;else process->system_us+=elapsed;
    if(thread && thread->owner==process) {
        if(user) thread->user_us+=elapsed;else thread->system_us+=elapsed;
    } else {
        if(user) process->main_user_us+=elapsed;else process->main_system_us+=elapsed;
    }
}
static void process_account_child(struct process *parent,struct process *child) {
    if(!parent || !child) return;
    uint64_t irq=process_irq_save();
    parent->child_user_us+=child->user_us+child->child_user_us;
    parent->child_system_us+=child->system_us+child->child_system_us;
    uint64_t peak=child->peak_mapped_bytes>child->child_peak_bytes?child->peak_mapped_bytes:child->child_peak_bytes;
    if(peak>parent->child_peak_bytes) parent->child_peak_bytes=peak;
    process_irq_restore(irq);
}
/* Limits belong to the process, survive exec and are inherited by spawn.
 * Unsupported finite policies must not silently succeed. */
int64_t process_umask(uint64_t mask) {
    struct process *process=process_current();if(!process) return SRV_ERR_INVAL;
    uint64_t irq=process_irq_save(),old=process->creation_mask;
    process->creation_mask=mask&0777;process_irq_restore(irq);return (int64_t)old;
}
uint64_t process_creation_mode(uint64_t mode) {
    struct process *process=process_current();return (mode&07777)&~(process?process->creation_mask:0);
}
int64_t process_rlimit(uint64_t resource,const struct srv_rlimit *set,struct srv_rlimit *out) {
    struct process *process=process_current();
    if(!process || resource>9) return SRV_ERR_INVAL;
    uint64_t irq=process_irq_save();
    struct srv_rlimit value={UINT64_MAX,UINT64_MAX};
    if(resource==SRV_RLIMIT_FSIZE) value=process->fsize_limit;
    else if(resource==SRV_RLIMIT_STACK) value=(struct srv_rlimit){USER_STACK_PAGES*PAGE_SIZE,USER_STACK_PAGES*PAGE_SIZE};
    else if(resource==SRV_RLIMIT_CORE) value=(struct srv_rlimit){0,0};
    int64_t result=0;
    if(set) {
        if(set->current>set->maximum) result=SRV_ERR_INVAL;
        else if(resource==SRV_RLIMIT_FSIZE) {
            /* Single-user root profile: raising a previously lowered hard limit
             * is permitted. The backing filesystem has independent size limits. */
            process->fsize_limit=*set;value=*set;
        } else if(set->current!=value.current || set->maximum!=value.maximum) result=SRV_ERR_NOTSUP;
    }
    if(!result && out) *out=value;
    process_irq_restore(irq);return result;
}

int64_t process_usage(int64_t who,struct srv_usage *out) {
    struct process *process=process_current();
    if(!process || !out || (who!=SRV_USAGE_SELF && who!=SRV_USAGE_CHILDREN && who!=SRV_USAGE_THREAD)) return SRV_ERR_INVAL;
    uint64_t irq=process_irq_save();scheduler_account_now();
    *out=(struct srv_usage){.resolution_us=timer_reference_ready()?1:10000};
    if(who==SRV_USAGE_CHILDREN) {
        out->user_us=process->child_user_us;out->system_us=process->child_system_us;
        out->peak_mapped_bytes=process->child_peak_bytes;
    } else if(who==SRV_USAGE_SELF) {
        out->user_us=process->user_us;out->system_us=process->system_us;
        out->peak_mapped_bytes=process->peak_mapped_bytes;
    } else {
        struct process_user_thread *thread=scheduler_current_user_thread_context();
        out->user_us=thread && thread->owner==process?thread->user_us:process->main_user_us;
        out->system_us=thread && thread->owner==process?thread->system_us:process->main_system_us;
        out->peak_mapped_bytes=process->peak_mapped_bytes;
    }
    process_irq_restore(irq);return 0;
}

static struct process_user_thread *process_current_user_thread(void) {
    return (struct process_user_thread *)scheduler_current_user_thread_context();
}

static uint64_t process_current_signal_mask(struct process *process) {
    struct process_user_thread *thread = process_current_user_thread();
    if (thread != NULL && thread->owner == process && thread->used) {
        return thread->signal_mask;
    }
    return process != NULL ? process->main_signal_mask : 0;
}

static void process_set_current_signal_mask(struct process *process, uint64_t mask) {
    struct process_user_thread *thread = process_current_user_thread();
    if (thread != NULL && thread->owner == process && thread->used) {
        thread->signal_mask = mask;
    } else if (process != NULL) {
        process->main_signal_mask = mask;
    }
}

/* Process-directed signals are eligible in any unmasked thread. A targeted
 * pending signal is visible and consumable only by its destination thread. */
static uint64_t *process_local_pending(struct process *process) {
    struct process_user_thread *thread=process_current_user_thread();
    return thread && thread->owner==process && thread->used
        ? &thread->pending_signals : &process->main_pending_signals;
}
static uint64_t process_visible_pending(struct process *process) {
    return process->pending_signals | *process_local_pending(process);
}
static void process_consume_signal_bit(struct process *process,uint64_t bit) {
    uint64_t *local=process_local_pending(process);
    if (*local & bit) *local &= ~bit;
    else process->pending_signals &= ~bit;
}
static void process_discard_signals(struct process *process,uint64_t bits) {
    process->pending_signals &= ~bits;
    process->main_pending_signals &= ~bits;
    for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++)
        process->user_threads[i].pending_signals &= ~bits;
}
static bool process_all_threads_block_signal(struct process *process,uint64_t bit) {
    if (!process->main_thread_exited && !(process->main_signal_mask & bit)) return false;
    for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++) {
        struct process_user_thread *thread=&process->user_threads[i];
        if(thread->used && thread->active && !(thread->signal_mask & bit)) return false;
    }
    return true;
}

static uint64_t process_supported_signal_mask(void) {
    return (1ull << SRV_SIGNAL_HUP) | (1ull << SRV_SIGNAL_WINCH) | (1ull << SRV_SIGNAL_INT) |
        (1ull << SRV_SIGNAL_QUIT) | (1ull << SRV_SIGNAL_ABRT) |
        (1ull << SRV_SIGNAL_SEGV) | (1ull << SRV_SIGNAL_PROF) |
        (1ull << SRV_SIGNAL_USR1) |
        (1ull << SRV_SIGNAL_USR2) |
        (1ull << SRV_SIGNAL_PIPE) | (1ull << SRV_SIGNAL_XFSZ) |
        (1ull << SRV_SIGNAL_ALRM) |
        (1ull << SRV_SIGNAL_TERM) |
        (1ull << SRV_SIGNAL_CHLD);
}

bool process_signal_is_supported(uint64_t signal) {
    return signal==0 || signal==SRV_SIGNAL_KILL ||
        (signal<64 && (process_supported_signal_mask() & (1ull<<signal))!=0);
}

static void process_apply_signal_defaults(struct process *process, uint64_t signal_default) {
    if (process == NULL) {
        return;
    }
    uint64_t mask = signal_default & process_supported_signal_mask();
    process->signal_catch_mask &= ~mask;
    process->signal_ignore_mask &= ~mask;
    if(mask&(1ull<<SRV_SIGNAL_USR1)) {
        process->usr1_action=(struct signal_action_descriptor){0};
        process->usr1_action_generation=0;
    }
    process_discard_signals(process,mask);
}

static void process_wake_for_signal(struct process *process) {
    net_process_wake(process);
    scheduler_wake_all(&process_wait_queue);
    scheduler_wake_all(&pipe_wait_queue);
    scheduler_wake_all(&unix_wait_queue);
    scheduler_wake_all(&file_lock_wait_queue);
    if (process != NULL) {
        scheduler_wake_all(&process->thread_wait_queue);
    }
    keyboard_wake_waiters();
    pty_wake();
    process_file_poll_wake();
    process_futex_wake_process(process);
}

bool process_record_user_fault(uint64_t signal) {
    uint64_t irq = process_irq_save();
    struct process *process = process_current();
    if (process == NULL || !process->active ||
        (signal != SRV_SIGNAL_ILL && signal != SRV_SIGNAL_FPE && signal != SRV_SIGNAL_SEGV)) {
        process_irq_restore(irq);
        return false;
    }
    /* Synchronous faults cannot be resumed through the asynchronous signal
     * API. Preserve an already selected fatal cause, and wake every class of
     * waiter so the initial thread can complete process-wide teardown. */
    if (!process->killed) {
        process->killed = true;
        process->kill_signal = signal;
    }
    process_wake_for_signal(process);
    process_irq_restore(irq);
    return true;
}

static bool process_queue_signal_locked(struct process *process, uint64_t signal) {
    if (process == NULL || !process->active || signal == 0 || signal >= 64) {
        return false;
    }
    uint64_t mask = 1ull << signal;
    if ((process->signal_ignore_mask & mask) != 0) {
        return true;
    }
    if ((process->signal_catch_mask & mask) != 0 ||
        process_all_threads_block_signal(process,mask)) {
        process->pending_signals |= mask;
        process_wake_for_signal(process);
        return true;
    }
    if (signal == SRV_SIGNAL_CHLD || signal == SRV_SIGNAL_WINCH) {
        return true;
    }
    process->killed = true;
    process->kill_signal = signal;
    process_wake_for_signal(process);
    return true;
}
static bool process_queue_signal(struct process *process, uint64_t signal) {
    uint64_t irq=process_irq_save();
    bool result=process_queue_signal_locked(process,signal);
    process_irq_restore(irq);return result;
}

static void process_notify_parent_child_exit(struct process *child) {
    if (child == NULL || child->parent_pid == 0) {
        return;
    }
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        struct process *parent = &processes[i];
        if (parent->allocated && parent->pid == child->parent_pid) {
            (void)process_queue_signal(parent, SRV_SIGNAL_CHLD);
            return;
        }
    }
}

static void process_clear_current_signal(uint64_t signal) {
    struct process *process = process_current();
    if (process == NULL || signal == 0 || signal >= 64) {
        return;
    }
    process->pending_signals &= ~(1ull << signal);
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

static struct capture_write_snapshot capture_writes;
static uint64_t capture_write_now(void) {
#ifdef SRVROS_CAPTURE_WRITE_CLOCK_HOST_TEST
    extern uint64_t capture_write_test_clock(void);return capture_write_test_clock();
#elif defined(SRVROS_PROCESS_HOST_TEST)
    return 0;
#else
    return timer_monotonic_us();
#endif
}
static uint64_t capture_write_pid(void) {
    struct process *p=process_current();
    return capture_writes.enabled && p && p->allocated && p->active &&
        process_path_equal(p->executable_path,"/fat/bin/shotworker")?p->pid:0;
}
bool process_capture_write_timing_enable(void) {
    uint64_t flags=process_irq_save();
    bool ok=!capture_writes.enabled && timer_reference_ready();
    if(ok)capture_writes.enabled=1;
    process_irq_restore(flags);return ok;
}
uint64_t process_capture_write_clock(void) {
    if(!capture_writes.enabled)return 0;
    uint64_t flags=process_irq_save(),pid=capture_write_pid(),now=0;
    if(pid) {now=capture_write_now();if(!capture_write_touch(&capture_writes,pid,now))now=0;}
    process_irq_restore(flags);return now;
}
uint64_t process_capture_write_begin(void) {
    uint64_t now=process_capture_write_clock();if(!now)return 0;
    uint64_t flags=process_irq_save();struct capture_write_record *r=capture_write_find(&capture_writes,capture_write_pid());
    if(r && !capture_write_add(&r->attempts,1))r->invalid=1;
    process_irq_restore(flags);return now;
}
void process_capture_write_record(enum capture_write_site site,uint64_t start) {
    if(!start)return;
    uint64_t flags=process_irq_save();
    capture_write_sample(capture_write_find(&capture_writes,capture_write_pid()),site,start,capture_write_now());
    process_irq_restore(flags);
}
void process_capture_write_result(uint64_t start,uint64_t requested,int64_t result) {
    if(!start)return;
    uint64_t flags=process_irq_save();
    capture_write_result(capture_write_find(&capture_writes,capture_write_pid()),start,capture_write_now(),requested,result);
    process_irq_restore(flags);
}
void process_capture_write_fast_result(uint64_t requested,int64_t result) {
    uint64_t flags=process_irq_save();
    capture_write_fast_result(capture_write_find(&capture_writes,capture_write_pid()),requested,result);
    process_irq_restore(flags);
}
void process_capture_write_invalidate(void) {
    uint64_t flags=process_irq_save();
    struct capture_write_record *record=capture_write_find(&capture_writes,capture_write_pid());
    if(record)record->invalid=1;
    process_irq_restore(flags);
}
void process_capture_write_gate_wait(void) {
    uint64_t flags=process_irq_save();
    struct capture_write_record *r=capture_write_find(&capture_writes,capture_write_pid());
    scheduler_io_wait_observed(r?&r->gate:NULL);
    process_irq_restore(flags);
}
void process_capture_write_growth(void) {
    if(!capture_writes.enabled)return;
    uint64_t flags=process_irq_save();struct capture_write_record *r=capture_write_find(&capture_writes,capture_write_pid());
    if(r && !capture_write_add(&r->growths,1))r->invalid=1;
    process_irq_restore(flags);
}
void process_capture_write_snapshot(struct capture_write_snapshot *out) {
    if(!out)return;
    uint64_t flags=process_irq_save();*out=capture_writes;process_irq_restore(flags);
}

static void clear_process(struct process *process) {
    struct process_latency_span span=process_latency_begin();
    struct process_service_scope scope=process_service_begin();
    uint8_t *bytes = (uint8_t *)process;
    for(uint64_t offset=0;offset<sizeof(*process);) {
        uint64_t end=offset+65536;if(end>sizeof(*process))end=sizeof(*process);
        while(offset<end)bytes[offset++]=0;
        process_service_checkpoint();
    }
    process_service_end(scope);
    process_latency_end(LATENCY_PROCESS_RECORD,span);
    process->fsize_limit=(struct srv_rlimit){UINT64_MAX,UINT64_MAX};
    process->cwd[0]='/';
    process->cwd_storage[0]='/';
    for(unsigned i=0;i<3;i++) process->files[i]=(struct process_file){.used=true,.type=PROCESS_FILE_STDIO,.handle=i};
}

static struct process_lifetime *lifetime_find(uint64_t token) {
    if (!token) return NULL;
    for (unsigned i=0;i<PROCESS_MAX_PROCESSES;i++)
        if (process_lifetimes[i].token==token) return &process_lifetimes[i];
    return NULL;
}

/* Owner authority requires both the live process association and token. */
static struct process_lifetime *user_scope_owned(uint64_t token) {
    struct process *owner=process_current();
    struct process_lifetime *life=lifetime_find(token);
    return owner && owner->allocated && owner->active && !owner->exiting && !owner->killed &&
        owner->owned_user_scope==token && life && life->user && life->owner==owner ? life : NULL;
}
static void user_scope_detach(struct process *owner) {
    uint64_t flags=process_irq_save();
    struct process_lifetime *life=owner?lifetime_find(owner->owned_user_scope):NULL;
    if(life && life->user && life->owner==owner) {
        life->revoked=true;life->owner=NULL;
        /* Explicit revoke permits graceful retirement. Losing the authority
         * that could finish that protocol instead aborts the entire scope.
         * Keep references/resources until each member's normal teardown/reap. */
        for(unsigned i=0;i<PROCESS_MAX_PROCESSES;i++) {
            struct process *member=&processes[i];
            if(member->allocated && member->active && member->lifetime==life->token) {
                member->killed=true;member->kill_signal=SRV_SIGNAL_KILL;
                process_wake_for_signal(member);
            }
        }
        if(life->reservation) {life->reservation=false;life->members--;}
        if(!life->members) *life=(struct process_lifetime){0};
    }
    if(owner) owner->owned_user_scope=0;
    process_irq_restore(flags);
}
static void user_scope_member_done(struct process_lifetime *life,uint64_t status,bool retired) {
    if(!life) return;
    if(life->user && retired && status) {
        if(!life->nonzero_exits) life->first_exit_status=status;
        if(life->nonzero_exits!=UINT64_MAX) life->nonzero_exits++;
    }
    if(life->members) life->members--;
    if(life->user && !life->owner && life->revoked && !life->members)
        *life=(struct process_lifetime){0};
}
int64_t process_scope_create(uint64_t *out) {
    uint64_t flags=process_irq_save();struct process *owner=process_current();
    int64_t result=SRV_ERR_AGAIN;
    if(!out || !owner || !owner->allocated || !owner->active || owner->exiting || owner->killed) result=SRV_ERR_INVAL;
    else if(owner->lifetime) result=SRV_ERR_NOTSUP;
    else if(owner->owned_user_scope) result=SRV_ERR_BUSY;
    else {
        for(unsigned i=0;i<PROCESS_MAX_PROCESSES;i++) {
            struct process_lifetime *life=&process_lifetimes[i];
            if(life->user && !life->owner && life->revoked && life->members) {result=SRV_ERR_BUSY;goto done;}
        }
        for(unsigned i=0;next_lifetime && i<PROCESS_MAX_PROCESSES;i++) if(!process_lifetimes[i].token) {
            uint64_t token=next_lifetime++;
            process_lifetimes[i]=(struct process_lifetime){.token=token,.user=true,.owner=owner};
            owner->owned_user_scope=token;*out=token;result=0;break;
        }
    }
done: process_irq_restore(flags);return result;
}
int64_t process_scope_reserve(uint64_t token) {
    uint64_t flags=process_irq_save();struct process_lifetime *life=user_scope_owned(token);
    int64_t result=!life?SRV_ERR_INVAL:(life->revoked || life->root_reserved?SRV_ERR_BUSY:0);
    if(!result) {life->root_reserved=true;life->reservation=true;life->members++;life->peak=life->members;}
    process_irq_restore(flags);return result;
}
void process_scope_cancel_reservation(uint64_t token) {
    uint64_t flags=process_irq_save();struct process_lifetime *life=lifetime_find(token);
    if(life && life->user && life->reservation) {life->reservation=false;user_scope_member_done(life,0,false);}
    process_irq_restore(flags);
}
int64_t process_scope_control(unsigned op,uint64_t token,struct srv_scope_status *out) {
    uint64_t flags=process_irq_save();struct process_lifetime *life=user_scope_owned(token);int64_t result=0;
    if(!life) result=SRV_ERR_INVAL;
    else if(op==SRV_SCOPE_REVOKE) life->revoked=true;
    else if(op==SRV_SCOPE_STATUS && out) *out=(struct srv_scope_status){.version=SRV_SCOPE_VERSION,.size=sizeof(*out),.revoked=life->revoked,.members=life->members,.nonzero_exits=life->nonzero_exits,.first_exit_status=life->first_exit_status};
    else if(op==SRV_SCOPE_RELEASE) {
        if(!life->revoked || life->members) result=SRV_ERR_BUSY;
        else {process_current()->owned_user_scope=0;*life=(struct process_lifetime){0};}
    } else result=SRV_ERR_INVAL;
    process_irq_restore(flags);return result;
}

uint64_t process_lifetime_create(void) {
    if (process_current()) return 0;
    uint64_t flags=process_irq_save(),token=0;
    for (unsigned i=0;next_lifetime && i<PROCESS_MAX_PROCESSES;i++) {
        if (process_lifetimes[i].token) continue;
        token=next_lifetime++; /* Exhaustion refuses; never reuse a stale token. */
        process_lifetimes[i]=(struct process_lifetime){.token=token};
        break;
    }
    process_irq_restore(flags);return token;
}

bool process_lifetime_revoke(uint64_t token) {
    if (process_current()) return false;
    uint64_t flags=process_irq_save();
    struct process_lifetime *life=lifetime_find(token);
    if (life) life->revoked=true;
    process_irq_restore(flags);return life!=NULL;
}

bool process_lifetime_release(uint64_t token) {
    if (process_current()) return false;
    uint64_t flags=process_irq_save();
    struct process_lifetime *life=lifetime_find(token);
    bool ok=life && life->revoked && !life->members;
    if (ok) *life=(struct process_lifetime){0};
    process_irq_restore(flags);return ok;
}

bool process_lifetime_status(uint64_t token,struct process_lifetime_status *out) {
    if (!out || process_current()) return false;
    uint64_t flags=process_irq_save();
    struct process_lifetime *life=lifetime_find(token);
    if (life) *out=(struct process_lifetime_status){.members=life->members,.peak=life->peak,.revoked=life->revoked};
    process_irq_restore(flags);return life!=NULL;
}

uint64_t process_lifetime_of(uint64_t pid) {
    uint64_t flags=process_irq_save(),token=0;
    for (unsigned i=0;i<PROCESS_MAX_PROCESSES;i++)
        if(processes[i].allocated && processes[i].pid==pid) { token=processes[i].lifetime;break; }
    process_irq_restore(flags);return token;
}

static struct process *alloc_process_lifetime(const char *path,bool detached,uint64_t root_token) {
    struct process *parent=process_current();
    uint64_t token=root_token?root_token:(parent?parent->lifetime:0);
    uint64_t flags=process_irq_save(),slot=PROCESS_MAX_PROCESSES,pid=0;
    struct process_lifetime *life=lifetime_find(token);
    if (root_token && parent && !user_scope_owned(root_token)) {process_irq_restore(flags);return NULL;}
    if (token && (!life || (life->revoked && !(root_token && life->reservation)) || (root_token && life->root_reserved && !life->reservation))) {
        process_irq_restore(flags);return NULL;
    }
    for(uint64_t i=0;i<PROCESS_MAX_PROCESSES;i++) {
        if(!processes[i].allocated && !process_slots_busy[i]) {
            slot=i;process_slots_busy[i]=true;pid=next_pid++;
            if(!next_pid) next_pid=1;
            if(life) {
                if(root_token && life->reservation) life->reservation=false;
                else {life->members++;if(life->members>life->peak)life->peak=life->members;}
                if(root_token)life->root_reserved=true;
            }
            break;
        }
    }
    process_irq_restore(flags);
    if(slot==PROCESS_MAX_PROCESSES) return NULL;
    struct process *process=&processes[slot];
    clear_process(process);
    process->active=true;process->detached=detached;process->pid=pid;
    process->lifetime=token;
    process->parent_pid=process_pid(parent);
    if(parent) { process->creation_mask=parent->creation_mask;process->fsize_limit=parent->fsize_limit;process->signal_ignore_mask=parent->signal_ignore_mask;
        copy_process_path(process->cwd,sizeof(process->cwd),parent->cwd);
        copy_process_path(process->cwd_storage,sizeof(process->cwd_storage),parent->cwd_storage); }
    process->usr1_action.mode=(process->signal_ignore_mask&(1ull<<SRV_SIGNAL_USR1)) ?
        SIGNAL_ACTION_IGNORE : SIGNAL_ACTION_DEFAULT;
    process->process_group=parent?parent->process_group:pid;
    process->session_id=parent?parent->session_id:pid;
    process->controlling_tty=parent?parent->controlling_tty:0;
    process->main_signal_mask=process_current_signal_mask(parent);
    if(!process->process_group) process->process_group=pid;
    if(!process->session_id) process->session_id=pid;
    process->stack_top=USER_STACK_TOP;
    process->kernel_stack_top=gdt_default_kernel_stack_top();
    process->next_thread_id=2;
    fpu_init_state(&process->fpu);
    set_process_name(process,path);
    (void)copy_process_path(process->executable_path,sizeof(process->executable_path),path);
    flags=process_irq_save();
    /* clear_process can yield after acquiring the lifetime reference. An
     * owner may disappear there while this slot is still unpublished. */
    life=lifetime_find(token);
    if(life && life->user && life->revoked && !life->owner) {
        process->killed=true;process->kill_signal=SRV_SIGNAL_KILL;
    }
    process->allocated=true;process_slots_busy[slot]=false;
    process_irq_restore(flags);
    return process;
}

static struct process *alloc_process(const char *path,bool detached) {
    return alloc_process_lifetime(path,detached,0);
}

static void release_process(struct process *process) {
    if (process == NULL) {
        return;
    }

    user_scope_detach(process);
    uint64_t token=process->lifetime;
    bool retired=!process->active;uint64_t status=process->exit_status;
    uint64_t slot=(uint64_t)(process-processes),flags=process_irq_save();
    process_slots_busy[slot]=true;
    process_signal_retire_image(process);
    process->image_generation=0;
    process->allocated=false;
    process_irq_restore(flags);

    if (process->kernel_stack_physical != 0 && process->kernel_stack_frames != 0) {
        pmm_free_frame_range(process->kernel_stack_physical, process->kernel_stack_frames);
        process->kernel_stack_physical = 0;
        process->kernel_stack_frames = 0;
        process->kernel_stack = NULL;
    }

    clear_process(process);
    flags=process_irq_save();
    struct process_lifetime *life=lifetime_find(token);
    user_scope_member_done(life,status,retired);
    process_slots_busy[slot]=false;process_irq_restore(flags);
    /* A competing waiter may have slept after another reaper claimed the
     * zombie but before allocated was cleared. Publish target disappearance. */
    scheduler_wake_all(&process_wait_queue);
}

static void release_process_runtime(struct process *process) {
    user_scope_detach(process);
    if (process == NULL) {
        return;
    }

    uint64_t irq=process_irq_save();
    struct capture_write_record *trace=capture_write_find(&capture_writes,process->pid);
    if(trace){uint64_t now=capture_write_now();if(now<trace->last_us)trace->invalid=1;trace->retired=1;trace->last_us=now;}
    process_signal_retire_image(process);
    process->image_generation=0;
    process_irq_restore(irq);
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

static uint64_t align_up_u64(uint64_t value, uint64_t align) {
    if (align <= 1) {
        return value;
    }
    uint64_t mask = align - 1;
    if ((align & mask) != 0) {
        align = 16;
        mask = align - 1;
    }
    return (value + mask) & ~mask;
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

static bool user_space_write_mode(struct process *process, uint64_t virtual_address,
    const void *source, uint64_t length,bool service) {
    const uint8_t *bytes = source;
    unsigned pages=0;
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
        if(service && !(++pages&31))process_service_checkpoint();
    }
    return true;
}

static bool user_space_write(struct process *process,uint64_t address,const void *source,uint64_t length) {
    return user_space_write_mode(process,address,source,length,true);
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
    uint64_t envp[PROCESS_MAX_ENV + 1];
    uint64_t argv_lengths[PROCESS_MAX_ARGS];
    uint64_t env_lengths[PROCESS_MAX_ENV];
    uint64_t stack_cursor;

    if (process == NULL || process->address_space == 0 || argv_values == NULL) {
        return false;
    }
    if (argc == 0 || argc > PROCESS_MAX_ARGS || envc > PROCESS_MAX_ENV) {
        return false;
    }

    uint64_t budget=SRV_EXEC_BYTES_MAX-sizeof(uint64_t)*(argc+envc+2);
    for (uint64_t i = 0; i < argc; i++) {
        if (argv_values[i] == NULL) {
            return false;
        }
        argv_lengths[i] = local_strlen_limit(argv_values[i], budget);
        if(argv_lengths[i]>=budget) return false;
        budget-=argv_lengths[i]+1;
    }
    for (uint64_t i = 0; i < envc; i++) {
        if (env_values == NULL || env_values[i] == NULL) {
            return false;
        }
        env_lengths[i] = local_strlen_limit(env_values[i], budget);
        if(env_lengths[i]>=budget) return false;
        budget-=env_lengths[i]+1;
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

    const struct elf64_program_header *programs =
        (const struct elf64_program_header *)(data + candidate->phoff);
    bool entry_executable = false;
    for (uint64_t i = 0; i < candidate->phnum; i++) {
        const struct elf64_program_header *program = &programs[i];
        if (program->type != PT_LOAD && program->type != PT_TLS) continue;
        if (program->filesz > program->memsz || program->offset > size ||
            program->filesz > size - program->offset ||
            (program->align > 1 && (program->align & (program->align - 1)) != 0)) return false;
        if (program->type == PT_TLS) {
            /* TLS slots are page-aligned and cannot honor larger alignment. */
            if (program->align > PAGE_SIZE) return false;
            continue;
        }
        if (program->align > 1 &&
            (program->vaddr & (program->align - 1)) != (program->offset & (program->align - 1)))
            return false;
        /* Leave room for page rounding and never publish a mapping outside
         * canonical user space or alias a shared kernel page-table entry. */
        if (program->vaddr >= (1ull << 47) ||
            program->memsz > (1ull << 47) - program->vaddr ||
            program->vaddr + program->memsz > (1ull << 47) - (PAGE_SIZE - 1))
            return false;
        if ((program->flags & PF_X) && candidate->entry >= program->vaddr &&
            candidate->entry - program->vaddr < program->memsz) entry_executable = true;
    }
    if (!entry_executable) return false;
    *header = candidate;
    return true;
}

static bool map_process_page_mode(struct process *process, uint64_t virtual_page, uint64_t flags,bool service) {
    if (process == NULL || process->mapping_count >= PROCESS_MAX_MAPPINGS ||
        (process->mapping_count && !process->mappings)) {
        if(service)console_printf("run: map page rejected process=%x count=%u limit=%u virt=%x\n",
            (uint64_t)process,
            process != NULL ? process->mapping_count : 0,
            (uint64_t)PROCESS_MAX_MAPPINGS,
            virtual_page);
        return false;
    }

    /* Ordinary/private image loading owns this record. Atomic growth requires
     * its existing backing and never allocates a tracking array here. The full
     * original capacity is reserved in one checked heap allocation; entries
     * are fully zeroed privately in bounded service batches before publication. */
    if (!process->mappings) {
        if (!service) return false;
        uint64_t bytes=sizeof(*process->mappings)*PROCESS_MAX_MAPPINGS;
        struct process_mapping *backing=kmalloc(bytes);
        if (!backing) return false;
        for(uint64_t at=0;at<bytes;) {
            uint64_t end=at+65536;if(end>bytes)end=bytes;
            while(at<end)((uint8_t *)backing)[at++]=0;
            process_service_checkpoint();
        }
        process->mappings=backing;
    }

    /* Atomic callers return failure silently: console rendering may yield. */
    uint64_t physical = pmm_alloc_frames_above_tagged(1,
        PROCESS_USER_PAGE_MIN_PHYSICAL,
        PMM_FRAME_USER);
    if (physical == 0) {
        if(service)console_printf("run: map page pmm exhausted virt=%x\n", virtual_page);
        return false;
    }

    zero_page(pmm_phys_to_virt(physical));
    if (!vmm_map_page_in_space(process->address_space, virtual_page, physical, flags)) {
        if(service)console_printf("run: map page failed as=%x virt=%x phys=%x flags=%x count=%u\n",
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
    uint64_t bytes=process->mapping_count*PAGE_SIZE;
    if(bytes>process->peak_mapped_bytes) process->peak_mapped_bytes=bytes;
    if(service && !(process->mapping_count&31))process_service_checkpoint();

    return true;
}

static bool map_process_page(struct process *process,uint64_t page,uint64_t flags) {
    return map_process_page_mode(process,page,flags,true);
}

static int64_t process_mapping_index(struct process *process, uint64_t virtual_page) {
    if (process == NULL || (process->mapping_count && !process->mappings)) {
        return -1;
    }
    for (uint64_t i = 0; i < process->mapping_count; i++) {
        if (process->mappings[i].virtual_address == virtual_page) {
            return (int64_t)i;
        }
    }
    return -1;
}

static void process_unmap_mapping_at(struct process *process, uint64_t index) {
    uint64_t irq = process_irq_save();
    process_futex_invalidate_page(process, process->mappings[index].virtual_address);
    uint64_t physical = 0;
    if (vmm_unmap_page_in_space(process->address_space,
            process->mappings[index].virtual_address, &physical)) {
        pmm_free_frame(physical);
    } else if (process->mappings[index].physical_address != 0) {
        pmm_free_frame(process->mappings[index].physical_address);
    }
    process->mappings[index] = process->mappings[--process->mapping_count];
    process_irq_restore(irq);
}

static bool process_unmap_tracked_page(struct process *process, uint64_t virtual_page) {
    int64_t index = process_mapping_index(process, virtual_page);
    if (index < 0) return false;
    process_unmap_mapping_at(process, (uint64_t)index);
    return true;
}

static bool process_unmap_tracked_page_checked(struct process *process,uint64_t page) {
    int64_t i=process_mapping_index(process,page);uint64_t physical=0;
    if(i<0 || !vmm_virt_to_phys_in_space(process->address_space,page,&physical) ||
       physical!=process->mappings[i].physical_address)return false;
    process_futex_invalidate_page(process,page);
    uint64_t removed=0;
    if(!vmm_unmap_page_in_space(process->address_space,page,&removed) || removed!=physical)return false;
    pmm_free_frame(physical);process->mappings[i]=process->mappings[--process->mapping_count];return true;
}

static void cleanup_process_mappings(struct process *process) {
    if (process == NULL) {
        return;
    }

    /* Publish each removed mapping before a service window. No freed frame
     * remains in the live tracking prefix, including failure/rollback cleanup. */
    unsigned batch=0;
    while(process->mapping_count) {
        process_unmap_mapping_at(process,process->mapping_count-1);
        if(!(++batch&31))process_service_checkpoint();
    }
    struct process_mapping *retired=process->mappings;
    process->mappings=NULL;
    if(retired) kfree(retired);
    for (uint64_t i = 0; i < PROCESS_MAX_MMAP_REGIONS; i++) {
        process->mmap_regions[i].used = false;
    }
    process->mmap_next = USER_MMAP_BASE;
}

void process_refresh_mappings(struct process *process) {
    if (process == NULL || process->address_space == 0) {
        return;
    }

    struct process_latency_span span=process_latency_begin();
    struct process_service_scope scope=process_service_begin();
    vmm_refresh_kernel_mappings(process->address_space);
    for (uint64_t i = 0; i < process->mapping_count; i++) {
        if(!(i&31))process_service_checkpoint();
        if (process->mappings[i].physical_address == 0) {
            continue;
        }
        (void)vmm_ensure_page_in_space(process->address_space,
            process->mappings[i].virtual_address,
            process->mappings[i].physical_address,
            process->mappings[i].flags);
    }
    process_service_end(scope);
    process_latency_end(LATENCY_PROCESS_REFRESH,span);
}

static bool cleanup_process_files(struct process *process) {
    if (process == NULL) {
        return true;
    }
    bool ok = process_stream_cleanup(process->stream_instance);
    for (uint64_t i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        if (!process->files[i].used) {
            continue;
        }
        if (process_file_close(process, i) < 0) ok = false;
    }
    return ok;
}

static uint64_t finish_process_files(struct process *process, uint64_t status) {
    bool ok=true;
    /* Flush redirected standard files even when a parent still owns a copy. */
    for(unsigned i=0;i<3;i++) {
        struct process_file *file=process_file_at(process,i);
        if(file && file->type==PROCESS_FILE_VFS_WRITE && process_file_flush(process,i)<0) ok=false;
    }
    if(!cleanup_process_files(process)) ok=false;
    return status!=0 || ok?status:1;
}



static void close_cloexec_files(struct process *process) {
    if (process == NULL) {
        return;
    }
    /* Opaque stream resources are always close-on-exec, never inherited. This
     * function runs after the successful image commit, not failed exec. */
    (void)process_stream_cleanup(process->stream_instance);
    for (uint64_t i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        uint64_t fd = i;
        if (process->files[i].used &&
            (process->files[i].flags & SRV_FD_CLOEXEC) != 0) {
            (void)process_file_close(process, fd);
        }
    }
}

static bool load_segment(struct process *process, const uint8_t *file, uint64_t file_size, const struct elf64_program_header *program) {
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
        if (!map_process_page(process, page, flags)) {
            return false;
        }
    }

    for (uint64_t copied = 0; copied < program->filesz;) {
        uint64_t virtual_address = program->vaddr + copied;
        uint64_t physical_address;
        if (!vmm_virt_to_phys_in_space(process->address_space, virtual_address, &physical_address)) {
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

static bool map_user_stack(struct process *process, uint64_t stack_top) {
    uint64_t bottom = stack_top - (USER_STACK_PAGES * PAGE_SIZE);
    for (uint64_t page = bottom; page < stack_top; page += PAGE_SIZE) {
        if (!map_process_page(process, page, VMM_PAGE_USER | VMM_PAGE_WRITABLE | VMM_PAGE_NO_EXECUTE)) {
            return false;
        }
    }
    return true;
}

static bool choose_process_stack_top(uint64_t image_end, uint64_t requested_stack_top, uint64_t *stack_top_out) {
    uint64_t stack_size = USER_STACK_PAGES * PAGE_SIZE;
    uint64_t guard_size = USER_STACK_GUARD_PAGES * PAGE_SIZE;
    uint64_t stack_top = requested_stack_top;
    if (stack_top < stack_size + guard_size) {
        return false;
    }

    uint64_t stack_bottom = stack_top - stack_size;
    if (page_up(image_end) + guard_size <= stack_bottom) {
        *stack_top_out = stack_top;
        return true;
    }

    stack_top = USER_DYNAMIC_STACK_TOP;
    if (stack_top < stack_size + guard_size) {
        return false;
    }
    stack_bottom = stack_top - stack_size;
    if (page_up(image_end) + guard_size > stack_bottom) {
        console_printf("run: image too large for user stack image_end=%x stack_bottom=%x\n",
            image_end,
            stack_bottom);
        return false;
    }

    *stack_top_out = stack_top;
    return true;
}

static bool process_capture_tls_template(struct process *process,
    const uint8_t *file,
    uint64_t file_size,
    const struct elf64_program_header *program) {
    if (process == NULL || program == NULL || program->memsz == 0) {
        return true;
    }
    if (program->filesz > program->memsz ||
        program->filesz > PROCESS_TLS_TEMPLATE_MAX ||
        program->memsz > PROCESS_TLS_TEMPLATE_MAX ||
        program->offset > file_size ||
        program->filesz > file_size - program->offset) {
        console_printf("run: unsupported TLS filesz=%x memsz=%x\n",
            program->filesz,
            program->memsz);
        return false;
    }

    for (uint64_t i = 0; i < PROCESS_TLS_TEMPLATE_MAX; i++) {
        process->tls_template[i] = 0;
    }
    copy_bytes(process->tls_template, file + program->offset, program->filesz);
    process->tls_filesz = program->filesz;
    process->tls_memsz = program->memsz;
    process->tls_align = program->align != 0 ? program->align : 16;
    return true;
}

/* Slot zero belongs to the main thread. Worker slots are reused only after
 * their old TLS has been unmapped; addresses never advance for process life. */
static bool process_setup_tls_block_mode(struct process *process, unsigned slot,
    uint64_t *base_out, uint64_t *thread_pointer_out,bool service) {
    if (process == NULL || process->tls_memsz == 0) {
        if (base_out != NULL) {
            *base_out = 0;
        }
        if (thread_pointer_out != NULL) {
            *thread_pointer_out = 0;
        }
        return true;
    }

    uint64_t block_size = align_up_u64(process->tls_memsz, process->tls_align);
    uint64_t mapped_length = page_up(block_size + USER_TLS_TCB_SIZE);
    if (!mapped_length || slot>PROCESS_MAX_USER_THREADS ||
        mapped_length>(USER_TLS_LIMIT-USER_TLS_BASE)/(PROCESS_MAX_USER_THREADS+1)) {
        return false;
    }
    uint64_t base=USER_TLS_BASE+slot*mapped_length;
    uint64_t thread_pointer=base+block_size;
    for (uint64_t page=0;page<mapped_length;page+=PAGE_SIZE)
        if (process_mapping_index(process,base+page)>=0) return false;

    uint64_t mapped = 0;
    for (; mapped < mapped_length; mapped += PAGE_SIZE) {
        if (!map_process_page_mode(process, base + mapped, VMM_PAGE_USER | VMM_PAGE_WRITABLE | VMM_PAGE_NO_EXECUTE,service)) {
            for (uint64_t rollback = 0; rollback < mapped; rollback += PAGE_SIZE) {
                if(service)(void)process_unmap_tracked_page(process,base+rollback);
                else (void)process_unmap_tracked_page_checked(process,base+rollback);
            }
            return false;
        }
    }
    if (process->tls_filesz != 0 &&
        !user_space_write_mode(process, base, process->tls_template, process->tls_filesz,service)) {
        for (uint64_t rollback = 0; rollback < mapped_length; rollback += PAGE_SIZE) {
            if(service)(void)process_unmap_tracked_page(process,base+rollback);
                else (void)process_unmap_tracked_page_checked(process,base+rollback);
        }
        return false;
    }
    if (!user_space_write_mode(process, thread_pointer, &thread_pointer, sizeof(thread_pointer),service)) {
        for (uint64_t rollback = 0; rollback < mapped_length; rollback += PAGE_SIZE) {
            if(service)(void)process_unmap_tracked_page(process,base+rollback);
                else (void)process_unmap_tracked_page_checked(process,base+rollback);
        }
        return false;
    }

    if (base_out != NULL) {
        *base_out = base;
    }
    if (thread_pointer_out != NULL) {
        *thread_pointer_out = thread_pointer;
    }
    return true;
}

static bool process_setup_tls_block(struct process *process,unsigned slot,uint64_t *base,uint64_t *pointer) {
    return process_setup_tls_block_mode(process,slot,base,pointer,true);
}

static void process_release_thread_tls(struct process_user_thread *thread) {
    if (!thread || !thread->owner || !thread->tls_base) return;
    struct process *process=thread->owner;
    uint64_t size=page_up(align_up_u64(process->tls_memsz,process->tls_align)+USER_TLS_TCB_SIZE);
    for (uint64_t page=0;page<size;page+=PAGE_SIZE)
        (void)process_unmap_tracked_page(process,thread->tls_base+page);
    thread->tls_base=0;thread->tls_thread_pointer=0;
}

static int64_t load_elf_image_checked_impl(const char *path, struct process *process, uint64_t stack_top, uint64_t *entry_out) {
    if (process == NULL || process->address_space != 0) {
        return SRV_ERR_NOMEM;
    }
    if (!vmm_create_address_space(&process->address_space)) {
        console_write("run: failed to create address space\n");
        return SRV_ERR_NOMEM;
    }


    const struct vfs_node *node = vfs_lookup(path);
    const uint8_t *data;
    uint64_t size;
    const struct elf64_header *header;

    int64_t error = node == NULL ? SRV_ERR_NOENT :
        node->type != VFS_NODE_FILE || !(node->metadata.mode & 0111) ? SRV_ERR_ACCES : 0;
    if (!error && !vfs_read_all(node, &data, &size)) error = SRV_ERR_IO;
    if (error) {
        vmm_destroy_address_space(process->address_space);
        process->address_space = 0;
        return error;
    }

    if (!validate_elf(data, size, &header)) {
        console_printf("run: invalid executable: %s\n", path);
        vfs_release_data(node, data);
        vmm_destroy_address_space(process->address_space);
        process->address_space = 0;
        return SRV_ERR_NOEXEC;
    }

    const struct elf64_program_header *programs =
        (const struct elf64_program_header *)(data + header->phoff);

    uint64_t image_end = 0;
    uint64_t tls_program = UINT64_MAX;
    for (uint64_t i = 0; i < header->phnum; i++) {
        if (programs[i].type == PT_LOAD) {
            uint64_t end = programs[i].vaddr + programs[i].memsz;
            if (end > image_end) {
                image_end = end;
            }
        } else if (programs[i].type == PT_TLS) {
            tls_program = i;
        }
    }

    if (!choose_process_stack_top(image_end, stack_top, &stack_top)) {
        vfs_release_data(node, data);
        vmm_destroy_address_space(process->address_space);
        process->address_space = 0;
        return SRV_ERR_NOMEM;
    }

    for (uint64_t i = 0; i < header->phnum; i++) {
        if (programs[i].type == PT_LOAD && !load_segment(process, data, size, &programs[i])) {
            console_printf("run: failed to load segment %u\n", i);
            vfs_release_data(node, data);
            cleanup_process_mappings(process);
            vmm_destroy_address_space(process->address_space);
            process->address_space = 0;
            return SRV_ERR_NOMEM;
        }
    }

    if (tls_program != UINT64_MAX &&
        !process_capture_tls_template(process, data, size, &programs[tls_program])) {
        vfs_release_data(node, data);
        cleanup_process_mappings(process);
        vmm_destroy_address_space(process->address_space);
        process->address_space = 0;
        return SRV_ERR_NOMEM;
    }

    if (!process_setup_tls_block(process, 0, &process->tls_base, &process->tls_thread_pointer)) {
        console_write("run: failed to map TLS block\n");
        vfs_release_data(node, data);
        cleanup_process_mappings(process);
        vmm_destroy_address_space(process->address_space);
        process->address_space = 0;
        return SRV_ERR_NOMEM;
    }

    if (!map_user_stack(process, stack_top)) {
        console_write("run: failed to map user stack\n");
        vfs_release_data(node, data);
        cleanup_process_mappings(process);
        vmm_destroy_address_space(process->address_space);
        process->address_space = 0;
        return SRV_ERR_NOMEM;
    }

    *entry_out = header->entry;
    process->entry = header->entry;
    process->stack_top = stack_top;
    process->heap_base = page_up(image_end);
    process->program_break = process->heap_base;
    process->mapped_break = process->heap_base;
    process->heap_limit = stack_top - (USER_STACK_PAGES * PAGE_SIZE) - (USER_STACK_GUARD_PAGES * PAGE_SIZE);
    vfs_release_data(node, data);
    return 0;
}

static int64_t load_elf_image_checked(const char *path,struct process *process,uint64_t stack_top,uint64_t *entry_out) {
    struct process_latency_span span=process_latency_begin();
    struct process_service_scope scope=process_service_begin();
    char logical[PROCESS_PATH_MAX],storage[PROCESS_PATH_MAX];
    struct process *current=process_current();
    int64_t result=0;
#if !defined(SRVROS_PROCESS_HOST_TEST) || defined(SRVROS_PROCESS_SERVICE_HOST_TEST)
    /* Before scheduling starts there is no current user process; retain the
     * boot loader path. A running process must own admission before lookup. */
    if(!scope.held && (current || scheduler_io_active())) result=SRV_ERR_IO;
#endif
    if(result>=0) result=current?process_path_context_for(current,SRV_PATH_RESOLVE,path,logical,sizeof(logical)):
        (copy_process_path(logical,sizeof(logical),path)?0:SRV_ERR_NAMETOOLONG);
    if(result>=0) result=vfs_namespace_system_path(logical,storage,sizeof(storage));
    if(result>=0) result=load_elf_image_checked_impl(storage,process,stack_top,entry_out);
    process_service_end(scope);
    process_latency_end(LATENCY_PROCESS_IMAGE,span);return result;
}

/* Only process_spawn_exec calls this path: its fresh child has no runnable
 * thread, and no parent files/vectors/mappings are changed until this scope ends.
 * Snapshot even the path again here so the service contract is self-contained. */
static struct process_service_scope process_spawn_image_scope(struct process *parent,
    struct process *child,const char *path,char *snapshot,uint64_t capacity) {
    struct process_service_scope scope={0};
    bool private=parent && child && child!=parent && child->allocated && child->active &&
        child->parent_pid==parent->pid && !child->startup_ready && !child->entry &&
        !child->address_space && !child->mapping_count && !child->mappings && !child->killed && !child->exiting;
    if(private)for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++)
        if(child->user_threads[i].used)private=false;
    if(!copy_process_path(snapshot,capacity,path)) {if(snapshot && capacity)snapshot[0]=0;private=false;}
#if !defined(SRVROS_PROCESS_HOST_TEST) || defined(SRVROS_PROCESS_SERVICE_HOST_TEST)
    scope.flags=process_irq_save();
    if(private)scope.held=scheduler_io_begin_process_image(child);
    if(!scope.held)scope.held=scheduler_io_begin();
#else
    (void)private;
#endif
    return scope;
}
static int64_t load_spawn_image_checked(struct process *parent,struct process *child,const char *path) {
    char snapshot[PROCESS_PATH_MAX]={0};
    struct process_latency_span span=process_latency_begin();
    /* Resolve while the parent is admitted, then keep IRQs masked over the
     * transition to the private-image scope. The private scope sees only the
     * copied raw path and never inspects the parent cwd or namespace. */
    uint64_t irq=process_irq_save();
    struct process_service_scope preparation=process_service_begin();
    char logical[PROCESS_PATH_MAX],storage[PROCESS_PATH_MAX];
    int64_t result=0;
#if !defined(SRVROS_PROCESS_HOST_TEST) || defined(SRVROS_PROCESS_SERVICE_HOST_TEST)
    if(parent && !preparation.held) result=SRV_ERR_IO;
#endif
    if(result>=0) result=parent?process_path_context_for(parent,SRV_PATH_RESOLVE,path,logical,sizeof(logical)):
        (copy_process_path(logical,sizeof(logical),path)?0:SRV_ERR_NAMETOOLONG);
    if(result>=0) result=vfs_namespace_system_path(logical,storage,sizeof(storage));
    process_service_end(preparation);
    if(result>=0) {
        struct process_service_scope scope=process_spawn_image_scope(parent,child,storage,snapshot,sizeof(snapshot));
#if !defined(SRVROS_PROCESS_HOST_TEST) || defined(SRVROS_PROCESS_SERVICE_HOST_TEST)
        if(parent && !scope.held) result=SRV_ERR_IO;
#endif
        if(result>=0) result=snapshot[0]?load_elf_image_checked_impl(snapshot,child,USER_STACK_TOP,&child->entry):SRV_ERR_NAMETOOLONG;
        process_service_end(scope);
    }
    process_irq_restore(irq);
    process_latency_end(LATENCY_PROCESS_IMAGE,span);return result;
}

static bool load_elf_image(const char *path, struct process *process, uint64_t stack_top, uint64_t *entry_out) {
    return load_elf_image_checked(path,process,stack_top,entry_out)==0;
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

    uint64_t irq=process_irq_save();
    process_signal_retire_image(process);
    process->image_generation=0;
    process_irq_restore(irq);
    struct process_latency_span span=process_latency_begin();
    struct process_service_scope scope=process_service_begin();
    cleanup_process_mappings(process);
    if (process->address_space != 0) {
        vmm_destroy_address_space(process->address_space);
        process->address_space = 0;
    }
    process_service_end(scope);
    process_latency_end(LATENCY_PROCESS_MEMORY,span);
}

static bool enter_process(struct process *process, const char *label) {
    uint64_t restored = process_context_save(&process->context);
    if (restored == 0) {
        scheduler_set_user_context(process, process->address_space, process->kernel_stack_top, process->image_generation);
        if (!process->quiet) {
            console_printf("%s: entering pid=%u %s entry=%x stack=%x\n",
                label,
                process->pid,
                process->name,
                process->entry,
                process->stack_top);
        }
        if(process->fork_resume) {
#ifdef SRVROS_SIGNAL_PUBLIC_GUEST_TEST
            (void)process_irq_save();
            if(process_should_exit_current())process_exit(process_current_kill_status());
            struct signal_state_identity child_identity={process->image_generation,1,scheduler_current_thread_id()};
            if(!signal_thread_state_bind(&process->main_signal_state,child_identity))process_exit(128+SRV_SIGNAL_SEGV);
#endif
            usermode_resume(&process->fork_frame,process->tls_thread_pointer);
        }
        usermode_enter(process->entry,
            process->stack_top,
            process->argc,
            process->argv,
            process->envp,
            process->tls_thread_pointer);
    }

    scheduler_clear_user_context();
    PROCESS_ARCH_STI();
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
        !setup_process_arguments(process, path, args != NULL ? args : "") ||
        process_prepare_image(process) < 0) {
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

    /* A foreground child rebinds the caller's scheduler context. Worker
     * bindings cannot yet be restored safely after that synchronous switch. */
    if (process_current_user_thread()) return SRV_ERR_NOTSUP;

    struct process *child = alloc_process(path, false);
    if (child == NULL) {
        return -1;
    }
    if (!allocate_kernel_stack(child)) {
        release_process(child);
        return -1;
    }

    if (!load_elf_image(path, child, USER_STACK_TOP, &child->entry) ||
        !setup_process_arguments(child, path, args) || process_prepare_image(child) < 0) {
        cleanup_process_address_space(child);
        release_process(child);
        return -1;
    }

    uint64_t restored = process_context_save(&child->context);
    if (restored == 0) {
        scheduler_set_user_context(child, child->address_space, child->kernel_stack_top, child->image_generation);
        console_printf("spawn: entering pid=%u %s entry=%x stack=%x\n",
            child->pid,
            path,
            child->entry,
            child->stack_top);
        usermode_enter(child->entry,
            child->stack_top,
            child->argc,
            child->argv,
            child->envp,
            child->tls_thread_pointer);
    }

    int64_t status = (int64_t)child->exit_status;
    process_account_child(parent,child);
    scheduler_set_user_context(parent, parent->address_space, parent->kernel_stack_top, parent->image_generation);
    PROCESS_ARCH_STI();
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

    /* A foreground child rebinds the caller's scheduler context. Worker
     * bindings cannot yet be restored safely after that synchronous switch. */
    if (process_current_user_thread()) return SRV_ERR_NOTSUP;

    struct process *child = alloc_process(path, false);
    if (child == NULL) {
        return -1;
    }

    if (!allocate_kernel_stack(child)) {
        release_process(child);
        return -1;
    }

    if (!load_elf_image(path, child, USER_STACK_TOP, &child->entry) ||
        !setup_process_arguments(child, path, args) || process_prepare_image(child) < 0 ||
        !process_set_stdout_redirect(child, stdout_path, append)) {
        cleanup_process_address_space(child);
        release_process(child);
        return -1;
    }

    uint64_t restored = process_context_save(&child->context);
    if (restored == 0) {
        scheduler_set_user_context(child, child->address_space, child->kernel_stack_top, child->image_generation);
        console_printf("spawn: entering pid=%u %s entry=%x stack=%x\n",
            child->pid,
            path,
            child->entry,
            child->stack_top);
        usermode_enter(child->entry,
            child->stack_top,
            child->argc,
            child->argv,
            child->envp,
            child->tls_thread_pointer);
    }

    int64_t status = (int64_t)child->exit_status;
    process_account_child(parent,child);
    scheduler_set_user_context(parent, parent->address_space, parent->kernel_stack_top, parent->image_generation);
    PROCESS_ARCH_STI();
    release_process(child);
    return status;
}

int64_t process_spawn_exec_scoped(uint64_t scope_token,const char *path,
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
    uint64_t file_action_count,
    uint64_t signal_flags,
    uint64_t signal_mask,
    uint64_t signal_default) {
    struct process *parent = process_current();
    if (parent == NULL || path == NULL || path[0] == '\0') {
        return SRV_ERR_INVAL;
    }

    /* Detached children run on their own scheduler thread. */
    if (!detached && process_current_user_thread()) return SRV_ERR_NOTSUP;

    struct process *child = alloc_process_lifetime(path, detached, scope_token);
    if (child == NULL) {
        console_write("exec: process table full\n");
        return SRV_ERR_AGAIN;
    }
    if (!allocate_kernel_stack(child)) {
        console_write("exec: kernel stack alloc failed\n");
        release_process(child);
        return SRV_ERR_NOMEM;
    }

    int64_t error=load_spawn_image_checked(parent,child,path);
    if(!error && !setup_process_vectors(child,argc,argv,envc,envp)) error=SRV_ERR_NOMEM;
    if(!error) error=process_prepare_image(child);
    if(!error && (signal_flags&SRV_EXEC_INHERIT_FILES)) {
        error=process_inherit_files(child,parent);
        if(!error) error=process_posix_spawn_actions(child,file_actions,file_action_count);
        if(!error) close_cloexec_files(child);
    } else if(!error && (!process_configure_stdio_fds(child,parent,stdin_fd,stdout_fd,stderr_fd) ||
        !process_apply_spawn_file_actions(child,parent,file_actions,file_action_count))) error=SRV_ERR_BADF;
    if(error) {
        cleanup_process_address_space(child);
        cleanup_process_files(child);
        release_process(child);
        return error;
    }

    struct process_file *terminal_input=process_file_at(child,0);
    if(terminal_input && terminal_input->type==PROCESS_FILE_PTY_SLAVE)
        child->controlling_tty=terminal_input->handle;
    if (process_group == UINT64_MAX) {
        child->process_group = child->pid;
    } else if (process_group != 0) {
        child->process_group = process_group;
    }
    if ((signal_flags & SRV_EXEC_SET_SIGNAL_DEFAULT) != 0) {
        process_apply_signal_defaults(child, signal_default);
    }
    if ((signal_flags & SRV_EXEC_SET_SIGNAL_MASK) != 0) {
        child->main_signal_mask = signal_mask;
    }
    if (foreground && child->process_group != 0) {
        struct pty_model *terminal=pty_state(child->controlling_tty);
        if(terminal) terminal->foreground_group=child->process_group;
        else if(!child->controlling_tty) foreground_process_group = child->process_group;
    }

    if (detached) {
        uint64_t pid = child->pid;
        child->quiet = true;
        if (!scheduler_spawn("user", background_process_thread, child)) {
            console_write("exec: scheduler admission failed (stack/storage resources)\n");
            cleanup_process_address_space(child);
            cleanup_process_files(child);
            release_process(child);
            return SRV_ERR_AGAIN;
        }
        return (int64_t)pid;
    }

    uint64_t restored = process_context_save(&child->context);
    if (restored == 0) {
        scheduler_set_user_context(child, child->address_space, child->kernel_stack_top, child->image_generation);
        console_printf("spawn: entering pid=%u %s entry=%x stack=%x\n",
            child->pid,
            path,
            child->entry,
            child->stack_top);
        usermode_enter(child->entry,
            child->stack_top,
            child->argc,
            child->argv,
            child->envp,
            child->tls_thread_pointer);
    }

    int64_t status = (int64_t)child->exit_status;
    process_account_child(parent,child);
    scheduler_set_user_context(parent, parent->address_space, parent->kernel_stack_top, parent->image_generation);
    PROCESS_ARCH_STI();
    release_process(child);
    return status;
}

int64_t process_spawn_exec(const char *path,uint64_t argc,const char *const *argv,
    uint64_t envc,const char *const *envp,bool detached,int64_t stdin_fd,
    int64_t stdout_fd,int64_t stderr_fd,uint64_t process_group,bool foreground,
    const struct process_spawn_file_action *file_actions,uint64_t file_action_count,
    uint64_t signal_flags,uint64_t signal_mask,uint64_t signal_default) {
    return process_spawn_exec_scoped(0,path,argc,argv,envc,envp,detached,stdin_fd,
        stdout_fd,stderr_fd,process_group,foreground,file_actions,file_action_count,
        signal_flags,signal_mask,signal_default);
}

static uint64_t load_process_u64(const uint64_t *value) {
    const volatile uint64_t *source = value;
    return *source;
}

static void move_process_image(struct process *destination, struct process *source) {
    uint64_t irq=process_irq_save();
    destination->entry = load_process_u64(&source->entry);
    destination->stack_top = load_process_u64(&source->stack_top);
    destination->argc = load_process_u64(&source->argc);
    destination->argv = load_process_u64(&source->argv);
    destination->envp = load_process_u64(&source->envp);
    destination->address_space = load_process_u64(&source->address_space);
    destination->image_generation = load_process_u64(&source->image_generation);
    source->image_generation = 0;
    destination->heap_base = load_process_u64(&source->heap_base);
    destination->program_break = load_process_u64(&source->program_break);
    destination->mapped_break = load_process_u64(&source->mapped_break);
    destination->heap_limit = load_process_u64(&source->heap_limit);
    destination->tls_filesz = load_process_u64(&source->tls_filesz);
    destination->tls_memsz = load_process_u64(&source->tls_memsz);
    destination->tls_align = load_process_u64(&source->tls_align);
    destination->tls_base = load_process_u64(&source->tls_base);
    destination->tls_thread_pointer = load_process_u64(&source->tls_thread_pointer);
    for (uint64_t i = 0; i < PROCESS_TLS_TEMPLATE_MAX; i++) {
        destination->tls_template[i] = source->tls_template[i];
    }
    destination->mapping_count = load_process_u64(&source->mapping_count);
    if(source->peak_mapped_bytes>destination->peak_mapped_bytes)
        destination->peak_mapped_bytes=source->peak_mapped_bytes;
    destination->mappings=source->mappings;
    source->mappings=NULL;

    source->address_space = 0;
    source->mapping_count = 0;
    process_irq_restore(irq);
}

/* The exec image is private until fully loaded. Heap mutation stays atomic;
 * initialization may service other processes only after kmalloc returns. */
static struct process *process_allocate_exec_image(void) {
    struct process_service_scope scope=process_service_begin();
    struct process *image=NULL;
    if(scope.held || !scheduler_io_active()) {
        image=kmalloc(sizeof(*image));
        if(image)for(uint64_t at=0;at<sizeof(*image);) {
            uint64_t end=at+65536;if(end>sizeof(*image))end=sizeof(*image);
            while(at<end)((uint8_t *)image)[at++]=0;
            process_service_checkpoint();
        }
    }
    process_service_end(scope);return image;
}

int64_t process_exec_replace(const char *path,
    uint64_t argc,
    const char *const *argv,
    uint64_t envc,
    const char *const *envp,
    int64_t stdin_fd,
    int64_t stdout_fd,
    int64_t stderr_fd,
    uint64_t signal_flags,
    uint64_t signal_mask,
    uint64_t signal_default) {
    struct process *process = process_current();
    if (process == NULL || path == NULL || path[0] == '\0') {
        return SRV_ERR_INVAL;
    }

    if(process_current_user_thread() || process_signal_records_active(process)) return SRV_ERR_NOTSUP;
    for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++) if(process->user_threads[i].used) return SRV_ERR_NOTSUP;
    struct process *image = process_allocate_exec_image();
    if (image == NULL) {
        return SRV_ERR_NOMEM;
    }
    image->stack_top = USER_STACK_TOP;

    int64_t error=load_elf_image_checked(path,image,USER_STACK_TOP,&image->entry);
    if (!error && !setup_process_vectors(image,argc,argv,envc,envp)) error=SRV_ERR_NOMEM;
    if (!error) error=process_prepare_image(image);
    if (error) {
        cleanup_process_address_space(image);
        kfree(image);
        return error;
    }
    if (!process_apply_stdio_fds(process, stdin_fd, stdout_fd, stderr_fd)) {
        cleanup_process_address_space(image);
        kfree(image);
        return SRV_ERR_BADF;
    }

    uint64_t commit_irq=process_irq_save();
    process_signal_retire_image(process);
    user_scope_detach(process);
    scheduler_clear_user_context();
    process->image_generation=0;
    process_irq_restore(commit_irq);
    cleanup_process_address_space(process);
    move_process_image(process, image);
    set_process_name(process, path);
    (void)copy_process_path(process->executable_path, sizeof(process->executable_path), path);
    fpu_init_state(&process->fpu);
    process->signal_catch_mask = 0;
    process->fork_resume=false;
    process->main_thread_exited = false;
    process->main_thread_joined = process->main_thread_detached = false;
    process->main_thread_join_claimed = false;
    process->main_thread_return_value = 0;
    /* Ignored dispositions, pending signals, masks and alarms survive exec. */
    if ((signal_flags & SRV_EXEC_SET_SIGNAL_DEFAULT) != 0) {
        process_apply_signal_defaults(process, signal_default);
    }
    if ((signal_flags & SRV_EXEC_SET_SIGNAL_MASK) != 0) {
        process->main_signal_mask = signal_mask;
    }
    kfree(image);

    close_cloexec_files(process);

    scheduler_set_user_context(process, process->address_space, process->kernel_stack_top, process->image_generation);
    console_printf("exec: entering pid=%u %s entry=%x stack=%x\n",
        process->pid,
        path,
        process->entry,
        process->stack_top);
    /* Caught dispositions were reset for the committed image. Pending bits
     * now subject to a default must be evaluated before its first instruction,
     * even if no later mask syscall or signal arrives. Keep this decision and
     * user entry indivisible; usermode_enter installs the user RFLAGS itself.
     * An earlier fatal cause during image retirement retains priority. */
    (void)process_irq_save();
    if (!process->killed) {
        (void)process_signal_mask_current_locked(SRV_SIGNAL_SETMASK,
            process->main_signal_mask, NULL);
    }
    if (process->killed) process_exit(process_current_kill_status());
    usermode_enter(process->entry,
        process->stack_top,
        process->argc,
        process->argv,
        process->envp,
        process->tls_thread_pointer);
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
    process_notify_parent_child_exit(process);
    /* No access to process after publication: another thread may reap it. */
    process->reapable = true;
    scheduler_wake_all(&process_wait_queue);
}

/* Eager cloning is intentional for the first CLI fork profile. There is no
 * shared writable page or COW ownership to retire on a failed reservation. */
static bool process_clone_address_space_impl(struct process *child,const struct process *parent) {
    if(!vmm_create_address_space(&child->address_space)) return false;
    for(uint64_t i=0;i<parent->mapping_count;i++) {
        const struct process_mapping *source=&parent->mappings[i];
        if(!map_process_page(child,source->virtual_address,source->flags)) return false;
        const struct process_mapping *target=&child->mappings[child->mapping_count-1];
        copy_bytes(pmm_phys_to_virt(target->physical_address),pmm_phys_to_virt(source->physical_address),PAGE_SIZE);
    }
    return true;
}

static bool process_clone_address_space(struct process *child,const struct process *parent) {
    struct process_service_scope scope=process_service_begin();
    bool result=process_clone_address_space_impl(child,parent);
    process_service_end(scope);return result;
}

int64_t process_fork(const struct isr_frame *frame) {
    struct process *parent=process_current();
    if(!parent || !frame || frame->cs!=0x23 || frame->ss!=0x1b) return SRV_ERR_INVAL;
    /* A stopped worker may own a userspace lock or in-flight runtime state.
     * General multithreaded fork needs explicit libc/descriptor preparation. */
    if(process_current_user_thread() || process_signal_records_active(parent)) return SRV_ERR_NOTSUP;
    for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++) if(parent->user_threads[i].used) return SRV_ERR_NOTSUP;
    for(unsigned i=0;i<PROCESS_MAX_OPEN_FILES;i++) if(parent->files[i].used) {
        enum process_file_type type=parent->files[i].type;
        if(type==PROCESS_FILE_NET_LISTENER || type==PROCESS_FILE_NET_CONNECTION ||
           type==PROCESS_FILE_NET_UDP || type==PROCESS_FILE_UNIX_LISTENER) return SRV_ERR_NOTSUP;
    }
    struct process *child=alloc_process(parent->executable_path,true);
    if(!child) return SRV_ERR_AGAIN;
    int64_t result=SRV_ERR_NOMEM;
    if(!allocate_kernel_stack(child) || !process_clone_address_space(child,parent)) goto fail;
    child->quiet=true;
    child->entry=frame->rip;child->stack_top=frame->rsp;
    child->argc=parent->argc;child->argv=parent->argv;child->envp=parent->envp;
    child->heap_base=parent->heap_base;child->program_break=parent->program_break;
    child->mapped_break=parent->mapped_break;child->heap_limit=parent->heap_limit;
    child->tls_filesz=parent->tls_filesz;child->tls_memsz=parent->tls_memsz;
    child->tls_align=parent->tls_align;child->tls_base=parent->tls_base;
    child->tls_thread_pointer=parent->tls_thread_pointer;
    copy_bytes(child->tls_template,parent->tls_template,sizeof(child->tls_template));
    copy_bytes((uint8_t *)child->mmap_regions,(const uint8_t *)parent->mmap_regions,sizeof(child->mmap_regions));
    child->mmap_next=parent->mmap_next;
    child->signal_catch_mask=parent->signal_catch_mask;
    child->signal_ignore_mask=parent->signal_ignore_mask;
    child->usr1_action=parent->usr1_action;
    child->usr1_action_generation=parent->usr1_action_generation;
    /* alloc_process inherited the caller mask, resource limits and group/session;
     * pending signals, alarms, usage and other threads start empty. */
    child->fpu=parent->fpu;
    fpu_invalidate_user_trap(&child->fpu);
    result=process_prepare_image(child);
    if (result<0) goto fail;
#ifdef SRVROS_SIGNAL_PUBLIC_GUEST_TEST
    {
        uint64_t irq=process_irq_save();struct process_signal_owner owner;
        struct signal_thread_state state=parent->main_signal_state;
        bool copied=process_signal_observe_owner(&owner,true) && owner.process==parent && !owner.worker &&
            signal_thread_state_bind(&state,owner.identity) &&
            signal_state_fork_clone(&child->main_signal_state,&state,owner.identity,child->image_generation);
        process_irq_restore(irq);
        if(!copied){result=SRV_ERR_NOTSUP;goto fail;}
    }
#endif
    (void)cleanup_process_files(child);
    for(unsigned i=0;i<PROCESS_MAX_OPEN_FILES;i++) if(parent->files[i].used) {
        uint64_t irq=process_irq_save();
        bool copied=process_file_clone_entry(&child->files[i],&parent->files[i]);
        process_irq_restore(irq);
        if(!copied) { result=SRV_ERR_AGAIN;goto fail; }
    }
#ifdef SRVROS_SIGNAL_PUBLIC_GUEST_TEST
    copy_bytes((uint8_t *)child->file_generations,(const uint8_t *)parent->file_generations,sizeof(child->file_generations));
#endif
    child->fork_frame=*frame;child->fork_frame.rax=0;child->fork_resume=true;
    uint64_t pid=child->pid;
    if(!scheduler_spawn("fork",background_process_thread,child)) { result=SRV_ERR_AGAIN;goto fail; }
    return (int64_t)pid;
fail:
    (void)cleanup_process_files(child);
    cleanup_process_address_space(child);
    release_process(child);
    return result;
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

    if(!process->parent_pid && system_reaper_pid) process->parent_pid=system_reaper_pid;
    if (!allocate_kernel_stack(process)) {
        release_process(process);
        return false;
    }

    if (!load_elf_image(path, process, USER_STACK_TOP, &process->entry) ||
        !setup_process_arguments(process, path, "") || process_prepare_image(process) < 0) {
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



/* POSIX spawn starts from the complete table; native GUI spawn deliberately
 * retains its explicit-transfer contract. These entries own independent refs. */
static int64_t process_inherit_files(struct process *child,struct process *parent) {
    (void)cleanup_process_files(child);
    uint64_t irq=process_irq_save();int64_t result=0;
    for(unsigned i=0;i<PROCESS_MAX_OPEN_FILES;i++) if(parent->files[i].used &&
        !process_file_clone_entry(&child->files[i],&parent->files[i])) { result=SRV_ERR_NOMEM;break; }
    process_irq_restore(irq);return result;
}
static int64_t process_posix_spawn_actions(struct process *child,
    const struct process_spawn_file_action *actions,uint64_t count) {
    if(count>SRV_SPAWN_FILE_ACTION_MAX || (count && !actions)) return SRV_ERR_INVAL;
    for(uint64_t i=0;i<count;i++) {
        const struct process_spawn_file_action *a=&actions[i];
        if(a->reserved || a->fd<0 || a->fd>=PROCESS_MAX_OPEN_FILES) return SRV_ERR_BADF;
        if(a->type==SRV_SPAWN_FILE_ACTION_CLOSE) {
            if(process_file_at(child,(uint64_t)a->fd)) (void)process_file_close(child,(uint64_t)a->fd);
        } else if(a->type==SRV_SPAWN_FILE_ACTION_DUP2) {
            int64_t result=process_file_dup2(child,(uint64_t)a->fd,(uint64_t)a->new_fd);
            if(result<0) return result;
            /* Spawn's same-FD action also clears CLOEXEC, unlike plain dup2. */
            (void)process_file_set_fd_flags(child,(uint64_t)a->new_fd,0);
        } else if(a->type==SRV_SPAWN_FILE_ACTION_OPEN) {
            char path[PROCESS_PATH_MAX];
            int64_t result=process_path_context_for(child,SRV_PATH_RESOLVE,a->path,path,sizeof(path));
            if(result<0) return result;
            if(process_file_at(child,(uint64_t)a->fd)) (void)process_file_close(child,(uint64_t)a->fd);
            result=process_file_open_flags(child,path,a->open_flags);
            if(result<0) return result;
            if(result!=a->fd) {
                int64_t opened=result;
                result=process_file_dup2(child,(uint64_t)opened,(uint64_t)a->fd);
                (void)process_file_close(child,(uint64_t)opened);
                if(result<0) return result;
            }
        } else return SRV_ERR_INVAL;
    }
    return 0;
}

static bool process_apply_stdio_fds(struct process *process, int64_t input, int64_t output, int64_t error) {
    return process_configure_stdio_fds(process,process,input,output,error);
}



static struct process_file *process_fd_source(struct process *process, int64_t fd,
    struct process_file *unused, bool resolve) {
    (void)unused;(void)resolve;return fd>=0?process_file_at(process,(uint64_t)fd):NULL;
}

static bool process_spawn_dup2_action(struct process *child,
    struct process *parent,
    int64_t old_fd,
    int64_t new_fd) {
    if (child == NULL ||
        parent == NULL ||
        old_fd < 0 ||
        new_fd < 0 ||
        new_fd >= PROCESS_MAX_OPEN_FILES) {
        return false;
    }
    if (old_fd == new_fd && process_file_at(child, (uint64_t)new_fd) != NULL) {
        return true;
    }

    struct process_file parent_stdio;
    struct process_file *source = process_file_at(child, (uint64_t)old_fd);
    if (source == NULL) {
        source = process_fd_source(parent, old_fd, &parent_stdio, true);
    }
    if (source == NULL) {
        return false;
    }

    struct process_file source_copy = *source;
    uint64_t target_index = (uint64_t)new_fd;
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
            if (action->fd >= 0 && action->fd < PROCESS_MAX_OPEN_FILES &&
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



static bool process_configure_stdio_fds(struct process *child, struct process *parent,
    int64_t input, int64_t output, int64_t error) {
    if(!child) return false;
    if(!parent) parent=child;
    int64_t fds[3]={input,output,error};
    struct process_file snapshots[3]={{0}};
    bool change[3]={false,false,false},ok=true;
    /* Snapshot all sources first: swapping stdout/stderr cannot make one
     * redirection observe the other's newly replaced slot. */
    uint64_t irq=process_irq_save();
    for(unsigned i=0;i<3;i++) {
        if(parent==child && (fds[i]==-1 || fds[i]==(int64_t)i)) continue;
        change[i]=true;
        if(fds[i]==PROCESS_STDIO_CLOSED_FD) continue;
        int64_t source_fd=fds[i]<0?(int64_t)i:fds[i];
        struct process_file *source=process_file_at(parent,(uint64_t)source_fd);
        if(!source) { if(fds[i]<0) continue;ok=false;break; }
        if(!process_file_clone_entry(&snapshots[i],source)) { ok=false;break; }
        snapshots[i].flags&=~SRV_FD_CLOEXEC;
    }
    process_irq_restore(irq);
    if(!ok) { for(unsigned i=0;i<3;i++) if(snapshots[i].used) process_file_release_entry(&snapshots[i]); pipe_rights_collect(); return false; }
    for(unsigned i=0;i<3;i++) if(change[i]) {
        if(process_file_at(child,i)) (void)process_file_close(child,i);
        child->files[i]=snapshots[i];
    }
    struct process_file *terminal=process_file_at(child,0);
    if(terminal && terminal->type==PROCESS_FILE_PTY_SLAVE) child->controlling_tty=terminal->handle;
    return true;
}

static int64_t process_spawn_background_elf_args_quiet_flag(const char *path, const char *args, bool quiet, uint64_t lifetime) {
    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    struct process *process = alloc_process_lifetime(path, true, lifetime);
    if (process == NULL) {
        return -1;
    }

    if (!allocate_kernel_stack(process)) {
        release_process(process);
        return -1;
    }

    if (!load_elf_image(path, process, USER_STACK_TOP, &process->entry) ||
        !setup_process_arguments(process, path, args != NULL ? args : "") ||
        process_prepare_image(process) < 0) {
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

int64_t process_lifetime_spawn(uint64_t token,const char *path,const char *args) {
    if (!token || process_current()) return -1;
    return process_spawn_background_elf_args_quiet_flag(path,args,true,token);
}

int64_t process_spawn_background_elf_args(const char *path, const char *args) {
    return process_spawn_background_elf_args_quiet_flag(path, args, false, 0);
}

int64_t process_spawn_background_elf_args_quiet(const char *path, const char *args) {
    return process_spawn_background_elf_args_quiet_flag(path, args, true, 0);
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
        process_prepare_image(process) < 0 ||
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

struct process_wait_request { uint64_t pid,owner,group; };
bool process_set_reaper(uint64_t pid) {
    if(!pid) return false;
    for(unsigned i=0;i<PROCESS_MAX_PROCESSES;i++) if(processes[i].allocated && processes[i].active && processes[i].pid==pid) {
        system_reaper_pid=pid;return true;
    }
    return false;
}
static bool process_wait_owned(const struct process *candidate,uint64_t owner) {
    if(candidate->parent_pid==owner) return true;
    if(!owner || owner!=system_reaper_pid || candidate->pid==owner || !candidate->parent_pid) return false;
    /* Parent zero denotes a kernel-owned task, such as a hardware autotest.
     * Init must not consume its result. Ordinary orphaned userspace children
     * remain eligible for init without touching a still-running worker. */
    for(unsigned i=0;i<PROCESS_MAX_PROCESSES;i++) if(processes[i].allocated &&
        processes[i].pid==candidate->parent_pid && processes[i].active) return false;
    return true;
}
int64_t process_scope_reap(uint64_t token) {
    uint64_t flags=process_irq_save();struct process_lifetime *life=user_scope_owned(token);
    struct process *child=NULL;
    if(!life) {process_irq_restore(flags);return SRV_ERR_INVAL;}
    if(!life->revoked) {process_irq_restore(flags);return SRV_ERR_BUSY;}
    for(unsigned i=0;i<PROCESS_MAX_PROCESSES;i++) {
        struct process *candidate=&processes[i];
        if(!candidate->allocated || candidate->lifetime!=token || !candidate->detached ||
           !candidate->reapable || candidate->active || !candidate->parent_pid ||
           candidate->address_space || candidate->mapping_count || candidate->mappings || candidate->kernel_stack ||
           candidate->kernel_stack_physical || candidate->kernel_stack_frames ||
           process_slots_busy[i] || !scheduler_io_process_retired(candidate)) continue;
        bool resources=false;
        for(unsigned j=0;j<PROCESS_MAX_OPEN_FILES;j++) if(candidate->files[j].used) {resources=true;break;}
        for(unsigned j=0;j<PROCESS_MAX_USER_THREADS;j++) if(candidate->user_threads[j].used) {resources=true;break;}
        if(resources) continue;
        bool live_parent=false;
        for(unsigned j=0;j<PROCESS_MAX_PROCESSES;j++) if(processes[j].allocated &&
            processes[j].active && processes[j].pid==candidate->parent_pid) {live_parent=true;break;}
        if(!live_parent) {child=candidate;child->reapable=false;break;}
    }
    process_irq_restore(flags);
    if(!child) return 0;
    process_record_exit_status(child->pid,child->exit_status);
    process_account_child(process_current(),child);
    release_process(child);
    return 1;
}

static struct process *find_reapable_process(const struct process_wait_request *request) {
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        struct process *process=&processes[i];
        if(process->allocated && process->detached && process->reapable &&
           (!request->pid || process->pid==request->pid) &&
           (!request->group || process->process_group==request->group) && process_wait_owned(process,request->owner)) return process;
    }
    return NULL;
}
static bool has_wait_target(const struct process_wait_request *request) {
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        struct process *process=&processes[i];
        if(process->allocated && process->detached && (!request->pid || process->pid==request->pid) &&
           (!request->group || process->process_group==request->group) &&
           process_wait_owned(process,request->owner)) return true;
    }
    return false;
}
static bool process_try_wait_retired_impl(uint64_t pid,uint64_t *status,int64_t *result,
    process_wait_copy_fn copy,void *context) {
    if(!pid || (!status && !copy) || !result)return false;
    uint64_t flags=process_irq_save();struct process *parent=process_current(),*child=NULL;
    if(!parent || !parent->allocated || !parent->active)goto refused;
    for(unsigned i=0;i<PROCESS_MAX_PROCESSES;i++)if(processes[i].allocated && processes[i].pid==pid) {child=&processes[i];break;}
    if(!child || child==parent || !child->detached || !child->reapable || child->active ||
       child->parent_pid!=parent->pid || child->address_space || child->mapping_count || child->mappings ||
       child->kernel_stack || child->kernel_stack_physical || child->kernel_stack_frames ||
       process_slots_busy[child-processes] || !scheduler_io_process_retired(child))goto refused;
    for(unsigned i=0;i<PROCESS_MAX_OPEN_FILES;i++)if(child->files[i].used)goto refused;
    for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++)if(child->user_threads[i].used)goto refused;
    struct process_lifetime *life=child->lifetime?lifetime_find(child->lifetime):NULL;
    if(child->lifetime && (!life || !life->members))goto refused;
    if(copy && !copy(context,child->exit_status)){
        *result=SRV_ERR_FAULT;process_irq_restore(flags);return true;
    }
    if(status)*status=child->exit_status;
    *result=(int64_t)child->pid;
    process_record_exit_status_locked(child->pid,child->exit_status);
    process_account_child(parent,child);
    /* Runtime cleanup and stack release precede reapable publication. Leave
     * the empty record intact: alloc_process_lifetime clears its reserved slot
     * at the ordinary gate before reuse. Nothing is freed or yielded here. */
    child->image_generation=0;child->reapable=false;child->allocated=false;
    user_scope_member_done(life,child->exit_status,true);
    struct process_wait_request any={.owner=parent->pid};
    if(!find_reapable_process(&any))process_clear_current_signal(SRV_SIGNAL_CHLD);
    scheduler_wake_all(&process_wait_queue);
    process_irq_restore(flags);return true;
refused:
    process_irq_restore(flags);return false;
}
bool process_try_wait_retired(uint64_t pid,uint64_t *status,int64_t *result) {
    return process_try_wait_retired_impl(pid,status,result,NULL,NULL);
}
bool process_try_wait_retired_checked(uint64_t pid,process_wait_copy_fn copy,void *context,int64_t *result) {
    return copy && process_try_wait_retired_impl(pid,NULL,result,copy,context);
}
static bool process_wait_ready(void *arg) {
    const struct process_wait_request *request=arg;
    return process_signal_pending_current() || find_reapable_process(request)!=NULL || !has_wait_target(request);
}
struct process_event_request {
    struct process_wait_request target;
    bool (*condition)(void *);
    void *arg;
    uint64_t deadline;
};
static bool process_event_ready(void *arg) {
    struct process_event_request *request=arg;
    return process_wait_ready(&request->target) || request->condition(request->arg) ||
        (request->deadline && timer_ticks()>=request->deadline);
}
bool process_wait_event(uint64_t pid,bool (*condition)(void *),void *arg,uint64_t deadline_ticks) {
    if(!pid || !condition)return false;
    struct process_event_request request={
        .target={.pid=pid,.owner=process_pid(process_current())},
        .condition=condition,.arg=arg,.deadline=deadline_ticks};
    return scheduler_wait_timeout(&process_wait_queue,process_event_ready,&request,deadline_ticks);
}
void process_wait_notify(void) { scheduler_wake_all(&process_wait_queue); }
static int64_t process_wait_request(struct process_wait_request request,uint64_t *status_out,bool nohang,bool posix_status,
    process_wait_copy_fn copy,void *context) {
    for (;;) {
        uint64_t flags=process_irq_save();
        struct process *process = find_reapable_process(&request);
        uint64_t encoded=process?(posix_status?
            (process->killed && process->kill_signal ? (process->kill_signal&0x7f):((process->exit_status&0xff)<<8)):
            process->exit_status):0;
        if(process && copy && !copy(context,encoded)) {process_irq_restore(flags);return SRV_ERR_FAULT;}
        if(process && status_out)*status_out=encoded;
        if(process) process->reapable=false;
        process_irq_restore(flags);
        if (process != NULL) {
            uint64_t waited_pid = process->pid,status = process->exit_status;
            process_record_exit_status(waited_pid, status);
            process_account_child(process_current(),process);
            release_process(process);
            struct process_wait_request any={.owner=request.owner};
            if (find_reapable_process(&any) == NULL) process_clear_current_signal(SRV_SIGNAL_CHLD);
            return (int64_t)waited_pid;
        }
        if (!has_wait_target(&request)) return -1;
        if (process_signal_pending_current()) return SRV_ERR_INTR;
        if (nohang) return 0;
        if (!scheduler_wait(&process_wait_queue, process_wait_ready, &request)) scheduler_yield();
    }
}

int64_t process_wait(uint64_t pid,uint64_t *status_out,bool nohang) {
    struct process_wait_request request={.pid=pid,.owner=process_pid(process_current())};
    return process_wait_request(request,status_out,nohang,false,NULL,NULL);
}
int64_t process_wait_posix(int64_t selector,uint64_t *status_out,bool nohang) {
    struct process *current=process_current();
    if(!current || selector==INT64_MIN) return SRV_ERR_INVAL;
    struct process_wait_request request={.owner=current->pid};
    if(selector>0) request.pid=(uint64_t)selector;
    else if(!selector) request.group=current->process_group;
    else if(selector < -1) request.group=(uint64_t)-selector;
    return process_wait_request(request,status_out,nohang,true,NULL,NULL);
}
int64_t process_wait_checked(int64_t selector,uint64_t flags,process_wait_copy_fn copy,void *context) {
    struct process *current=process_current();
    if(!current || !copy || (flags&~(uint64_t)(SRV_WAIT_NOHANG|SRV_WAIT_POSIX)) ||
        ((flags&SRV_WAIT_POSIX) && selector==INT64_MIN))return SRV_ERR_INVAL;
    struct process_wait_request request={.owner=current->pid};
    if(flags&SRV_WAIT_POSIX) {
        if(selector>0)request.pid=(uint64_t)selector;
        else if(!selector)request.group=current->process_group;
        else if(selector < -1)request.group=(uint64_t)-selector;
    } else request.pid=(uint64_t)selector;
    return process_wait_request(request,NULL,(flags&SRV_WAIT_NOHANG)!=0,(flags&SRV_WAIT_POSIX)!=0,copy,context);
}

/* One nonblocking shutdown pass. Revocation prevents new reservations; refs
 * remain through loading, pending I/O, thread exit, zombie reaping and release.
 * A caller must keep its external resources until status.members is zero. */
bool process_lifetime_drain(uint64_t token) {
    if (process_current()) return false;
    uint64_t flags=process_irq_save();
    struct process_lifetime *life=lifetime_find(token);
    if (!life || !life->revoked) { process_irq_restore(flags);return false; }
    process_irq_restore(flags);
    for (unsigned i=0;i<PROCESS_MAX_PROCESSES;i++) {
        flags=process_irq_save();
        struct process *p=&processes[i];
        bool reap=p->allocated && p->lifetime==token && p->detached && p->reapable;
        if(reap) p->reapable=false;
        else if(p->allocated && p->lifetime==token && p->active) {
            /* Kernel cancellation is not a maskable user signal. */
            p->killed=true;p->kill_signal=SRV_SIGNAL_KILL;process_wake_for_signal(p);
        }
        process_irq_restore(flags);
        if(reap) release_process(p);
    }
    return true;
}

uint64_t process_list(uint64_t start_index,
    uint64_t *pid_out,
    const char **name_out,
    const char **state_out) {
    return process_list_details(start_index,pid_out,name_out,state_out,NULL,NULL);
}

uint64_t process_list_details(uint64_t start_index, uint64_t *pid_out,
    const char **name_out, const char **state_out, uint64_t *parent_out, uint64_t *mapped_bytes_out) {
    bool by_pid=(start_index&SRV_PROC_LIST_BY_PID)!=0;
    uint64_t target=start_index&~SRV_PROC_LIST_BY_PID;
    if(by_pid && !target)return 0;
    for (uint64_t i = by_pid?0:start_index; i < PROCESS_MAX_PROCESSES; i++) {
        if (processes[i].allocated && (!by_pid || processes[i].pid==target)) {
            if (pid_out != NULL) {
                *pid_out = processes[i].pid;
            }
            if (name_out != NULL) {
                *name_out = processes[i].name;
            }
            if (state_out != NULL) {
                *state_out = process_state(&processes[i]);
            }
            if(parent_out) *parent_out=processes[i].parent_pid;
            if(mapped_bytes_out) *mapped_bytes_out=processes[i].mapping_count*PMM_FRAME_SIZE;
            return i + 1;
        }
    }
    return 0;
}

bool process_kill_pid(uint64_t pid) {
    return process_signal_pid(pid, SRV_SIGNAL_TERM);
}

bool process_signal_pid(uint64_t pid, uint64_t signal) {
    if (!process_signal_is_supported(signal)) {
        return false;
    }
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        if (processes[i].allocated && processes[i].pid == pid) {
            if (signal == 0) {
                return processes[i].active || processes[i].reapable;
            }
            if (!processes[i].active) {
                return false;
            }
            return process_queue_signal(&processes[i], signal);
        }
    }
    return false;
}

bool process_signal_target(int64_t pid, uint64_t signal) {
    if (pid==INT64_MIN || !process_signal_is_supported(signal)) {
        return false;
    }
    if (pid > 0) {
        return process_signal_pid((uint64_t)pid, signal);
    }

    uint64_t target_group = 0;
    if (pid == 0) {
        target_group = process_group(0);
        if (target_group == 0) {
            return false;
        }
    } else if (pid < -1) {
        target_group = (uint64_t)(-pid);
    }

    bool matched = false;
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        struct process *candidate = &processes[i];
        if (!candidate->allocated) {
            continue;
        }
        if (pid == -1) {
            if (!candidate->active && !candidate->reapable) {
                continue;
            }
        } else if (candidate->process_group != target_group) {
            continue;
        }
        if (signal == 0) {
            matched = matched || candidate->active || candidate->reapable;
            continue;
        }
        if (candidate->active) {
            matched = process_signal_pid(candidate->pid, signal) || matched;
        }
    }
    return matched;
}

/* A disposition is process-wide, including peer-local pending queues. Choose
 * an eligible local bit before a shared duplicate, and the lowest public TID
 * among eligible local recipients. Caller holds CPU0 IRQ exclusion throughout
 * preparation and commit; no scheduler/pointer observation follows publication. */
static struct signal_default_decision process_prepare_signal_default_view(
    struct process *process,uint64_t signal,const struct signal_default_view *view) {
    struct signal_default_decision decision={0};
    if(!process->allocated || !process->active || process->killed || process->exiting ||
       signal==SRV_SIGNAL_CHLD || signal==SRV_SIGNAL_WINCH) return decision;
    uint64_t bit=1ull<<signal,lowest=UINT64_MAX;
    bool shared_eligible=false;
    uint64_t main_mask=view && !view->worker?view->mask:process->main_signal_mask;
    uint64_t main_pending=view && !view->worker?view->local_pending:process->main_pending_signals;
    if(!process->main_thread_exited && !(main_mask&bit)) {
        shared_eligible=true;
        if(main_pending&bit) {
            decision.pending=&process->main_pending_signals;lowest=1;
        }
    }
    for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++) {
        struct process_user_thread *thread=&process->user_threads[i];
        uint64_t mask=view && view->worker==thread?view->mask:thread->signal_mask;
        uint64_t pending=view && view->worker==thread?view->local_pending:thread->pending_signals;
        if(!thread->used || !thread->active || thread->owner!=process ||
           thread->tid<2 || thread->tid>(uint64_t)INT64_MAX || (mask&bit)) continue;
        shared_eligible=true;
        if((pending&bit) && thread->tid<lowest) {
            decision.pending=&thread->pending_signals;lowest=thread->tid;
        }
    }
    uint64_t shared_pending=view?view->shared_pending:process->pending_signals;
    if(!decision.pending && shared_eligible && (shared_pending&bit))
        decision.pending=&process->pending_signals;
    if(decision.pending) {decision.bit=bit;decision.signal=signal;}
    return decision;
}
static struct signal_default_decision process_prepare_signal_default(
    struct process *process,uint64_t signal) {
    return process_prepare_signal_default_view(process,signal,NULL);
}
static void process_commit_signal_default(struct process *process,
    struct signal_default_decision decision) {
    if(decision.pending) {
        *decision.pending &= ~decision.bit;
        process->killed=true;process->kill_signal=decision.signal;
    }
}

/* No fallible work follows descriptor/mirror publication. previous is a
 * caller-owned kernel output, not a user copy or pointer into process state. */
static bool process_usr1_waits_permit_action(const struct process *process,
    const struct signal_action_descriptor *next) {
    for(unsigned t=0;t<=PROCESS_MAX_USER_THREADS;t++) {
        const struct signal_thread_state *s=t?&process->user_threads[t-1].signal_state:&process->main_signal_state;
        for(unsigned i=0;i<4;i++) {
            const struct signal_wait_slot *w=&s->waits[i];
            uint64_t phase=w->phase_flags&255;
            if((w->phase_flags&SIGNAL_WAIT_INFO_ONLY) && phase!=SIGNAL_WAIT_HANDLER &&
                phase!=SIGNAL_WAIT_INFO_DONE &&
                (next->mode!=SIGNAL_ACTION_INFO || (next->flags&SIGNAL_ACTION_RESET)))return false;
            if(next->mode==SIGNAL_ACTION_INFO && (s->io[i].flags&SIGNAL_IO_LEGACY_MASKED))return false;
        }
    }
    return true;
}
static bool process_usr1_commit_action(struct process *process,
    const struct signal_action_descriptor *replacement,
    struct signal_action_descriptor *previous,uint64_t *generation_out) {
    if(process->usr1_action_generation==UINT64_MAX || !process_usr1_waits_permit_action(process,replacement)) return false;
    struct signal_action_descriptor next=*replacement,old=process->usr1_action;
    struct signal_default_decision decision={0};
    if(next.mode==SIGNAL_ACTION_DEFAULT)
        decision=process_prepare_signal_default(process,SRV_SIGNAL_USR1);
    uint64_t bit=1ull<<SRV_SIGNAL_USR1;
    process->usr1_action=next;
    process->usr1_action_generation++;
    process->signal_catch_mask &= ~bit;
    process->signal_ignore_mask &= ~bit;
    if(next.mode==SIGNAL_ACTION_LEGACY || next.mode==SIGNAL_ACTION_INFO)
        process->signal_catch_mask |= bit;
    else if(next.mode==SIGNAL_ACTION_IGNORE) {
        process->signal_ignore_mask |= bit;
        process_discard_signals(process,bit);
    }
    process_commit_signal_default(process,decision);
    if(previous) *previous=old;
    if(generation_out) *generation_out=process->usr1_action_generation;
    return true;
}

bool process_signal_action_exchange(const struct signal_action_descriptor *replacement,
    struct signal_action_descriptor *previous,uint64_t *generation_out) {
    /* Permit the normal replacement/old-action alias without reading it again
     * after commit. All other storage ownership is a private caller contract. */
    struct signal_action_descriptor next;
    if(replacement) next=*replacement;
    if((!replacement && !previous && !generation_out) ||
        (replacement && !signal_action_descriptor_valid(&next))) return false;
    uint64_t irq=process_irq_save();struct process_signal_owner owner;
    bool result=false;
    if(!process_signal_current_owner(&owner)) goto done;
    if(!replacement) {
        if(previous) *previous=owner.process->usr1_action;
        if(generation_out) *generation_out=owner.process->usr1_action_generation;
        result=true;goto done;
    }
    if((next.handler && !vmm_user_exec_current(next.handler,owner.root)) ||
        (next.trampoline && !vmm_user_exec_current(next.trampoline,owner.root))) goto done;
    result=process_usr1_commit_action(owner.process,&next,previous,generation_out);
done:
    process_irq_restore(irq);return result;
}

static bool process_signal_config_current_locked(uint64_t signal, uint64_t action) {
    struct process *process = process_current();
    if (process == NULL || signal == 0 || signal >= 64 ||
        ((process_supported_signal_mask() & (1ull << signal)) == 0) ||
        (action != SRV_SIGNAL_DEFAULT &&
            action != SRV_SIGNAL_CATCH &&
            action != SRV_SIGNAL_IGNORE)) {
        return false;
    }
    if(signal==SRV_SIGNAL_USR1) {
        struct signal_action_descriptor next={.mode=action==SRV_SIGNAL_CATCH ? SIGNAL_ACTION_LEGACY :
            action==SRV_SIGNAL_IGNORE ? SIGNAL_ACTION_IGNORE : SIGNAL_ACTION_DEFAULT};
        return process_usr1_commit_action(process,&next,NULL,NULL);
    }
    struct signal_default_decision decision={0};
    if(action==SRV_SIGNAL_DEFAULT) decision=process_prepare_signal_default(process,signal);
    uint64_t mask = 1ull << signal;
    process->signal_catch_mask &= ~mask;
    process->signal_ignore_mask &= ~mask;
    if (action == SRV_SIGNAL_CATCH) {
        process->signal_catch_mask |= mask;
        return true;
    }
    if (action == SRV_SIGNAL_IGNORE) {
        process->signal_ignore_mask |= mask;
        process_discard_signals(process,mask);
        return true;
    }
    if (signal == SRV_SIGNAL_CHLD || signal == SRV_SIGNAL_WINCH)
        process_discard_signals(process,mask);
    process_commit_signal_default(process,decision);
    return true;
}
int64_t process_signal_action_current(uint64_t signal) {
    struct process *process=process_current();
    if(!process || !signal || signal>=64) return SRV_ERR_INVAL;
    uint64_t mask=1ull<<signal;
    return (process->signal_ignore_mask&mask)?SRV_SIGNAL_IGNORE:
        (process->signal_catch_mask&mask)?SRV_SIGNAL_CATCH:SRV_SIGNAL_DEFAULT;
}
bool process_signal_config_current(uint64_t signal, uint64_t action) {
    uint64_t irq=process_irq_save();
    bool result=process_signal_config_current_locked(signal,action);
    process_irq_restore(irq);return result;
}

bool process_signal_pending_current(void) {
    struct process *process = process_current();
    return process != NULL && (process_visible_pending(process) & ~process_current_signal_mask(process)) != 0;
}

uint64_t process_signal_pending_mask_current(void) {
    struct process *process = process_current();
    return process != NULL ? process_visible_pending(process) : 0;
}

static uint64_t process_signal_poll_current_locked(void) {
    struct process *process = process_current();
    if (process == NULL) {
        return 0;
    }
    uint64_t pending = process_visible_pending(process);
    if(process->usr1_action.mode==SIGNAL_ACTION_INFO) pending &= ~(1ull<<SRV_SIGNAL_USR1);
    if (pending == 0) {
        return 0;
    }
    for (uint64_t signal = 1; signal < 64; signal++) {
        uint64_t mask = 1ull << signal;
        if ((pending & mask) != 0) {
            process_consume_signal_bit(process,mask);
            return signal;
        }
    }
    return 0;
}
uint64_t process_signal_poll_current(void) {
    uint64_t irq=process_irq_save();
    uint64_t result=process_signal_poll_current_locked();
    process_irq_restore(irq);return result;
}

static bool process_signal_mask_current_locked(uint64_t how, uint64_t set, uint64_t *oldset_out) {
    struct process *process = process_current();
    if (process == NULL) {
        return false;
    }

    uint64_t current = process_current_signal_mask(process);
    if (oldset_out != NULL) {
        *oldset_out = current;
    }

    uint64_t next = current;
    if (how == SRV_SIGNAL_BLOCK) {
        next |= set;
    } else if (how == SRV_SIGNAL_UNBLOCK) {
        next &= ~set;
    } else if (how == SRV_SIGNAL_SETMASK) {
        next = set;
    } else {
        return false;
    }
    next &= ~((1ull << 9) | (1ull << 19)); /* SIGKILL/SIGSTOP cannot be blocked. */
    process_set_current_signal_mask(process, next);

    uint64_t unblocked = process_visible_pending(process) & ~next &
        ~(process->signal_catch_mask | process->signal_ignore_mask);
    /* Default-ignored signals can be retained while blocked for sigwait,
     * but must not keep interrupting I/O after unmasking without a handler. */
    uint64_t ignored_defaults=(1ull << SRV_SIGNAL_CHLD) | (1ull << SRV_SIGNAL_WINCH);
    process->pending_signals &= ~(unblocked & ignored_defaults);
    *process_local_pending(process) &= ~(unblocked & ignored_defaults);
    unblocked &= ~ignored_defaults;
    if (unblocked != 0) {
        for (uint64_t signal = 1; signal < 64; signal++) {
            uint64_t mask = 1ull << signal;
            if ((unblocked & mask) != 0) {
                process_consume_signal_bit(process,mask);
                process->killed = true;
                process->kill_signal = signal;
                break;
            }
        }
    }
    return true;
}
bool process_signal_mask_current(uint64_t how, uint64_t set, uint64_t *oldset_out) {
    uint64_t irq=process_irq_save();
    bool result=process_signal_mask_current_locked(how,set,oldset_out);
    process_irq_restore(irq);return result;
}

static uint64_t process_signal_consume_current_locked(uint64_t mask) {
    struct process *process = process_current();
    if (process == NULL || mask == 0) {
        return 0;
    }
    uint64_t pending = process_visible_pending(process) & mask;
    if(process->usr1_action.mode==SIGNAL_ACTION_INFO) pending &= ~(1ull<<SRV_SIGNAL_USR1);
    for (uint64_t signal = 1; signal < 64; signal++) {
        uint64_t signal_mask = 1ull << signal;
        if ((pending & signal_mask) != 0) {
            process_consume_signal_bit(process,signal_mask);
            return signal;
        }
    }
    return 0;
}
uint64_t process_signal_consume_current(uint64_t mask) {
    uint64_t irq=process_irq_save();
    uint64_t result=process_signal_consume_current_locked(mask);
    process_irq_restore(irq);return result;
}

bool process_set_group(uint64_t pid, uint64_t group) {
    if (pid == 0) {
        struct process *current = process_current();
        if (current == NULL) {
            return false;
        }
        pid = current->pid;
    }
    if (group == 0) {
        group = pid;
    }
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        if (processes[i].allocated && processes[i].pid == pid) {
            processes[i].process_group = group;
            return true;
        }
    }
    return false;
}

uint64_t process_group(uint64_t pid) {
    if (pid == 0) {
        struct process *current = process_current();
        return current != NULL ? current->process_group : 0;
    }
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        if (processes[i].allocated && processes[i].pid == pid) {
            return processes[i].process_group;
        }
    }
    return 0;
}

uint64_t process_session(uint64_t pid) {
    if (pid == 0) {
        struct process *current = process_current();
        return current != NULL ? current->session_id : 0;
    }
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        if (processes[i].allocated && processes[i].pid == pid) {
            return processes[i].session_id;
        }
    }
    return 0;
}

bool process_create_session_current(uint64_t *session_out) {
    struct process *current = process_current();
    if (current == NULL || current->process_group == current->pid) {
        return false;
    }
    current->session_id = current->pid;
    current->process_group = current->pid;
    current->controlling_tty = UINT64_MAX; /* Detached; never fall back to the console. */
    if (session_out != NULL) {
        *session_out = current->session_id;
    }
    return true;
}

void process_set_foreground_group(uint64_t group) {
    struct process *current=process_current();
    if(current && current->controlling_tty) {
        struct pty_model *terminal=pty_state(current->controlling_tty);
        if(terminal) terminal->foreground_group=group;
    } else foreground_process_group = group;
}

uint64_t process_foreground_group(void) {
    struct process *current=process_current();
    if(current && current->controlling_tty) {
        struct pty_model *terminal=pty_state(current->controlling_tty);
        return terminal?terminal->foreground_group:0;
    }
    return foreground_process_group;
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

/* Resolve every traversed component before folding dot/dot-dot. A regular
 * file followed by /.. must fail, not escape into its parent. The final name
 * may be absent so create operations can use the same checked resolver. */
static int64_t process_path_context_for(struct process *process,uint64_t operation,const char *path,char *out,uint64_t capacity) {
    if(!process) return SRV_ERR_INVAL;
    char resolved[PROCESS_PATH_MAX];
    if(operation==SRV_PATH_GETCWD) {
        if(!copy_process_path(resolved,sizeof(resolved),process->cwd)) return SRV_ERR_NOENT;
    } else if(operation==SRV_PATH_CHDIR || operation==SRV_PATH_RESOLVE || operation==SRV_PATH_RESOLVE_DIRECTORY) {
        if(!path) return SRV_ERR_FAULT;
        if(!path[0]) return SRV_ERR_NOENT;
        if(!copy_process_path(resolved,sizeof(resolved),path[0]=='/'?"/":process->cwd)) return SRV_ERR_NAMETOOLONG;
        uint64_t length=0;while(resolved[length]) length++;
        uint64_t at=0;
        while(path[at]) {
            while(path[at]=='/') at++;
            if(!path[at]) break;
            uint64_t start=at;while(path[at] && path[at]!='/') at++;
            uint64_t count=at-start;
            if(count==1 && path[start]=='.') continue;
            if(count==2 && path[start]=='.' && path[start+1]=='.') {
                while(length>1 && resolved[length-1]!='/') length--;
                if(length>1) length--;
                resolved[length]=0;continue;
            }
            if(count>SRV_NAME_MAX) return SRV_ERR_NAMETOOLONG;
            uint64_t slash=length>1?1:0;
            if(length+slash+count>=sizeof(resolved)) return SRV_ERR_NAMETOOLONG;
            if(slash) resolved[length++]='/';
            for(uint64_t i=0;i<count;i++) resolved[length++]=path[start+i];
            resolved[length]=0;
            if(path[at]=='/') {
                char storage[PROCESS_PATH_MAX];
                int64_t error=vfs_namespace_system_path(resolved,storage,sizeof(storage));
                if(error<0) return error;
                const struct vfs_node *node=vfs_lookup(storage);
                uint64_t next=at;while(path[next]=='/') next++;
                if(!node && operation==SRV_PATH_RESOLVE_DIRECTORY && !path[next]) continue;
                if(!node) return SRV_ERR_NOENT;
                if(node->type!=VFS_NODE_DIRECTORY) return SRV_ERR_NOTDIR;
            }
        }
        if(operation==SRV_PATH_CHDIR) {
            char storage[PROCESS_PATH_MAX];
            int64_t error=vfs_namespace_system_path(resolved,storage,sizeof(storage));
            if(error<0) return error;
            const struct vfs_node *node=vfs_lookup(storage);
            if(!node) return SRV_ERR_NOENT;
            if(node->type!=VFS_NODE_DIRECTORY) return SRV_ERR_NOTDIR;
            copy_process_path(process->cwd,sizeof(process->cwd),resolved);
            copy_process_path(process->cwd_storage,sizeof(process->cwd_storage),storage);
            return 0;
        }
    } else return SRV_ERR_INVAL;
    uint64_t size=0;while(resolved[size]) size++;size++;
    if(!out) return SRV_ERR_FAULT;
    if(capacity<size) return SRV_ERR_RANGE;
    for(uint64_t i=0;i<size;i++) out[i]=resolved[i];
    return 0;
}

int64_t process_path_context(uint64_t operation,const char *path,char *out,uint64_t capacity) {
    return process_path_context_for(process_current(),operation,path,out,capacity);
}

int64_t process_resolve_storage_path(const char *input,char *out,uint64_t capacity) {
    char logical[PROCESS_PATH_MAX];
    int64_t result=process_path_context_for(process_current(),SRV_PATH_RESOLVE,input,logical,sizeof(logical));
    if(result<0) return result;
    return vfs_namespace_system_path(logical,out,capacity);
}

bool process_has_open_vfs_prefix(const char *prefix) {
    if(process_stream_busy(prefix,true))return true;
    /* Keep the current pathname stable until directory identities can survive
     * rename/removal. This is an explicit busy boundary, not stale PWD state. */
    for(unsigned i=0;i<PROCESS_MAX_PROCESSES;i++)
        if(processes[i].allocated && path_under_prefix(processes[i].cwd_storage,prefix)) return true;
    for(unsigned i=0;i<PROCESS_MAX_WRITE_FILES+PROCESS_MAX_READ_FILES;i++)
        if(file_contents[i].refs && !file_contents[i].unlinked && path_under_prefix(file_contents[i].path,prefix)) return true;
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
    return thread == NULL ||
        !thread->used ||
        !thread->active ||
        process_should_exit_current() ||
        process_signal_pending_current();
}

static void process_user_thread_start(void *arg) {
    struct process_user_thread *thread = arg;
    if (thread == NULL || thread->owner == NULL || !thread->used) {
        return;
    }

    /* scheduler_spawn_user_thread published the complete process/TLS/FPU/CR3
     * context before READY. Do not rerun the general context setter here: its
     * kernel-map refresh can allocate while an unrelated I/O owner is paused. */
    if(process_current()!=thread->owner || process_current_user_thread()!=thread)return;
    usermode_enter(thread->entry, thread->stack_top, thread->arg, 0, 0, thread->tls_thread_pointer);
}

static int64_t process_thread_create_impl(uint64_t entry, uint64_t arg, uint64_t stack_top, uint64_t flags,bool service) {
    struct process *process = process_current();
    if (process == NULL || !process->active || process->killed || process->exiting ||
        !process->image_generation || entry == 0 || stack_top < PAGE_SIZE) {
        return -1;
    }
    if ((flags & ~SRV_THREAD_DETACHED) != 0) {
        return -1;
    }

    uint64_t irq = process_irq_save();
    /* Thread IDs are positive signed syscall results. Exhaustion is permanent
     * for this image; never publish a worker whose ID looks like an error. */
    if (process->next_thread_id < 2 || process->next_thread_id > (uint64_t)INT64_MAX) {
        process_irq_restore(irq);
        return SRV_ERR_AGAIN;
    }
    struct process_user_thread *thread = NULL;
    for (uint64_t i = 0; i < PROCESS_MAX_USER_THREADS; i++) {
        if (!process->user_threads[i].used) {
            thread = &process->user_threads[i];
            break;
        }
    }
    if (thread == NULL) {
        process_irq_restore(irq);
        return -1;
    }

    uint64_t tid = process->next_thread_id++;

    /* Reserve before allocation can yield to a second creator. */
    *thread = (struct process_user_thread){ .used = true, .owner = process };
    process_irq_restore(irq);

    uint64_t tls_base = 0;
    uint64_t tls_thread_pointer = 0;
    if (!process_setup_tls_block_mode(process, (unsigned)(thread-process->user_threads)+1, &tls_base, &tls_thread_pointer,service)) {
        thread->used = false;
        return -1;
    }

    *thread = (struct process_user_thread) {
        .used = true,
        .active = true,
        .detached = (flags & SRV_THREAD_DETACHED) != 0,
        .joined = false,
        .tid = tid,
        .entry = entry,
        .arg = arg,
        .stack_top = stack_top,
        .tls_base = tls_base,
        .tls_thread_pointer = tls_thread_pointer,
        .return_value = 0,
        .signal_mask = process_current_signal_mask(process),
        .owner = process,
    };
    fpu_init_state(&thread->fpu);

    if (!scheduler_spawn_user_thread("uthread", process_user_thread_start, thread,&thread->fpu,thread->tls_thread_pointer,process->image_generation)) {
        if(service)process_release_thread_tls(thread);
        else if(thread->tls_base) {
            uint64_t size=page_up(align_up_u64(process->tls_memsz,process->tls_align)+USER_TLS_TCB_SIZE);
            for(uint64_t page=0;page<size;page+=PAGE_SIZE)
                (void)process_unmap_tracked_page_checked(process,thread->tls_base+page);
        }
        thread->used = false;
        thread->active = false;
        return -1;
    }
    return (int64_t)tid;
}

int64_t process_thread_create(uint64_t entry,uint64_t arg,uint64_t stack_top,uint64_t flags) {
    return process_thread_create_impl(entry,arg,stack_top,flags,true);
}
/* First/reused sole-main worker only. All allocation/publication is IRQ-masked;
 * no filesystem access, service checkpoint or borrowed userspace buffer. */
bool process_try_thread_create_private(uint64_t entry,uint64_t arg,uint64_t stack_top,uint64_t flags,int64_t *result) {
    if(!result || !entry || stack_top<16 || (flags&~SRV_THREAD_DETACHED) ||
       ((stack_top-8)&~(PAGE_SIZE-1))!=((stack_top-1)&~(PAGE_SIZE-1)))return false;
    uint64_t irq=process_irq_save();struct process *p=process_current();
    if(!p || !p->allocated || !p->active || p->killed || p->exiting || p->mapping_count>4094 ||
       !scheduler_io_process_independent(p) || p->address_space!=vmm_current_address_space() ||
       p->tls_memsz>PROCESS_TLS_TEMPLATE_MAX || p->tls_filesz>p->tls_memsz ||
       !p->tls_align || p->tls_align>PAGE_SIZE || (p->tls_align&(p->tls_align-1)))goto refused;
    for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++)if(p->user_threads[i].used)goto refused;
    int64_t code=process_mapping_index(p,entry&~(PAGE_SIZE-1));
    int64_t stack=process_mapping_index(p,(stack_top-8)&~(PAGE_SIZE-1));
    if(code<0 || stack<0 || !(p->mappings[code].flags&VMM_PAGE_USER) ||
       (p->mappings[code].flags&VMM_PAGE_NO_EXECUTE) ||
       (p->mappings[stack].flags&(VMM_PAGE_USER|VMM_PAGE_WRITABLE))!=(VMM_PAGE_USER|VMM_PAGE_WRITABLE))goto refused;
    uint64_t physical=0;
    if(!vmm_virt_to_phys_in_space(p->address_space,entry&~(PAGE_SIZE-1),&physical) || physical!=p->mappings[code].physical_address ||
       !vmm_virt_to_phys_in_space(p->address_space,(stack_top-8)&~(PAGE_SIZE-1),&physical) || physical!=p->mappings[stack].physical_address)goto refused;
    *result=process_thread_create_impl(entry,arg,stack_top,flags,false);process_irq_restore(irq);return true;
refused:
    process_irq_restore(irq);return false;
}

static bool process_last_user_thread_done(void *arg) {
    struct process *process = arg;
    if (process->killed || process->exiting) return true;
    for (uint64_t i = 0; i < PROCESS_MAX_USER_THREADS; i++) {
        if (process->user_threads[i].used && process->user_threads[i].active)
            return false;
    }
    return true;
}

void process_thread_exit(uint64_t value) {
    struct process_user_thread *thread = process_current_user_thread();
    if (thread == NULL || thread->owner == NULL) {
        struct process *process = process_current();
        if (process == NULL || !process->active) process_exit(0);
        /* Keep the launch/teardown continuation and shared image alive. Only
         * the user thread retires here; exited joinable slots do not keep the
         * process alive. The initial return value is not a process status. */
        (void)process_irq_save();
        scheduler_invalidate_current_user_trap();
        process->main_signal_state=(struct signal_thread_state){0};
        process->main_thread_return_value = value;
        process->main_thread_exited = true;
        process->main_pending_signals = 0;
        process->main_signal_mask = UINT64_MAX;
        scheduler_wake_all(&process->thread_wait_queue);
        process_irq_restore(1ull << 9);
        while (!process_last_user_thread_done(process)) {
            if (!scheduler_wait(&process->thread_wait_queue,
                    process_last_user_thread_done, process)) scheduler_yield();
        }
        if (process_should_exit_current()) process_exit(process_current_kill_status());
        /* Only this path returns: libc must immediately call exit(0), running
         * atexit handlers before SYS_EXIT releases files, mappings and TLS. */
        return;
    }

    (void)process_irq_save();
    thread->signal_state=(struct signal_thread_state){0};
    thread->return_value = value;
    thread->active = false;
    scheduler_wake_all(&thread->owner->thread_wait_queue);
    scheduler_clear_user_context();
    process_release_thread_tls(thread);
    if (thread->detached) {
        thread->used = false;
    }
    process_irq_restore(1ull << 9);
    scheduler_exit_current();
}

static bool process_main_thread_done(void *arg) {
    struct process *process = arg;
    return process->main_thread_exited || process_should_exit_current() ||
        process_signal_pending_current();
}

static int64_t process_main_thread_join(struct process *process, uint64_t *value_out) {
    uint64_t irq = process_irq_save();
    if (!process || !process->active || !process_current_user_thread() ||
        process->main_thread_joined || process->main_thread_detached ||
        process->main_thread_join_claimed) {
        process_irq_restore(irq);
        return -1;
    }
    process->main_thread_join_claimed = true;
    process_irq_restore(irq);
    PROCESS_ARCH_STI();
    int64_t result = 0;
    while (!process->main_thread_exited) {
        if (process_should_exit_current()) { result = -1; break; }
        if (process_signal_pending_current()) { result = SRV_ERR_INTR; break; }
        if (!scheduler_wait(&process->thread_wait_queue, process_main_thread_done, process))
            scheduler_yield();
    }
    irq = process_irq_save();
    if (!result) {
        if (value_out) *value_out = process->main_thread_return_value;
        /* Retire the public handle only. The parked continuation, initial
         * stack and TLS remain owned by the process until its last exit. */
        process->main_thread_joined = true;
    }
    process->main_thread_join_claimed = false;
    process_irq_restore(irq);
    return result;
}

int64_t process_thread_join(uint64_t tid, uint64_t *value_out) {
    struct process *process = process_current();
    if (tid == 1) return process_main_thread_join(process, value_out);
    struct process_user_thread *current_thread = process_current_user_thread();
    struct process_user_thread *thread = process_find_user_thread(process, tid);
    if (thread == NULL ||
        thread->detached ||
        thread->joined ||
        (current_thread != NULL && current_thread == thread)) {
        return -1;
    }

    PROCESS_ARCH_STI();
    while (thread->active) {
        if (process_should_exit_current()) {
            return -1;
        }
        if (process_signal_pending_current()) {
            return SRV_ERR_INTR;
        }
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
    if (tid == 1) {
        uint64_t irq = process_irq_save();
        int64_t result = -1;
        if (process && process->active && !process->main_thread_joined &&
            !process->main_thread_detached && !process->main_thread_join_claimed) {
            process->main_thread_detached = true;
            result = 0;
        }
        process_irq_restore(irq);
        return result;
    }
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
    if (tid == 1 && process != NULL && process->active) {
        if (process->main_thread_joined ||
            (process->main_thread_detached && process->main_thread_exited)) return -1;
        return process->main_thread_exited ? 0 : 1;
    }
    struct process_user_thread *thread = process_find_user_thread(process, tid);
    if (thread == NULL) {
        return -1;
    }
    return thread->active ? 1 : 0;
}

int64_t process_thread_signal(uint64_t tid,uint64_t signal) {
    struct process *process=process_current();
    if(signal>=64 || (signal && signal!=9 &&
        !(process_supported_signal_mask() & (1ull<<signal)))) return SRV_ERR_INVAL;
    uint64_t irq=process_irq_save();
    struct process_user_thread *thread=process_find_user_thread(process,tid);
    int64_t result=SRV_ERR_SRCH;
    if(!process || !process->active || (tid!=1 && !thread)) goto done;
    if(tid==1 && (process->main_thread_joined ||
        (process->main_thread_detached && process->main_thread_exited))) goto done;
    result=0;
    /* A joinable exited thread still has a valid ID (POSIX.1-2024), but
     * cannot receive delivery. Never apply its signal to another thread. */
    if(!signal || (thread && !thread->active) || (tid==1 && process->main_thread_exited)) goto done;
    uint64_t bit=1ull<<signal;
    if(process->signal_ignore_mask & bit) goto done;
    uint64_t mask=thread?thread->signal_mask:process->main_signal_mask;
    if((process->signal_catch_mask | mask) & bit) {
        uint64_t *pending=thread?&thread->pending_signals:&process->main_pending_signals;
        *pending |= bit;
    } else if(signal!=SRV_SIGNAL_CHLD && signal!=SRV_SIGNAL_WINCH) {
        process->killed=true;process->kill_signal=signal;
    }
    process_wake_for_signal(process);
done:
    process_irq_restore(irq);return result;
}

uint64_t process_thread_self(void) {
    struct process_user_thread *thread = process_current_user_thread();
    return thread != NULL && thread->used ? thread->tid : 1;
}

static struct process_futex *process_futex_find(struct process *process, uint64_t address, bool create) {
    struct process_futex *free_entry = NULL;
    for (uint64_t i = 0; i < PROCESS_MAX_FUTEXES; i++) {
        struct process_futex *entry = &futexes[i];
        if (entry->used && !entry->invalidated && entry->process == process && entry->address == address) {
            return entry;
        }
        if ((!entry->used || (entry->active_waits == 0 && entry->wait_queue.waiters == 0)) && free_entry == NULL) {
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

/* Called with mapping mutation serialized against futex reads on CPU0. */
static void process_futex_invalidate_page(struct process *process, uint64_t page) {
    for (uint64_t i = 0; i < PROCESS_MAX_FUTEXES; i++) {
        struct process_futex *entry = &futexes[i];
        if (entry->used && entry->process == process &&
            (entry->address & ~(uint64_t)(PAGE_SIZE - 1)) == page) {
            entry->invalidated = true;
            scheduler_wake_all(&entry->wait_queue);
        }
    }
}

struct process_futex_wait {
    uint32_t *address;
    uint32_t expected;
    struct process_futex *futex;
};

static bool process_futex_read(struct process_futex_wait *wait, uint32_t *value) {
    uint64_t flags = process_irq_save();
    bool valid = (!wait->futex || !wait->futex->invalidated) &&
        vmm_user_range_mapped((uint64_t)(uintptr_t)wait->address, sizeof(*wait->address), false);
    /* Validation and load must be indivisible with respect to mapping changes. */
    if (valid) *value = __atomic_load_n(wait->address, __ATOMIC_ACQUIRE);
    process_irq_restore(flags);
    return valid;
}

static bool process_futex_wait_ready(void *arg) {
    struct process_futex_wait *wait = arg;
    uint32_t value;
    return process_should_exit_current() || process_signal_pending_current() ||
        !process_futex_read(wait, &value) || value != wait->expected;
}

int64_t process_futex_wait(uint32_t *address, uint32_t expected, uint64_t timeout_ticks) {
    struct process *process = process_current();
    if (process == NULL || address == NULL || (((uint64_t)(uintptr_t)address) & 3u) != 0) {
        return -1;
    }
    struct process_futex_wait wait = { .address = address, .expected = expected };
    uint64_t flags = process_irq_save();
    uint32_t value;
    if (!process_futex_read(&wait, &value)) {
        process_irq_restore(flags); return SRV_ERR_FAULT;
    }
    if (value != expected) {
        process_irq_restore(flags); return SRV_ERR_AGAIN;
    }
    uint64_t start_ticks = timer_ticks();
    if (timeout_ticks > UINT64_MAX - start_ticks) {
        process_irq_restore(flags); return SRV_ERR_INVAL;
    }
    uint64_t deadline_ticks = timeout_ticks ? start_ticks + timeout_ticks : 0;
    struct process_futex *futex = process_futex_find(process, (uint64_t)(uintptr_t)address, true);
    /* Reserve the validated binding before restoring interrupts. */
    if (futex != NULL) futex->active_waits++;
    wait.futex = futex;
    process_irq_restore(flags);
    if (futex == NULL) return SRV_ERR_AGAIN;

    int64_t result;
    PROCESS_ARCH_STI();
    for (;;) {
        if (process_should_exit_current()) {
            result = -1; break;
        }
        if (!process_futex_read(&wait, &value)) {
            result = SRV_ERR_FAULT; break;
        }
        if (value != expected) {
            result = 0; break;
        }
        if (process_signal_pending_current()) {
            result = SRV_ERR_INTR; break;
        }
        if (timeout_ticks != 0 && timer_ticks() - start_ticks >= timeout_ticks) {
            result = -110; break;
        }
        if (!scheduler_wait_timeout(&futex->wait_queue, process_futex_wait_ready, &wait, deadline_ticks)) {
            scheduler_yield();
        }
    }
    flags = process_irq_save();
    futex->active_waits--;
    process_irq_restore(flags);
    return result;
}

int64_t process_futex_wake(uint32_t *address, uint64_t max_count) {
    struct process *process = process_current();
    if (process == NULL || address == NULL || (((uint64_t)(uintptr_t)address) & 3u) != 0) {
        return -1;
    }

    uint64_t flags = process_irq_save();
    struct process_futex *futex = process_futex_find(process, (uint64_t)(uintptr_t)address, false);
    /* Keep lookup and wake in one IRQ-disabled operation: an empty entry can
     * otherwise be reassigned to another address before the wake executes. */
    int64_t result = futex == NULL ? 0 :
        (int64_t)scheduler_wake_count(&futex->wait_queue, max_count);
    process_irq_restore(flags);
    return result;
}

static void process_futex_wake_process(struct process *process) {
    if (process == NULL) {
        return;
    }
    uint64_t flags = process_irq_save();
    for (uint64_t i = 0; i < PROCESS_MAX_FUTEXES; i++) {
        if (futexes[i].used && futexes[i].process == process) {
            scheduler_wake_all(&futexes[i].wait_queue);
        }
    }
    process_irq_restore(flags);
}

static void process_futex_cleanup_process(struct process *process) {
    if (process == NULL) {
        return;
    }
    uint64_t flags = process_irq_save();
    for (uint64_t i = 0; i < PROCESS_MAX_FUTEXES; i++) {
        if (futexes[i].used && futexes[i].process == process) {
            scheduler_wake_all(&futexes[i].wait_queue);
            futexes[i] = (struct process_futex){0};
        }
    }
    process_irq_restore(flags);
}

void process_mark_ready(void) {
    struct process *process = process_current();
    if (process == NULL) return;
    process->startup_ready = true;
    scheduler_wake_all(&process_wait_queue);
}

static bool startup_finished(void *arg) {
    uint64_t pid = *(uint64_t *)arg;
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        if (processes[i].allocated && processes[i].pid == pid)
            return processes[i].startup_ready || !processes[i].active;
    }
    return true;
}

bool process_wait_startup(uint64_t pid, uint64_t timeout_ticks) {
    uint64_t deadline = timer_ticks() + timeout_ticks;
    while (!startup_finished(&pid) && timer_ticks() < deadline) {
        if (!scheduler_wait_timeout(&process_wait_queue, startup_finished, &pid, deadline))
            scheduler_yield();
    }
    for (uint64_t i = 0; i < PROCESS_MAX_PROCESSES; i++) {
        if (processes[i].allocated && processes[i].pid == pid) return processes[i].startup_ready;
    }
    return false;
}

uint64_t process_pid(const struct process *process) {
    return process != NULL ? process->pid : 0;
}

uint64_t process_parent_pid(const struct process *process) {
    return process != NULL ? process->parent_pid : 0;
}

const char *process_name(const struct process *process) {
    return process != NULL ? process->name : "";
}

const char *process_executable_path(const struct process *process) {
    return process != NULL ? process->executable_path : "";
}

bool process_current_quiet(void) {
    struct process *process = process_current();
    return process != NULL && process->quiet;
}

bool process_set_stdout_redirect(struct process *process, const char *path, bool append) {
    if(!process || !path || !path[0]) return false;
    uint64_t flags=SRV_OPEN_WRITE|SRV_OPEN_CREATE|(append?SRV_OPEN_APPEND:SRV_OPEN_TRUNC);
    int64_t fd=process_file_open_write(process,path,flags);
    if(fd<0) return false;
    int64_t result=process_file_dup2(process,(uint64_t)fd,1);
    if(fd!=1) (void)process_file_close(process,(uint64_t)fd);
    return result==1;
}

bool process_fd_is_console(struct process *process, uint64_t fd, bool input) {
    struct process_file *file=process_file_at(process,fd);
    return file && file->type==PROCESS_FILE_STDIO &&
        (input?file->handle==0:(file->handle==1 || file->handle==2));
}

int64_t process_stdin_read(struct process *process, uint8_t *buffer, uint64_t length) {
    struct process_file *file=process_file_at(process,0);
    if(!file) return SRV_ERR_BADF;
    if(file->type==PROCESS_FILE_STDIO) return file->handle==0?-2:SRV_ERR_BADF;
    if(file->type==PROCESS_FILE_PIPE_READ || file->type==PROCESS_FILE_PIPE_DUPLEX)
        return process_file_pipe_read(process,0,buffer,length);
    return process_file_read(process,0,buffer,length);
}

int64_t process_input_write(struct process *process, const uint8_t *buffer, uint64_t length) {
    struct process_file *file=process_file_at(process,0);
    if(!file) return SRV_ERR_BADF;
    if(file->type==PROCESS_FILE_STDIO) return file->handle==1 || file->handle==2?-2:SRV_ERR_BADF;
    if(file->type==PROCESS_FILE_PIPE_WRITE || file->type==PROCESS_FILE_PIPE_DUPLEX)
        return process_file_pipe_write(process,0,buffer,length);
    return process_file_write(process,0,buffer,length);
}

int64_t process_output_write(struct process *process, uint64_t fd, const uint8_t *buffer, uint64_t length) {
    struct process_file *file=process_file_at(process,fd);
    if(!file) return SRV_ERR_BADF;
    if(file->type==PROCESS_FILE_STDIO) return file->handle==1 || file->handle==2?-2:SRV_ERR_BADF;
    if(file->type==PROCESS_FILE_PIPE_WRITE || file->type==PROCESS_FILE_PIPE_DUPLEX)
        return process_file_pipe_write(process,fd,buffer,length);
    return process_file_write(process,fd,buffer,length);
}

struct process_file *process_file_at(struct process *process, uint64_t fd) {
    if (process == NULL || fd >= PROCESS_MAX_OPEN_FILES) {
        return NULL;
    }

    struct process_file *file = &process->files[fd];
    return file->used ? file : NULL;
}

uint64_t process_tty_handle(struct process *process,uint64_t fd) {
    struct process_file *file=process_file_at(process,fd);
    return file && (file->type==PROCESS_FILE_PTY_MASTER || file->type==PROCESS_FILE_PTY_SLAVE)?file->handle:0;
}
struct pty_model *process_tty_model(struct process *process,uint64_t fd) {
    return pty_state(process_tty_handle(process,fd));
}
bool process_tty_set_group(struct process *process,uint64_t fd,uint64_t group) {
    uint64_t handle=process_tty_handle(process,fd);struct pty_model *terminal=pty_state(handle);
    if(!terminal || !group) return false;
    for(unsigned i=0;i<PROCESS_MAX_PROCESSES;i++) if(processes[i].allocated &&
        processes[i].process_group==group && processes[i].controlling_tty==handle) {
        terminal->foreground_group=group;return true;
    }
    return false;
}
void process_tty_signal(uint64_t handle,uint64_t signal) {
    struct pty_model *terminal=pty_state(handle);
    if(!terminal || !terminal->foreground_group) return;
    for(unsigned i=0;i<PROCESS_MAX_PROCESSES;i++) if(processes[i].allocated &&
        processes[i].controlling_tty==handle && processes[i].process_group==terminal->foreground_group)
        (void)process_queue_signal(&processes[i],signal);
}
void process_tty_hangup(uint64_t handle) {
    for(unsigned i=0;i<PROCESS_MAX_PROCESSES;i++) if(processes[i].allocated && processes[i].controlling_tty==handle)
        (void)process_queue_signal(&processes[i],SRV_SIGNAL_HUP);
}
int64_t process_file_pty_pair(struct process *process,uint64_t fds_out[2]) {
    if(!process || !fds_out) return -1;
    unsigned found=0,index[2];
    for(unsigned i=0;i<PROCESS_MAX_OPEN_FILES && found<2;i++) if(!process->files[i].used) index[found++]=i;
    if(found!=2) return -1;
    uint64_t handle=pty_create();if(!handle) return -1;
    process->files[index[0]]=(struct process_file){.used=true,.type=PROCESS_FILE_PTY_MASTER,.handle=handle};
    process->files[index[1]]=(struct process_file){.used=true,.type=PROCESS_FILE_PTY_SLAVE,.handle=handle};
    fds_out[0]=index[0];fds_out[1]=index[1];return 0;
}

static void write_revoke_append(struct process_write_file *write) {
    write->append.valid=false;write->fresh_exclusive=false;
}
static void content_revoke_append(struct process_file_content *content) {
    if(!content)return;
    for(unsigned i=0;i<PROCESS_MAX_WRITE_FILES;i++)
        if(write_files[i].used && write_files[i].content==content)write_revoke_append(&write_files[i]);
}
static void content_mutated(struct process_file_content *content) {
    if(!content->dirty_generation)content->dirty_generation=1;
    else if(content->dirty_generation!=UINT64_MAX)content->dirty_generation++;
    if(content->dirty_generation==UINT64_MAX)content_revoke_append(content);
    content->dirty=true;
}
static struct process_file_content *content_find(const char *path) {
    for(unsigned i=0;i<PROCESS_MAX_WRITE_FILES+PROCESS_MAX_READ_FILES;i++)
        if(file_contents[i].refs && !file_contents[i].unlinked && process_path_equal(file_contents[i].path,path)) return &file_contents[i];
    return NULL;
}
void process_revoke_reserved_append_path(const char *path) {
    if(!path)return;
    uint64_t irq=process_irq_save();content_revoke_append(content_find(path));process_irq_restore(irq);
}
bool process_open_file_size(const char *path,uint64_t *size) {
    struct process_file_content *content=content_find(path);
    if(!content) return false;
    *size=content->size;return true;
}
/* Takes the retained VFS snapshot even when a content object already exists. */
static struct process_file_content *content_acquire(const struct vfs_node *node,
    const char *path,const uint8_t *data,uint64_t size) {
    struct process_file_content *content=content_find(path);
    if(content) {
        if(content->writeback.active) {
            if(data)vfs_release_data(node,data);
            return NULL;
        }
        content_revoke_append(content);
        content->refs++;
        if(data) vfs_release_data(node,data);
        return content;
    }
    for(unsigned i=0;i<PROCESS_MAX_WRITE_FILES+PROCESS_MAX_READ_FILES;i++) if(!file_contents[i].refs) {
        content=&file_contents[i];
        *content=(struct process_file_content){.refs=1,.node=node,.data=data,.size=size,
            .dirty_generation=1,.committed_size=size};
        if(copy_process_path(content->path,sizeof(content->path),path)) {
            content->diagnostic_identity=sync_timing_content_identity();return content;
        }
        content->refs=0;break;
    }
    if(data) vfs_release_data(node,data);
    return NULL;
}
static int64_t content_flush_observed(struct process_file_content *content,uint64_t index,bool observe) {
    uint64_t irq=process_irq_save();
    if(content->writeback.active) {process_irq_restore(irq);return SRV_ERR_BUSY;}
    if(content->unlinked || content->failed || !content->dirty) {
        bool timed=observe && sync_timing_content_begin(index,content->diagnostic_identity,
            content->unlinked?content->orphan_metadata.inode:content->node?content->node->metadata.inode:0,content->size);
        int64_t result=content->failed && !content->unlinked?SRV_ERR_IO:0;
        if(content->unlinked)content->dirty=false;
        if(timed)sync_timing_content_end(result>=0,content->dirty);
        process_irq_restore(irq);return result;
    }
    if(!content->dirty_generation)content->dirty_generation=1;
    const uint8_t *data=content->data;
    uint64_t size=content->size,generation=content->dirty_generation;
    content->writeback=(__typeof__(content->writeback)){.active=true,.data=data,.size=size,.generation=generation};
    bool timed=observe && sync_timing_content_begin(index,content->diagnostic_identity,
        content->node?content->node->metadata.inode:0,size);
    /* Preserve incoming IF0 through the backend's ownership release and this
     * borrow's retirement. Backend service windows already enable IRQs while
     * waiting, then return CLI. Restoring an IF1 kernel caller here would let
     * a queued last close run after fs_leave but before active is cleared. */
    bool retryable=false;
    bool ok=exfat_write_file_retryable(content->path,data,size,&retryable);
    (void)process_irq_save();
    if(!ok) {
        /* A clean refusal retains dirty data for a later explicit flush/close.
         * Partial writes and failed cleanup keep the sticky error contract. */
        content->failed=!retryable;
    } else {
        content->committed_size=size;
        content->dirty=content->dirty_generation!=generation;
    }
    struct sync_content_prefix prefix={.valid=1,.start_generation=generation,
        .end_generation=content->dirty_generation,.live_size=content->size,
        .committed_size=content->committed_size,.append_calls=content->writeback.append_calls,
        .append_bytes=content->writeback.append_bytes};
    content->writeback.active=false;
    if(timed)sync_timing_content_end_snapshot(ok,content->dirty,&prefix);
    process_irq_restore(irq);return ok?0:SRV_ERR_IO;
}
static int64_t content_flush(struct process_file_content *content) {
    return content_flush_observed(content,0,false);
}
static int64_t content_release(struct process_file_content *content) {
    uint64_t irq=process_irq_save();
    if(!content || !content->refs) {process_irq_restore(irq);return SRV_ERR_BADF;}
    if(content->refs>1) {content->refs--;process_irq_restore(irq);return 0;}
    if(content->writeback.active) {process_irq_restore(irq);return SRV_ERR_BUSY;}
    /* The slot remains live while a backend service window can run, and until
     * its old data is freed. A zero refcount would let another pathname reuse
     * this fixed slot before the final free/reset of an IF1 kernel caller. */
    int64_t result=content_flush(content);
    if(content->data) {
        if(content->owned) kfree((void *)content->data);
        else if(content->node) vfs_release_data(content->node,content->data);
    }
    *content=(struct process_file_content){0};process_irq_restore(irq);return result;
}
int64_t process_file_alloc(struct process *process,
    const struct vfs_node *node,
    const uint8_t *data,
    uint64_t size) {
    if (process == NULL || node == NULL || (data == NULL && size != 0)) {
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

    struct process_file_content *content=content_acquire(node,node->path,data,size);
    if(!content) return SRV_ERR_NOMEM;
    struct process_read_file *read = &read_files[handle - 1];
    uint8_t *read_bytes = (uint8_t *)read;
    for (uint64_t i = 0; i < sizeof(*read); i++) {
        read_bytes[i] = 0;
    }
    read->used = true;
    read->refs = 1;
    read->node = node;
    read->directory = node->type==VFS_NODE_DIRECTORY;
    read->content = content;
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
    /* Contents are resolved through the description, never a stale pointer. */
    file->handle = handle;
    file->flags = 0;
    (void)copy_process_path(file->path, sizeof(file->path), node->path);
    return (int64_t)(free_index);
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

static int64_t read_file_close(uint64_t handle) {
    struct process_read_file *file = read_file_at(handle);
    if (file == NULL) {
        return SRV_ERR_BADF;
    }
    if(file->content && file->content->writeback.active)return SRV_ERR_BUSY;
    if (file->refs > 0) {
        file->refs--;
    }
    if (file->refs > 0) {
        return 0;
    }
    int64_t result=content_release(file->content);
    uint8_t *bytes = (uint8_t *)file;
    for (uint64_t i = 0; i < sizeof(*file); i++) {
        bytes[i] = 0;
    }
    return result;
}

/* Directory cookies share the open description, so dup/fork observe the same
 * position. Mutation during enumeration has the standard unspecified ordering;
 * no entry cache or unrelated per-stream pathname scan is retained in libc. */
static int64_t directory_next(struct process_read_file *read,struct srv_dirent_info *out,bool metadata) {
    struct srv_dirent entry={0};
    const struct vfs_node *selected=NULL;
    const char *path=read->content->path;
    uint64_t length=0;while(path[length]) length++;
    if(read->offset<2) {
        bool parent=read->offset==1;
        char parent_path[PROCESS_PATH_MAX];copy_process_path(parent_path,sizeof(parent_path),read->path);
        length=0;while(parent_path[length]) length++;
        if(parent) {
            while(length>1 && parent_path[length-1]!='/') length--;
            if(length>1) length--;
            parent_path[length]=0;
        }
        const struct vfs_node *node=vfs_namespace_system_lookup(parent_path);
        if(!node) return SRV_ERR_NOENT;
        selected=node;entry.inode=node->metadata.inode;entry.type=VFS_NODE_DIRECTORY;
        entry.name[0]='.';if(parent) entry.name[1]='.';
        read->offset++;
    } else {
        for(;;) {
            uint64_t index=read->offset-2;
            const struct vfs_node *node=vfs_node_at(index);
            if(!node) {
                if(path[0]!='/' || path[1]) return 0;
                char projected[PROCESS_PATH_MAX];
                int found=vfs_namespace_system_root((unsigned)(index-vfs_count()),projected,sizeof(projected),&node);
                if(found<=0) return found;
                read->offset++;
                selected=node;entry.inode=node->metadata.inode;entry.type=node->type;
                if(!copy_process_path(entry.name,sizeof(entry.name),projected[0]=='/'?projected+1:projected)) return SRV_ERR_NAMETOOLONG;
                break;
            }
            read->offset++;
            uint64_t i=0;while(i<length && node->path[i]==path[i]) i++;
            if(i!=length) continue;
            if(length>1) { if(node->path[i]!='/') continue;i++; }
            const char *name=node->path+i;
            uint64_t count=0;while(name[count] && name[count]!='/') count++;
            if(!count || name[count]=='/') continue;
            if(count>=sizeof(entry.name)) return SRV_ERR_NAMETOOLONG;
            selected=node;entry.inode=node->metadata.inode;entry.type=node->type;
            copy_process_path(entry.name,sizeof(entry.name),name);break;
        }
    }
    *out=(struct srv_dirent_info){.entry=entry,.size=selected->size,.modified=selected->metadata.mtime};
    if(metadata)(void)process_open_file_size(selected->path,&out->size);
    return 1;
}
int64_t process_directory(uint64_t operation,uint64_t fd,void *out,uint64_t capacity) {
    struct process_file *file=process_file_at(process_current(),fd);
    if(!file) return SRV_ERR_BADF;
    struct process_read_file *read=file->type==PROCESS_FILE_VFS?read_file_at(file->handle):NULL;
    if(!read || !read->directory) return SRV_ERR_NOTDIR;
    const char *path=read->path;
    if(operation==SRV_DIRECTORY_PATH) {
        uint64_t length=0;while(path[length]) length++;
        if(!out) return SRV_ERR_FAULT;
        if(capacity<=length) return SRV_ERR_RANGE;
        copy_process_path(out,capacity,path);return 0;
    }
    if(operation==SRV_DIRECTORY_CHDIR) return process_path_context(SRV_PATH_CHDIR,path,NULL,0);
    if(operation==SRV_DIRECTORY_READ_INFO) {
        if(!out || !capacity || capacity%sizeof(struct srv_dirent_info) ||
           capacity>SRV_DIRECTORY_BATCH_MAX*sizeof(struct srv_dirent_info))return SRV_ERR_INVAL;
        struct srv_dirent_info *entries=out;uint64_t start=read->offset,count=0;
        while(count<capacity/sizeof(*entries)) {
            int64_t result=directory_next(read,&entries[count],true);
            if(result<0){read->offset=start;return result;}
            if(!result)break;
            count++;
        }
        return (int64_t)count;
    }
    if(operation!=SRV_DIRECTORY_READ || capacity!=sizeof(struct srv_dirent) || !out)return SRV_ERR_INVAL;
    struct srv_dirent_info entry;int64_t result=directory_next(read,&entry,false);
    if(result>0)*(struct srv_dirent *)out=entry.entry;
    return result;
}

int64_t process_file_read(struct process *process, uint64_t fd, uint8_t *buffer, uint64_t length) {
    struct process_file *endpoint=process_file_at(process,fd);
    if(endpoint && (endpoint->type==PROCESS_FILE_PTY_MASTER || endpoint->type==PROCESS_FILE_PTY_SLAVE))
        return pty_read(endpoint->handle,endpoint->type==PROCESS_FILE_PTY_MASTER,buffer,length,process_file_nonblocking(process,fd));
    uint64_t flags = process_irq_save();
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL ||
        (file->type != PROCESS_FILE_VFS && file->type != PROCESS_FILE_VFS_WRITE) ||
        (buffer == NULL && length != 0)) {
        process_irq_restore(flags);
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
            process_irq_restore(flags);
            return -1;
        }
        if(read->directory) { process_irq_restore(flags);return SRV_ERR_ISDIR; }
        size = read->content->size;
        offset = read->offset;
        data = read->content->data;
    } else if (file->type == PROCESS_FILE_VFS_WRITE) {
        write = write_file_at(file->handle);
        if (write == NULL || !(write->open_flags & SRV_OPEN_READ)) {
            process_irq_restore(flags);
            return -1;
        }
        size = write->content->size;
        offset = write->offset;
        data = write->content->data;
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
    process_irq_restore(flags);
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

static bool file_content_for_fd(struct process_file *file,struct process_file_content **out) {
    if(!file || !out) return false;
    if(file->type==PROCESS_FILE_VFS) {
        struct process_read_file *read=read_file_at(file->handle);
        if(read) { *out=read->content;return true; }
    } else if(file->type==PROCESS_FILE_VFS_WRITE) {
        struct process_write_file *write=write_file_at(file->handle);
        if(write) { *out=write->content;return true; }
    }
    return false;
}

static bool file_lock_range_for_fd(struct process_file *file,
    const struct srv_flock *request,
    uint64_t *start_out,
    uint64_t *end_out) {
    if (file == NULL || request == NULL || start_out == NULL || end_out == NULL) {
        return false;
    }

    if(file->type==PROCESS_FILE_VFS && read_file_at(file->handle) && read_file_at(file->handle)->directory) return false;
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
            base = read->content->size;
        } else if (file->type == PROCESS_FILE_VFS_WRITE) {
            struct process_write_file *write = write_file_at(file->handle);
            if (write == NULL) {
                return false;
            }
            base = write->content->size;
        } else {
            base = file->size;
        }
    } else {
        return false;
    }

    if(base>INT64_MAX) return false;
    int64_t signed_base=(int64_t)base;
    if((request->start>0 && signed_base>INT64_MAX-request->start) ||
        (request->start<0 && signed_base<INT64_MIN-request->start)) return false;
    int64_t point=signed_base+request->start;
    if(point<0) return false;
    uint64_t start=(uint64_t)point,end;
    if(request->len<0) {
        if(request->len==INT64_MIN || point < -request->len) return false;
        end=start;start=(uint64_t)(point+request->len);
    } else {
        end=request->len?start+(uint64_t)request->len:UINT64_MAX;
        if(request->len && end>(uint64_t)INT64_MAX+1) return false;
    }
    if(end<=start) return false;
    *start_out=start;*end_out=end;return true;
}

static struct process_file_lock *file_lock_conflict(struct process_file_content *content,
    uint64_t pid,
    uint64_t start,
    uint64_t end,
    int16_t type) {
    for (uint64_t i = 0; i < PROCESS_MAX_FILE_LOCKS; i++) {
        struct process_file_lock *lock = &file_locks[i];
        if (!lock->used ||
            lock->pid == pid ||
            lock->content!=content ||
            !file_lock_ranges_overlap(lock->start, lock->end, start, end)) {
            continue;
        }
        if (type == SRV_F_WRLCK || lock->type == SRV_F_WRLCK) {
            return lock;
        }
    }
    return NULL;
}

/* Build a replacement table first: splitting a lock must not erase unrelated
 * bytes or lose the old lock if the bounded table cannot hold the result. */
static int64_t file_lock_update(uint64_t pid,struct process_file_content *content,
    uint64_t start,uint64_t end,int16_t type) {
    struct process_file_lock next[PROCESS_MAX_FILE_LOCKS*2+1];unsigned count=0;
    for(unsigned i=0;i<PROCESS_MAX_FILE_LOCKS;i++) {
        struct process_file_lock old=file_locks[i];
        if(!old.used) continue;
        if(old.pid!=pid || old.content!=content || !file_lock_ranges_overlap(old.start,old.end,start,end)) {
            next[count++]=old;continue;
        }
        if(old.start<start) { next[count]=old;next[count++].end=start; }
        if(old.end>end) { next[count]=old;next[count++].start=end; }
    }
    if(type!=SRV_F_UNLCK) next[count++]=(struct process_file_lock){
        .used=true,.pid=pid,.content=content,.start=start,.end=end,.type=type};
    for(unsigned i=0;i<count;i++) for(unsigned j=i+1;j<count;) {
        if(next[i].pid==next[j].pid && next[i].content==next[j].content && next[i].type==next[j].type &&
            next[i].start<=next[j].end && next[j].start<=next[i].end) {
            if(next[j].start<next[i].start) next[i].start=next[j].start;
            if(next[j].end>next[i].end) next[i].end=next[j].end;
            next[j]=next[--count];j=i+1;
        } else j++;
    }
    if(count>PROCESS_MAX_FILE_LOCKS) return SRV_ERR_NOLCK;
    for(unsigned i=0;i<PROCESS_MAX_FILE_LOCKS;i++) file_locks[i]=i<count?next[i]:(struct process_file_lock){0};
    scheduler_wake_all(&file_lock_wait_queue);return 0;
}

static void file_lock_release_path(uint64_t pid, struct process_file_content *content) {
    if (content == NULL) {
        return;
    }
    for (uint64_t i = 0; i < PROCESS_MAX_FILE_LOCKS; i++) {
        if (file_locks[i].used &&
            file_locks[i].pid == pid &&
            file_locks[i].content==content) {
            file_locks[i].used = false;
        }
    }
    scheduler_wake_all(&file_lock_wait_queue);
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
        write_revoke_append(file);
        file->refs++;
    }
}

static bool ensure_write_capacity(struct process_write_file *file,uint64_t needed) {
    if(!file || !file->content) return false;
    struct process_file_content *content=file->content;
    if(content->owned && needed<=content->capacity) return true;
    if(content->writeback.active)return false;
    uint64_t capacity=content->capacity?content->capacity:4096;
    if(needed<content->size) needed=content->size;
    if(needed>PROCESS_WRITE_BUFFER_MAX) return false;
    while(capacity<needed) capacity*=2;
    uint8_t *next=kmalloc(capacity);
    if(!next) return false;
    for(uint64_t i=0;i<content->size;i++) next[i]=content->data[i];
    for(uint64_t i=content->size;i<capacity;i++) next[i]=0;
    uint64_t irq=process_irq_save();
    const uint8_t *previous=content->data;bool owned=content->owned;
    for(unsigned i=0;i<PROCESS_MAX_WRITE_FILES;i++)
        if(write_files[i].used && write_files[i].content==content)write_files[i].append.valid=false;
    content->data=next;content->capacity=capacity;content->owned=true;
    if(previous) {
        if(owned) kfree((void *)previous);
        else if(content->node) vfs_release_data(content->node,previous);
    }
    process_irq_restore(irq);return true;
}

#define PROCESS_RESERVED_APPEND_COPY_MAX 65536u
#define PROCESS_RESERVED_APPEND_WINDOW (4u*PROCESS_RESERVED_APPEND_COPY_MAX)
/* Metadata-only lease proof. Caller holds CPU0 IRQ exclusion. No pointer from
 * this check may survive a wait; the ordinary path revalidates after admission. */
static struct process_write_file *reserved_append_binding(struct process *process,uint64_t fd) {
    if(!process || process!=process_current() || !process->allocated || !process->active ||
       !process->image_generation || fd>=PROCESS_MAX_OPEN_FILES ||
       process->file_generations[fd]==UINT64_MAX)return NULL;
    struct process_file *file=process_file_at(process,fd);
    if(!file || file->type!=PROCESS_FILE_VFS_WRITE)return NULL;
    struct process_write_file *write=write_file_at(file->handle);
    if(!write || write->refs!=1 || !write->append.valid ||
       write->append.owner!=process || write->append.image_generation!=process->image_generation ||
       write->append.fd!=fd || write->append.fd_generation!=process->file_generations[fd] ||
       write->append.open_flags!=write->open_flags || !(write->open_flags&SRV_OPEN_WRITE) ||
       (write->open_flags&SRV_OPEN_READ))return NULL;
    struct process_file_content *content=write->content;
    if(!content || content!=write->append.content || content->refs!=1 || !content->owned ||
       !content->data || content->failed || content->unlinked || !content->dirty_generation ||
       content->dirty_generation==UINT64_MAX || content->data!=write->append.data ||
       content->capacity!=write->append.capacity || content->size>write->append.limit ||
       write->append.limit>content->capacity || write->append.limit>PROCESS_WRITE_BUFFER_MAX)return NULL;
    if(content->writeback.active && (content->writeback.data!=content->data ||
       content->writeback.size>content->size))return NULL;
    return write;
}
static bool reserved_append_count(struct process *process,struct process_write_file *write,
    uint64_t requested,bool window,uint64_t *count) {
    if(!write || !requested || requested>PROCESS_RESERVED_APPEND_COPY_MAX)return false;
    struct process_file_content *content=write->content;
    uint64_t position=(write->open_flags&SRV_OPEN_APPEND)?content->size:write->offset;
    if(position!=content->size || position>=process->fsize_limit.current)return false;
    uint64_t amount=requested;
    if(amount>process->fsize_limit.current-position)amount=process->fsize_limit.current-position;
    if(!amount || amount>write->append.limit-position || position>PROCESS_WRITE_BUFFER_MAX ||
       amount>PROCESS_WRITE_BUFFER_MAX-position)return false;
    uint64_t base=content->writeback.active?content->writeback.size:content->committed_size;
    if(window && (base>position || position+amount-base>PROCESS_RESERVED_APPEND_WINDOW))return false;
    *count=amount;return true;
}
int64_t process_file_reserve_append_ram(struct process *process,uint64_t fd,uint64_t limit) {
    struct process_service_scope scope=process_service_begin();
#if !defined(SRVROS_PROCESS_HOST_TEST) || defined(SRVROS_PROCESS_SERVICE_HOST_TEST)
    if(!scope.held) {process_service_end(scope);return SRV_ERR_IO;}
#endif
    int64_t result=SRV_ERR_INVAL;
    if(!process || process!=process_current() || !process->allocated || !process->active ||
       !process->image_generation || fd>=PROCESS_MAX_OPEN_FILES ||
       process->file_generations[fd]==UINT64_MAX || !limit || limit>PROCESS_WRITE_BUFFER_MAX)goto done;
    struct process_file *file=process_file_at(process,fd);
    struct process_write_file *write=file && file->type==PROCESS_FILE_VFS_WRITE?write_file_at(file->handle):NULL;
    if(!write || write->refs!=1 || !write->fresh_exclusive ||
       !(write->open_flags&SRV_OPEN_WRITE) || (write->open_flags&SRV_OPEN_READ))goto done;
    struct process_file_content *content=write->content;
    if(!content || content->refs!=1 || content->size || write->offset || content->dirty ||
       content->failed || content->unlinked || content->writeback.active ||
       !content->dirty_generation || content->dirty_generation==UINT64_MAX)goto done;
    if(!ensure_write_capacity(write,limit)) {result=SRV_ERR_NOMEM;goto done;}
    write->append=(__typeof__(write->append)){.valid=true,.owner=process,.content=content,
        .data=content->data,.image_generation=process->image_generation,.fd=fd,
        .fd_generation=process->file_generations[fd],.capacity=content->capacity,
        .limit=limit,.open_flags=write->open_flags};
    result=0;
done:
    process_service_end(scope);return result;
}
bool process_file_reserved_append_entry_candidate(struct process *process,uint64_t fd,uint64_t requested) {
    uint64_t irq=process_irq_save(),count;
    bool allowed=reserved_append_count(process,reserved_append_binding(process,fd),requested,true,&count) &&
        scheduler_io_entry_process_independent(process);
    process_irq_restore(irq);return allowed;
}
bool process_file_try_reserved_append(struct process *process,uint64_t fd,uint64_t requested,
    process_reserved_append_copy_fn copy,void *context,int64_t *result) {
    if(!copy || !result)return false;
    uint64_t irq=process_irq_save(),count;
    uint64_t reserve_at=process_capture_write_clock();
    struct process_write_file *write=reserved_append_binding(process,fd);
    if(!reserved_append_count(process,write,requested,true,&count) ||
       !scheduler_io_process_independent(process)) {
        process_irq_restore(irq);return false;
    }
    struct process_file_content *content=write->content;
    process_capture_write_record(CAP_WRITE_RESERVE,reserve_at);
    uint64_t copy_at=process_capture_write_clock();
    bool copied=copy(context,(uint8_t *)content->data+content->size,count,requested);
    process_capture_write_record(CAP_WRITE_COPY,copy_at);
    if(!copied) {
        *result=SRV_ERR_FAULT;process_irq_restore(irq);return true;
    }
    content->size+=count;write->offset=content->size;content_mutated(content);
    if(content->writeback.active) {
        content->writeback.append_calls++;content->writeback.append_bytes+=count;
    }
    *result=(int64_t)count;process_irq_restore(irq);return true;
}

int64_t process_unlink_file(const char *path) {
    if(process_stream_busy(path,true))return SRV_ERR_BUSY;
    const struct vfs_node *node=vfs_lookup(path);
    if(!node) return SRV_ERR_NOENT;
    if(node->type!=VFS_NODE_FILE) return SRV_ERR_ISDIR;
    struct process_file_content *content=content_find(path);
    if(content && content->writeback.active)return SRV_ERR_BUSY;
    content_revoke_append(content);
    struct vfs_metadata metadata=node->metadata;
    if(content && !content->owned) {
        struct process_write_file temporary={.content=content};
        if(!ensure_write_capacity(&temporary,content->size)) return SRV_ERR_NOMEM;
    }
    bool ok=exfat_delete_file(path);
    /* Metadata/flush can fail after the backend removes the live name. Do not
     * leave that content indexed by a pathname which can now be reused. */
    if(content && !vfs_lookup(path)) {
        content->unlinked=true;content->node=NULL;content->dirty=false;
        metadata.nlink=0;content->orphan_metadata=metadata;
    }
    return ok?0:SRV_ERR_IO;
}

int64_t process_rename_file(const char *old_path,const char *new_path) {
    if(process_stream_busy(old_path,true) || process_stream_busy(new_path,true))return SRV_ERR_BUSY;
    const struct vfs_node *source=vfs_lookup(old_path),*target=vfs_lookup(new_path);
    if(!source) return SRV_ERR_NOENT;
    if(process_path_equal(old_path,new_path)) return 0;
    if(!exfat_same_mount(old_path,new_path)) return SRV_ERR_XDEV;
    if(source->type==VFS_NODE_DIRECTORY) {
        if(target && target->type!=VFS_NODE_DIRECTORY) return SRV_ERR_NOTDIR;
        if(path_under_prefix(new_path,old_path)) return SRV_ERR_INVAL;
        if(vfs_namespace_system_protects(old_path) || vfs_namespace_system_protects(new_path)) return SRV_ERR_BUSY;
        if(process_has_open_vfs_prefix(old_path) || process_has_open_vfs_prefix(new_path)) return SRV_ERR_BUSY;
        /* Directory subtree/replacement transactions are a separate backend
         * contract. Preserve the existing empty, unused directory operation. */
        if(target) return SRV_ERR_NOTSUP;
        return exfat_rename(old_path,new_path)?0:SRV_ERR_IO;
    }
    if(target && target->type==VFS_NODE_DIRECTORY) return SRV_ERR_ISDIR;
    if(vfs_namespace_system_protects(old_path) || vfs_namespace_system_protects(new_path)) return SRV_ERR_BUSY;
    uint64_t source_inode=source->metadata.inode;
    struct vfs_metadata target_metadata=target?target->metadata:(struct vfs_metadata){0};
    struct process_file_content *source_content=content_find(old_path),*target_content=content_find(new_path);
    if((source_content && source_content->writeback.active) ||
       (target_content && target_content->writeback.active))return SRV_ERR_BUSY;
    struct process_file_content *contents[2]={source_content,target_content};
    content_revoke_append(source_content);content_revoke_append(target_content);
    /* Promote retained backend snapshots before their directory slots can be
     * retired or reused. OOM leaves both names and contents unchanged. */
    for(unsigned i=0;i<2;i++) if(contents[i] && !contents[i]->owned) {
        struct process_write_file temporary={.content=contents[i]};
        if(!ensure_write_capacity(&temporary,contents[i]->size)) return SRV_ERR_NOMEM;
    }
    if(source_content && content_flush(source_content)<0) return SRV_ERR_IO;
    bool published=false;
    bool ok=target?exfat_replace_file(old_path,new_path,&published):exfat_rename(old_path,new_path);
    const struct vfs_node *renamed=vfs_lookup(new_path);
    if(!target) published=!vfs_lookup(old_path) && renamed && renamed->metadata.inode==source_inode;
    if(published) {
        if(target_content) {
            target_content->unlinked=true;target_content->node=NULL;target_content->dirty=false;
            target_metadata.nlink=0;target_content->orphan_metadata=target_metadata;
        }
        if(source_content) {
            (void)copy_process_path(source_content->path,sizeof(source_content->path),new_path);
            source_content->node=renamed;
            for(unsigned i=0;i<PROCESS_MAX_READ_FILES;i++) if(read_files[i].used && read_files[i].content==source_content) {
                read_files[i].node=renamed;
                (void)copy_process_path(read_files[i].path,sizeof(read_files[i].path),new_path);
            }
            for(unsigned i=0;i<PROCESS_MAX_WRITE_FILES;i++) if(write_files[i].used && write_files[i].content==source_content)
                (void)copy_process_path(write_files[i].path,sizeof(write_files[i].path),new_path);
        }
    }
    return ok?0:SRV_ERR_IO;
}

static int64_t process_file_open_write_raw(struct process *process,const char *path,uint64_t flags);

int64_t process_file_open_flags(struct process *process,const char *path,uint64_t flags) {
    if(!(flags&(SRV_OPEN_READ|SRV_OPEN_WRITE)) ||
        (flags&~(SRV_OPEN_READ|SRV_OPEN_WRITE|SRV_OPEN_CREATE|SRV_OPEN_TRUNC|SRV_OPEN_APPEND|SRV_OPEN_NONBLOCK|SRV_OPEN_EXCL|SRV_OPEN_CLOEXEC|SRV_OPEN_DIRECTORY|SRV_OPEN_PERMISSIONS|SRV_OPEN_MODE_MASK))) return SRV_ERR_INVAL;
    if((flags&SRV_OPEN_MODE_MASK) && !(flags&SRV_OPEN_PERMISSIONS)) return SRV_ERR_INVAL;
    if((flags&SRV_OPEN_DIRECTORY) && (flags&(SRV_OPEN_CREATE|SRV_OPEN_WRITE|SRV_OPEN_TRUNC))) return SRV_ERR_INVAL;
    /* Hold filesystem admission across lookup, exclusive create and descriptor
     * publication. O_EXCL must not be a userspace stat/create race. */
    struct process_service_scope scope=process_service_begin();
#if !defined(SRVROS_PROCESS_HOST_TEST) || defined(SRVROS_PROCESS_SERVICE_HOST_TEST)
    if(!scope.held) { process_service_end(scope);return SRV_ERR_IO; }
#endif
    char logical[PROCESS_PATH_MAX],storage[PROCESS_PATH_MAX];
    int64_t result=process_path_context_for(process,SRV_PATH_RESOLVE,path,logical,sizeof(logical));
    if(result<0) { process_service_end(scope);return result; }
    result=vfs_namespace_system_path(logical,storage,sizeof(storage));
    if(result<0) { process_service_end(scope);return result; }
    if(process_stream_busy(storage,false)) { process_service_end(scope);return SRV_ERR_BUSY; }
    if(flags&(SRV_OPEN_WRITE|SRV_OPEN_CREATE)) result=process_file_open_write_raw(process,storage,flags);
    else {
        const struct vfs_node *node=vfs_lookup(storage);const uint8_t *data;uint64_t size;
        if(!node) result=SRV_ERR_NOENT;
        else if((flags&SRV_OPEN_DIRECTORY) && node->type!=VFS_NODE_DIRECTORY) result=SRV_ERR_NOTDIR;
        else if(node->type==VFS_NODE_DIRECTORY) {
            result=process_file_alloc(process,node,NULL,0);
            if(result>=0) {
                struct process_file *opened=process_file_at(process,(uint64_t)result);
                struct process_read_file *read=opened?read_file_at(opened->handle):NULL;
                if(read) (void)copy_process_path(read->path,sizeof(read->path),logical);
                (void)process_file_set_flags(process,result,(flags&SRV_OPEN_NONBLOCK)?SRV_FD_NONBLOCK:0);
                (void)process_file_set_fd_flags(process,result,(flags&SRV_OPEN_CLOEXEC)?SRV_FD_CLOEXEC:0);
            }
        }
        else if(!vfs_read_all(node,&data,&size)) result=SRV_ERR_IO;
        else {
            result=process_file_alloc(process,node,data,size);
            if(result<0) result=SRV_ERR_MFILE;
            else {
                (void)process_file_set_flags(process,result,((flags&SRV_OPEN_NONBLOCK)?SRV_FD_NONBLOCK:0)|((flags&SRV_OPEN_APPEND)?SRV_FD_APPEND:0));
                (void)process_file_set_fd_flags(process,result,(flags&SRV_OPEN_CLOEXEC)?SRV_FD_CLOEXEC:0);
            }
        }
    }
    process_service_end(scope);return result;
}

int64_t process_file_open_write(struct process *process, const char *path, uint64_t flags) {
    struct process_service_scope scope=process_service_begin();
#if !defined(SRVROS_PROCESS_HOST_TEST) || defined(SRVROS_PROCESS_SERVICE_HOST_TEST)
    if(!scope.held) { process_service_end(scope);return SRV_ERR_IO; }
#endif
    char logical[PROCESS_PATH_MAX],storage[PROCESS_PATH_MAX];
    int64_t result=process_path_context_for(process,SRV_PATH_RESOLVE,path,logical,sizeof(logical));
    if(result>=0) result=vfs_namespace_system_path(logical,storage,sizeof(storage));
    if(result>=0) result=process_file_open_write_raw(process,storage,flags);
    process_service_end(scope);return result;
}

static int64_t process_file_open_write_raw(struct process *process, const char *path, uint64_t flags) {
    if(process_stream_busy(path,false))return SRV_ERR_BUSY;
    if (process == NULL ||
        path == NULL ||
        (flags & (SRV_OPEN_READ|SRV_OPEN_WRITE)) == 0 ||
        ((flags & SRV_OPEN_TRUNC) && !(flags & SRV_OPEN_WRITE))) {
        return SRV_ERR_INVAL;
    }

    const struct vfs_node *node = vfs_lookup(path);
    const uint8_t *old_data = NULL;
    uint64_t old_size = 0;
    bool exists = node != NULL;
    bool append = (flags & SRV_OPEN_APPEND) != 0;
    bool trunc = (flags & SRV_OPEN_TRUNC) != 0;
    bool ensure_empty = trunc || (!exists && (flags & SRV_OPEN_CREATE) != 0);
    struct process_file_content *existing_content=content_find(path);
    if(existing_content && existing_content->writeback.active)return SRV_ERR_BUSY;

    if(exists && (flags & SRV_OPEN_CREATE) && (flags & SRV_OPEN_EXCL)) return SRV_ERR_EXIST;
    if(exists && node->type==VFS_NODE_DIRECTORY) return SRV_ERR_ISDIR;
    if (!exists && (flags & SRV_OPEN_CREATE) == 0) return SRV_ERR_NOENT;

    if (exists && !content_find(path) && !vfs_read_all(node, &old_data, &old_size)) {
        return SRV_ERR_IO;
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
        if (old_data != NULL) vfs_release_data(node, old_data);
        return SRV_ERR_MFILE;
    }

    uint64_t handle = write_file_alloc();
    if (handle == 0) {
        if (old_data != NULL) {
            vfs_release_data(node, old_data);
        }
        return SRV_ERR_IO;
    }

    struct process_write_file *write=write_file_at(handle);
    write->content=content_acquire(node,path,old_data,old_size);
    if(!write->content || !copy_process_path(write->path,sizeof(write->path),path)) {
        if(write->content) (void)content_release(write->content);
        *write=(struct process_write_file){0};return SRV_ERR_NOMEM;
    }
    if((flags&SRV_OPEN_WRITE) && !ensure_write_capacity(write,write->content->size)) {
        (void)content_release(write->content);*write=(struct process_write_file){0};return SRV_ERR_NOMEM;
    }
    uint64_t mode=(flags&SRV_OPEN_PERMISSIONS)?
        ((flags&SRV_OPEN_MODE_MASK)>>SRV_OPEN_MODE_SHIFT)&~process->creation_mask:UINT64_MAX;
    if(ensure_empty && !exfat_write_file_mode(path,NULL,0,mode)) {
        (void)content_release(write->content);*write=(struct process_write_file){0};return SRV_ERR_IO;
    }
    if(ensure_empty) {
        content_revoke_append(write->content);write->content->size=0;
        content_mutated(write->content);write->content->dirty=false;write->content->committed_size=0;
    }
    write->open_flags=flags;
    write->fresh_exclusive=!exists &&
        (flags&(SRV_OPEN_CREATE|SRV_OPEN_EXCL|SRV_OPEN_WRITE))==(SRV_OPEN_CREATE|SRV_OPEN_EXCL|SRV_OPEN_WRITE) &&
        !(flags&SRV_OPEN_READ);
    write->offset=append?write->content->size:0;
    struct process_file *file = &process->files[free_index];
    uint8_t *bytes = (uint8_t *)file;
    for (uint64_t j = 0; j < sizeof(*file); j++) {
        bytes[j] = 0;
    }
    file->used = true;
    file->type = PROCESS_FILE_VFS_WRITE;
    file->handle = handle;
    file->flags = ((flags & SRV_OPEN_NONBLOCK)?SRV_FD_NONBLOCK:0) |
        ((flags & SRV_OPEN_CLOEXEC)?SRV_FD_CLOEXEC:0);
    (void)copy_process_path(file->path, sizeof(file->path), path);
    return (int64_t)(free_index);
}

static void process_fsize_signal(struct process *process) {
    if(process==process_current()) (void)process_thread_signal(process_thread_self(),SRV_SIGNAL_XFSZ);
    else (void)process_queue_signal_locked(process,SRV_SIGNAL_XFSZ);
}

/* Return a short write at the soft boundary, and signal only when no byte
 * fits. Zero-byte writes never extend the file or generate a signal. */
static int64_t process_fsize_write(struct process *process,uint64_t position,uint64_t length) {
    if(!length) return 0;
    uint64_t limit=process->fsize_limit.current;
    if(position>=limit) { process_fsize_signal(process);return SRV_ERR_FBIG; }
    return (int64_t)(length>limit-position?limit-position:length);
}

uint64_t process_stream_identity(struct process *process) {
    if(!process || !process->allocated || !process->active)return 0;
    uint64_t flags=process_irq_save();
    if(!process->stream_instance && next_stream_instance) {
        process->stream_instance=next_stream_instance++;
        /* Zero is permanent exhaustion, never a wrapped identity. */
    }
    uint64_t identity=process->stream_instance;process_irq_restore(flags);return identity;
}

int64_t process_stream_write_allowed(uint64_t position,uint64_t length) {
    struct process *process=process_current();
    if(!process || length>INT64_MAX || position>UINT64_MAX-length)return SRV_ERR_INVAL;
    return process_fsize_write(process,position,length);
}

int64_t process_file_write(struct process *process, uint64_t fd, const uint8_t *buffer, uint64_t length) {
    struct process_file *endpoint=process_file_at(process,fd);
    if(endpoint && (endpoint->type==PROCESS_FILE_PTY_MASTER || endpoint->type==PROCESS_FILE_PTY_SLAVE))
        return pty_write(endpoint->handle,endpoint->type==PROCESS_FILE_PTY_MASTER,buffer,length,process_file_nonblocking(process,fd));
    uint64_t flags = process_irq_save();
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL || file->type != PROCESS_FILE_VFS_WRITE || (buffer == NULL && length != 0)) {
        process_irq_restore(flags);
        return -1;
    }
    struct process_write_file *write = write_file_at(file->handle);
    if (write == NULL || !(write->open_flags & SRV_OPEN_WRITE)) {
        process_irq_restore(flags);
        return -1;
    }
    if (length == 0) {
        process_irq_restore(flags);
        return 0;
    }

    if(write->content->writeback.active) {process_irq_restore(flags);return SRV_ERR_BUSY;}

    uint64_t position=(write->open_flags & SRV_OPEN_APPEND)?write->content->size:write->offset;
    int64_t allowed=process_fsize_write(process,position,length);
    if(allowed<0) { process_irq_restore(flags);return allowed; }
    length=(uint64_t)allowed;
    if(position>PROCESS_WRITE_BUFFER_MAX || length>PROCESS_WRITE_BUFFER_MAX-position) {
        process_irq_restore(flags);return SRV_ERR_FBIG;
    }
    uint64_t needed=position+length;
    uint64_t retained_count=0;
    bool keep_append=reserved_append_binding(process,fd)==write &&
        reserved_append_count(process,write,length,false,&retained_count) && retained_count==length;
    uint64_t reserve_at=process_capture_write_clock(),capacity=write->content->capacity;
    bool reserved=ensure_write_capacity(write,needed);
    process_capture_write_record(CAP_WRITE_RESERVE,reserve_at);
    if(!reserved) { process_irq_restore(flags);return SRV_ERR_NOMEM; }
    if(reserve_at && write->content->capacity!=capacity)process_capture_write_growth();
    uint64_t copy_at=process_capture_write_clock();
    for(uint64_t i=write->content->size;i<position;i++) ((uint8_t *)write->content->data)[i]=0;
    copy_bytes((uint8_t *)write->content->data+position,buffer,length);
    process_capture_write_record(CAP_WRITE_COPY,copy_at);
    write->offset=needed;
    if (write->offset > write->content->size) {
        write->content->size = write->offset;
    }
    if(!keep_append)content_revoke_append(write->content);
    content_mutated(write->content);
    process_irq_restore(flags);
    return (int64_t)length;
}

/* Resolve inherited standard descriptors and their duplicates without turning
 * a console/pipe into a seekable stream. Bound traversal of malformed chains. */
static int64_t positional_fd(struct process *process, uint64_t fd) {
    struct process_file *file=process_file_at(process,fd);
    return !file?SRV_ERR_BADF:file->type==PROCESS_FILE_STDIO?SRV_ERR_SPIPE:(int64_t)fd;
}

int64_t process_file_pread(struct process *process, uint64_t fd, uint8_t *buffer,
    uint64_t length, int64_t offset) {
    if (offset<0 || length>INT64_MAX) return SRV_ERR_INVAL;
    if (!buffer && length) return SRV_ERR_FAULT;
    uint64_t irq=process_irq_save();
    int64_t resolved=positional_fd(process,fd), result=resolved;
    if (resolved<0) goto done;
    struct process_file *file=process_file_at(process,(uint64_t)resolved);
    const uint8_t *data=NULL;
    uint64_t size=0;
    if (file->type==PROCESS_FILE_VFS) {
        struct process_read_file *read=read_file_at(file->handle);
        if (!read) { result=SRV_ERR_BADF; goto done; }
        if(read->directory) { result=SRV_ERR_ISDIR;goto done; }
        data=read->content->data; size=read->content->size;
    } else if (file->type==PROCESS_FILE_VFS_WRITE) {
        struct process_write_file *write=write_file_at(file->handle);
        if (!write || !(write->open_flags&SRV_OPEN_READ)) { result=SRV_ERR_BADF; goto done; }
        data=write->content->data; size=write->content->size;
    } else { result=SRV_ERR_SPIPE; goto done; }
    uint64_t position=(uint64_t)offset;
    uint64_t count=position<size?size-position:0;
    if (count>length) count=length;
    for (uint64_t i=0;i<count;i++) buffer[i]=data[position+i];
    result=(int64_t)count;
done:
    process_irq_restore(irq);
    return result;
}

int64_t process_file_pwrite(struct process *process, uint64_t fd, const uint8_t *buffer,
    uint64_t length, int64_t offset) {
    if (offset<0 || length>INT64_MAX) return SRV_ERR_INVAL;
    if (!buffer && length) return SRV_ERR_FAULT;
    uint64_t irq=process_irq_save();
    int64_t resolved=positional_fd(process,fd), result=resolved;
    if (resolved<0) goto done;
    struct process_file *file=process_file_at(process,(uint64_t)resolved);
    if (file->type!=PROCESS_FILE_VFS_WRITE) {
        result=file->type==PROCESS_FILE_VFS?SRV_ERR_BADF:SRV_ERR_SPIPE; goto done;
    }
    struct process_write_file *write=write_file_at(file->handle);
    if (!write || !(write->open_flags & SRV_OPEN_WRITE)) { result=SRV_ERR_BADF; goto done; }
    if (!length) { result=0; goto done; }
    if(write->content->writeback.active) {result=SRV_ERR_BUSY;goto done;}
    uint64_t position=(uint64_t)offset;
    result=process_fsize_write(process,position,length);
    if(result<0) goto done;
    length=(uint64_t)result;
    if (position>PROCESS_WRITE_BUFFER_MAX || length>PROCESS_WRITE_BUFFER_MAX-position) {
        result=SRV_ERR_FBIG; goto done;
    }
    uint64_t end=position+length;
    if (!ensure_write_capacity(write,end)) { result=SRV_ERR_NOMEM; goto done; }
    /* Reserve first. A failed request must not poison or discard earlier writes.
     * O_APPEND intentionally does not affect a positional write. */
    for (uint64_t i=write->content->size;i<position;i++) ((uint8_t *)write->content->data)[i]=0;
    for (uint64_t i=0;i<length;i++) ((uint8_t *)write->content->data)[position+i]=buffer[i];
    if (end>write->content->size) write->content->size=end;
    content_revoke_append(write->content);content_mutated(write->content);
    result=(int64_t)length;
done:
    process_irq_restore(irq);
    return result;
}

bool process_file_nonblocking(struct process *process, uint64_t fd) {
    int64_t flags=process_file_get_flags(process,fd);
    return flags>=0 && (flags&SRV_FD_NONBLOCK)!=0;
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

static void pipe_rights_release(struct process_pipe_rights *rights) {
    if (rights == NULL || !rights->used) {
        return;
    }
    for (uint64_t i = 0; i < rights->count && i < SRV_RIGHTS_MAX; i++) {
        process_file_release_entry(&rights->files[i]);
    }
    uint8_t *bytes = (uint8_t *)rights;
    for (uint64_t i = 0; i < sizeof(*rights); i++) {
        bytes[i] = 0;
    }
}

/* The only non-external reader and listener references live in a rights queue
 * or a listener's pending accepts. Subtract those references to find roots,
 * then follow redeemable rights and pending accepts from each root. */
static bool pipe_rights_present;
static bool pipe_rights_collecting;
static unsigned pipe_rights_gc_defer;
static struct process_pipe_rights pipe_rights_retired;

static void pipe_rights_count_file(const struct process_file *file,
    uint64_t readers[PROCESS_MAX_PIPES], uint64_t listeners[PROCESS_MAX_UNIX_LISTENERS]) {
    if (!file->used) return;
    if ((file->type == PROCESS_FILE_PIPE_READ || file->type == PROCESS_FILE_PIPE_DUPLEX) &&
        file->handle && file->handle <= PROCESS_MAX_PIPES)
        readers[file->handle - 1]++;
    if (file->type == PROCESS_FILE_UNIX_LISTENER && file->handle &&
        file->handle <= PROCESS_MAX_UNIX_LISTENERS)
        listeners[file->handle - 1]++;
}

static bool pipe_rights_mark_file(const struct process_file *file,
    bool readers[PROCESS_MAX_PIPES], bool listeners[PROCESS_MAX_UNIX_LISTENERS]) {
    if (!file->used) return false;
    if ((file->type == PROCESS_FILE_PIPE_READ || file->type == PROCESS_FILE_PIPE_DUPLEX) &&
        file->handle && file->handle <= PROCESS_MAX_PIPES &&
        pipes[file->handle - 1].used && !readers[file->handle - 1]) {
        readers[file->handle - 1] = true;
        return true;
    }
    if (file->type == PROCESS_FILE_UNIX_LISTENER && file->handle &&
        file->handle <= PROCESS_MAX_UNIX_LISTENERS &&
        unix_listeners[file->handle - 1].used && !listeners[file->handle - 1]) {
        listeners[file->handle - 1] = true;
        return true;
    }
    return false;
}

/* Called only after the current descriptor/queue transaction is complete and
 * outside the storage-owner fast path. One static retirement slot bounds both
 * stack and allocation use; a nested collector never touches the slot. */
static void pipe_rights_collect(void) {
    uint64_t irq = process_irq_save();
    if (!pipe_rights_present || pipe_rights_collecting || pipe_rights_gc_defer) {
        process_irq_restore(irq);
        return;
    }
    pipe_rights_collecting = true;
    for (;;) {
        uint64_t reader_inflight[PROCESS_MAX_PIPES] = {0};
        uint64_t listener_inflight[PROCESS_MAX_UNIX_LISTENERS] = {0};
        bool readers[PROCESS_MAX_PIPES] = {0};
        bool listeners[PROCESS_MAX_UNIX_LISTENERS] = {0};
        bool any = false;
        for (unsigned p = 0; p < PROCESS_MAX_PIPES; p++) {
            if (!pipes[p].used) continue;
            for (unsigned r = 0; r < PROCESS_PIPE_RIGHTS_QUEUE && pipes[p].rights[r].used; r++) {
                struct process_pipe_rights *rights = &pipes[p].rights[r];
                any = true;
                for (unsigned i = 0; i < rights->count && i < SRV_RIGHTS_MAX; i++)
                    pipe_rights_count_file(&rights->files[i], reader_inflight, listener_inflight);
            }
        }
        for (unsigned l = 0; l < PROCESS_MAX_UNIX_LISTENERS; l++) {
            if (!unix_listeners[l].used) continue;
            for (unsigned i = 0; i < PROCESS_UNIX_ACCEPT_QUEUE; i++)
                if (unix_listeners[l].queue[i].used)
                    pipe_rights_count_file(&unix_listeners[l].queue[i].file, reader_inflight, listener_inflight);
        }
        if (!any) {
            pipe_rights_present = false;
            break;
        }
        for (unsigned p = 0; p < PROCESS_MAX_PIPES; p++)
            readers[p] = pipes[p].used && pipes[p].read_refs > reader_inflight[p];
        for (unsigned l = 0; l < PROCESS_MAX_UNIX_LISTENERS; l++)
            listeners[l] = unix_listeners[l].used && unix_listeners[l].refs > listener_inflight[l];
        bool changed;
        do {
            changed = false;
            for (unsigned p = 0; p < PROCESS_MAX_PIPES; p++) {
                if (!readers[p]) continue;
                for (unsigned r = 0; r < PROCESS_PIPE_RIGHTS_QUEUE && pipes[p].rights[r].used; r++) {
                    struct process_pipe_rights *rights = &pipes[p].rights[r];
                    for (unsigned i = 0; i < rights->count && i < SRV_RIGHTS_MAX; i++)
                        changed |= pipe_rights_mark_file(&rights->files[i], readers, listeners);
                }
            }
            for (unsigned l = 0; l < PROCESS_MAX_UNIX_LISTENERS; l++) {
                if (!listeners[l]) continue;
                for (unsigned i = 0; i < PROCESS_UNIX_ACCEPT_QUEUE; i++)
                    if (unix_listeners[l].queue[i].used)
                        changed |= pipe_rights_mark_file(&unix_listeners[l].queue[i].file, readers, listeners);
            }
        } while (changed);
        struct process_pipe *unreachable = NULL;
        for (unsigned p = 0; p < PROCESS_MAX_PIPES; p++) {
            if (pipes[p].used && !readers[p] && pipes[p].rights[0].used) {
                unreachable = &pipes[p];
                break;
            }
        }
        if (!unreachable) break;
        pipe_rights_retired = unreachable->rights[0];
        for (unsigned i = 1; i < PROCESS_PIPE_RIGHTS_QUEUE; i++)
            unreachable->rights[i - 1] = unreachable->rights[i];
        unreachable->rights[PROCESS_PIPE_RIGHTS_QUEUE - 1] = (struct process_pipe_rights){0};
        if (!unreachable->refs && !unreachable->rights[0].used) unreachable->used = false;
        scheduler_wake_all(&pipe_wait_queue);
        process_file_poll_wake();
        process_irq_restore(irq);
        pipe_rights_release(&pipe_rights_retired);
        irq = process_irq_save();
    }
    pipe_rights_collecting = false;
    process_irq_restore(irq);
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
    if (pipe->refs == 0 && !pipe->rights[0].used) pipe->used = false;
    /* A disconnected queue is drained by the collector after the enclosing
     * close completes; arbitrary file releases may sleep or reenter. */
    scheduler_wake_all(&pipe_wait_queue);
    process_file_poll_wake();
}

static int64_t write_file_close(uint64_t handle) {
    struct process_write_file *write = write_file_at(handle);
    if (write == NULL) {
        return SRV_ERR_BADF;
    }
    if(write->content && write->content->writeback.active)return SRV_ERR_BUSY;

    if (write->refs > 0) {
        write->refs--;
    }
    if (write->refs > 0) {
        return 0;
    }

    int64_t result = 0;
    if (process_file_flush(NULL, handle) < 0) {
        result = SRV_ERR_IO;
    }

    if(content_release(write->content)<0) result=SRV_ERR_IO;
    uint8_t *bytes = (uint8_t *)write;
    for (uint64_t i = 0; i < sizeof(*write); i++) {
        bytes[i] = 0;
    }
    return result;
}

int64_t process_file_truncate(struct process *process, uint64_t fd, uint64_t length) {
    uint64_t irq=process_irq_save();
    int64_t resolved=positional_fd(process,fd),result=SRV_ERR_BADF;
    if(resolved<0) goto done;
    struct process_file *file=process_file_at(process,(uint64_t)resolved);
    if(file->type!=PROCESS_FILE_VFS_WRITE) goto done;
    struct process_write_file *write=write_file_at(file->handle);
    if(!write || !(write->open_flags&SRV_OPEN_WRITE)) goto done;
    if(write->content->writeback.active) {result=SRV_ERR_BUSY;goto done;}
    if(length>write->content->size && length>process->fsize_limit.current) {
        process_fsize_signal(process);result=SRV_ERR_FBIG;goto done;
    }
    if(length>PROCESS_WRITE_BUFFER_MAX) { result=SRV_ERR_FBIG;goto done; }
    if(!ensure_write_capacity(write,length)) { result=SRV_ERR_NOMEM;goto done; }
    for(uint64_t i=write->content->size;i<length;i++) ((uint8_t *)write->content->data)[i]=0;
    write->content->size=length;content_revoke_append(write->content);content_mutated(write->content);result=0;
done:
    process_irq_restore(irq);return result;
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
        if(file->type==PROCESS_FILE_VFS) {
            struct process_read_file *read=read_file_at(file->handle);
            return read?content_flush(read->content):SRV_ERR_BADF;
        }
        if (file->type == PROCESS_FILE_STDIO ||
            file->type == PROCESS_FILE_PTY_MASTER || file->type == PROCESS_FILE_PTY_SLAVE ||
            file->type == PROCESS_FILE_PIPE_READ ||
            file->type == PROCESS_FILE_PIPE_WRITE ||
            file->type == PROCESS_FILE_PIPE_DUPLEX) {
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
    return content_flush(write->content);
}

int64_t process_file_sync_all(void) {
    int64_t result = 0;
    /* A failed writer close can leave dirty content retained only by readers.
     * Visit each live content object once, including those survivors, rather
     * than retrying once per independent write description. */
    for (uint64_t i = 0; i < PROCESS_MAX_WRITE_FILES + PROCESS_MAX_READ_FILES; i++) {
        struct process_file_content *content=&file_contents[i];
        if (content->refs && content->dirty) {
            int64_t flushed=content_flush_observed(content,i,true);
            if(flushed<0)result=-1;
        }
    }
    return result;
}

struct pipe_io_wait {
    struct process *process;
    uint64_t fd, generation, handle, minimum;
    enum process_file_type type;
    bool writing;
};

static struct process_pipe *pipe_io_current(const struct pipe_io_wait *wait) {
    struct process_file *file = process_file_at(wait->process, wait->fd);
    if (!file || wait->generation == UINT64_MAX ||
        wait->process->file_generations[wait->fd] != wait->generation || file->type != wait->type ||
        (wait->writing && file->type == PROCESS_FILE_PIPE_DUPLEX ? file->aux_handle : file->handle) != wait->handle)
        return NULL;
    struct process_pipe *pipe=pipe_at(wait->handle);
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    if((file->type!=PROCESS_FILE_PIPE_READ && file->type!=PROCESS_FILE_PIPE_WRITE) ||
        !process_experimental_pipe_simple(pipe))return NULL;
#endif
    return pipe;
}

static bool pipe_read_ready(void *arg) {
    struct pipe_io_wait *wait = arg;
    struct process_pipe *pipe = pipe_io_current(wait);
    return process_should_exit_current() ||
        process_signal_pending_current() ||
        pipe == NULL ||
        pipe->size > 0 ||
        pipe->write_refs == 0;
}

static bool pipe_write_ready(void *arg) {
    struct pipe_io_wait *wait = arg;
    struct process_pipe *pipe = pipe_io_current(wait);
    return process_should_exit_current() ||
        process_signal_pending_current() ||
        pipe == NULL ||
        pipe->read_refs == 0 ||
        PROCESS_PIPE_CAPACITY-pipe->size >= wait->minimum;
}

int64_t process_file_pipe(struct process *process, uint64_t fds_out[2]) {
    if (process == NULL || fds_out == NULL) {
        return SRV_ERR_INVAL;
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
        return SRV_ERR_MFILE;
    }

    uint64_t handle = pipe_alloc();
    if (handle == 0) {
        return SRV_ERR_NFILE;
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
    fds_out[0] = read_index;
    fds_out[1] = write_index;
    return 0;
}

int64_t process_file_pipe_pair(struct process *process, uint64_t fds_out[2]) {
    if (process == NULL || fds_out == NULL) {
        return SRV_ERR_INVAL;
    }

    uint64_t first_index = PROCESS_MAX_OPEN_FILES;
    uint64_t second_index = PROCESS_MAX_OPEN_FILES;
    for (uint64_t i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        if (process->files[i].used) {
            continue;
        }
        if (first_index == PROCESS_MAX_OPEN_FILES) {
            first_index = i;
        } else {
            second_index = i;
            break;
        }
    }
    if (second_index == PROCESS_MAX_OPEN_FILES) {
        return SRV_ERR_MFILE;
    }

    uint64_t pipe_ab = pipe_alloc();
    if (pipe_ab == 0) {
        return SRV_ERR_NFILE;
    }
    uint64_t pipe_ba = pipe_alloc();
    if (pipe_ba == 0) {
        pipe_close(pipe_ab, false);
        pipe_close(pipe_ab, true);
        return SRV_ERR_NFILE;
    }

    struct process_file *first_file = &process->files[first_index];
    struct process_file *second_file = &process->files[second_index];
    uint8_t *first_bytes = (uint8_t *)first_file;
    uint8_t *second_bytes = (uint8_t *)second_file;
    for (uint64_t i = 0; i < sizeof(*first_file); i++) {
        first_bytes[i] = 0;
        second_bytes[i] = 0;
    }
    first_file->used = true;
    first_file->type = PROCESS_FILE_PIPE_DUPLEX;
    first_file->handle = pipe_ba;
    first_file->aux_handle = pipe_ab;
    second_file->used = true;
    second_file->type = PROCESS_FILE_PIPE_DUPLEX;
    second_file->handle = pipe_ab;
    second_file->aux_handle = pipe_ba;
    fds_out[0] = first_index;
    fds_out[1] = second_index;
    return 0;
}

static struct process_unix_listener *unix_listener_at(uint64_t handle) {
    if (handle == 0 || handle > PROCESS_MAX_UNIX_LISTENERS) {
        return NULL;
    }
    struct process_unix_listener *listener = &unix_listeners[handle - 1];
    return listener->used ? listener : NULL;
}

static struct process_unix_listener *unix_listener_for_path(const char *path) {
    for (uint64_t i = 0; i < PROCESS_MAX_UNIX_LISTENERS; i++) {
        if (unix_listeners[i].used &&
            unix_listeners[i].bound &&
            process_path_equal(unix_listeners[i].path, path)) {
            return &unix_listeners[i];
        }
    }
    return NULL;
}

static uint64_t unix_listener_pending_count(const struct process_unix_listener *listener) {
    uint64_t count = 0;
    if (listener == NULL) {
        return 0;
    }
    for (uint64_t i = 0; i < PROCESS_UNIX_ACCEPT_QUEUE; i++) {
        if (listener->queue[i].used) {
            count++;
        }
    }
    return count;
}

struct unix_accept_wait { struct process *process;uint64_t fd,generation,handle;struct process_unix_listener *listener; };
static bool unix_accept_current(const struct unix_accept_wait *wait) {
    struct process_file *file=process_file_at(wait->process,wait->fd);
    return file && file->type==PROCESS_FILE_UNIX_LISTENER &&
        wait->process->file_generations[wait->fd]==wait->generation && file->handle==wait->handle &&
        unix_listener_at(file->handle)==wait->listener && wait->listener->listening;
}
static bool unix_accept_ready(void *arg) {
    const struct unix_accept_wait *wait=arg;
    return !wait || !unix_accept_current(wait) || process_should_exit_current() ||
        process_signal_pending_current() || unix_listener_pending_count(wait->listener)!=0;
}

static void unix_pending_release(struct process_unix_pending *pending) {
    if (pending == NULL || !pending->used) {
        return;
    }
    struct process_file retired = pending->file;
    *pending = (struct process_unix_pending){0};
    process_file_release_entry(&retired);
}

static void unix_listener_release(uint64_t handle) {
    struct process_unix_listener *listener = unix_listener_at(handle);
    if (listener == NULL) {
        return;
    }
    if(--listener->refs) return;
    /* Keep the slot reserved while releasing pending endpoints, but prevent
     * new connects from finding this listener during a blocking release. */
    listener->bound = false;
    listener->listening = false;
    for (uint64_t i = 0; i < PROCESS_UNIX_ACCEPT_QUEUE; i++) {
        unix_pending_release(&listener->queue[i]);
    }
    uint8_t *bytes = (uint8_t *)listener;
    for (uint64_t i = 0; i < sizeof(*listener); i++) {
        bytes[i] = 0;
    }
    scheduler_wake_all(&unix_wait_queue);
    process_file_poll_wake();
}

static bool unix_fill_duplex_pair(struct process_file *first_file, struct process_file *second_file) {
    if (first_file == NULL || second_file == NULL) {
        return false;
    }
    uint64_t pipe_ab = pipe_alloc();
    if (pipe_ab == 0) {
        return false;
    }
    uint64_t pipe_ba = pipe_alloc();
    if (pipe_ba == 0) {
        pipe_close(pipe_ab, false);
        pipe_close(pipe_ab, true);
        return false;
    }
    uint8_t *first_bytes = (uint8_t *)first_file;
    uint8_t *second_bytes = (uint8_t *)second_file;
    for (uint64_t i = 0; i < sizeof(*first_file); i++) {
        first_bytes[i] = 0;
        second_bytes[i] = 0;
    }
    first_file->used = true;
    first_file->type = PROCESS_FILE_PIPE_DUPLEX;
    first_file->handle = pipe_ba;
    first_file->aux_handle = pipe_ab;
    second_file->used = true;
    second_file->type = PROCESS_FILE_PIPE_DUPLEX;
    second_file->handle = pipe_ab;
    second_file->aux_handle = pipe_ba;
    return true;
}

int64_t process_unix_bind(struct process *process, const char *path) {
    if (process == NULL || path == NULL || path[0] == '\0') {
        return -1;
    }
    if (unix_listener_for_path(path) != NULL) {
        return -1;
    }

    uint64_t listener_index = PROCESS_MAX_UNIX_LISTENERS;
    for (uint64_t i = 0; i < PROCESS_MAX_UNIX_LISTENERS; i++) {
        if (!unix_listeners[i].used) {
            listener_index = i;
            break;
        }
    }
    if (listener_index == PROCESS_MAX_UNIX_LISTENERS) {
        return -1;
    }

    uint64_t fd_index = PROCESS_MAX_OPEN_FILES;
    for (uint64_t i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        if (!process->files[i].used) {
            fd_index = i;
            break;
        }
    }
    if (fd_index == PROCESS_MAX_OPEN_FILES) {
        return -1;
    }

    struct process_unix_listener *listener = &unix_listeners[listener_index];
    uint8_t *listener_bytes = (uint8_t *)listener;
    for (uint64_t i = 0; i < sizeof(*listener); i++) {
        listener_bytes[i] = 0;
    }
    if (!copy_process_path(listener->path, sizeof(listener->path), path)) {
        return -1;
    }
    listener->used = true;
    listener->refs = 1;
    listener->bound = true;
    listener->backlog = 1;

    struct process_file *file = &process->files[fd_index];
    uint8_t *file_bytes = (uint8_t *)file;
    for (uint64_t i = 0; i < sizeof(*file); i++) {
        file_bytes[i] = 0;
    }
    file->used = true;
    file->type = PROCESS_FILE_UNIX_LISTENER;
    file->handle = listener_index + 1;
    (void)copy_process_path(file->path, sizeof(file->path), path);
    return (int64_t)(fd_index);
}

int64_t process_unix_listen(struct process *process, uint64_t fd, uint64_t backlog) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL || file->type != PROCESS_FILE_UNIX_LISTENER) {
        return -1;
    }
    struct process_unix_listener *listener = unix_listener_at(file->handle);
    if (listener == NULL || !listener->bound) {
        return -1;
    }
    if (backlog == 0) {
        backlog = 1;
    }
    if (backlog > PROCESS_UNIX_ACCEPT_QUEUE) {
        backlog = PROCESS_UNIX_ACCEPT_QUEUE;
    }
    listener->backlog = backlog;
    listener->listening = true;
    process_file_poll_wake();
    return 0;
}

int64_t process_unix_connect(struct process *process, const char *path) {
    if (process == NULL || path == NULL || path[0] == '\0') {
        return -1;
    }
    struct process_unix_listener *listener = unix_listener_for_path(path);
    if (listener == NULL || !listener->listening) {
        return -1;
    }
    if (unix_listener_pending_count(listener) >= listener->backlog) {
        return SRV_ERR_AGAIN;
    }

    struct process_unix_pending *pending = NULL;
    for (uint64_t i = 0; i < PROCESS_UNIX_ACCEPT_QUEUE; i++) {
        if (!listener->queue[i].used) {
            pending = &listener->queue[i];
            break;
        }
    }
    if (pending == NULL) {
        return SRV_ERR_AGAIN;
    }

    uint64_t fd_index = PROCESS_MAX_OPEN_FILES;
    for (uint64_t i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        if (!process->files[i].used) {
            fd_index = i;
            break;
        }
    }
    if (fd_index == PROCESS_MAX_OPEN_FILES) {
        return -1;
    }

    if (!unix_fill_duplex_pair(&process->files[fd_index], &pending->file)) {
        return -1;
    }
    pending->used = true;
    scheduler_wake_all(&unix_wait_queue);
    process_file_poll_wake();
    return (int64_t)(fd_index);
}

int64_t process_unix_accept(struct process *process, uint64_t fd) {
    uint64_t irq=process_irq_save();
    struct process_file *file=process_file_at(process,fd);
    if(!file || file->type!=PROCESS_FILE_UNIX_LISTENER){process_irq_restore(irq);return SRV_ERR_BADF;}
    struct process_unix_listener *listener=unix_listener_at(file->handle);
    if(!listener || !listener->listening){process_irq_restore(irq);return SRV_ERR_INVAL;}
    struct unix_accept_wait wait={process,fd,process->file_generations[fd],file->handle,listener};
    process_irq_restore(irq);
    for(;;) {
        irq=process_irq_save();
        if(!unix_accept_current(&wait)){process_irq_restore(irq);return SRV_ERR_BADF;}
        if(process_should_exit_current()){process_irq_restore(irq);return SRV_ERR_INTR;}
        struct process_unix_pending *pending=NULL;
        for(unsigned i=0;i<PROCESS_UNIX_ACCEPT_QUEUE;i++)if(listener->queue[i].used){pending=&listener->queue[i];break;}
        if(pending) {
            uint64_t index=PROCESS_MAX_OPEN_FILES;
            for(uint64_t i=0;i<PROCESS_MAX_OPEN_FILES;i++)if(!process->files[i].used && process->file_generations[i]!=UINT64_MAX){index=i;break;}
            if(index==PROCESS_MAX_OPEN_FILES){process_irq_restore(irq);return SRV_ERR_MFILE;}
            /* Identity, queue dequeue and new descriptor publication are one
             * bounded IF0 commit; no stale listener pointer survives a wait. */
            process->files[index]=pending->file;*pending=(struct process_unix_pending){0};
            process_file_poll_wake();process_irq_restore(irq);return (int64_t)index;
        }
        if(process_file_nonblocking(process,fd)){process_irq_restore(irq);return SRV_ERR_AGAIN;}
        if(process_signal_pending_current()){process_irq_restore(irq);return SRV_ERR_INTR;}
        process_irq_restore(irq);
        if(!scheduler_wait(&unix_wait_queue,unix_accept_ready,&wait))scheduler_yield();
    }
}

int64_t process_unix_unlink(const char *path) {
    struct process_unix_listener *listener = unix_listener_for_path(path);
    if (listener == NULL) {
        return -1;
    }
    listener->bound = false;
    listener->path[0] = '\0';
    process_file_poll_wake();
    return 0;
}

bool process_file_pipe_io_safe(struct process *process, uint64_t fd, uint64_t length, bool writing) {
    struct process_file *file=process_file_at(process,fd);
    if(!file || length>SRV_PIPE_BUF || !process_file_nonblocking(process,fd))return false;
    if(file->type!=PROCESS_FILE_PIPE_DUPLEX &&
       file->type!=(writing?PROCESS_FILE_PIPE_WRITE:PROCESS_FILE_PIPE_READ))return false;
    struct process_pipe *pipe=pipe_at(writing && file->type==PROCESS_FILE_PIPE_DUPLEX?file->aux_handle:file->handle);
    if(!pipe)return false;
    /* Existing byte operations cannot wait on these descriptors. Keep signal
     * dispatch and descriptor release outside an owned storage service window.
     * Called with interrupts disabled; state cannot change before the copy. */
    if(writing)return pipe->read_refs!=0;
    for(unsigned i=0;i<PROCESS_PIPE_RIGHTS_QUEUE;i++)if(pipe->rights[i].used)return false;
    return true;
}

/* Commit bytes and detach the records they carry before releasing any files.
 * A prefix read still advances offsets even when it detaches no records. */
static void pipe_commit_read(struct process_pipe *pipe, uint64_t count,
    struct process_pipe_rights *retired, unsigned records) {
    pipe->read_pos = (pipe->read_pos + count) % PROCESS_PIPE_CAPACITY;
    pipe->size -= count;
    if (records) {
        for (unsigned i = 0; i < records; i++) retired[i] = pipe->rights[i];
        for (unsigned i = 0; i + records < PROCESS_PIPE_RIGHTS_QUEUE; i++)
            pipe->rights[i] = pipe->rights[i + records];
        for (unsigned i = PROCESS_PIPE_RIGHTS_QUEUE - records; i < PROCESS_PIPE_RIGHTS_QUEUE; i++)
            pipe->rights[i] = (struct process_pipe_rights){0};
    }
    for (unsigned i = 0; i < PROCESS_PIPE_RIGHTS_QUEUE && pipe->rights[i].used; i++)
        pipe->rights[i].offset -= count;
}

static int64_t pipe_read_common(struct process *process, uint64_t fd, uint8_t *buffer,
    uint64_t length, bool dontwait, process_pipe_copyout copyout, void *context) {
    uint64_t irq = process_irq_save();
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL ||
        (file->type != PROCESS_FILE_PIPE_READ && file->type != PROCESS_FILE_PIPE_DUPLEX) ||
        (buffer == NULL && copyout == NULL && length != 0)) {
        process_irq_restore(irq);
        return -1;
    }
    if (length == 0) { process_irq_restore(irq); return 0; }
    uint64_t generation = process->file_generations[fd], handle = file->handle;
    struct pipe_io_wait wait = {process, fd, generation, handle, 0, file->type, false};
    struct process_pipe_rights *retired = NULL;
    for (;;) {
        struct process_pipe *pipe = generation == UINT64_MAX ? pipe_at(handle) : pipe_io_current(&wait);
        if (!pipe) {
            process_irq_restore(irq);
            if (retired) kfree(retired);
            return SRV_ERR_BADF;
        }
        if (pipe->size || !pipe->write_refs) {
            uint64_t count = length < pipe->size ? length : pipe->size;
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
            if(file->type!=PROCESS_FILE_PIPE_READ || !process_experimental_pipe_simple(pipe)) {
                process_irq_restore(irq);return SRV_ERR_NOTSUP;
            }
            uint64_t first=PROCESS_PIPE_CAPACITY-pipe->read_pos;if(first>count)first=count;
            if(count) {
                if(copyout) {
                    if(!copyout(context,pipe->buffer+pipe->read_pos,first,pipe->buffer,count-first)) {
                        process_irq_restore(irq);return SRV_ERR_FAULT;
                    }
                } else {copy_bytes(buffer,pipe->buffer+pipe->read_pos,first);copy_bytes(buffer+first,pipe->buffer,count-first);}
                pipe->read_pos=(pipe->read_pos+count)%PROCESS_PIPE_CAPACITY;pipe->size-=count;
                scheduler_wake_all(&pipe_wait_queue);process_file_poll_wake();
            }
            process_irq_restore(irq);return (int64_t)count;
#else
            unsigned records = 0;
            while (records < PROCESS_PIPE_RIGHTS_QUEUE && pipe->rights[records].used &&
                pipe->rights[records].offset < count) records++;
            if (records && !retired) {
                if (generation == UINT64_MAX) { process_irq_restore(irq); return SRV_ERR_BADF; }
                process_irq_restore(irq);
                retired = kmalloc(sizeof(*retired) * PROCESS_PIPE_RIGHTS_QUEUE);
                if (!retired) return SRV_ERR_NOMEM;
                irq = process_irq_save();
                continue;
            }
            uint64_t first = PROCESS_PIPE_CAPACITY - pipe->read_pos;
            if (first > count) first = count;
            if (count) {
                if (copyout) {
                    if (!copyout(context, pipe->buffer + pipe->read_pos, first,
                        pipe->buffer, count - first)) {
                        process_irq_restore(irq);
                        if (retired) kfree(retired);
                        return SRV_ERR_FAULT;
                    }
                } else {
                    copy_bytes(buffer, pipe->buffer + pipe->read_pos, first);
                    copy_bytes(buffer + first, pipe->buffer, count - first);
                }
                pipe_commit_read(pipe, count, retired, records);
                scheduler_wake_all(&pipe_wait_queue);
                process_file_poll_wake();
            }
            process_irq_restore(irq);
            for (unsigned i = 0; i < records; i++) pipe_rights_release(&retired[i]);
            if (retired) kfree(retired);
            if (records) pipe_rights_collect();
            return (int64_t)count;
#endif
        }
        if (process_should_exit_current()) {
            process_irq_restore(irq);
            if (retired) kfree(retired);
            return -1;
        }
        if (dontwait || process_file_nonblocking(process, fd)) {
            process_irq_restore(irq);
            if (retired) kfree(retired);
            return SRV_ERR_AGAIN;
        }
        if (generation == UINT64_MAX) {
            process_irq_restore(irq);
            if (retired) kfree(retired);
            return SRV_ERR_BADF;
        }
        process_irq_restore(irq);
        bool woke = scheduler_wait(&pipe_wait_queue, pipe_read_ready, &wait);
        irq = process_irq_save();
        if (!pipe_io_current(&wait)) {
            process_irq_restore(irq);
            if (retired) kfree(retired);
            return SRV_ERR_BADF;
        }
        if (process_signal_pending_current()) {
            process_irq_restore(irq);
            if (retired) kfree(retired);
            return SRV_ERR_INTR;
        }
        if (!woke) {
            process_irq_restore(irq);
            if (retired) kfree(retired);
            return SRV_ERR_INTR;
        }
    }
}

int64_t process_file_pipe_read_checked(struct process *process, uint64_t fd,
    uint64_t length, bool dontwait, process_pipe_copyout copyout, void *context) {
    if (!copyout) return SRV_ERR_INVAL;
    return pipe_read_common(process, fd, NULL, length, dontwait, copyout, context);
}

int64_t process_file_pipe_read_mode(struct process *process, uint64_t fd, uint8_t *buffer,
    uint64_t length, bool dontwait) {
    return pipe_read_common(process, fd, buffer, length, dontwait, NULL, NULL);
}

int64_t process_file_pipe_read(struct process *process, uint64_t fd, uint8_t *buffer, uint64_t length) {
    return process_file_pipe_read_mode(process, fd, buffer, length, false);
}

static int64_t pipe_broken_write(struct process *process,uint64_t done) {
    if(process==process_current()) (void)process_thread_signal(process_thread_self(),SRV_SIGNAL_PIPE);
    else (void)process_queue_signal_locked(process,SRV_SIGNAL_PIPE);
    return done?(int64_t)done:SRV_ERR_PIPE;
}

static int64_t pipe_write_common(struct process *process, uint64_t fd, const uint8_t *buffer,
    uint64_t length, bool dontwait, process_pipe_copyin copyin, void *context) {
    uint64_t irq = process_irq_save();
    int64_t result;
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL ||
        (file->type != PROCESS_FILE_PIPE_WRITE && file->type != PROCESS_FILE_PIPE_DUPLEX) ||
        (buffer == NULL && copyin == NULL && length != 0)) {
        result = -1;
        goto out;
    }
    struct process_pipe *pipe = pipe_at(file->type == PROCESS_FILE_PIPE_DUPLEX ? file->aux_handle : file->handle);
    if (pipe == NULL) { result = SRV_ERR_BADF; goto out; }
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    if(file->type!=PROCESS_FILE_PIPE_WRITE || !process_experimental_pipe_simple(pipe)){result=SRV_ERR_NOTSUP;goto out;}
#endif
    if (!length) { result = 0; goto out; }
    if (!pipe->read_refs) { result = pipe_broken_write(process, 0); goto out; }
    struct pipe_io_wait wait = {process, fd, process->file_generations[fd],
        file->type == PROCESS_FILE_PIPE_DUPLEX ? file->aux_handle : file->handle,
        length <= SRV_PIPE_BUF ? length : 1, file->type, true};
    uint64_t done = 0;
    while (done < length) {
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
        if(!process_experimental_pipe_simple(pipe)){result=done?(int64_t)done:SRV_ERR_NOTSUP;goto out;}
#endif
        while (PROCESS_PIPE_CAPACITY-pipe->size < wait.minimum && pipe->read_refs > 0) {
            if (process_should_exit_current()) {
                result = done > 0 ? (int64_t)done : -1;
                goto out;
            }
            if (dontwait || process_file_nonblocking(process,fd)) {
                result = done > 0 ? (int64_t)done : SRV_ERR_AGAIN;
                goto out;
            }
            pipe = pipe_io_current(&wait);
            if (!pipe) {
                result = done > 0 ? (int64_t)done : SRV_ERR_BADF;
                goto out;
            }
            process_irq_restore(irq);
            bool woke = scheduler_wait(&pipe_wait_queue, pipe_write_ready, &wait);
            irq = process_irq_save();
            pipe = pipe_io_current(&wait);
            if (!pipe) {
                result = done > 0 ? (int64_t)done : SRV_ERR_BADF;
                goto out;
            }
            if (process_signal_pending_current()) {
                result = done > 0 ? (int64_t)done : SRV_ERR_INTR;
                goto out;
            }
            if (!woke) break;
        }
        if (pipe->read_refs == 0) {
            result = pipe_broken_write(process,done);
            goto out;
        }

        uint64_t space = PROCESS_PIPE_CAPACITY - pipe->size;
        if (space < wait.minimum) {
            result = done > 0 ? (int64_t)done : SRV_ERR_INTR;
            goto out;
        }
        uint64_t remaining = length - done;
        uint64_t count = remaining < space ? remaining : space;
        uint64_t first = PROCESS_PIPE_CAPACITY - pipe->write_pos;
        if (first > count) first = count;
        if (copyin) {
            if (!copyin(context, done, pipe->buffer + pipe->write_pos, first,
                pipe->buffer, count - first)) {
                result = done ? (int64_t)done : SRV_ERR_FAULT;
                goto out;
            }
        } else {
            copy_bytes(pipe->buffer + pipe->write_pos, buffer + done, first);
            copy_bytes(pipe->buffer, buffer + done + first, count - first);
        }
        pipe->write_pos = (pipe->write_pos + count) % PROCESS_PIPE_CAPACITY;
        pipe->size += count;
        done += count;
        scheduler_wake_all(&pipe_wait_queue);
        process_file_poll_wake();
    }
    result = (int64_t)done;
out:
    process_irq_restore(irq);
    return result;
}

int64_t process_file_pipe_write_checked(struct process *process, uint64_t fd,
    uint64_t length, bool dontwait, process_pipe_copyin copyin, void *context) {
    if (!copyin) return SRV_ERR_INVAL;
    return pipe_write_common(process, fd, NULL, length, dontwait, copyin, context);
}

int64_t process_file_pipe_write_mode(struct process *process, uint64_t fd, const uint8_t *buffer,
    uint64_t length, bool dontwait) {
    return pipe_write_common(process, fd, buffer, length, dontwait, NULL, NULL);
}

struct scheduler_wait_queue *process_file_poll_wait_queue(void) {
    return &fd_poll_wait_queue;
}

void process_file_poll_wake(void) {
    scheduler_wake_all(&fd_poll_wait_queue);
}

int64_t process_file_pipe_write(struct process *process, uint64_t fd, const uint8_t *buffer, uint64_t length) {
    return process_file_pipe_write_mode(process, fd, buffer, length, false);
}

struct process_file_lock_wait {
    struct process *process;
    uint64_t fd,generation,start,end;
    struct process_file_content *content;
    int16_t type;
};
static bool file_lock_wait_current(const struct process_file_lock_wait *wait) {
    struct process_file_content *current=NULL;
    struct process_file *file=process_file_at(wait->process,wait->fd);
    return file && wait->process->file_generations[wait->fd]==wait->generation &&
        file_content_for_fd(file,&current) && current==wait->content;
}
static bool file_lock_wait_ready(void *arg) {
    const struct process_file_lock_wait *wait=arg;
    /* scheduler_wait evaluates this under IF0 before enrolling. Recheck the
     * identity before touching its content, then the condition which woke us. */
    return !wait || !file_lock_wait_current(wait) || process_should_exit_current() ||
        process_signal_pending_current() || wait->type==SRV_F_UNLCK ||
        file_lock_conflict(wait->content,wait->process->pid,wait->start,wait->end,wait->type)==NULL;
}

static bool file_watch_supported(struct process *process,int64_t fd,uint16_t events) {
    if(!events || (events&~(SRV_POLLIN|SRV_POLLOUT)))return false;
    struct process_file *file=process_file_at(process,(uint64_t)fd);
    if(!file)return false;
    /* Refuse other families before even resolving their shared status flags. */
    if(file->type==PROCESS_FILE_PIPE_READ) { if(events!=SRV_POLLIN)return false; }
    else if(file->type==PROCESS_FILE_PIPE_WRITE) { if(events!=SRV_POLLOUT)return false; }
    else if(file->type!=PROCESS_FILE_PIPE_DUPLEX && file->type!=PROCESS_FILE_PTY_MASTER && file->type!=PROCESS_FILE_PTY_SLAVE)return false;
    return process_file_nonblocking(process,fd);
}
int process_file_watch_capture(struct process *process,int64_t fd,uint16_t events,uint64_t *generation) {
    uint64_t irq=process_irq_save();int result=-1;
    if(generation && file_watch_supported(process,fd,events) && process->file_generations[fd]!=UINT64_MAX) {
        *generation=process->file_generations[fd];result=0;
    }
    process_irq_restore(irq);return result;
}
uint16_t process_file_watch_poll(struct process *process,int64_t fd,uint16_t events,uint64_t generation,bool deferred) {
    uint64_t irq=process_irq_save();uint16_t result=SRV_POLLNVAL;
    if(file_watch_supported(process,fd,events) && generation!=UINT64_MAX && process->file_generations[fd]==generation)
        result=deferred?0:process_file_poll(process,fd,events);
    process_irq_restore(irq);return result;
}
uint64_t process_fd_generation(struct process *process,uint64_t fd) {
    return process_file_at(process,fd)?process->file_generations[fd]:UINT64_MAX;
}
static void file_generation_invalidate(struct process *process,uint64_t fd) {
    if(process->file_generations[fd]!=UINT64_MAX)process->file_generations[fd]++;
    process_file_poll_wake();
    scheduler_wake_all(&file_lock_wait_queue);
    scheduler_wake_all(&unix_wait_queue);
}

uint16_t process_file_poll(struct process *process, int64_t fd, uint16_t events) {
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    struct process_file *file=fd>=0?process_file_at(process,(uint64_t)fd):NULL;
    if(!file)return SRV_POLLNVAL;
    if(file->type==PROCESS_FILE_STDIO) {
        if(file->handle==0)return events&SRV_POLLIN;
        return file->handle<=2?events&SRV_POLLOUT:SRV_POLLNVAL;
    }
    if(file->type!=PROCESS_FILE_PIPE_READ && file->type!=PROCESS_FILE_PIPE_WRITE)return SRV_POLLNVAL;
    struct process_pipe *pipe=pipe_at(file->handle);
    if(!process_experimental_pipe_simple(pipe))return SRV_POLLNVAL;
    if(file->type==PROCESS_FILE_PIPE_READ)return ((pipe->size?events&SRV_POLLIN:0)|(!pipe->write_refs?SRV_POLLHUP:0));
    return ((pipe->read_refs && pipe->size<PROCESS_PIPE_CAPACITY?events&SRV_POLLOUT:0)|(!pipe->read_refs?SRV_POLLERR:0));
#else
    if (process == NULL) {
        return SRV_POLLNVAL;
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
        if ((events & SRV_POLLOUT) != 0 && !write->content->failed) {
            revents |= SRV_POLLOUT;
        }
        if (write->content->failed) {
            revents |= SRV_POLLERR;
        }
        return revents;
    }

    if(file->type==PROCESS_FILE_PTY_MASTER || file->type==PROCESS_FILE_PTY_SLAVE)
        return pty_poll(file->handle,file->type==PROCESS_FILE_PTY_MASTER,events);
    if (file->type == PROCESS_FILE_PIPE_DUPLEX) {
        struct process_pipe *read_pipe = pipe_at(file->handle);
        struct process_pipe *write_pipe = pipe_at(file->aux_handle);
        if (read_pipe == NULL || write_pipe == NULL) {
            return SRV_POLLNVAL;
        }

        uint16_t revents = 0;
        if ((events & SRV_POLLIN) != 0 && read_pipe->size > 0) {
            revents |= SRV_POLLIN;
        }
        if (read_pipe->write_refs == 0) {
            revents |= SRV_POLLHUP;
        }
        if ((events & SRV_POLLOUT) != 0 &&
            write_pipe->read_refs != 0 &&
            write_pipe->size < PROCESS_PIPE_CAPACITY) {
            revents |= SRV_POLLOUT;
        }
        if (write_pipe->read_refs == 0) {
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

    if (file->type == PROCESS_FILE_UNIX_LISTENER) {
        struct process_unix_listener *listener = unix_listener_at(file->handle);
        if (listener == NULL) {
            return SRV_POLLNVAL;
        }
        uint16_t revents = 0;
        if ((events & SRV_POLLIN) != 0 && unix_listener_pending_count(listener) != 0) {
            revents |= SRV_POLLIN;
        }
        return revents;
    }

    return SRV_POLLNVAL;
#endif
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
            return (int64_t)(i);
        }
    }

    return -1;
}

static bool process_file_clone_entry(struct process_file *target, const struct process_file *source) {
    if (target == NULL || source == NULL || !source->used) {
        return false;
    }

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
        return true;
    }

    if(source->type==PROCESS_FILE_UNIX_LISTENER) {
        struct process_unix_listener *listener=unix_listener_at(source->handle);
        if(!listener || listener->refs==UINT64_MAX) return false;
        listener->refs++;*target=*source;return true;
    }
    if(source->type==PROCESS_FILE_PTY_MASTER || source->type==PROCESS_FILE_PTY_SLAVE) {
        if(!pty_ref(source->handle,source->type==PROCESS_FILE_PTY_MASTER)) return false;
        *target=*source;return true;
    }
    if (source->type == PROCESS_FILE_PIPE_DUPLEX) {
        if (pipe_at(source->handle) == NULL || pipe_at(source->aux_handle) == NULL) {
            return false;
        }
        uint8_t *bytes = (uint8_t *)target;
        for (uint64_t i = 0; i < sizeof(*target); i++) {
            bytes[i] = 0;
        }
        pipe_ref(source->handle, false);
        pipe_ref(source->aux_handle, true);
        target->used = true;
        target->type = source->type;
        target->handle = source->handle;
        target->aux_handle = source->aux_handle;
        target->flags = source->flags;
        return true;
    }

    if (source->type == PROCESS_FILE_PIPE_READ || source->type == PROCESS_FILE_PIPE_WRITE) {
        uint64_t handle = source->handle;
        bool write_end = source->type == PROCESS_FILE_PIPE_WRITE;
        if (pipe_at(handle) == NULL) {
            return false;
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
        return true;
    }

    if (source->type == PROCESS_FILE_VFS_WRITE) {
        uint64_t handle = source->handle;
        if (write_file_at(handle) == NULL) {
            return false;
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
        return true;
    }

    if (source->type == PROCESS_FILE_NET_LISTENER ||
        source->type == PROCESS_FILE_NET_CONNECTION ||
        source->type == PROCESS_FILE_NET_UDP) {
        if (net_hold(source->handle) < 0) {
            return false;
        }
        uint8_t *bytes = (uint8_t *)target;
        for (uint64_t i = 0; i < sizeof(*target); i++) {
            bytes[i] = 0;
        }
        target->used = true;
        target->type = source->type;
        target->handle = source->handle;
        target->flags = source->flags;
        return true;
    }

    if (source->type != PROCESS_FILE_VFS || source->handle == 0 || source->path[0] == '\0') {
        return false;
    }
    struct process_read_file *read = read_file_at(source->handle);
    if (read == NULL) {
        return false;
    }
    read_file_ref(source->handle);

    uint8_t *bytes = (uint8_t *)target;
    for (uint64_t i = 0; i < sizeof(*target); i++) {
        bytes[i] = 0;
    }
    target->used = true;
    target->type = PROCESS_FILE_VFS;
    target->node = read->node;
    /* Shared content is reached through the retained description. */
    target->handle = source->handle;
    target->flags = source->flags;
    (void)copy_process_path(target->path, sizeof(target->path), source->path);
    return true;
}

static void process_file_release_entry(struct process_file *file) {
    if (file == NULL || !file->used) {
        return;
    }

    if ((file->type == PROCESS_FILE_NET_LISTENER ||
            file->type == PROCESS_FILE_NET_CONNECTION ||
            file->type == PROCESS_FILE_NET_UDP)) {
        (void)net_close(file->handle);
    } else if (file->type == PROCESS_FILE_VFS) {
        read_file_close(file->handle);
    } else if (file->type == PROCESS_FILE_PIPE_READ) {
        pipe_close(file->handle, false);
    } else if (file->type == PROCESS_FILE_PIPE_WRITE) {
        pipe_close(file->handle, true);
    } else if (file->type == PROCESS_FILE_PTY_MASTER || file->type == PROCESS_FILE_PTY_SLAVE) {
        pty_close(file->handle,file->type==PROCESS_FILE_PTY_MASTER);
    } else if (file->type == PROCESS_FILE_PIPE_DUPLEX) {
        pipe_close(file->handle, false);
        pipe_close(file->aux_handle, true);
    } else if (file->type == PROCESS_FILE_UNIX_LISTENER) {
        unix_listener_release(file->handle);
    } else if (file->type == PROCESS_FILE_VFS_WRITE) {
        (void)write_file_close(file->handle);
    }

    uint8_t *bytes = (uint8_t *)file;
    for (uint64_t i = 0; i < sizeof(*file); i++) {
        bytes[i] = 0;
    }
}

static int64_t process_file_dup_into(struct process *process, const struct process_file *source, uint64_t target_index) {
    if (process == NULL || source == NULL || target_index >= PROCESS_MAX_OPEN_FILES) {
        return -1;
    }

    if (!process_file_clone_entry(&process->files[target_index], source)) {
        return -1;
    }
    return (int64_t)(target_index);
}

int64_t process_file_dup_min(struct process *process, uint64_t old_fd, uint64_t minimum, bool cloexec) {
    if(minimum>=PROCESS_MAX_OPEN_FILES) return SRV_ERR_INVAL;
    uint64_t irq=process_irq_save();
    struct process_file *source=process_file_at(process,old_fd);
    int64_t result=SRV_ERR_BADF;
    if(source) {
        result=SRV_ERR_MFILE;
        for(uint64_t i=minimum;i<PROCESS_MAX_OPEN_FILES;i++) if(!process->files[i].used) {
            result=process_file_dup_into(process,source,i);
            if(result>=0) {
                process->files[i].flags&=~SRV_FD_CLOEXEC;
                if(cloexec) process->files[i].flags|=SRV_FD_CLOEXEC;
            } else result=SRV_ERR_NOMEM;
            break;
        }
    }
    process_irq_restore(irq);return result;
}

int64_t process_file_dup(struct process *process, uint64_t old_fd) {
    return process_file_dup_min(process,old_fd,0,false);
}

static int64_t process_file_dup_replace(struct process *process, uint64_t old_fd, uint64_t new_fd, bool cloexec) {
    uint64_t irq=process_irq_save();
    struct process_file *source=process_file_at(process,old_fd);
    if(!source || new_fd>=PROCESS_MAX_OPEN_FILES) { process_irq_restore(irq);return SRV_ERR_BADF; }
    if(old_fd==new_fd) { process_irq_restore(irq);return (int64_t)new_fd; }
    struct process_file_content *target_content=NULL;
    if(file_content_for_fd(process_file_at(process,new_fd),&target_content) && target_content->writeback.active) {
        process_irq_restore(irq);return SRV_ERR_BUSY;
    }
    struct process_file replacement={0};
    if(!process_file_clone_entry(&replacement,source)) { process_irq_restore(irq);return SRV_ERR_NOMEM; }
    replacement.flags&=~SRV_FD_CLOEXEC;
    if(cloexec) replacement.flags|=SRV_FD_CLOEXEC;
    struct process_file retired=process->files[new_fd];
    process->files[new_fd]=replacement;file_generation_invalidate(process,new_fd);
    process_irq_restore(irq);
    if(retired.used) {
        (void)process_file_close_entry(process,&retired);
        pipe_rights_collect();
    }
    return (int64_t)new_fd;
}

int64_t process_file_dup2(struct process *process, uint64_t old_fd, uint64_t new_fd) {
    return process_file_dup_replace(process,old_fd,new_fd,false);
}

int64_t process_file_dup3(struct process *process, uint64_t old_fd, uint64_t new_fd, uint64_t flags) {
    if(old_fd==new_fd || (flags&~SRV_FD_CLOEXEC)) return SRV_ERR_INVAL;
    return process_file_dup_replace(process,old_fd,new_fd,(flags&SRV_FD_CLOEXEC)!=0);
}

static const struct process_file *process_file_source_for_fd(struct process *process,
    uint64_t fd, struct process_file *unused) {
    (void)unused;return process_file_at(process,fd);
}

static int64_t process_file_send_rights_locked(struct process *process, uint64_t fd, const int32_t *fds, uint64_t count) {
    if (process == NULL || fds == NULL || count == 0 || count > SRV_RIGHTS_MAX) {
        return -1;
    }

    struct process_file *endpoint = process_file_at(process, fd);
    if (endpoint == NULL ||
        (endpoint->type != PROCESS_FILE_PIPE_WRITE && endpoint->type != PROCESS_FILE_PIPE_DUPLEX)) {
        return -1;
    }
    struct process_pipe *pipe = pipe_at(endpoint->type == PROCESS_FILE_PIPE_DUPLEX ? endpoint->aux_handle : endpoint->handle);
    if (pipe == NULL || pipe->read_refs == 0) {
        return -1;
    }

    struct process_pipe_rights *slot = NULL;
    for (uint64_t i = 0; i < PROCESS_PIPE_RIGHTS_QUEUE; i++) {
        if (!pipe->rights[i].used) {
            slot = &pipe->rights[i];
            break;
        }
    }
    if (slot == NULL) {
        return SRV_ERR_AGAIN;
    }

    uint8_t *slot_bytes = (uint8_t *)slot;
    for (uint64_t i = 0; i < sizeof(*slot); i++) {
        slot_bytes[i] = 0;
    }
    slot->used = true;
    slot->offset = pipe->size;

    for (uint64_t i = 0; i < count; i++) {
        if (fds[i] < 0) {
            pipe_rights_release(slot);
            return -1;
        }
        struct process_file stdio_source;
        const struct process_file *source = process_file_source_for_fd(process, (uint64_t)fds[i], &stdio_source);
        if (source == NULL || !process_file_clone_entry(&slot->files[i], source)) {
            pipe_rights_release(slot);
            return -1;
        }
        slot->count++;
    }

    pipe_rights_present = true;
    process_file_poll_wake();
    return 0;
}

int64_t process_file_send_rights(struct process *process, uint64_t fd, const int32_t *fds, uint64_t count) {
    uint64_t irq = process_irq_save();
    int64_t result = process_file_send_rights_locked(process, fd, fds, count);
    process_irq_restore(irq);
    return result;
}

/* Queued references keep the source alive while failed installations are
 * rolled back under IRQ exclusion. A negative cloexec preserves legacy flags. */
static int64_t pipe_rights_install(struct process *process,
    const struct process_file *source, int cloexec) {
    for (uint64_t fd = 0; fd < PROCESS_MAX_OPEN_FILES; fd++) {
        if (process->files[fd].used) continue;
        if (process_file_dup_into(process, source, fd) < 0) return SRV_ERR_MFILE;
        if (cloexec >= 0) {
            process->files[fd].flags &= ~SRV_FD_CLOEXEC;
            if (cloexec) process->files[fd].flags |= SRV_FD_CLOEXEC;
        }
        return (int64_t)fd;
    }
    return SRV_ERR_MFILE;
}

static void pipe_rights_rollback(struct process *process, const int32_t *fds, uint64_t count) {
    pipe_rights_gc_defer++;
    for (uint64_t i = 0; i < count; i++) (void)process_file_close(process, (uint64_t)fds[i]);
    pipe_rights_gc_defer--;
}

int64_t process_file_recv_rights_checked(struct process *process, uint64_t fd,
    uint64_t capacity, process_rights_copyout copyout, void *context) {
    if (!process || capacity > SRV_RIGHTS_MAX || !copyout) return SRV_ERR_INVAL;
    struct process_pipe_rights *retired = NULL;
    uint64_t irq = process_irq_save();
    struct process_file *endpoint = process_file_at(process, fd);
    if (!endpoint || (endpoint->type != PROCESS_FILE_PIPE_READ &&
        endpoint->type != PROCESS_FILE_PIPE_DUPLEX)) {
        process_irq_restore(irq);
        return SRV_ERR_BADF;
    }
    uint64_t generation = process->file_generations[fd], handle = endpoint->handle;
    if (generation == UINT64_MAX) { process_irq_restore(irq); return SRV_ERR_BADF; }
    for (;;) {
        endpoint = process_file_at(process, fd);
        struct process_pipe *pipe = endpoint && endpoint->handle == handle &&
            process->file_generations[fd] == generation &&
            (endpoint->type == PROCESS_FILE_PIPE_READ || endpoint->type == PROCESS_FILE_PIPE_DUPLEX)
            ? pipe_at(handle) : NULL;
        if (!pipe) {
            process_irq_restore(irq);
            if (retired) kfree(retired);
            return SRV_ERR_BADF;
        }
        if (!pipe->rights[0].used || pipe->rights[0].offset != 0) {
            bool copied = copyout(context, NULL, 0, 0);
            process_irq_restore(irq);
            if (retired) kfree(retired);
            return copied ? 0 : SRV_ERR_FAULT;
        }
        if (!retired) {
            process_irq_restore(irq);
            retired = kmalloc(sizeof(*retired));
            if (!retired) return SRV_ERR_NOMEM;
            irq = process_irq_save();
            continue;
        }
        struct process_pipe_rights *rights = &pipe->rights[0];
        uint64_t deliver = rights->count < capacity ? rights->count : capacity;
        uint64_t flags = deliver < rights->count ? SRV_MSG_CTRUNC : 0;
        int32_t delivered[SRV_RIGHTS_MAX] = {0};
        for (uint64_t i = 0; i < deliver; i++) {
            int64_t installed = pipe_rights_install(process, &rights->files[i], -1);
            if (installed < 0) {
                pipe_rights_rollback(process, delivered, i);
                process_irq_restore(irq);
                kfree(retired);
                return SRV_ERR_MFILE;
            }
            delivered[i] = (int32_t)installed;
        }
        if (!copyout(context, delivered, deliver, flags)) {
            pipe_rights_rollback(process, delivered, deliver);
            process_irq_restore(irq);
            kfree(retired);
            return SRV_ERR_FAULT;
        }
        *retired = pipe->rights[0];
        for (unsigned i = 1; i < PROCESS_PIPE_RIGHTS_QUEUE; i++)
            pipe->rights[i - 1] = pipe->rights[i];
        pipe->rights[PROCESS_PIPE_RIGHTS_QUEUE - 1] = (struct process_pipe_rights){0};
        scheduler_wake_all(&pipe_wait_queue);
        process_file_poll_wake();
        process_irq_restore(irq);
        pipe_rights_release(retired);
        kfree(retired);
        pipe_rights_collect();
        return (int64_t)deliver;
    }
}

/* A message wait watches the descriptor generation as well as the pipe. A
 * close/reuse wakes the waiter and cannot redirect it to the replacement. */
struct pipe_message_wait {
    struct process *process;
    uint64_t fd, generation, handle;
    bool writing, needs_rights;
    struct process_user_thread *thread;
};

static struct process_pipe *pipe_message_current(struct pipe_message_wait *wait) {
    struct process_file *file = process_file_at(wait->process, wait->fd);
    if (!file || file->type != PROCESS_FILE_PIPE_DUPLEX ||
        wait->process->file_generations[wait->fd] != wait->generation ||
        (wait->writing ? file->aux_handle : file->handle) != wait->handle) return NULL;
    return pipe_at(wait->handle);
}

static bool pipe_message_ready(void *arg) {
    struct pipe_message_wait *wait = arg;
    struct process_pipe *pipe = pipe_message_current(wait);
    struct process *process = wait->process;
    struct process_user_thread *thread = wait->thread;
    uint64_t pending = process->pending_signals;
    uint64_t mask = process->main_signal_mask;
    if (thread && thread->used && thread->owner == process) {
        pending |= thread->pending_signals;
        mask = thread->signal_mask;
    } else {
        pending |= process->main_pending_signals;
    }
    if (!pipe || process->killed || process->exiting || (pending & ~mask)) return true;
    if (!wait->writing) return pipe->size != 0 || pipe->write_refs == 0;
    if (!pipe->read_refs) return true;
    if (pipe->size == PROCESS_PIPE_CAPACITY) return false;
    return !wait->needs_rights || !pipe->rights[PROCESS_PIPE_RIGHTS_QUEUE - 1].used;
}

int64_t process_file_send_message(struct process *process, uint64_t fd,
    const uint8_t *buffer, uint64_t length, const struct srv_pipe_message *message) {
    if (!message || (!buffer && length) || length > SRV_PIPE_MESSAGE_MAX || message->count > SRV_RIGHTS_MAX ||
        (message->flags & ~(SRV_PIPE_MSG_DONTWAIT | SRV_PIPE_MSG_NOSIGNAL)) || (message->count && !length)) return SRV_ERR_INVAL;
    uint64_t irq = process_irq_save();
    struct process_file *file = process_file_at(process, fd);
    if (!file) { process_irq_restore(irq); return SRV_ERR_BADF; }
    if (file->type != PROCESS_FILE_PIPE_DUPLEX) { process_irq_restore(irq); return SRV_ERR_NOTSUP; }
    if (!length) { process_irq_restore(irq); return 0; }
    /* Do not retain rights across a wait. Revalidate each source when the
     * pipe becomes writable; a source-only close need not wake this waiter. */
    uint64_t source_generations[SRV_RIGHTS_MAX];
    for (uint64_t i = 0; i < message->count; i++) {
        if (message->fds[i] < 0 || !process_file_at(process, (uint64_t)message->fds[i])) {
            process_irq_restore(irq);
            return SRV_ERR_BADF;
        }
        source_generations[i] = process->file_generations[message->fds[i]];
        if (source_generations[i] == UINT64_MAX) {
            process_irq_restore(irq);
            return SRV_ERR_BADF;
        }
    }
    struct pipe_message_wait wait = {process, fd, process->file_generations[fd], file->aux_handle,
        true, message->count != 0, process_current_user_thread()};
    if (wait.generation == UINT64_MAX) { process_irq_restore(irq); return SRV_ERR_BADF; }
    for (;;) {
        struct process_pipe *pipe = pipe_message_current(&wait);
        if (!pipe) { process_irq_restore(irq); return SRV_ERR_BADF; }
        if (!pipe->read_refs) {
            process_irq_restore(irq);
            return (message->flags & SRV_PIPE_MSG_NOSIGNAL) ? SRV_ERR_PIPE : pipe_broken_write(process, 0);
        }
        if (pipe->size < PROCESS_PIPE_CAPACITY &&
            (!wait.needs_rights || !pipe->rights[PROCESS_PIPE_RIGHTS_QUEUE - 1].used)) {
            struct process_pipe_rights *rights = NULL;
            if (message->count) {
                for (unsigned i = 0; i < PROCESS_PIPE_RIGHTS_QUEUE; i++)
                    if (!pipe->rights[i].used) { rights = &pipe->rights[i]; break; }
                if (!rights) { process_irq_restore(irq); return SRV_ERR_AGAIN; }
                *rights = (struct process_pipe_rights){.used = true, .offset = pipe->size};
                for (uint64_t i = 0; i < message->count; i++) {
                    struct process_file *source = message->fds[i] < 0 ? NULL :
                        process_file_at(process, (uint64_t)message->fds[i]);
                    if (!source || process->file_generations[message->fds[i]] != source_generations[i] ||
                        !process_file_clone_entry(&rights->files[i], source)) {
                        pipe_rights_release(rights);
                        process_irq_restore(irq);
                        return SRV_ERR_BADF;
                    }
                    rights->count++;
                }
            }
            if (rights) pipe_rights_present = true;
            uint64_t count = PROCESS_PIPE_CAPACITY - pipe->size;
            if (count > length) count = length;
            uint64_t first = PROCESS_PIPE_CAPACITY - pipe->write_pos;
            if (first > count) first = count;
            copy_bytes(pipe->buffer + pipe->write_pos, buffer, first);
            copy_bytes(pipe->buffer, buffer + first, count - first);
            pipe->write_pos = (pipe->write_pos + count) % PROCESS_PIPE_CAPACITY;
            pipe->size += count;
            scheduler_wake_all(&pipe_wait_queue);
            process_file_poll_wake();
            process_irq_restore(irq);
            return (int64_t)count;
        }
        if ((message->flags & SRV_PIPE_MSG_DONTWAIT) || process_file_nonblocking(process, fd)) {
            process_irq_restore(irq); return SRV_ERR_AGAIN;
        }
        process_irq_restore(irq);
        bool woke = scheduler_wait(&pipe_wait_queue, pipe_message_ready, &wait);
        irq = process_irq_save();
        if (!pipe_message_current(&wait)) { process_irq_restore(irq); return SRV_ERR_BADF; }
        if (process_signal_pending_current()) { process_irq_restore(irq); return SRV_ERR_INTR; }
        if (!woke || process_should_exit_current()) { process_irq_restore(irq); return SRV_ERR_INTR; }
    }
}

int64_t process_file_recv_message_checked(struct process *process, uint64_t fd,
    uint8_t *buffer, uint64_t length, struct srv_pipe_message *message,
    process_message_copyout copyout, void *context) {
    if (!message || (!buffer && length) || length > SRV_PIPE_MESSAGE_MAX || message->count > SRV_RIGHTS_MAX ||
        (message->flags & ~(SRV_PIPE_MSG_DONTWAIT | SRV_PIPE_MSG_CLOEXEC | SRV_PIPE_MSG_PEEK))) return SRV_ERR_INVAL;
    uint32_t capacity = message->count, request = message->flags;
    struct process_pipe_rights *retired = NULL;
    int64_t failure;
    uint64_t irq = process_irq_save();
    struct process_file *file = process_file_at(process, fd);
    if (!file) { failure = SRV_ERR_BADF; goto fail; }
    if (file->type != PROCESS_FILE_PIPE_DUPLEX) { failure = SRV_ERR_NOTSUP; goto fail; }
    struct pipe_message_wait wait = {process, fd, process->file_generations[fd], file->handle,
        false, false, process_current_user_thread()};
    if (wait.generation == UINT64_MAX) { failure = SRV_ERR_BADF; goto fail; }
    for (;;) {
        struct process_pipe *pipe = pipe_message_current(&wait);
        if (!pipe) { failure = SRV_ERR_BADF; goto fail; }
        if (!length || pipe->size || !pipe->write_refs) {
            uint64_t count = length < pipe->size ? length : pipe->size;
            unsigned records = 0;
            if (count && pipe->rights[0].used) {
                if (pipe->rights[0].offset) {
                    if (count > pipe->rights[0].offset) count = pipe->rights[0].offset;
                } else {
                    while (records < PROCESS_PIPE_RIGHTS_QUEUE && pipe->rights[records].used &&
                        pipe->rights[records].offset == 0) records++;
                    if (records < PROCESS_PIPE_RIGHTS_QUEUE && pipe->rights[records].used &&
                        count > pipe->rights[records].offset) count = pipe->rights[records].offset;
                }
            }
            /* Detach up to the bounded queue size into heap storage. Never
             * release arbitrary final file references inside the transaction. */
            if (records && !(request & SRV_PIPE_MSG_PEEK) && !retired) {
                process_irq_restore(irq);
                retired = kmalloc(sizeof(*retired) * PROCESS_PIPE_RIGHTS_QUEUE);
                if (!retired) return SRV_ERR_NOMEM;
                irq = process_irq_save();
                continue;
            }
            struct srv_pipe_message result = {0};
            for (unsigned r = 0; r < records; r++) {
                struct process_pipe_rights *rights = &pipe->rights[r];
                for (uint64_t i = 0; i < rights->count; i++) {
                    if (result.count == capacity) {
                        result.flags |= SRV_PIPE_MSG_CTRUNC;
                        continue;
                    }
                    int64_t installed = pipe_rights_install(process, &rights->files[i],
                        (request & SRV_PIPE_MSG_CLOEXEC) != 0);
                    if (installed < 0) {
                        pipe_rights_rollback(process, result.fds, result.count);
                        failure = SRV_ERR_MFILE;
                        goto fail;
                    }
                    result.fds[result.count++] = (int32_t)installed;
                }
            }
            uint64_t first = PROCESS_PIPE_CAPACITY - pipe->read_pos;
            if (first > count) first = count;
            if (count) {
                copy_bytes(buffer, pipe->buffer + pipe->read_pos, first);
                copy_bytes(buffer + first, pipe->buffer, count - first);
            }
            if (copyout && !copyout(context, buffer, count, &result)) {
                pipe_rights_rollback(process, result.fds, result.count);
                failure = SRV_ERR_FAULT;
                goto fail;
            }
            *message = result;
            if (!(request & SRV_PIPE_MSG_PEEK) && count) {
                pipe_commit_read(pipe, count, retired, records);
                scheduler_wake_all(&pipe_wait_queue);
                process_file_poll_wake();
            }
            process_irq_restore(irq);
            if (!(request & SRV_PIPE_MSG_PEEK))
                for (unsigned i = 0; i < records; i++) pipe_rights_release(&retired[i]);
            if (retired) kfree(retired);
            if (!(request & SRV_PIPE_MSG_PEEK) && records) pipe_rights_collect();
            return (int64_t)count;
        }
        if ((request & SRV_PIPE_MSG_DONTWAIT) || process_file_nonblocking(process, fd)) {
            failure = SRV_ERR_AGAIN;
            goto fail;
        }
        process_irq_restore(irq);
        bool woke = scheduler_wait(&pipe_wait_queue, pipe_message_ready, &wait);
        irq = process_irq_save();
        if (!pipe_message_current(&wait)) { failure = SRV_ERR_BADF; goto fail; }
        if (process_signal_pending_current()) { failure = SRV_ERR_INTR; goto fail; }
        if (!woke || process_should_exit_current()) { failure = SRV_ERR_INTR; goto fail; }
    }
fail:
    process_irq_restore(irq);
    if (retired) kfree(retired);
    return failure;
}

/* Status belongs to an endpoint's open description; CLOEXEC belongs to an FD. */
static uint64_t *endpoint_status_flags(struct process_file *file) {
    if(file->type==PROCESS_FILE_PIPE_READ || file->type==PROCESS_FILE_PIPE_WRITE || file->type==PROCESS_FILE_PIPE_DUPLEX) {
        struct process_pipe *pipe=pipe_at(file->handle);
        return !pipe?NULL:file->type==PROCESS_FILE_PIPE_WRITE?&pipe->write_flags:&pipe->read_flags;
    }
    if(file->type==PROCESS_FILE_UNIX_LISTENER) {
        struct process_unix_listener *listener=unix_listener_at(file->handle);
        return listener?&listener->status_flags:NULL;
    }
    return NULL;
}
int64_t process_file_get_flags(struct process *process, uint64_t fd) {
    struct process_file *file=process_file_at(process,fd);
    if(!file) return SRV_ERR_BADF;
    if(file->type==PROCESS_FILE_VFS) {
        struct process_read_file *read=read_file_at(file->handle);
        return read?(int64_t)(SRV_FD_READ|read->status_flags):SRV_ERR_BADF;
    }
    if(file->type==PROCESS_FILE_VFS_WRITE) {
        struct process_write_file *write=write_file_at(file->handle);
        if(!write) return SRV_ERR_BADF;
        return ((write->open_flags&SRV_OPEN_READ)?SRV_FD_READ:0) |
            ((write->open_flags&SRV_OPEN_WRITE)?SRV_FD_WRITE:0) |
            ((write->open_flags&SRV_OPEN_APPEND)?SRV_FD_APPEND:0) |
            ((write->open_flags&SRV_OPEN_NONBLOCK)?SRV_FD_NONBLOCK:0);
    }
    uint64_t access=file->type==PROCESS_FILE_STDIO?(file->handle==0?SRV_FD_READ:SRV_FD_WRITE):
        file->type==PROCESS_FILE_PIPE_READ?SRV_FD_READ:
        file->type==PROCESS_FILE_PIPE_WRITE?SRV_FD_WRITE:SRV_FD_READ|SRV_FD_WRITE;
    if(file->type==PROCESS_FILE_PTY_MASTER || file->type==PROCESS_FILE_PTY_SLAVE) {
        int64_t flags=pty_get_flags(file->handle,file->type==PROCESS_FILE_PTY_MASTER);
        return flags<0?flags:(int64_t)(access|(uint64_t)flags);
    }
    uint64_t *status=endpoint_status_flags(file);
    return access|((status?*status:file->flags)&SRV_FD_NONBLOCK);
}
int64_t process_file_set_flags(struct process *process, uint64_t fd, uint64_t flags) {
    struct process_file *file=process_file_at(process,fd);
    if(!file) return SRV_ERR_BADF;
    uint64_t irq=process_irq_save();
    if(file->type==PROCESS_FILE_VFS) {
        struct process_read_file *read=read_file_at(file->handle);
        if(!read) { process_irq_restore(irq);return SRV_ERR_BADF; }
        read->status_flags=flags&(SRV_FD_NONBLOCK|SRV_FD_APPEND);
    } else if(file->type==PROCESS_FILE_VFS_WRITE) {
        struct process_write_file *write=write_file_at(file->handle);
        if(!write) { process_irq_restore(irq);return SRV_ERR_BADF; }
        if(write->content->writeback.active) {process_irq_restore(irq);return SRV_ERR_BUSY;}
        write_revoke_append(write);
        write->open_flags=(write->open_flags&~(SRV_OPEN_APPEND|SRV_OPEN_NONBLOCK)) |
            ((flags&SRV_FD_APPEND)?SRV_OPEN_APPEND:0) | ((flags&SRV_FD_NONBLOCK)?SRV_OPEN_NONBLOCK:0);
    }
    if(file->type==PROCESS_FILE_PTY_MASTER || file->type==PROCESS_FILE_PTY_SLAVE) {
        int64_t result=pty_set_flags(file->handle,file->type==PROCESS_FILE_PTY_MASTER,flags&SRV_FD_NONBLOCK);
        if(result<0) { process_irq_restore(irq);return result; }
    }
    uint64_t *status=endpoint_status_flags(file);
    if(status) *status=flags&SRV_FD_NONBLOCK;
    file->flags=(file->flags&SRV_FD_CLOEXEC)|(flags&SRV_FD_NONBLOCK);
    process_irq_restore(irq);return 0;
}

int64_t process_file_get_fd_flags(struct process *process, uint64_t fd) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL) {
        return SRV_ERR_BADF;
    }
    return (file->flags & SRV_FD_CLOEXEC) != 0 ? SRV_FD_CLOEXEC : 0;
}

int64_t process_file_set_fd_flags(struct process *process, uint64_t fd, uint64_t flags) {
    struct process_file *file = process_file_at(process, fd);
    if (file == NULL) {
        return SRV_ERR_BADF;
    }
    if(file->type==PROCESS_FILE_VFS_WRITE) {
        struct process_write_file *write=write_file_at(file->handle);
        if(write && write->content->writeback.active)return SRV_ERR_BUSY;
        if(write)write_revoke_append(write);
    }
    file->flags = (file->flags & ~SRV_FD_CLOEXEC) | (flags & SRV_FD_CLOEXEC);
    return 0;
}

int64_t process_file_get_lock(struct process *process, uint64_t fd, struct srv_flock *lock) {
    struct process_file *file = process_file_at(process, fd);
    struct process_file_content *content=NULL;
    uint64_t start = 0;
    uint64_t end = 0;
    if (process == NULL ||
        lock == NULL ||
        file == NULL ||
        !file_lock_type_valid(lock->type) ||
        !file_content_for_fd(file,&content) ||
        !file_lock_range_for_fd(file, lock, &start, &end)) {
        return -1;
    }

    if (lock->type == SRV_F_UNLCK) {
        lock->pid = 0;
        return 0;
    }

    struct process_file_lock *conflict = file_lock_conflict(content, process->pid, start, end, lock->type);
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
    uint64_t initial_irq=process_irq_save();
    struct process_file *file = process_file_at(process, fd);
    struct process_file_content *content=NULL;
    uint64_t start = 0;
    uint64_t end = 0;
    if (process == NULL ||
        lock == NULL ||
        file == NULL ||
        !file_lock_type_valid(lock->type) ||
        !file_content_for_fd(file,&content) ||
        !file_lock_range_for_fd(file, lock, &start, &end)) {
        int64_t result=!file?SRV_ERR_BADF:SRV_ERR_INVAL;process_irq_restore(initial_irq);return result;
    }
    int64_t access=process_file_get_flags(process,fd);
    if((lock->type==SRV_F_WRLCK && !(access&SRV_FD_WRITE)) ||
        (lock->type==SRV_F_RDLCK && !(access&SRV_FD_READ))) {process_irq_restore(initial_irq);return SRV_ERR_BADF;}
    struct process_file_lock_wait request={process,fd,process->file_generations[fd],start,end,content,lock->type};
    process_irq_restore(initial_irq);
    for (;;) {
        uint64_t irq=process_irq_save();
        if(!file_lock_wait_current(&request)){process_irq_restore(irq);return SRV_ERR_BADF;}
        if(process_should_exit_current()){process_irq_restore(irq);return SRV_ERR_INTR;}
        if(lock->type==SRV_F_UNLCK || file_lock_conflict(content,process->pid,start,end,lock->type)==NULL) {
            int64_t result=file_lock_update(process->pid,content,start,end,lock->type);
            process_irq_restore(irq);return result;
        }
        process_irq_restore(irq);
        if (!wait) {
            return SRV_ERR_AGAIN;
        }
        if (process_signal_pending_current()) {
            return SRV_ERR_INTR;
        }
        if (!scheduler_wait(&file_lock_wait_queue, file_lock_wait_ready, &request)) {
            scheduler_yield();
        }
    }
}

static int64_t process_file_close_entry(struct process *process, struct process_file *file) {
    int64_t result = 0;
    struct process_file_content *lock_content=NULL;
    if (process != NULL && file_content_for_fd(file,&lock_content)) {
        file_lock_release_path(process->pid, lock_content);
    }
    if ((file->type == PROCESS_FILE_NET_LISTENER ||
            file->type == PROCESS_FILE_NET_CONNECTION ||
            file->type == PROCESS_FILE_NET_UDP) &&
        net_close(file->handle) < 0) {
        result = SRV_ERR_IO;
    }

    if (file->type == PROCESS_FILE_VFS) {
        result=read_file_close(file->handle);
    } else if (file->type == PROCESS_FILE_PIPE_READ) {
        pipe_close(file->handle, false);
    } else if (file->type == PROCESS_FILE_PIPE_WRITE) {
        pipe_close(file->handle, true);
    } else if (file->type == PROCESS_FILE_PTY_MASTER || file->type == PROCESS_FILE_PTY_SLAVE) {
        pty_close(file->handle,file->type==PROCESS_FILE_PTY_MASTER);
    } else if (file->type == PROCESS_FILE_PIPE_DUPLEX) {
        pipe_close(file->handle, false);
        pipe_close(file->aux_handle, true);
    } else if (file->type == PROCESS_FILE_UNIX_LISTENER) {
        unix_listener_release(file->handle);
    } else if (file->type == PROCESS_FILE_VFS_WRITE) {
        if (write_file_close(file->handle) < 0) {
            result = SRV_ERR_IO;
        }
    }

    uint8_t *bytes = (uint8_t *)file;
    for (uint64_t i = 0; i < sizeof(*file); i++) {
        bytes[i] = 0;
    }
    return result;
}

/* A simple pipe carries no filesystem ownership. Reject rights-bearing pipes:
 * final reference release can recursively close files and flush their data.
 * Check and detach in one IRQ-excluded operation, not an admission snapshot. */
bool process_file_try_close_pipe(struct process *process,uint64_t fd,int64_t *result) {
    if(!result)return false;
    uint64_t irq=process_irq_save();
    struct process_file *file=process_file_at(process,fd);
    if(!file || (file->type!=PROCESS_FILE_PIPE_READ && file->type!=PROCESS_FILE_PIPE_WRITE))goto refused;
    bool writer=file->type==PROCESS_FILE_PIPE_WRITE;
    struct process_pipe *pipe=pipe_at(file->handle);
    if(!pipe || !pipe->refs || !(writer?pipe->write_refs:pipe->read_refs))goto refused;
    for(unsigned i=0;i<PROCESS_PIPE_RIGHTS_QUEUE;i++)if(pipe->rights[i].used)goto refused;
    uint64_t handle=file->handle;
    *file=(struct process_file){0};file_generation_invalidate(process,fd);
    pipe_close(handle,writer);*result=0;
    process_irq_restore(irq);return true;
refused:
    process_irq_restore(irq);return false;
}

int64_t process_file_close(struct process *process, uint64_t fd) {
    uint64_t irq=process_irq_save();
    struct process_file *file=process_file_at(process,fd);
    if(!file) { process_irq_restore(irq);return SRV_ERR_BADF; }
    struct process_file_content *content=NULL;
    if(file_content_for_fd(file,&content) && content->writeback.active) {
        process_irq_restore(irq);return SRV_ERR_BUSY;
    }
    struct process_file retired=*file;*file=(struct process_file){0};
    file_generation_invalidate(process,fd);
    process_irq_restore(irq);
    enum process_file_type retired_type = retired.type;
    int64_t result = process_file_close_entry(process,&retired);
    if (retired_type == PROCESS_FILE_PIPE_READ || retired_type == PROCESS_FILE_PIPE_WRITE ||
        retired_type == PROCESS_FILE_PIPE_DUPLEX || retired_type == PROCESS_FILE_UNIX_LISTENER)
        pipe_rights_collect();
    return result;
}

int64_t process_file_seek(struct process *process, uint64_t fd, int64_t offset, uint64_t whence) {
    uint64_t flags = process_irq_save();
    struct process_file *file = process_file_at(process, fd);
    if (!file) { process_irq_restore(flags);return SRV_ERR_BADF; }
    if(file->type!=PROCESS_FILE_VFS && file->type!=PROCESS_FILE_VFS_WRITE) {
        process_irq_restore(flags);return SRV_ERR_SPIPE;
    }

    struct process_write_file *write = NULL;
    struct process_read_file *read = NULL;
    uint64_t current_offset = file->offset;
    uint64_t current_size = file->size;
    if (file->type == PROCESS_FILE_VFS) {
        read = read_file_at(file->handle);
        if (read == NULL) {
            process_irq_restore(flags);
            return SRV_ERR_BADF;
        }
        current_offset = read->offset;
        current_size = read->content->size;
    } else if (file->type == PROCESS_FILE_VFS_WRITE) {
        write = write_file_at(file->handle);
        if (write == NULL) {
            process_irq_restore(flags);
            return SRV_ERR_BADF;
        }
        current_offset = write->offset;
        current_size = write->content->size;
    }

    int64_t base;
    if (whence == 0) {
        base = 0;
    } else if (whence == 1) {
        base = (int64_t)current_offset;
    } else if (whence == 2) {
        base = (int64_t)current_size;
    } else {
        process_irq_restore(flags);
        return SRV_ERR_INVAL;
    }

    if((offset>0 && base>INT64_MAX-offset) || (offset<0 && base<INT64_MIN-offset)) {
        process_irq_restore(flags);return SRV_ERR_OVERFLOW;
    }
    int64_t target = base + offset;
    if (target < 0) {
        process_irq_restore(flags);
        return SRV_ERR_INVAL;
    }

    if (write != NULL) {
        if(write->content->writeback.active) {process_irq_restore(flags);return SRV_ERR_BUSY;}
        write_revoke_append(write);
        write->offset = (uint64_t)target;
        process_irq_restore(flags);
        return (int64_t)write->offset;
    }
    if (read != NULL) {
        read->offset = (uint64_t)target;
        process_irq_restore(flags);
        return (int64_t)read->offset;
    }
    file->offset = (uint64_t)target;
    process_irq_restore(flags);
    return (int64_t)file->offset;
}

int64_t process_file_stat(struct process *process,
    uint64_t fd,
    struct vfs_metadata *metadata_out,
    uint64_t *size_out,
    uint64_t *type_out) {
    if (process == NULL || size_out == NULL || type_out == NULL) {
        return -1;
    }
    struct process_file *file=process_file_at(process,fd);
    if(!file) return SRV_ERR_BADF;
    if(file->type==PROCESS_FILE_STDIO) {
        *size_out=0;*type_out=0;
        if(metadata_out) *metadata_out=(struct vfs_metadata){.inode=file->handle+1,.mode=VFS_MODE_IFCHR|0600,.nlink=1};
        return 0;
    }

    if(file->type==PROCESS_FILE_PTY_MASTER || file->type==PROCESS_FILE_PTY_SLAVE) {
        if(!pty_state(file->handle)) return -1;
        *size_out=0;*type_out=0;
        if(metadata_out) *metadata_out=(struct vfs_metadata){.inode=file->handle,.mode=VFS_MODE_IFCHR|0600,.nlink=1};
        return 0;
    }

    struct process_file_content *content=NULL;
    if(file_content_for_fd(file,&content) && content->unlinked) {
        *size_out=content->size;*type_out=VFS_NODE_FILE;
        if(metadata_out) *metadata_out=content->orphan_metadata;
        return 0;
    }
    if (file->type == PROCESS_FILE_VFS || file->type == PROCESS_FILE_VFS_WRITE) {
        *type_out = 0;
        if (file->type == PROCESS_FILE_VFS_WRITE) {
            struct process_write_file *write = write_file_at(file->handle);
            if (write == NULL) {
                return -1;
            }
            *size_out = write->content->size;
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
        *size_out = read->content->size;
        if (read->node != NULL) {
            *type_out = (uint64_t)read->node->type;
            if (metadata_out != NULL) {
                *metadata_out = read->node->metadata;
            }
        }
        return 0;
    }

    if (file->type == PROCESS_FILE_PIPE_READ ||
        file->type == PROCESS_FILE_PIPE_WRITE ||
        file->type == PROCESS_FILE_PIPE_DUPLEX) {
        struct process_pipe *pipe = pipe_at(file->handle);
        if (pipe == NULL) {
            return -1;
        }
        *size_out = pipe->size;
        *type_out = 0;
        if (metadata_out != NULL) {
            *metadata_out = (struct vfs_metadata) {
                .inode = file->handle,
                .mode = VFS_MODE_IFIFO | 0600,
                .nlink = 1,
            };
        }
        return 0;
    }

    if (file->type == PROCESS_FILE_UNIX_LISTENER) {
        struct process_unix_listener *listener = unix_listener_at(file->handle);
        if (listener == NULL) {
            return -1;
        }
        *size_out = unix_listener_pending_count(listener);
        *type_out = 0;
        if (metadata_out != NULL) {
            *metadata_out = (struct vfs_metadata) {
                .inode = file->handle,
                .mode = VFS_MODE_IFIFO | 0600,
                .nlink = 1,
            };
        }
        return 0;
    }

    if (file->type == PROCESS_FILE_NET_LISTENER ||
        file->type == PROCESS_FILE_NET_CONNECTION ||
        file->type == PROCESS_FILE_NET_UDP) {
        *size_out = 0;
        *type_out = 0;
        if (metadata_out != NULL) {
            *metadata_out = (struct vfs_metadata) {
                .inode = file->handle,
                .mode = VFS_MODE_IFSOCK | 0600,
                .nlink = 1,
            };
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

int64_t process_file_chmod(struct process *process,uint64_t fd,uint64_t mode) {
    struct process_file_content *content=NULL;
    if(!file_content_for_fd(process_file_at(process,fd),&content)) return SRV_ERR_BADF;
    if(content->writeback.active)return SRV_ERR_BUSY;
    content_revoke_append(content);
    if(content->unlinked) { content->orphan_metadata.mode=VFS_MODE_IFREG|(mode&07777);return 0; }
    return vfs_chmod(content->path,mode)?0:SRV_ERR_IO;
}
int64_t process_path_utime(const char *path,uint64_t atime,uint64_t mtime) {
    if(!vfs_lookup(path)) return SRV_ERR_NOENT;
    if(atime==SRV_TIME_OMIT && mtime==SRV_TIME_OMIT) return 0;
    struct process_file_content *content=content_find(path);
    if(content && content->writeback.active)return SRV_ERR_BUSY;
    content_revoke_append(content);
    if(content && content_flush(content)<0) return SRV_ERR_IO;
    return vfs_utime(path,atime,mtime)?0:SRV_ERR_IO;
}
int64_t process_file_utime(struct process *process,uint64_t fd,uint64_t atime,uint64_t mtime) {
    struct process_file_content *content=NULL;
    if(!file_content_for_fd(process_file_at(process,fd),&content)) return SRV_ERR_BADF;
    if(atime==SRV_TIME_OMIT && mtime==SRV_TIME_OMIT) return 0;
    if(content->writeback.active)return SRV_ERR_BUSY;
    content_revoke_append(content);
    if(content->unlinked) { vfs_metadata_utime(&content->orphan_metadata,atime,mtime);return 0; }
    if(content_flush(content)<0) return SRV_ERR_IO;
    return vfs_utime(content->path,atime,mtime)?0:SRV_ERR_IO;
}

static int64_t process_sbrk_impl(struct process *process, int64_t increment, uint64_t *previous_out) {
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
    if (process == NULL) {
        return false;
    }
    uint64_t end = base + length;
    for (uint64_t i = 0; i < PROCESS_MAX_MMAP_REGIONS; i++) {
        struct process_mmap_region *region = &process->mmap_regions[i];
        if (!region->used) {
            continue;
        }
        uint64_t region_start = region->base;
        uint64_t region_end = region_start + region->length;
        if (base < region_end && end > region_start) {
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

/* Validate coverage by region boundaries rather than rescanning 512 regions
 * for every page. Call only under the ordinary process service owner. */
static bool mmap_range_registered(struct process *process,uint64_t base,uint64_t length) {
    uint64_t end=base+length;
    while(base<end) {
        int64_t i=mmap_region_index_for_page(process,base);
        if(i<0)return false;
        uint64_t next=process->mmap_regions[i].base+process->mmap_regions[i].length;
        base=next<end?next:end;
        process_service_checkpoint();
    }
    return true;
}

static struct process_mmap_region *mmap_free_region_slot(struct process *process) {
    for (uint64_t i = 0; i < PROCESS_MAX_MMAP_REGIONS; i++) {
        if (!process->mmap_regions[i].used) {
            return &process->mmap_regions[i];
        }
    }
    return NULL;
}

static bool mmap_record_region(struct process *process, uint64_t base, uint64_t length,bool anonymous) {
    struct process_mmap_region *region = mmap_free_region_slot(process);
    if (region == NULL) {
        return false;
    }
    *region = (struct process_mmap_region) {
        .used = true,
        .anonymous = anonymous,
        .base = base,
        .length = length,
    };
    return true;
}

static int64_t mmap_find_free_range(struct process *process, uint64_t length, uint64_t hint) {
    if(!process || !length || length>USER_MMAP_LIMIT-USER_MMAP_BASE)return -1;
    uint64_t start=USER_MMAP_BASE;
    if(hint>=USER_MMAP_BASE && hint<USER_MMAP_LIMIT)start=page_up(hint);
    else if(process->mmap_next>=USER_MMAP_BASE && process->mmap_next<USER_MMAP_LIMIT)
        start=page_up(process->mmap_next);
    for(unsigned pass=0;pass<2;pass++) {
        uint64_t cursor=pass?USER_MMAP_BASE:start;
        uint64_t stop=pass?start:USER_MMAP_LIMIT;
        while(cursor<stop && length<=USER_MMAP_LIMIT-cursor) {
            uint64_t next=cursor;
            for(unsigned i=0;i<PROCESS_MAX_MMAP_REGIONS;i++) {
                const struct process_mmap_region *r=&process->mmap_regions[i];
                if(r->used && cursor<r->base+r->length && cursor+length>r->base &&
                   next<r->base+r->length)next=r->base+r->length;
            }
            if(next==cursor)return (int64_t)cursor;
            /* Every intervening start still overlaps at least one region.
             * Skip to its end, not by the request size: that stride lost fits. */
            cursor=next;process_service_checkpoint();
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
        (void)mmap_record_region(process, tail_base, tail_length,region->anonymous);
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
        if (read == NULL || read->directory) {
            return false;
        }
        *data_out = read->content->data;
        *size_out = read->content->size;
        return true;
    }
    if (file->type == PROCESS_FILE_VFS_WRITE) {
        struct process_write_file *write = write_file_at(file->handle);
        if (write == NULL) {
            return false;
        }
        *data_out = write->content->data;
        *size_out = write->content->size;
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
    uint64_t index,
    uint64_t page_flags) {
    if (index >= process->mapping_count) {
        return false;
    }
    uint64_t page=process->mappings[index].virtual_address;
    uint64_t irq = process_irq_save();
    if (!vmm_protect_page_in_space(process->address_space, page, page_flags)) {
        process_irq_restore(irq); return false;
    }
    if (!(page_flags & VMM_PAGE_USER)) process_futex_invalidate_page(process, page);
    process->mappings[index].flags = page_flags;
    process_irq_restore(irq);
    return true;
}

/* Failed mmap allocations are an appended suffix while this owner excludes
 * siblings. Publish each removal before permitting a service turn. */
static void mmap_rollback_pages(struct process *process,uint64_t original_count) {
    unsigned retired=0;
    while(process->mapping_count>original_count) {
        process_unmap_mapping_at(process,process->mapping_count-1);
        if(!(++retired&31))process_service_checkpoint();
    }
}

static int64_t process_mmap_impl(struct process *process,
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

    uint64_t original_count=process->mapping_count;
    uint64_t mapped = 0;
    uint64_t page_flags = mmap_page_flags_for_protection(protection);
    if (protection != SRV_PROT_NONE) {
        for (; mapped < mapped_length; mapped += PAGE_SIZE) {
            if (!map_process_page(process, base + mapped, page_flags)) {
                mmap_rollback_pages(process,original_count);
                return -1;
            }
        }
    }

    if (protection != SRV_PROT_NONE && !anonymous && (uint64_t)offset < file_size) {
        uint64_t available = file_size - (uint64_t)offset;
        uint64_t copy_length = length < available ? length : available;
        if (copy_length != 0 &&
            !user_space_write(process, base, file_data + (uint64_t)offset, copy_length)) {
            mmap_rollback_pages(process,original_count);
            return -1;
        }
    }

    if (!mmap_record_region(process, base, mapped_length,anonymous)) {
        mmap_rollback_pages(process,original_count);
        return -1;
    }

    uint64_t next = base + mapped_length;
    process->mmap_next = next < USER_MMAP_LIMIT ? next : USER_MMAP_BASE;
    return (int64_t)base;
}

bool process_try_mmap_private(struct process *p,uint64_t address,uint64_t length,uint64_t protection,uint64_t flags,int64_t fd,int64_t offset,int64_t *result) {
    if(!result || !p || address || !length || length>2u*1024u*1024u ||
       protection!=(SRV_PROT_READ|SRV_PROT_WRITE) || flags!=(SRV_MAP_PRIVATE|SRV_MAP_ANONYMOUS) || fd!=-1 || offset)return false;
    uint64_t irq=process_irq_save(),mapped=page_up(length);
    if(!p->allocated || !p->active || p->killed || p->exiting || p->mapping_count>4096-mapped/PAGE_SIZE ||
       !scheduler_io_process_independent(p) || p->address_space!=vmm_current_address_space())goto refused;
    for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++)if(p->user_threads[i].used)goto refused;
    uint64_t base=p->mmap_next>=USER_MMAP_BASE && p->mmap_next<USER_MMAP_LIMIT?page_up(p->mmap_next):USER_MMAP_BASE;
    if(!mmap_range_valid(base,mapped) || !mmap_range_free(p,base,mapped) || !mmap_free_region_slot(p))goto refused;
    for(uint64_t i=0;i<p->mapping_count;i++)
        if(p->mappings[i].virtual_address>=base && p->mappings[i].virtual_address<base+mapped)goto refused;
    (void)mmap_record_region(p,base,mapped,true);
    for(uint64_t page=0;page<mapped;page+=PAGE_SIZE) {
        if(!map_process_page_mode(p,base+page,VMM_PAGE_USER|VMM_PAGE_WRITABLE|VMM_PAGE_NO_EXECUTE,false)) {
            int64_t ignored;
            /* Region/tracking stay owned if a checked unmap cannot finish. */
            (void)process_try_munmap_private(p,base,mapped,&ignored);
            *result=-1;process_irq_restore(irq);return true;
        }
    }
    p->mmap_next=base+mapped<USER_MMAP_LIMIT?base+mapped:USER_MMAP_BASE;
    *result=(int64_t)base;process_irq_restore(irq);return true;
refused:
    process_irq_restore(irq);return false;
}

bool process_try_munmap_private(struct process *process,uint64_t address,uint64_t length,int64_t *result) {
    if(!result || !process || !length || length>2u*1024u*1024u || address%PAGE_SIZE)return false;
    uint64_t mapped=page_up(length);
    if(!mmap_range_valid(address,mapped))return false;
    uint64_t irq=process_irq_save();
    if(!process->allocated || !process->active || process->exiting || !process->address_space ||
       process->mapping_count>4096 ||
       process->address_space!=vmm_current_address_space())goto refused;
    struct process_user_thread *helper=process_current_user_thread();
    if(helper) {
        bool owned=false;
        for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++)
            if(helper==&process->user_threads[i] && helper->used && helper->active && helper->owner==process)owned=true;
        if(!owned || !scheduler_io_process_helper_memory(process,process_file_poll_wait_queue()))goto refused;
    } else {
        if(!scheduler_io_process_independent(process))goto refused;
        for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++)if(process->user_threads[i].used)goto refused;
    }
    struct process_mmap_region *region=NULL;
    for(unsigned i=0;i<PROCESS_MAX_MMAP_REGIONS;i++) {
        struct process_mmap_region *r=&process->mmap_regions[i];
        if(!r->used || r->base>=address+mapped || address>=r->base+r->length)continue;
        if(region || !r->anonymous || r->base!=address || r->length!=mapped)goto refused;
        region=r;
    }
    if(!region)goto refused;
    /* No allocation, callbacks or service windows. Verify every owned PTE
     * before mutation; the other I/O owner and all IRQ handlers stay paused. */
    for(uint64_t i=0;i<process->mapping_count;i++) {
        struct process_mapping *m=&process->mappings[i];
        if(m->virtual_address<address || m->virtual_address>=address+mapped)continue;
        uint64_t physical=0;
        if(!m->physical_address || !vmm_virt_to_phys_in_space(process->address_space,m->virtual_address,&physical) ||
           physical!=m->physical_address)goto refused;
    }
    for(uint64_t i=0;i<process->mapping_count;) {
        struct process_mapping *m=&process->mappings[i];
        if(m->virtual_address<address || m->virtual_address>=address+mapped) {i++;continue;}
        process_futex_invalidate_page(process,m->virtual_address);
        uint64_t physical=0;
        if(!vmm_unmap_page_in_space(process->address_space,m->virtual_address,&physical) || physical!=m->physical_address) {
            /* Never free a frame on a refused/unexpected unmap. Keep the
             * remaining region and tracking entries for retry or final exit. */
            *result=-1;process_irq_restore(irq);return true;
        }
        pmm_free_frame(physical);
        process->mappings[i]=process->mappings[--process->mapping_count];
    }
    region->used=false;
    if(address<process->mmap_next)process->mmap_next=address;
    *result=0;process_irq_restore(irq);return true;
refused:
    process_irq_restore(irq);return false;
}

static int64_t process_munmap_impl(struct process *process, uint64_t address, uint64_t length) {
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
    if(!mmap_range_registered(process,address,mapped_length))return -1;
    /* Visit each tracked mapping once. Searching the complete table twice
     * per page made large compositor-buffer teardown quadratic. Removal swaps
     * in the last entry, so examine that replacement before advancing. Pages
     * without physical mappings (for example PROT_NONE) need no frame release. */
    unsigned scanned=0;
    for (uint64_t index = 0; index < process->mapping_count;) {
        if(!(++scanned&31))process_service_checkpoint();
        uint64_t page = process->mappings[index].virtual_address;
        if (page >= address && page < address + mapped_length) {
            process_unmap_mapping_at(process, index);
        } else {
            index++;
        }
    }
    mmap_forget_range(process, address, mapped_length);
    if (address < process->mmap_next) {
        process->mmap_next = address;
    }
    return 0;
}

static int64_t process_mprotect_impl(struct process *process,
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
    if(!mmap_range_registered(process,address,mapped_length))return -1;
    uint64_t page_flags=mmap_page_flags_for_protection(protection);
    /* The tracking array is unsorted. Visit it once, publishing each PTE and
     * matching flags before a service turn; siblings remain excluded. */
    for(uint64_t i=0;i<process->mapping_count;i++) {
        uint64_t page=process->mappings[i].virtual_address;
        if(page>=address && page<address+mapped_length &&
           !mmap_apply_protection(process,i,page_flags))return -1;
        if(!((i+1)&31))process_service_checkpoint();
    }
    if(protection!=SRV_PROT_NONE) {
        for(uint64_t page=address;page<address+mapped_length;page+=PAGE_SIZE) {
            uint64_t physical;
            /* PROT_NONE reservations may have no frame yet. Existing PTEs
             * were handled above; new mappings publish tracking atomically. */
            if(!vmm_virt_to_phys_in_space(process->address_space,page,&physical) &&
               !map_process_page(process,page,page_flags))return -1;
            if(!(((page-address)/PAGE_SIZE+1)&31))process_service_checkpoint();
        }
    }
    return 0;
}

static int64_t process_msync_impl(struct process *process,
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
    if(!mmap_range_registered(process,address,mapped_length))return -1;
    uint64_t pages=0;
    for(uint64_t i=0;i<process->mapping_count;i++) {
        uint64_t page=process->mappings[i].virtual_address;
        if(page>=address && page<address+mapped_length)pages++;
        if(!((i+1)&31))process_service_checkpoint();
    }
    /* Private mappings have no file writeback; retain the existing requirement
     * that every requested page owns a tracked frame (including PROT_NONE). */
    if(pages!=mapped_length/PAGE_SIZE)return -1;
    return 0;
}

static void process_exit_gate(void) {
    /* A parked initial thread can resume here after a worker fault. Keep the
     * gate and teardown indivisible even when that continuation enabled IRQs. */
    (void)process_irq_save();
    scheduler_io_set_user_safe(false);
    scheduler_io_wait();
}

void process_exit(uint64_t status) {
    process_exit_gate();
    struct process *process = process_current();
    if (process == NULL || !process->active) {
        for (;;) {
            PROCESS_ARCH_HALT();
        }
    }

    scheduler_invalidate_current_user_trap();
    process_signal_clear_records(process);
    user_scope_detach(process);
    struct process_user_thread *thread = process_current_user_thread();
    if (thread != NULL) {
        (void)process_irq_save();
        process->exit_status = status;
        process_record_exit_status(process->pid, status);
        process->exiting = true;
        thread->active = false;
        net_process_wake(process);
        scheduler_wake_all(&pipe_wait_queue);
        scheduler_wake_all(&process->thread_wait_queue);
        process_file_poll_wake();
        /* Sibling wait continuations can still run while exit propagates.
         * Preserve their bindings; final teardown kills them before cleanup. */
        process_futex_wake_process(process);
        scheduler_clear_user_context();
        process_irq_restore(1ull << 9);
        scheduler_exit_current();
    }

    (void)process_irq_save();
    struct process_latency_span exit_span=process_latency_begin();
    scheduler_kill_user_threads(process, NULL);
    process_futex_cleanup_process(process);
    for (uint64_t i = 0; i < PROCESS_MAX_USER_THREADS; i++) {
        process->user_threads[i].used = false;
        process->user_threads[i].active = false;
    }
    audio_process_cleanup(process->pid);
    process->exit_status = finish_process_files(process, status);
    process_record_exit_status(process->pid, process->exit_status);
    gui_process_cleanup(process->pid);
    console_release_framebuffer_mute_owner(process->pid);
    net_process_cleanup(process);
    process->active = false;
    scheduler_clear_user_context();
    process->image_generation=0;
    cleanup_process_address_space(process);

    process_latency_end(LATENCY_PROCESS_EXIT,exit_span);
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

uint64_t process_fs_base(void *process_ptr) {
    struct process *process = process_ptr;
    if (process == NULL || !process->allocated) {
        return 0;
    }
    struct process_user_thread *thread = process_current_user_thread();
    if (thread != NULL && thread->owner == process && thread->used) {
        return thread->tls_thread_pointer;
    }
    return process->tls_thread_pointer;
}

void process_fpu_restore(void *process_ptr) {
    struct process *process = process_ptr;
    if (process == NULL || !process->allocated) {
        return;
    }
    fpu_restore(&process->fpu);
}

int64_t process_sbrk(struct process *process,int64_t increment,uint64_t *previous_out) {
    struct process_latency_span span=process_latency_begin();
    struct process_service_scope scope=process_service_begin();
    int64_t result=process_sbrk_impl(process,increment,previous_out);
    process_service_end(scope);
    process_latency_end(LATENCY_PROCESS_MAPPING,span);return result;
}

int64_t process_mmap(struct process *process,uint64_t address,uint64_t length,uint64_t protection,uint64_t flags,int64_t fd,int64_t offset) {
    struct process_latency_span span=process_latency_begin();
    struct process_service_scope scope=process_service_begin();
    int64_t result=process_mmap_impl(process,address,length,protection,flags,fd,offset);
    process_service_end(scope);
    process_latency_end(LATENCY_PROCESS_MAPPING,span);return result;
}

int64_t process_munmap(struct process *process,uint64_t address,uint64_t length) {
    struct process_latency_span span=process_latency_begin();
    struct process_service_scope scope=process_service_begin();
    int64_t result=process_munmap_impl(process,address,length);
    process_service_end(scope);
    process_latency_end(LATENCY_PROCESS_MAPPING,span);return result;
}

int64_t process_mprotect(struct process *process,uint64_t address,uint64_t length,uint64_t protection) {
    struct process_latency_span span=process_latency_begin();
    struct process_service_scope scope=process_service_begin();
    int64_t result=(!scope.held && scheduler_io_active())?-1:
        process_mprotect_impl(process,address,length,protection);
    process_service_end(scope);process_latency_end(LATENCY_PROCESS_MAPPING,span);return result;
}
int64_t process_msync(struct process *process,uint64_t address,uint64_t length,uint64_t flags) {
    struct process_service_scope scope=process_service_begin();
    int64_t result=(!scope.held && scheduler_io_active())?-1:
        process_msync_impl(process,address,length,flags);
    process_service_end(scope);return result;
}

#include "process_signal_public.inc"
