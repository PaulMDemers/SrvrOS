#include "accounts.h"
#include "console.h"
#include "crypto.h"

#include <errno.h>
#include <srvros/process_credentials.h>
#include <srvros/sys.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SESSION_PATH "/fat/bin/local-session"

static bool trusted_directory(const char *path) {
    struct stat directory;
    return stat(path, &directory) == 0 && S_ISDIR(directory.st_mode) &&
        directory.st_uid == 0 && !(directory.st_mode & 0022);
}

static bool admissible_session(const struct local_account *account) {
    struct stat home, executable;
    return trusted_directory("/fat/home") && trusted_directory("/fat/bin") &&
        stat(account->home, &home) == 0 && S_ISDIR(home.st_mode) &&
        home.st_uid == account->uid && home.st_gid == account->gid &&
        (home.st_mode & 07777) == 0700 &&
        stat(SESSION_PATH, &executable) == 0 && S_ISREG(executable.st_mode) &&
        executable.st_uid == 0 && executable.st_gid == 0 &&
        (executable.st_mode & 07777) == 0755;
}

static bool run_session(const struct local_account *account) {
    char home[sizeof("HOME=") + LOCAL_AUTH_HOME_MAX];
    if (!admissible_session(account)) return false;
    if (snprintf(home, sizeof(home), "HOME=%s", account->home) >= (int)sizeof(home)) return false;
    char *argv[] = {SESSION_PATH, NULL};
    char *envp[] = {home, NULL};
    pid_t child = fork();
    if (child < 0) return false;
    if (child == 0) {
        if (srv_syscall3(SYS_PROCESS_CREDENTIALS, SRV_PROCESS_CREDENTIALS_DROP,
                account->uid, account->gid) < 0 || chdir(account->home) < 0)
            _exit(126);
        execve(SESSION_PATH, argv, envp);
        _exit(127);
    }
    int status;
    pid_t waited;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    return waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/* Bound guessing on the shared console, including unknown account names. */
static bool delay_failure(unsigned failures) {
    struct timespec delay = {.tv_sec = (time_t)(1u << (failures - 1))};
    while (nanosleep(&delay, &delay) < 0) {
        if (errno != EINTR) return false;
    }
    return true;
}

int main(void) {
    if (geteuid() != 0) return 1;
    unsigned failures = 0;
    for (;;) {
        char name[LOCAL_AUTH_NAME_MAX + 1] = {0};
        char password[LOCAL_AUTH_PASSWORD_MAX + 1] = {0};
        struct local_account account = {0};
        uint8_t derived[32] = {0};
        int named = local_auth_read_name("Login: ", name, sizeof(name));
        int entered = local_auth_read_secret("Password: ", password, sizeof(password));
        int found = named == 0 ? local_account_lookup(name, &account) : 0;
        bool computed = local_auth_verifier(password, strlen(password), account.salt, derived);
        bool matches = local_auth_equal(derived, account.verifier);
        bool authenticated = entered == 0 && found == 1 && account.enabled && computed && matches;
        local_auth_wipe(password, sizeof(password));
        local_auth_wipe(derived, sizeof(derived));
        local_auth_wipe(account.verifier, sizeof(account.verifier));
        if (authenticated) {
            failures = 0;
            if (!run_session(&account)) puts("Session unavailable.");
        } else {
            if (failures < 3) failures++;
            if (!delay_failure(failures)) return 1;
            puts("Sign-in failed.");
        }
        local_auth_wipe(&account, sizeof(account));
    }
}
