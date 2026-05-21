#ifndef SRVROS_POSIX_SYS_PARAM_H
#define SRVROS_POSIX_SYS_PARAM_H

#include <limits.h>

#ifndef MAXPATHLEN
#define MAXPATHLEN PATH_MAX
#endif

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN HOST_NAME_MAX
#endif

#ifndef howmany
#define howmany(x, y) (((x) + ((y) - 1)) / (y))
#endif

#ifndef roundup
#define roundup(x, y) ((((x) + ((y) - 1)) / (y)) * (y))
#endif

#endif
