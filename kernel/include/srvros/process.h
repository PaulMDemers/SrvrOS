#ifndef SRVROS_PROCESS_H
#define SRVROS_PROCESS_H

#include <stdbool.h>
#include <stdint.h>

#include <srvros/vfs.h>

#define PROCESS_MAX_OPEN_FILES 16
#define PROCESS_MAX_PROCESSES 16

enum process_file_type {
    PROCESS_FILE_UNUSED,
    PROCESS_FILE_VFS,
    PROCESS_FILE_VFS_WRITE,
    PROCESS_FILE_NET_LISTENER,
    PROCESS_FILE_NET_CONNECTION,
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
    bool dirty;
    bool failed;
    char path[160];
};

struct process;

bool process_run_elf(const char *path);
bool process_run_elf_args(const char *path, const char *args);
int64_t process_spawn_elf(const char *path);
int64_t process_spawn_elf_args(const char *path, const char *args);
int64_t process_spawn_elf_args_redirect(const char *path,
    const char *args,
    const char *stdout_path,
    bool append);
int64_t process_spawn_background_elf(const char *path);
int64_t process_spawn_background_elf_args(const char *path, const char *args);
int64_t process_wait(uint64_t pid, uint64_t *status_out, bool nohang);
bool process_start_background(const char *path);
uint64_t process_list(uint64_t start_index,
    uint64_t *pid_out,
    const char **name_out,
    const char **state_out);
bool process_kill_pid(uint64_t pid);
bool process_background_active(void);
bool process_background_killed(void);
const char *process_background_name(void);
uint64_t process_background_status(void);
bool process_kill_background(void);
bool process_should_exit_current(void);
bool process_has_open_vfs_prefix(const char *prefix);
struct process *process_current(void);
uint64_t process_pid(const struct process *process);
const char *process_name(const struct process *process);
bool process_set_stdout_redirect(struct process *process, const char *path, bool append);
int64_t process_stdout_write(struct process *process, const uint8_t *buffer, uint64_t length);
void process_refresh_mappings(struct process *process);
struct process_file *process_file_at(struct process *process, uint64_t fd);
int64_t process_file_alloc(struct process *process,
    const struct vfs_node *node,
    const uint8_t *data,
    uint64_t size);
int64_t process_file_open_write(struct process *process, const char *path, uint64_t flags);
int64_t process_file_write(struct process *process, uint64_t fd, const uint8_t *buffer, uint64_t length);
int64_t process_handle_alloc(struct process *process,
    enum process_file_type type,
    uint64_t handle);
int64_t process_file_close(struct process *process, uint64_t fd);
int64_t process_file_seek(struct process *process, uint64_t fd, uint64_t offset);
void process_exit(uint64_t status) __attribute__((noreturn));

#endif
