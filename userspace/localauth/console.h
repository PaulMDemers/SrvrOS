#ifndef SRVROS_LOCAL_AUTH_CONSOLE_H
#define SRVROS_LOCAL_AUTH_CONSOLE_H

#include <stddef.h>

int local_auth_read_secret(const char *prompt, char *buffer, size_t capacity);
int local_auth_read_name(const char *prompt, char *buffer, size_t capacity);

#endif
