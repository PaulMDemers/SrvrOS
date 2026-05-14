#include <errno.h>
#include <spawn.h>
#include <srvros/sys.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define POSIX_PATH_MAX 160

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

    struct srv_exec_request request = {
        .path = full,
        .argv = argv,
        .envp = effective_envp(envp),
        .flags = SRV_EXEC_BACKGROUND,
        .stdin_fd = file_actions != 0 ? file_actions->stdin_fd : -1,
        .stdout_fd = file_actions != 0 ? file_actions->stdout_fd : -1,
        .stderr_fd = file_actions != 0 ? file_actions->stderr_fd : -1,
    };
    long result = srv_exec(&request);
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
        return 0;
    }
    if (newfd == STDOUT_FILENO) {
        file_actions->stdout_fd = oldfd;
        return 0;
    }
    if (newfd == STDERR_FILENO) {
        file_actions->stderr_fd = oldfd;
        return 0;
    }
    return ENOSYS;
}

int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *file_actions,
    int fd) {
    (void)file_actions;
    (void)fd;
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

    long status = srv_execve(full, argv, effective_envp(envp));
    if (status < 0) {
        errno = ENOENT;
        return -1;
    }

    /*
     * srvros does not have process image replacement yet. This compatibility
     * wrapper runs the target as a foreground child and returns its exit code.
     */
    return (int)status;
}
