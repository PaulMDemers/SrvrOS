#ifndef SRVROS_POSIX_ASSERT_H
#define SRVROS_POSIX_ASSERT_H

#include <stdlib.h>

#ifdef NDEBUG
#define assert(expression) ((void)0)
#else
#define assert(expression) ((expression) ? (void)0 : abort())
#endif

#endif
