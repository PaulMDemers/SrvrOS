#ifndef SRVROS_HTTP_CHILD_PREFLIGHT_H
#define SRVROS_HTTP_CHILD_PREFLIGHT_H
#include <stdbool.h>
/* Explicit process/pipe-only guest diagnostic. No sockets/USB/network grant. */
void http_child_preflight_enable(bool isolated_init_ready);
bool http_child_preflight_run(void);
#endif
