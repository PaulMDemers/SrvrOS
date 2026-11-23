#ifndef LOCAL_AUTH_ACCOUNTS_H
#define LOCAL_AUTH_ACCOUNTS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define LOCAL_AUTH_NAME_MAX 31
#define LOCAL_AUTH_HOME_MAX 80
#define LOCAL_AUTH_PASSWORD_MAX 128
#define LOCAL_AUTH_PASSWORD_MIN 15
struct local_session_identity {
    char home[LOCAL_AUTH_HOME_MAX + 1];
    uint32_t uid, gid;
};
/* Read-only committed-store validation; does not resolve pending markers. */
int local_account_validate_store(void);
/* Privileged callers supply NULL for rejected input. Identity is cleared on failure. */
bool local_account_authenticate(const char *name, const char *password, size_t length,
                                struct local_session_identity *identity);
int local_account_add(const char *name, uint32_t uid, uint32_t gid, const char *password);
int local_account_disable(const char *name);
int local_account_enable(const char *name);
int local_account_recover_store(bool clear);
int local_account_reset_password(const char *name, const char *password);
#ifdef LOCAL_AUTH_ACCOUNT_VM_TEST
enum local_auth_account_phase {
    LOCAL_AUTH_ACCOUNT_BEFORE_RENAME,
    LOCAL_AUTH_ACCOUNT_AFTER_RENAME,
};
void local_auth_account_checkpoint(enum local_auth_account_phase phase);
#endif
#endif
