#ifndef SRVROS_POSIX_SYS_WAIT_H
#define SRVROS_POSIX_SYS_WAIT_H

#include <sys/types.h>

#define WNOHANG 1

#define WIFEXITED(status) ((status) < 128)
#define WEXITSTATUS(status) ((status) & 0xff)
#define WIFSIGNALED(status) ((status) >= 128)
#define WTERMSIG(status) ((status) - 128)

pid_t waitpid(pid_t pid, int *status, int options);

#endif
