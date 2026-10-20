#ifndef SRVROS_WORKSPACE_LOG_H
#define SRVROS_WORKSPACE_LOG_H
#include <stdbool.h>
bool workspace_log_start(void);
bool workspace_log_stop(void);
/* Drain the logger and persist a nonfinal checkpoint; stop can save exit.log
 * in the same directory after a later remount. */
bool workspace_log_pause(void);
#endif
