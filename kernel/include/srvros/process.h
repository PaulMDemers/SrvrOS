#ifndef SRVROS_PROCESS_H
#define SRVROS_PROCESS_H

#include <stdbool.h>
#include <stdint.h>

#include <srvros/fpu.h>
#include <srvros/syscall_numbers.h>
#include <srvros/vfs.h>

#define PROCESS_MAX_OPEN_FILES 16
#define PROCESS_MAX_PROCESSES 16

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
    uint64_t flags;
    bool dirty;
    bool failed;
    char path[160];
};

struct process_spawn_file_action {
    uint32_t type;
    int32_t reserved;
    int64_t fd;
    int64_t new_fd;
};

struct process;
struct scheduler_wait_queue;

bool process_run_elf(const char *path);
bool process_run_elf_args(const char *path, const char *args);
int64_t process_spawn_elf(const char *path);
int64_t process_spawn_elf_args(const char *path, const char *args);
int64_t process_spawn_elf_args_redirect(const char *path,
    const char *args,
    const char *stdout_path,
    bool append);
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
    uint64_t file_action_count);
int64_t process_exec_replace(const char *path,
    uint64_t argc,
    const char *const *argv,
    uint64_t envc,
    const char *const *envp,
    int64_t stdin_fd,
    int64_t stdout_fd,
    int64_t stderr_fd);
int64_t process_spawn_background_elf(const char *path);
int64_t process_spawn_background_elf_args(const char *path, const char *args);
int64_t process_spawn_background_elf_args_quiet(const char *path, const char *args);
int64_t process_spawn_background_elf_args_fds(const char *path,
    const char *args,
    int64_t stdin_fd,
    int64_t stdout_fd);
int64_t process_wait(uint64_t pid, uint64_t *status_out, bool nohang);
bool process_start_background(const char *path);
uint64_t process_list(uint64_t start_index,
    uint64_t *pid_out,
    const char **name_out,
    const char **state_out);
bool process_kill_pid(uint64_t pid);
bool process_signal_pid(uint64_t pid, uint64_t signal);
bool process_interrupt_foreground(uint64_t signal);
bool process_set_group(uint64_t pid, uint64_t group);
void process_set_foreground_group(uint64_t group);
bool process_background_active(void);
bool process_background_killed(void);
const char *process_background_name(void);
uint64_t process_background_status(void);
bool process_kill_background(void);
bool process_should_exit_current(void);
uint64_t process_current_kill_status(void);
bool process_has_open_vfs_prefix(const char *prefix);
struct process *process_current(void);
uint64_t process_pid(const struct process *process);
const char *process_name(const struct process *process);
bool process_current_quiet(void);
bool process_set_stdout_redirect(struct process *process, const char *path, bool append);
int64_t process_stdin_read(struct process *process, uint8_t *buffer, uint64_t length);
int64_t process_output_write(struct process *process, uint64_t fd, const uint8_t *buffer, uint64_t length);
void process_refresh_mappings(struct process *process);
struct process_file *process_file_at(struct process *process, uint64_t fd);
int64_t process_file_alloc(struct process *process,
    const struct vfs_node *node,
    const uint8_t *data,
    uint64_t size);
int64_t process_file_open_write(struct process *process, const char *path, uint64_t flags);
int64_t process_file_read(struct process *process, uint64_t fd, uint8_t *buffer, uint64_t length);
int64_t process_file_write(struct process *process, uint64_t fd, const uint8_t *buffer, uint64_t length);
bool process_file_nonblocking(struct process *process, uint64_t fd);
int64_t process_file_pipe(struct process *process, uint64_t fds_out[2]);
int64_t process_file_pipe_read(struct process *process, uint64_t fd, uint8_t *buffer, uint64_t length);
int64_t process_file_pipe_write(struct process *process, uint64_t fd, const uint8_t *buffer, uint64_t length);
uint16_t process_file_poll(struct process *process, int64_t fd, uint16_t events);
struct scheduler_wait_queue *process_file_poll_wait_queue(void);
void process_file_poll_wake(void);
int64_t process_handle_alloc(struct process *process,
    enum process_file_type type,
    uint64_t handle);
int64_t process_file_close(struct process *process, uint64_t fd);
int64_t process_file_dup(struct process *process, uint64_t old_fd);
int64_t process_file_dup2(struct process *process, uint64_t old_fd, uint64_t new_fd);
int64_t process_file_get_flags(struct process *process, uint64_t fd);
int64_t process_file_set_flags(struct process *process, uint64_t fd, uint64_t flags);
int64_t process_file_get_fd_flags(struct process *process, uint64_t fd);
int64_t process_file_set_fd_flags(struct process *process, uint64_t fd, uint64_t flags);
int64_t process_file_get_lock(struct process *process, uint64_t fd, struct srv_flock *lock);
int64_t process_file_set_lock(struct process *process, uint64_t fd, const struct srv_flock *lock, bool wait);
int64_t process_file_truncate(struct process *process, uint64_t fd, uint64_t length);
int64_t process_file_flush(struct process *process, uint64_t fd);
int64_t process_file_seek(struct process *process, uint64_t fd, int64_t offset, uint64_t whence);
int64_t process_file_stat(struct process *process,
    uint64_t fd,
    struct vfs_metadata *metadata_out,
    uint64_t *size_out,
    uint64_t *type_out);
int64_t process_file_chmod(struct process *process, uint64_t fd, uint64_t mode);
int64_t process_sbrk(struct process *process, int64_t increment, uint64_t *previous_out);
int64_t process_mmap(struct process *process,
    uint64_t address,
    uint64_t length,
    uint64_t protection,
    uint64_t flags,
    int64_t fd,
    int64_t offset);
int64_t process_munmap(struct process *process, uint64_t address, uint64_t length);
int64_t process_mprotect(struct process *process,
    uint64_t address,
    uint64_t length,
    uint64_t protection);
int64_t process_msync(struct process *process,
    uint64_t address,
    uint64_t length,
    uint64_t flags);
struct fpu_state *process_fpu_state(void *process);
void process_fpu_save(void *process);
void process_fpu_restore(void *process);
void process_exit(uint64_t status) __attribute__((noreturn));

#endif
