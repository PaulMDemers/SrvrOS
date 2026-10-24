#ifndef SRVROS_POSIX_SPAWN_H
#define SRVROS_POSIX_SPAWN_H

#include <srvros/path_limits.h>
#include <sys/types.h>
#include <signal.h>

#define POSIX_SPAWN_RESETIDS 0x01
#define POSIX_SPAWN_SETPGROUP 0x02
#define POSIX_SPAWN_SETSIGDEF 0x04
#define POSIX_SPAWN_SETSIGMASK 0x08
#define POSIX_SPAWN_FILE_ACTIONS_INLINE 8
#define POSIX_SPAWN_FILE_ACTIONS_MAX 32

typedef struct {
    int type;
    int fd;
    int newfd;
    int oflag;
    mode_t mode;
    char path[SRV_PATH_MAX];
} posix_spawn_file_action_t;

typedef struct {
    size_t action_count;
    size_t action_capacity;
    posix_spawn_file_action_t *dynamic_actions;
    posix_spawn_file_action_t actions[POSIX_SPAWN_FILE_ACTIONS_INLINE];
} posix_spawn_file_actions_t;

typedef struct {
    short flags;
    pid_t pgroup;
    sigset_t sigdefault;
    sigset_t sigmask;
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
int posix_spawnattr_getflags(const posix_spawnattr_t *attr, short *flags);
int posix_spawnattr_setflags(posix_spawnattr_t *attr, short flags);
int posix_spawnattr_getpgroup(const posix_spawnattr_t *attr, pid_t *pgroup);
int posix_spawnattr_setpgroup(posix_spawnattr_t *attr, pid_t pgroup);
int posix_spawnattr_getsigdefault(const posix_spawnattr_t *attr, sigset_t *sigdefault);
int posix_spawnattr_setsigdefault(posix_spawnattr_t *attr, const sigset_t *sigdefault);
int posix_spawnattr_getsigmask(const posix_spawnattr_t *attr, sigset_t *sigmask);
int posix_spawnattr_setsigmask(posix_spawnattr_t *attr, const sigset_t *sigmask);

#endif
