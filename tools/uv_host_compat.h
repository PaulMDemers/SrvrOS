/* Native host declarations for compiling the local bridge on macOS; these
 * Linux-only entry points are not exercised by the bounds regression. */
#ifdef __APPLE__
#include <unistd.h>
#include <stddef.h>
extern char **environ;
int pipe2(int *, int);
ssize_t getrandom(void *, size_t, unsigned int);
#define MSG_CMSG_CLOEXEC 0
#define SOCK_CLOEXEC 0
#endif
