#ifndef SRVROS_PROCESS_H
#define SRVROS_PROCESS_H

#include <srvros/path_limits.h>
#include <stdbool.h>
#include <stdint.h>

#include <srvros/fpu.h>
#include <srvros/syscall_numbers.h>
#include <srvros/vfs.h>

#define PROCESS_MAX_OPEN_FILES 67
#define PROCESS_MAX_PROCESSES 64

enum process_file_type {
    PROCESS_FILE_UNUSED,
    PROCESS_FILE_STDIO,
    PROCESS_FILE_VFS,
    PROCESS_FILE_VFS_WRITE,
    PROCESS_FILE_NET_LISTENER,
    PROCESS_FILE_NET_CONNECTION,
    PROCESS_FILE_NET_UDP,
    PROCESS_FILE_PIPE_READ,
    PROCESS_FILE_PIPE_WRITE,
    PROCESS_FILE_PIPE_DUPLEX,
    PROCESS_FILE_UNIX_LISTENER,
    PROCESS_FILE_PTY_MASTER,
    PROCESS_FILE_PTY_SLAVE,
};

struct process_file {
    bool used;
    enum process_file_type type;
    const struct vfs_node *node;
    const uint8_t *data;
    uint64_t size;
    uint64_t offset;
    uint64_t capacity;
    uint64_t handle;
    uint64_t aux_handle;
    uint64_t flags;
    bool dirty;
    bool failed;
    char path[SRV_PATH_MAX];
};

struct process_spawn_file_action {
    uint32_t type;
    int32_t reserved;
    int64_t fd;
    int64_t new_fd;
    const char *path;
    uint64_t open_flags;
};

struct process;
struct isr_frame;
int64_t process_fork(const struct isr_frame *frame);
struct scheduler_wait_queue;

bool process_run_elf(const char *path);
bool process_run_elf_args(const char *path, const char *args);
int64_t process_spawn_elf(const char *path);
int64_t process_spawn_elf_args(const char *path, const char *args);
int64_t process_spawn_elf_args_redirect(const char *path,
    const char *args,
    const char *stdout_path,
    bool append);
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
    uint64_t signal_default);
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
    uint64_t file_action_count,
    uint64_t signal_flags,
    uint64_t signal_mask,
    uint64_t signal_default);
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
    uint64_t signal_default);
int64_t process_spawn_background_elf(const char *path);
int64_t process_spawn_background_elf_args(const char *path, const char *args);
int64_t process_spawn_background_elf_args_quiet(const char *path, const char *args);
int64_t process_spawn_background_elf_args_fds(const char *path,
    const char *args,
    int64_t stdin_fd,
    int64_t stdout_fd);
/* Backend for the userspace session-scope syscall. Authority belongs to the
 * current unscoped owner and its exact token; reservations count as members.
 * Revoke closes admission without signals. Reap claims at most one retired
 * orphan; release requires revocation and zero members. */
struct srv_scope_status;
int64_t process_scope_create(uint64_t *out);
int64_t process_scope_reserve(uint64_t token);
void process_scope_cancel_reservation(uint64_t token);
int64_t process_scope_control(unsigned op,uint64_t token,struct srv_scope_status *out);
int64_t process_scope_reap(uint64_t token);

/* Kernel-only owner capability; no userspace setter or syscall. One root per
 * token, inherited by all spawn paths; exec/job control do not change it.
 * CPU0 only, matching process management. Members include preparation/release.
 * Revoke, drain until members==0, then release. A timeout is not retirement. */
struct process_lifetime_status { uint64_t members, peak; bool revoked; };
uint64_t process_lifetime_create(void);
int64_t process_lifetime_spawn(uint64_t token,const char *path,const char *args);
bool process_lifetime_revoke(uint64_t token);
bool process_lifetime_drain(uint64_t token);
bool process_lifetime_status(uint64_t token,struct process_lifetime_status *out);
bool process_lifetime_release(uint64_t token);
uint64_t process_lifetime_of(uint64_t pid);

int64_t process_wait(uint64_t pid, uint64_t *status_out, bool nohang);
/* Kernel session supervisor: wait without reaping until this caller's target
 * is reapable/absent, a signal, external predicate or absolute tick deadline.
 * Predicate runs IRQ-masked and must not block or mutate ownership. Producers
 * publish before notify; compare-and-sleep prevents lost notifications.
 * Spurious wakes are allowed. False requires the caller's bounded fallback. */
bool process_wait_event(uint64_t pid,bool (*condition)(void *),void *arg,uint64_t deadline_ticks);
void process_wait_notify(void);
int64_t process_wait_posix(int64_t selector,uint64_t *status_out,bool nohang);
/* Kernel-designated init may reap children whose userspace parent has exited. */
bool process_set_reaper(uint64_t pid);
bool process_exit_status(uint64_t pid, uint64_t *status_out);
bool process_start_background(const char *path);
uint64_t process_list(uint64_t start_index,
    uint64_t *pid_out,
    const char **name_out,
    const char **state_out);
uint64_t process_list_details(uint64_t start_index, uint64_t *pid_out,
    const char **name_out, const char **state_out, uint64_t *parent_out, uint64_t *mapped_bytes_out);
bool process_kill_pid(uint64_t pid);
/* Record an uncatchable synchronous fault after acquiring the I/O exit gate. */
bool process_record_user_fault(uint64_t signal);
bool process_signal_is_supported(uint64_t signal);
bool process_signal_pid(uint64_t pid, uint64_t signal);
bool process_signal_target(int64_t pid, uint64_t signal);
int64_t process_signal_action_current(uint64_t signal);
bool process_signal_config_current(uint64_t signal, uint64_t action);
bool process_signal_pending_current(void);
uint64_t process_signal_pending_mask_current(void);
uint64_t process_signal_poll_current(void);
bool process_signal_mask_current(uint64_t how, uint64_t set, uint64_t *oldset_out);
uint64_t process_signal_consume_current(uint64_t mask);
bool process_interrupt_foreground(uint64_t signal);
bool process_set_group(uint64_t pid, uint64_t group);
uint64_t process_group(uint64_t pid);
uint64_t process_session(uint64_t pid);
bool process_create_session_current(uint64_t *session_out);
void process_set_foreground_group(uint64_t group);
uint64_t process_foreground_group(void);
bool process_background_active(void);
bool process_background_killed(void);
const char *process_background_name(void);
uint64_t process_background_status(void);
bool process_kill_background(void);
bool process_should_exit_current(void);
uint64_t process_current_kill_status(void);
bool process_has_open_vfs_prefix(const char *prefix);
struct process *process_current(void);
void process_mark_ready(void);
bool process_wait_startup(uint64_t pid, uint64_t timeout_ticks);
uint64_t process_pid(const struct process *process);
uint64_t process_parent_pid(const struct process *process);
const char *process_name(const struct process *process);
const char *process_executable_path(const struct process *process);
bool process_current_quiet(void);
bool process_set_stdout_redirect(struct process *process, const char *path, bool append);
int64_t process_stdin_read(struct process *process, uint8_t *buffer, uint64_t length);
int64_t process_input_write(struct process *process, const uint8_t *buffer, uint64_t length);
int64_t process_output_write(struct process *process, uint64_t fd, const uint8_t *buffer, uint64_t length);
void process_refresh_mappings(struct process *process);
struct process_file *process_file_at(struct process *process, uint64_t fd);
/* Side-effect-free classification; redirected files/pipes are never console I/O. */
bool process_fd_is_console(struct process *process, uint64_t fd, bool input);
int64_t process_file_alloc(struct process *process,
    const struct vfs_node *node,
    const uint8_t *data,
    uint64_t size);
int64_t process_file_open_flags(struct process *process,const char *path,uint64_t flags);
int64_t process_file_open_write(struct process *process, const char *path, uint64_t flags);
int64_t process_file_read(struct process *process, uint64_t fd, uint8_t *buffer, uint64_t length);
int64_t process_file_write(struct process *process, uint64_t fd, const uint8_t *buffer, uint64_t length);
/* Opt-in owned RAM capacity for a freshly created private exclusive empty file;
 * ordinary admission is required. No length/cursor/dirty/media change. */
int64_t process_file_reserve_append_ram(struct process *process,uint64_t fd,uint64_t limit);
/* IRQ-masked metadata-only prelude proof; actual copy revalidates its lease. */
bool process_file_reserved_append_entry_candidate(struct process *process,uint64_t fd,uint64_t requested);
bool process_file_wait_reserved_append(struct process *process,uint64_t fd,uint64_t requested);
/* Callback must validate the FULL requested user range before copying count
 * bytes, without allocation, I/O, yielding, signals or IRQ restoration. False
 * may touch only unpublished reserved tail; handled FAULT publishes no size,
 * cursor, dirty or generation state and never changes the captured prefix.
 * False from the
 * outer attempt leaves all file/result state unchanged and requires admission. */
typedef bool (*process_reserved_append_copy_fn)(void *context,void *destination,uint64_t count,uint64_t requested);
bool process_file_try_reserved_append(struct process *process,uint64_t fd,uint64_t requested,
    process_reserved_append_copy_fn copy,void *context,int64_t *result);
/* Caller holds ordinary admission; revoke private optimization on path mutation.
 * Fixed metadata scans only, without allocation, filesystem calls or waiting. */
void process_revoke_reserved_append_path(const char *path);
/* Positional operations never change the shared open-file-description offset.
 * Errors use SRV_ERR_*; buffers must already have passed user access checks. */
int64_t process_file_pread(struct process *process, uint64_t fd, uint8_t *buffer, uint64_t length, int64_t offset);
int64_t process_file_pwrite(struct process *process, uint64_t fd, const uint8_t *buffer, uint64_t length, int64_t offset);
bool process_file_nonblocking(struct process *process, uint64_t fd);
int64_t process_file_pipe(struct process *process, uint64_t fds_out[2]);
struct pty_model;
int64_t process_file_pty_pair(struct process *process,uint64_t fds_out[2]);
struct pty_model *process_tty_model(struct process *process,uint64_t fd);
uint64_t process_tty_handle(struct process *process,uint64_t fd);
bool process_tty_set_group(struct process *process,uint64_t fd,uint64_t group);
void process_tty_hangup(uint64_t handle);
void process_tty_signal(uint64_t handle,uint64_t signal);
int64_t process_file_pipe_pair(struct process *process, uint64_t fds_out[2]);
/* IRQ-disabled bounded nonblocking byte transfer, with no rights cleanup or
 * broken-pipe signal. Scheduler same-process exclusion still applies. */
bool process_file_pipe_io_safe(struct process *process, uint64_t fd, uint64_t length, bool writing);
/* Copy both ring segments while mappings and pipe state are stable. A false
 * return leaves bytes and attached rights queued. */
typedef bool (*process_pipe_copyout)(void *context, const uint8_t *first,
    uint64_t first_length, const uint8_t *second, uint64_t second_length);
int64_t process_file_pipe_read_checked(struct process *process, uint64_t fd,
    uint64_t length, bool dontwait, process_pipe_copyout copyout, void *context);
int64_t process_file_pipe_read_mode(struct process *process, uint64_t fd, uint8_t *buffer, uint64_t length, bool dontwait);
/* Copy a single writable prefix while its user mapping is stable. */
typedef bool (*process_pipe_copyin)(void *context, uint64_t offset, uint8_t *first,
    uint64_t first_length, uint8_t *second, uint64_t second_length);
int64_t process_file_pipe_write_checked(struct process *process, uint64_t fd,
    uint64_t length, bool dontwait, process_pipe_copyin copyin, void *context);
int64_t process_file_pty_read_checked(struct process *,uint64_t,uint64_t,process_pipe_copyout,void *);
int64_t process_file_pty_write_checked(struct process *,uint64_t,uint64_t,process_pipe_copyin,void *);
int64_t process_file_pty_setattr(struct process *,uint64_t,const struct srv_termios *,unsigned);
uint64_t process_fd_generation(struct process *,uint64_t);
int64_t process_file_pipe_write_mode(struct process *process, uint64_t fd, const uint8_t *buffer, uint64_t length, bool dontwait);
int64_t process_file_pipe_read(struct process *process, uint64_t fd, uint8_t *buffer, uint64_t length);
int64_t process_file_pipe_write(struct process *process, uint64_t fd, const uint8_t *buffer, uint64_t length);
int64_t process_unix_bind(struct process *process, const char *path);
int64_t process_unix_listen(struct process *process, uint64_t fd, uint64_t backlog);
int64_t process_unix_connect(struct process *process, const char *path);
int64_t process_unix_accept(struct process *process, uint64_t fd);
int64_t process_unix_unlink(const char *path);
/* The checked receive copies to userspace before committing queue consumption. */
typedef bool (*process_message_copyout)(void *context, const void *data,
    uint64_t length, const struct srv_pipe_message *message);
int64_t process_file_send_message(struct process *process, uint64_t fd,
    const uint8_t *buffer, uint64_t length, const struct srv_pipe_message *message);
int64_t process_file_recv_message_checked(struct process *process, uint64_t fd,
    uint8_t *buffer, uint64_t length, struct srv_pipe_message *message,
    process_message_copyout copyout, void *context);
int64_t process_file_send_rights(struct process *process, uint64_t fd, const int32_t *fds, uint64_t count);
typedef bool (*process_rights_copyout)(void *context, const int32_t *fds,
    uint64_t count, uint64_t flags);
int64_t process_file_recv_rights_checked(struct process *process, uint64_t fd,
    uint64_t capacity, process_rights_copyout copyout, void *context);
uint16_t process_file_poll(struct process *process, int64_t fd, uint16_t events);
/* GUI wait snapshots only explicit nonblocking pipe/PTY endpoints. Replaced
 * descriptor numbers report NVAL; readiness is suppressed during owned I/O. */
int process_file_watch_capture(struct process *process,int64_t fd,uint16_t events,uint64_t *generation);
uint16_t process_file_watch_poll(struct process *process,int64_t fd,uint16_t events,uint64_t generation,bool deferred);
struct scheduler_wait_queue *process_file_poll_wait_queue(void);
void process_file_poll_wake(void);
int64_t process_handle_alloc(struct process *process,
    enum process_file_type type,
    uint64_t handle);
int64_t process_file_close(struct process *process, uint64_t fd);
/* Atomic simple-pipe cleanup only. False leaves descriptor/result unchanged;
 * callers must obtain ordinary I/O admission before the general close path. */
bool process_file_try_close_pipe(struct process *process, uint64_t fd, int64_t *result);
int64_t process_file_dup(struct process *process, uint64_t old_fd);
int64_t process_file_dup_min(struct process *process, uint64_t old_fd, uint64_t minimum, bool cloexec);
int64_t process_file_dup2(struct process *process, uint64_t old_fd, uint64_t new_fd);
int64_t process_file_dup3(struct process *process, uint64_t old_fd, uint64_t new_fd, uint64_t flags);
int64_t process_file_get_flags(struct process *process, uint64_t fd);
int64_t process_file_set_flags(struct process *process, uint64_t fd, uint64_t flags);
int64_t process_file_get_fd_flags(struct process *process, uint64_t fd);
int64_t process_file_set_fd_flags(struct process *process, uint64_t fd, uint64_t flags);
int64_t process_file_get_lock(struct process *process, uint64_t fd, struct srv_flock *lock);
int64_t process_file_set_lock(struct process *process, uint64_t fd, const struct srv_flock *lock, bool wait);
int64_t process_file_truncate(struct process *process, uint64_t fd, uint64_t length);
int64_t process_file_flush(struct process *process, uint64_t fd);
int64_t process_file_sync_all(void);
int64_t process_file_seek(struct process *process, uint64_t fd, int64_t offset, uint64_t whence);
/* These mutation helpers and process_open_file_size consume canonical raw
 * storage paths. Caller owns filesystem exclusion across resolve and use. */
int64_t process_path_utime(const char *path,uint64_t atime,uint64_t mtime);
int64_t process_unlink_file(const char *path);
int64_t process_rename_file(const char *old_path, const char *new_path);
int64_t process_path_context(uint64_t operation,const char *path,char *out,uint64_t capacity);
/* Caller holds CPU0 filesystem exclusion across resolution and use. */
int64_t process_resolve_storage_path(const char *input,char *out,uint64_t capacity);
int64_t process_directory(uint64_t operation,uint64_t fd,void *out,uint64_t capacity);
bool process_open_file_size(const char *path,uint64_t *size);
int64_t process_file_stat(struct process *process,
    uint64_t fd,
    struct vfs_metadata *metadata_out,
    uint64_t *size_out,
    uint64_t *type_out);
int64_t process_file_chmod(struct process *process, uint64_t fd, uint64_t mode);
int64_t process_file_utime(struct process *process, uint64_t fd, uint64_t atime, uint64_t mtime);
int64_t process_sbrk(struct process *process, int64_t increment, uint64_t *previous_out);
int64_t process_mmap(struct process *process,
    uint64_t address,
    uint64_t length,
    uint64_t protection,
    uint64_t flags,
    int64_t fd,
    int64_t offset);
int64_t process_munmap(struct process *process, uint64_t address, uint64_t length);
/* Atomic bounded whole-anonymous-region retirement, otherwise no mutation.
 * A handled failure retains remaining ownership; false requires the I/O gate. */
/* CPU0, validated sole-main atomic attempts; refusals leave result/state intact.
 * Anonymous RW mapping uses one candidate <=2MiB; first-thread TLS <=2 pages.
 * No file work, service windows or foreign-gate mutation. Generic calls gate. */
bool process_try_thread_create_private(uint64_t entry,uint64_t arg,uint64_t stack_top,uint64_t flags,int64_t *result);
bool process_try_mmap_private(struct process *,uint64_t address,uint64_t length,uint64_t protection,uint64_t flags,int64_t fd,int64_t offset,int64_t *result);
bool process_try_munmap_private(struct process *process, uint64_t address, uint64_t length, int64_t *result);
/* Exact direct child only; consume fully cleaned reapable metadata atomically.
 * False leaves ownership/status unchanged and requires ordinary wait admission. */
bool process_try_wait_retired(uint64_t pid, uint64_t *status, int64_t *result);
/* Copy/status callback runs under IF0 before child claim/accounting. It must
 * validate the complete output before writing and cannot yield or allocate. */
typedef bool (*process_wait_copy_fn)(void *,uint64_t);
bool process_try_wait_retired_checked(uint64_t pid,process_wait_copy_fn,void *,int64_t *result);
int64_t process_wait_checked(int64_t selector,uint64_t flags,process_wait_copy_fn,void *);
int64_t process_mprotect(struct process *process,
    uint64_t address,
    uint64_t length,
    uint64_t protection);
int64_t process_msync(struct process *process,
    uint64_t address,
    uint64_t length,
    uint64_t flags);
int64_t process_thread_create(uint64_t entry, uint64_t arg, uint64_t stack_top, uint64_t flags);
/* Initial thread returns only after all workers exit, for libc exit(0). */
void process_thread_exit(uint64_t value);
int64_t process_thread_join(uint64_t tid, uint64_t *value_out);
int64_t process_thread_detach(uint64_t tid);
int64_t process_thread_status(uint64_t tid);
int64_t process_thread_signal(uint64_t tid,uint64_t signal);
/* CPU0 scheduler-owned accounting, while IRQs are masked. */
void process_account_cpu(void *process,void *thread,bool user,uint64_t elapsed_us);
int64_t process_rlimit(uint64_t resource,const struct srv_rlimit *set,struct srv_rlimit *out);
/* Existing file-size limit and signal semantics for explicit bounded streams. */
uint64_t process_stream_identity(struct process *process);
int64_t process_stream_write_allowed(uint64_t position,uint64_t length);
int64_t process_usage(int64_t who,struct srv_usage *out);
uint64_t process_thread_self(void);
int64_t process_futex_wait(uint32_t *address, uint32_t expected, uint64_t timeout_ticks);
int64_t process_futex_wake(uint32_t *address, uint64_t max_count);
struct process_user_binding {
    struct fpu_state *user_fpu;
    uint64_t fs_base;
    uint64_t kernel_stack_top;
    uint64_t kernel_stack_base,kernel_stack_physical,kernel_stack_frames;
    bool new_worker_allowed;
};
/* CPU0, IRQs already excluded; output unchanged on refusal. Structural ownership
 * only: callers must separately check root preparation before publication.
 * The resolver permits intact killed/exiting bindings for ordinary unwind;
 * worker admission refuses them. Neither operation establishes an FPU proof. */
bool process_resolve_user_binding(void *process, void *user_thread,
    uint64_t image_generation, uint64_t address_space,
    struct process_user_binding *out);
/* New worker READY admission; IRQs already excluded. */
bool process_admit_user_worker(void *process, void *worker,
    uint64_t image_generation, uint64_t address_space,
    struct process_user_binding *out);
struct fpu_state *process_fpu_state(void *process);
uint64_t process_fs_base(void *process);
void process_fpu_save(void *process);
void process_fpu_restore(void *process);
void process_exit(uint64_t status) __attribute__((noreturn));

/* Process-wide creation policy, shared by threads and inherited by children. */
int64_t process_umask(uint64_t mask);
uint64_t process_creation_mode(uint64_t mode);

#endif
