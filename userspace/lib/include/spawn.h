#ifndef SRVROS_POSIX_SPAWN_H
#define SRVROS_POSIX_SPAWN_H

#include <sys/types.h>

typedef struct {
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
    int stdin_action;
    int stdout_action;
    int stderr_action;
    int stdin_oflag;
    int stdout_oflag;
    int stderr_oflag;
    mode_t stdin_mode;
    mode_t stdout_mode;
    mode_t stderr_mode;
    char stdin_path[160];
    char stdout_path[160];
    char stderr_path[160];
} posix_spawn_file_actions_t;

typedef struct {
    int reserved;
} posix_spawnattr_t;

int posix_spawn(pid_t *pid,
    const char *path,
    const posix_spawn_file_actions_t *file_actions,
    const posix_spawnattr_t *attrp,
    char *const argv[],
    char *const envp[]);
int posix_spawnp(pid_t *pid,
    const char *file,
    const posix_spawn_file_actions_t *file_actions,
    const posix_spawnattr_t *attrp,
    char *const argv[],
    char *const envp[]);

int posix_spawn_file_actions_init(posix_spawn_file_actions_t *file_actions);
int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *file_actions);
int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *file_actions,
    int oldfd,
    int newfd);
int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *file_actions,
    int fd);
int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t *file_actions,
    int fd,
    const char *path,
    int oflag,
    mode_t mode);
int posix_spawnattr_init(posix_spawnattr_t *attr);
int posix_spawnattr_destroy(posix_spawnattr_t *attr);

#endif
