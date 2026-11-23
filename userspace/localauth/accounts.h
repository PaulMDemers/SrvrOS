#ifndef LOCAL_AUTH_ACCOUNTS_H
#define LOCAL_AUTH_ACCOUNTS_H
#include <stdbool.h>
#include <stdint.h>
#define LOCAL_AUTH_NAME_MAX 31
#define LOCAL_AUTH_HOME_MAX 80
#define LOCAL_AUTH_PASSWORD_MAX 128
#define LOCAL_AUTH_PASSWORD_MIN 15
struct local_account {
    char name[LOCAL_AUTH_NAME_MAX+1], home[LOCAL_AUTH_HOME_MAX+1];
    uint32_t uid,gid;
    bool enabled;
    uint8_t salt[16], verifier[32];
};
/* 1 found, 0 unknown, -1 invalid or unavailable. Privileged caller only. */
int local_account_lookup(const char *name, struct local_account *out);
int local_account_add(const char *name, uint32_t uid, uint32_t gid, const char *password);
int local_account_disable(const char *name);
#endif
