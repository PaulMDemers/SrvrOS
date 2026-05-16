#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <srvros/sys.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define POSIX_PATH_MAX 160
#define SPAWN_ACTION_DUP2 0
#define SPAWN_ACTION_CLOSE 1
#define SPAWN_ACTION_OPEN 2
#define SPAWN_CLOSED_FD (-2)

int __posix_make_path(const char *path, char *out, size_t capacity);

static char *const *effective_envp(char *const envp[]) {
    return envp != 0 ? envp : environ;
}

static int make_exec_path(const char *path, char *out, size_t capacity) {
    if (__posix_make_path(path, out, capacity) < 0) {
        return errno != 0 ? errno : EINVAL;
    }
    return 0;
}

static int spawn_path(pid_t *pid,
    const char *path,
    const posix_spawn_file_actions_t *file_actions,
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
                    for (int j = 0; j < 3; j++) {
                        if (opened[j] >= 0) {
                            close(opened[j]);
                        }
                    }
                    return saved;
                }
                *fds[i] = opened[i];
            }
        }
    }

    struct srv_exec_request request = {
        .path = full,
        .argv = argv,
        .envp = effective_envp(envp),
        .flags = SRV_EXEC_BACKGROUND,
        .stdin_fd = stdin_fd,
        .stdout_fd = stdout_fd,
        .stderr_fd = stderr_fd,
    };
    long result = srv_exec(&request);
    int saved_errno = errno;
    for (int i = 0; i < 3; i++) {
        if (opened[i] >= 0) {
            close(opened[i]);
        }
    }
    errno = saved_errno;
    if (result < 0) {
        return ENOENT;
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
    (void)attrp;
    if (pid == 0 || path == 0 || path[0] == '\0' || argv == 0) {
        return EINVAL;
    }
    return spawn_path(pid, path, file_actions, argv, envp);
}

int posix_spawnp(pid_t *pid,
    const char *file,
    const posix_spawn_file_actions_t *file_actions,
    const posix_spawnattr_t *attrp,
    char *const argv[],
    char *const envp[]) {
    (void)attrp;
    if (pid == 0 || file == 0 || file[0] == '\0' || argv == 0) {
        return EINVAL;
    }
    if (strchr(file, '/') != 0) {
        return spawn_path(pid, file, file_actions, argv, envp);
    }

    const char *path = getenv("PATH");
    if (path == 0 || path[0] == '\0') {
        path = "/fat/bin:/:/fat";
    }
    while (*path != '\0') {
        char candidate[POSIX_PATH_MAX];
        size_t used = 0;
        while (*path == ':') {
            path++;
        }
        while (path[used] != '\0' && path[used] != ':' &&
            used + 1 < sizeof(candidate)) {
            candidate[used] = path[used];
            used++;
        }
        if (used == 0) {
            candidate[used++] = '.';
        }
        if (used + 1 >= sizeof(candidate)) {
            return ENAMETOOLONG;
        }
        if (candidate[used - 1] != '/') {
            candidate[used++] = '/';
        }
        for (size_t i = 0; file[i] != '\0'; i++) {
            if (used + 1 >= sizeof(candidate)) {
                return ENAMETOOLONG;
            }
            candidate[used++] = file[i];
        }
        candidate[used] = '\0';
        if (access(candidate, X_OK) == 0) {
            return spawn_path(pid, candidate, file_actions, argv, envp);
        }
        path += strcspn(path, ":");
    }
    return ENOENT;
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
    return 0;
}

int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *file_actions) {
    if (file_actions == 0) {
        return EINVAL;
    }
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
    return ENOSYS;
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
    return ENOSYS;
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
    return ENOSYS;
}

int posix_spawnattr_init(posix_spawnattr_t *attr) {
    if (attr == 0) {
        return EINVAL;
    }
    attr->reserved = 0;
    return 0;
}

int posix_spawnattr_destroy(posix_spawnattr_t *attr) {
    if (attr == 0) {
        return EINVAL;
    }
    return 0;
}

pid_t waitpid(pid_t pid, int *status, int options) {
    uint64_t raw_status = 0;
    uint64_t flags = 0;
    if (pid < 0 || (options & ~WNOHANG) != 0) {
        errno = EINVAL;
        return -1;
    }
    if ((options & WNOHANG) != 0) {
        flags |= SRV_WAIT_NOHANG;
    }
    long result = srv_wait((uint64_t)pid, &raw_status, flags);
    if (result < 0) {
        errno = ECHILD;
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

int execve(const char *path, char *const argv[], char *const envp[]) {
    char full[POSIX_PATH_MAX];
    int error = make_exec_path(path, full, sizeof(full));
    if (error != 0 || argv == 0) {
        errno = error != 0 ? error : EINVAL;
        return -1;
    }

    long result = srv_execve(full, argv, effective_envp(envp));
    errno = result < 0 ? ENOENT : EIO;
    return -1;
}
