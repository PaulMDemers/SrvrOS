#include <srvros/exec_limits.h>
#include <srvros/path_limits.h>
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdarg.h>
#include <srvros/sys.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define POSIX_PATH_MAX SRV_PATH_MAX
#define SPAWN_ACTION_DUP2 0
#define SPAWN_ACTION_CLOSE 1
#define SPAWN_ACTION_OPEN 2
#define SPAWN_CLOSED_FD (-2)

int __posix_make_path(const char *path, char *out, size_t capacity);
int __posix_signal_dispatch_pending(void);
int __posix_signal_dispatch_pending_restartable(void);

static char *const *effective_envp(char *const envp[]) {
    return envp != 0 ? envp : environ;
}

static int make_exec_path(const char *path, char *out, size_t capacity) {
    if (__posix_make_path(path, out, capacity) < 0) {
        return errno != 0 ? errno : EINVAL;
    }
    return 0;
}

static posix_spawn_file_action_t *spawn_actions(posix_spawn_file_actions_t *file_actions) {
    if (file_actions == 0) {
        return 0;
    }
    return file_actions->dynamic_actions != 0 ?
        file_actions->dynamic_actions :
        file_actions->actions;
}

static const posix_spawn_file_action_t *spawn_actions_const(const posix_spawn_file_actions_t *file_actions) {
    if (file_actions == 0) {
        return 0;
    }
    return file_actions->dynamic_actions != 0 ?
        file_actions->dynamic_actions :
        file_actions->actions;
}

static int spawn_file_actions_reserve(posix_spawn_file_actions_t *file_actions) {
    if (file_actions->action_count < file_actions->action_capacity) {
        return 0;
    }
    if (file_actions->action_capacity >= POSIX_SPAWN_FILE_ACTIONS_MAX) {
        return ENOMEM;
    }
    size_t next = file_actions->action_capacity * 2;
    if (next < POSIX_SPAWN_FILE_ACTIONS_INLINE) {
        next = POSIX_SPAWN_FILE_ACTIONS_INLINE;
    }
    if (next > POSIX_SPAWN_FILE_ACTIONS_MAX) {
        next = POSIX_SPAWN_FILE_ACTIONS_MAX;
    }
    posix_spawn_file_action_t *grown = malloc(next * sizeof(grown[0]));
    if (grown == 0) {
        return ENOMEM;
    }
    const posix_spawn_file_action_t *current = spawn_actions_const(file_actions);
    if (current != 0 && file_actions->action_count != 0) {
        memcpy(grown, current, file_actions->action_count * sizeof(grown[0]));
    }
    free(file_actions->dynamic_actions);
    file_actions->dynamic_actions = grown;
    file_actions->action_capacity = next;
    return 0;
}

static int spawn_file_actions_append(posix_spawn_file_actions_t *file_actions,
    const posix_spawn_file_action_t *action) {
    int error = spawn_file_actions_reserve(file_actions);
    if (error != 0) {
        return error;
    }
    spawn_actions(file_actions)[file_actions->action_count++] = *action;
    return 0;
}

static void close_spawn_opened(int opened[3], int opened_actions[POSIX_SPAWN_FILE_ACTIONS_MAX]) {
    for (int i = 0; i < 3; i++) {
        if (opened[i] >= 0) {
            close(opened[i]);
        }
    }
    for (size_t i = 0; i < POSIX_SPAWN_FILE_ACTIONS_MAX; i++) {
        if (opened_actions[i] >= 0) {
            close(opened_actions[i]);
        }
    }
}

static int spawn_path(pid_t *pid,
    const char *path,
    const posix_spawn_file_actions_t *file_actions,
    const posix_spawnattr_t *attrp,
    char *const argv[],
    char *const envp[]) {
    char full[POSIX_PATH_MAX];
    int error = make_exec_path(path, full, sizeof(full));
    if (error != 0) {
        return error;
    }

    int stdin_fd = file_actions != 0 ? file_actions->stdin_fd : -1;
    int stdout_fd = file_actions != 0 ? file_actions->stdout_fd : -1;
    int stderr_fd = file_actions != 0 ? file_actions->stderr_fd : -1;
    int opened[3] = {-1, -1, -1};
    int opened_actions[POSIX_SPAWN_FILE_ACTIONS_MAX];
    struct srv_spawn_file_action srv_actions[POSIX_SPAWN_FILE_ACTIONS_MAX];
    uint64_t srv_action_count = 0;
    for (size_t i = 0; i < POSIX_SPAWN_FILE_ACTIONS_MAX; i++) {
        opened_actions[i] = -1;
    }
    if (file_actions != 0) {
        int actions[3] = {file_actions->stdin_action, file_actions->stdout_action, file_actions->stderr_action};
        int oflags[3] = {file_actions->stdin_oflag, file_actions->stdout_oflag, file_actions->stderr_oflag};
        mode_t modes[3] = {file_actions->stdin_mode, file_actions->stdout_mode, file_actions->stderr_mode};
        const char *paths[3] = {file_actions->stdin_path, file_actions->stdout_path, file_actions->stderr_path};
        int *fds[3] = {&stdin_fd, &stdout_fd, &stderr_fd};
        for (int i = 0; i < 3; i++) {
            if (actions[i] == SPAWN_ACTION_CLOSE) {
                *fds[i] = SPAWN_CLOSED_FD;
            } else if (actions[i] == SPAWN_ACTION_OPEN) {
                opened[i] = open(paths[i], oflags[i], modes[i]);
                if (opened[i] < 0) {
                    int saved = errno != 0 ? errno : EIO;
                    close_spawn_opened(opened, opened_actions);
                    return saved;
                }
                *fds[i] = opened[i];
            }
        }
        for (size_t i = 0; i < file_actions->action_count; i++) {
            const posix_spawn_file_action_t *actions = spawn_actions_const(file_actions);
            const posix_spawn_file_action_t *action = &actions[i];
            if (srv_action_count >= POSIX_SPAWN_FILE_ACTIONS_MAX) {
                close_spawn_opened(opened, opened_actions);
                return ENOMEM;
            }
            if (action->type == SPAWN_ACTION_CLOSE) {
                srv_actions[srv_action_count++] = (struct srv_spawn_file_action) {
                    .type = SRV_SPAWN_FILE_ACTION_CLOSE,
                    .fd = action->fd,
                    .new_fd = -1,
                };
            } else if (action->type == SPAWN_ACTION_DUP2) {
                srv_actions[srv_action_count++] = (struct srv_spawn_file_action) {
                    .type = SRV_SPAWN_FILE_ACTION_DUP2,
                    .fd = action->fd,
                    .new_fd = action->newfd,
                };
            } else if (action->type == SPAWN_ACTION_OPEN) {
                int fd = open(action->path, action->oflag, action->mode);
                if (fd < 0) {
                    int saved = errno != 0 ? errno : EIO;
                    close_spawn_opened(opened, opened_actions);
                    return saved;
                }
                opened_actions[i] = fd;
                srv_actions[srv_action_count++] = (struct srv_spawn_file_action) {
                    .type = SRV_SPAWN_FILE_ACTION_DUP2,
                    .fd = fd,
                    .new_fd = action->fd,
                };
            } else {
                close_spawn_opened(opened, opened_actions);
                return EINVAL;
            }
        }
    }

    uint64_t exec_flags = SRV_EXEC_BACKGROUND;
    if (attrp != 0 && (attrp->flags & POSIX_SPAWN_SETSIGMASK) != 0) {
        exec_flags |= SRV_EXEC_SET_SIGNAL_MASK;
    }
    if (attrp != 0 && (attrp->flags & POSIX_SPAWN_SETSIGDEF) != 0) {
        exec_flags |= SRV_EXEC_SET_SIGNAL_DEFAULT;
    }

    struct srv_exec_request request = {
        .path = full,
        .argv = argv,
        .envp = effective_envp(envp),
        .flags = exec_flags,
        .stdin_fd = stdin_fd,
        .stdout_fd = stdout_fd,
        .stderr_fd = stderr_fd,
        .process_group = attrp != 0 && (attrp->flags & POSIX_SPAWN_SETPGROUP) != 0 ?
            (attrp->pgroup == 0 ? SRV_EXEC_GROUP_SELF : (uint64_t)attrp->pgroup) :
            0,
        .file_actions = srv_action_count != 0 ? srv_actions : 0,
        .file_action_count = srv_action_count,
        .signal_mask = attrp != 0 ? (uint64_t)attrp->sigmask : 0,
        .signal_default = attrp != 0 ? (uint64_t)attrp->sigdefault : 0,
    };
    long result = srv_exec(&request);
    int saved_errno = errno;
    close_spawn_opened(opened, opened_actions);
    errno = saved_errno;
    if (result < 0) {
        return (int)-result;
    }
    if (pid != 0) {
        *pid = (pid_t)result;
    }
    return 0;
}

int posix_spawn(pid_t *pid,
    const char *path,
    const posix_spawn_file_actions_t *file_actions,
    const posix_spawnattr_t *attrp,
    char *const argv[],
    char *const envp[]) {
    if (pid == 0 || path == 0 || path[0] == '\0' || argv == 0) {
        return EINVAL;
    }
    return spawn_path(pid, path, file_actions, attrp, argv, envp);
}

int posix_spawnp(pid_t *pid,
    const char *file,
    const posix_spawn_file_actions_t *file_actions,
    const posix_spawnattr_t *attrp,
    char *const argv[],
    char *const envp[]) {
    if (pid == 0 || file == 0 || file[0] == '\0' || argv == 0) {
        return EINVAL;
    }
    if (strchr(file, '/') != 0) {
        return spawn_path(pid, file, file_actions, attrp, argv, envp);
    }

    const char *path=getenv("PATH");
    if(!path) path="/fat/bin:/:/fat";
    int denied=0;
    for(;;) {
        size_t length=strcspn(path,":");
        size_t name=strlen(file),directory=length?length:1;
        char candidate[POSIX_PATH_MAX];
        if(directory+1+name>=sizeof(candidate)) return ENAMETOOLONG;
        if(length) memcpy(candidate,path,length);else candidate[0]='.';
        candidate[directory]='/';memcpy(candidate+directory+1,file,name+1);
        int error=spawn_path(pid,candidate,file_actions,attrp,argv,envp);
        if(!error) return 0;
        if(error==EACCES) denied=1;
        else if(error!=ENOENT && error!=ENOTDIR) return error;
        if(!path[length]) break;
        path+=length+1;
    }
    return denied?EACCES:ENOENT;
}

static int exec_shell_fallback(const char *path,char *const argv[]) {
    char *shell_args[SRV_EXEC_ARGS_MAX+1]={"sh",(char *)path};
    unsigned count=2;
    if(argv[0]) for(unsigned i=1;argv[i];i++) {
        if(count==SRV_EXEC_ARGS_MAX) { errno=E2BIG;return -1; }
        shell_args[count++]=argv[i];
    }
    shell_args[count]=NULL;
    return execve("/fat/bin/sh",shell_args,environ);
}

int execvp(const char *file,char *const argv[]) {
    if(!file || !*file || !argv) { errno=EINVAL;return -1; }
    if(strchr(file,'/')) {
        execve(file,argv,environ);
        return errno==ENOEXEC?exec_shell_fallback(file,argv):-1;
    }
    const char *path=getenv("PATH");
    if(!path) path="/fat/bin:/:/fat";
    int denied=0;
    for(;;) {
        size_t length=strcspn(path,":");
        size_t name=strlen(file),directory=length?length:1;
        char candidate[POSIX_PATH_MAX];
        if(directory+1+name>=sizeof(candidate)) { errno=ENAMETOOLONG;return -1; }
        if(length) memcpy(candidate,path,length);else candidate[0]='.';
        candidate[directory]='/';memcpy(candidate+directory+1,file,name+1);
        execve(candidate,argv,environ);
        if(errno==ENOEXEC) return exec_shell_fallback(candidate,argv);
        if(errno==EACCES) denied=1;
        else if(errno!=ENOENT && errno!=ENOTDIR) return -1;
        if(!path[length]) break;
        path+=length+1;
    }
    errno=denied?EACCES:ENOENT;return -1;
}
int execv(const char *path,char *const argv[]) { return execve(path,argv,environ); }
/* Match the native vector bound without truncating the caller's arguments. */
static int exec_list(const char *path,const char *arg,va_list ap,int search,int explicit_env) {
    char *args[SRV_EXEC_ARGS_MAX+1];unsigned count=0;
    while(arg) {
        if(count==SRV_EXEC_ARGS_MAX) { errno=E2BIG;return -1; }
        args[count++]=(char *)arg;arg=va_arg(ap,const char *);
    }
    args[count]=NULL;
    char *const *env=explicit_env?va_arg(ap,char *const *):environ;
    return search?execvp(path,args):execve(path,args,(char *const *)env);
}
int execl(const char *path,const char *arg,...) {
    va_list ap;va_start(ap,arg);int result=exec_list(path,arg,ap,0,0);va_end(ap);return result;
}
int execlp(const char *file,const char *arg,...) {
    va_list ap;va_start(ap,arg);int result=exec_list(file,arg,ap,1,0);va_end(ap);return result;
}
int execle(const char *path,const char *arg,...) {
    va_list ap;va_start(ap,arg);int result=exec_list(path,arg,ap,0,1);va_end(ap);return result;
}

int posix_spawn_file_actions_init(posix_spawn_file_actions_t *file_actions) {
    if (file_actions == 0) {
        return EINVAL;
    }
    file_actions->stdin_fd = -1;
    file_actions->stdout_fd = -1;
    file_actions->stderr_fd = -1;
    file_actions->stdin_action = SPAWN_ACTION_DUP2;
    file_actions->stdout_action = SPAWN_ACTION_DUP2;
    file_actions->stderr_action = SPAWN_ACTION_DUP2;
    file_actions->stdin_oflag = 0;
    file_actions->stdout_oflag = 0;
    file_actions->stderr_oflag = 0;
    file_actions->stdin_mode = 0;
    file_actions->stdout_mode = 0;
    file_actions->stderr_mode = 0;
    file_actions->stdin_path[0] = '\0';
    file_actions->stdout_path[0] = '\0';
    file_actions->stderr_path[0] = '\0';
    file_actions->action_count = 0;
    file_actions->action_capacity = POSIX_SPAWN_FILE_ACTIONS_INLINE;
    file_actions->dynamic_actions = 0;
    return 0;
}

int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *file_actions) {
    if (file_actions == 0) {
        return EINVAL;
    }
    free(file_actions->dynamic_actions);
    file_actions->dynamic_actions = 0;
    file_actions->action_count = 0;
    file_actions->action_capacity = POSIX_SPAWN_FILE_ACTIONS_INLINE;
    return 0;
}

int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *file_actions,
    int oldfd,
    int newfd) {
    if (file_actions == 0 || oldfd < 0 || newfd < 0) {
        return EINVAL;
    }
    if (newfd == STDIN_FILENO) {
        file_actions->stdin_fd = oldfd;
        file_actions->stdin_action = SPAWN_ACTION_DUP2;
        return 0;
    }
    if (newfd == STDOUT_FILENO) {
        file_actions->stdout_fd = oldfd;
        file_actions->stdout_action = SPAWN_ACTION_DUP2;
        return 0;
    }
    if (newfd == STDERR_FILENO) {
        file_actions->stderr_fd = oldfd;
        file_actions->stderr_action = SPAWN_ACTION_DUP2;
        return 0;
    }
    posix_spawn_file_action_t action = {
        .type = SPAWN_ACTION_DUP2,
        .fd = oldfd,
        .newfd = newfd,
    };
    return spawn_file_actions_append(file_actions, &action);
}

int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *file_actions,
    int fd) {
    if (file_actions == 0 || fd < 0) {
        return EINVAL;
    }
    if (fd == STDIN_FILENO) {
        file_actions->stdin_action = SPAWN_ACTION_CLOSE;
        file_actions->stdin_fd = SPAWN_CLOSED_FD;
        return 0;
    }
    if (fd == STDOUT_FILENO) {
        file_actions->stdout_action = SPAWN_ACTION_CLOSE;
        file_actions->stdout_fd = SPAWN_CLOSED_FD;
        return 0;
    }
    if (fd == STDERR_FILENO) {
        file_actions->stderr_action = SPAWN_ACTION_CLOSE;
        file_actions->stderr_fd = SPAWN_CLOSED_FD;
        return 0;
    }
    posix_spawn_file_action_t action = {
        .type = SPAWN_ACTION_CLOSE,
        .fd = fd,
        .newfd = -1,
    };
    return spawn_file_actions_append(file_actions, &action);
}

static int spawn_action_store_open(char *destination,
    size_t capacity,
    const char *path) {
    if (path == 0 || path[0] == '\0') {
        return EINVAL;
    }
    size_t length = strlen(path);
    if (length + 1 > capacity) {
        return ENAMETOOLONG;
    }
    memcpy(destination, path, length + 1);
    return 0;
}

int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t *file_actions,
    int fd,
    const char *path,
    int oflag,
    mode_t mode) {
    if (file_actions == 0 || fd < 0) {
        return EINVAL;
    }
    if (fd == STDIN_FILENO) {
        int error = spawn_action_store_open(file_actions->stdin_path, sizeof(file_actions->stdin_path), path);
        if (error != 0) {
            return error;
        }
        file_actions->stdin_action = SPAWN_ACTION_OPEN;
        file_actions->stdin_oflag = oflag;
        file_actions->stdin_mode = mode;
        return 0;
    }
    if (fd == STDOUT_FILENO) {
        int error = spawn_action_store_open(file_actions->stdout_path, sizeof(file_actions->stdout_path), path);
        if (error != 0) {
            return error;
        }
        file_actions->stdout_action = SPAWN_ACTION_OPEN;
        file_actions->stdout_oflag = oflag;
        file_actions->stdout_mode = mode;
        return 0;
    }
    if (fd == STDERR_FILENO) {
        int error = spawn_action_store_open(file_actions->stderr_path, sizeof(file_actions->stderr_path), path);
        if (error != 0) {
            return error;
        }
        file_actions->stderr_action = SPAWN_ACTION_OPEN;
        file_actions->stderr_oflag = oflag;
        file_actions->stderr_mode = mode;
        return 0;
    }
    posix_spawn_file_action_t action = {0};
    int error = spawn_action_store_open(action.path, sizeof(action.path), path);
    if (error != 0) {
        return error;
    }
    action.type = SPAWN_ACTION_OPEN;
    action.fd = fd;
    action.newfd = -1;
    action.oflag = oflag;
    action.mode = mode;
    return spawn_file_actions_append(file_actions, &action);
}

int posix_spawnattr_init(posix_spawnattr_t *attr) {
    if (attr == 0) {
        return EINVAL;
    }
    attr->flags = 0;
    attr->pgroup = 0;
    attr->sigdefault = 0;
    attr->sigmask = 0;
    return 0;
}

int posix_spawnattr_destroy(posix_spawnattr_t *attr) {
    if (attr == 0) {
        return EINVAL;
    }
    return 0;
}

int posix_spawnattr_getflags(const posix_spawnattr_t *attr, short *flags) {
    if (attr == 0 || flags == 0) {
        return EINVAL;
    }
    *flags = attr->flags;
    return 0;
}

int posix_spawnattr_setflags(posix_spawnattr_t *attr, short flags) {
    const short supported = POSIX_SPAWN_RESETIDS |
        POSIX_SPAWN_SETPGROUP |
        POSIX_SPAWN_SETSIGDEF |
        POSIX_SPAWN_SETSIGMASK;
    if (attr == 0 || (flags & ~supported) != 0) {
        return EINVAL;
    }
    attr->flags = flags;
    return 0;
}

int posix_spawnattr_getpgroup(const posix_spawnattr_t *attr, pid_t *pgroup) {
    if (attr == 0 || pgroup == 0) {
        return EINVAL;
    }
    *pgroup = attr->pgroup;
    return 0;
}

int posix_spawnattr_setpgroup(posix_spawnattr_t *attr, pid_t pgroup) {
    if (attr == 0 || pgroup < 0) {
        return EINVAL;
    }
    attr->pgroup = pgroup;
    return 0;
}

int posix_spawnattr_getsigdefault(const posix_spawnattr_t *attr, sigset_t *sigdefault) {
    if (attr == 0 || sigdefault == 0) {
        return EINVAL;
    }
    *sigdefault = attr->sigdefault;
    return 0;
}

int posix_spawnattr_setsigdefault(posix_spawnattr_t *attr, const sigset_t *sigdefault) {
    if (attr == 0 || sigdefault == 0) {
        return EINVAL;
    }
    attr->sigdefault = *sigdefault;
    return 0;
}

int posix_spawnattr_getsigmask(const posix_spawnattr_t *attr, sigset_t *sigmask) {
    if (attr == 0 || sigmask == 0) {
        return EINVAL;
    }
    *sigmask = attr->sigmask;
    return 0;
}

int posix_spawnattr_setsigmask(posix_spawnattr_t *attr, const sigset_t *sigmask) {
    if (attr == 0 || sigmask == 0) {
        return EINVAL;
    }
    attr->sigmask = *sigmask;
    return 0;
}

pid_t waitpid(pid_t pid, int *status, int options) {
    uint64_t raw_status = 0;
    uint64_t flags = SRV_WAIT_POSIX;
    if ((options & ~WNOHANG) != 0) {
        errno = EINVAL;
        return -1;
    }
    if ((options & WNOHANG) != 0) {
        flags |= SRV_WAIT_NOHANG;
    }
    long result;
    for (;;) {
        (void)__posix_signal_dispatch_pending();
        result = srv_wait((uint64_t)pid, &raw_status, flags);
        if (result != SRV_ERR_INTR) {
            break;
        }
        if (!__posix_signal_dispatch_pending_restartable()) {
            errno = EINTR;
            return -1;
        }
    }
    if (result < 0) {
        errno = result < -1 ? (int)-result : ECHILD;
        return -1;
    }
    if (result == 0) {
        return 0;
    }
    if (status != 0) {
        *status = (int)raw_status;
    }
    return (pid_t)result;
}

pid_t wait(int *status) { return waitpid(-1,status,0); }

int execve(const char *path, char *const argv[], char *const envp[]) {
    char full[POSIX_PATH_MAX];
    int error = make_exec_path(path, full, sizeof(full));
    if (error != 0 || argv == 0) {
        errno = error != 0 ? error : EINVAL;
        return -1;
    }

    long result = srv_execve(full, argv, effective_envp(envp));
    errno = result < 0 ? (int)-result : EIO;
    return -1;
}
