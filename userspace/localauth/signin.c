#include "accounts.h"
#include "console.h"
#include "crypto.h"

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <srvros/sys.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define SESSION_PATH "/fat/bin/local-session"
#define SESSION_RETIRE_US 10000000

enum session_result { SESSION_CLOSED, SESSION_UNAVAILABLE, SESSION_UNSAFE };

static bool trusted_directory(const char *path) {
    struct stat directory;
    return stat(path, &directory) == 0 && S_ISDIR(directory.st_mode) &&
        directory.st_uid == 0 && !(directory.st_mode & 0022);
}

static bool admissible_session(const struct local_session_identity *account) {
    struct stat home, executable;
    return trusted_directory("/fat/home") && trusted_directory("/fat/bin") &&
        stat(account->home, &home) == 0 && S_ISDIR(home.st_mode) &&
        home.st_uid == account->uid && home.st_gid == account->gid &&
        (home.st_mode & 07777) == 0700 &&
        stat(SESSION_PATH, &executable) == 0 && S_ISREG(executable.st_mode) &&
        executable.st_uid == 0 && executable.st_gid == 0 &&
        (executable.st_mode & 07777) == 0755;
}

static long launch_session(uint64_t scope, int slave, const struct local_session_identity *account) {
    char home[sizeof("HOME=") + LOCAL_AUTH_HOME_MAX];
    char uid[21], gid[21];
    if (snprintf(home, sizeof(home), "HOME=%s", account->home) >= (int)sizeof(home))
        return SRV_ERR_INVAL;
    snprintf(uid, sizeof(uid), "%" PRIu32, account->uid);
    snprintf(gid, sizeof(gid), "%" PRIu32, account->gid);
    char *argv[] = {SESSION_PATH, "--enter", uid, gid, NULL};
    char *envp[] = {home, NULL};
    struct srv_exec_request request = {
        .path = SESSION_PATH, .argv = argv, .envp = envp,
        .flags = SRV_EXEC_BACKGROUND | SRV_EXEC_SET_SIGNAL_DEFAULT,
        .stdin_fd = slave, .stdout_fd = slave, .stderr_fd = slave,
        .process_group = UINT64_MAX, .foreground = 1,
        .signal_default = UINT64_C(1) << SIGINT
    };
    struct srv_scope_exec scoped = {
        .version = SRV_SCOPE_VERSION, .size = sizeof(scoped), .request = &request
    };
    return srv_session_scope(SRV_SCOPE_EXEC, scope, &scoped);
}

static bool retire_session(uint64_t scope, long child, bool waited) {
    if (srv_session_scope(SRV_SCOPE_REVOKE, scope, NULL) != 0) return false;
    if (srv_session_scope(SRV_SCOPE_TERMINATE, scope, NULL) != 0) return false;
    long deadline = srv_monotonic_us() + SESSION_RETIRE_US;
    do {
        if (child > 0 && !waited) {
            uint64_t status;
            long result = srv_wait(child, &status, SRV_WAIT_NOHANG);
            if (result == child) waited = true;
            else if (result && result != SRV_ERR_INTR) return false;
        }
        for (unsigned i = 0; i < 16; i++) {
            long result = srv_session_scope(SRV_SCOPE_REAP_RETIRED, scope, NULL);
            if (result < 0) return false;
            if (!result) break;
        }
        struct srv_scope_status state = {
            .version = SRV_SCOPE_VERSION, .size = sizeof(state)
        };
        if (srv_session_scope(SRV_SCOPE_STATUS, scope, &state) != 0 || !state.revoked)
            return false;
        if (!state.members)
            return srv_session_scope(SRV_SCOPE_RELEASE, scope, NULL) == 0;
        srv_sleep_ticks(1);
    } while (srv_monotonic_us() < deadline);
    return false;
}

/* Physical console VMIN=0 makes this a nonblocking discard with no echo. */
static bool discard_console_input(void) {
    unsigned char byte;
    for (unsigned i = 0; i < 4096; i++) {
        long count = srv_read(STDIN_FILENO, &byte, 1);
        if (!count) return true;
        if (count != 1 && count != SRV_ERR_INTR) return false;
    }
    return false;
}

/* One bounded read each turn leaves time to check the leader and relay input. */
static long forward_output(int master) {
    char data[256];
    long count = srv_read(master, data, sizeof(data));
    if (count == SRV_ERR_AGAIN || count == SRV_ERR_INTR) return 0;
    if (count <= 0) return count;
    for (long done = 0; done < count;) {
        long sent = srv_write(STDOUT_FILENO, data + done, (size_t)(count - done));
        if (sent == SRV_ERR_INTR) continue;
        if (sent <= 0) return SRV_ERR_IO;
        done += sent;
    }
    return count;
}

static bool relay_session(int master, long child, uint64_t *status, bool *waited) {
    bool started = false, pending = false;
    unsigned char byte = 0;
    for (;;) {
        long exited = srv_wait(child, status, SRV_WAIT_NOHANG);
        if (exited == child) {
            *waited = true;
            return true;
        }
        if (exited && exited != SRV_ERR_INTR) return false;
        struct srv_pollfd fds[] = {
            {.fd = master, .events = SRV_POLLIN},
            {.fd = started && !pending ? STDIN_FILENO : -1, .events = SRV_POLLIN}
        };
        long ready = srv_poll(fds, 2, 25);
        if (ready == SRV_ERR_INTR) continue;
        if (ready < 0 || (fds[0].revents & (SRV_POLLERR | SRV_POLLNVAL))) return false;
        if (fds[0].revents & SRV_POLLIN) {
            long count = forward_output(master);
            if (count < 0) return false;
            /* The trusted entry code produces no output before dropping. */
            if (count) started = true;
        }
        if (fds[0].revents & SRV_POLLHUP) {
            srv_sleep_ticks(1);
            continue;
        }
        if (fds[1].revents & SRV_POLLIN) {
            long count = srv_read(STDIN_FILENO, &byte, 1);
            if (count < 0 && count != SRV_ERR_INTR) return false;
            pending = count == 1 && ((byte >= ' ' && byte <= '~') ||
                byte == '\n' || byte == '\r' || byte == 127 || byte == '\b' ||
                byte == 3 || byte == 4 || byte == 21);
            if (byte == '\b') byte = 127;
        }
        if (pending) {
            long count = srv_write(master, &byte, 1);
            if (count == 1) pending = false;
            else if (count != SRV_ERR_AGAIN && count != SRV_ERR_INTR && count != SRV_ERR_PERM)
                return false;
            /* A closing slave can refuse input before wait reports its exit. */
        }
    }
}

static enum session_result run_session(const struct local_session_identity *account) {
    if (!admissible_session(account)) return SESSION_UNAVAILABLE;
    struct termios saved, terminal;
    if (tcgetattr(STDIN_FILENO, &saved) < 0) return SESSION_UNSAFE;
    int pty[2];
    if (srv_pty_create(pty) < 0) return SESSION_UNAVAILABLE;
    bool configured = srv_fcntl(pty[0], SRV_F_SETFL, SRV_FD_NONBLOCK) == 0 &&
        tcgetattr(pty[1], &terminal) == 0;
    if (configured) {
        /* The physical console already handles line-ending translation. */
        terminal.c_oflag &= ~OPOST;
        configured = tcsetattr(pty[1], TCSANOW, &terminal) == 0;
    }
    struct termios relay = saved;
    relay.c_lflag &= ~(ECHO | ICANON | ISIG);
    relay.c_cc[VMIN] = 0;
    bool console = configured && tcsetattr(STDIN_FILENO, TCSANOW, &relay) == 0;
    bool clean_input = console && discard_console_input();
    uint64_t scope = 0, status = 0;
    long child = -1;
    bool waited = false;
    bool created = clean_input && srv_session_scope(SRV_SCOPE_CREATE, 0, &scope) == 0;
    if (created) child = launch_session(scope, pty[1], account);
    bool closed_slave = srv_close(pty[1]) == 0;
    bool relayed = child > 0 && closed_slave && relay_session(pty[0], child, &status, &waited);
    bool retired = !created || retire_session(scope, child, waited);
    if (relayed && retired) {
        /* The retired scope cannot refill the PTY while the tail is drained. */
        long count;
        do {
            count = forward_output(pty[0]);
        } while (count > 0);
        relayed = count == 0;
    }
    bool closed_master = srv_close(pty[0]) == 0;
    bool drained = console && discard_console_input();
    bool restored = tcsetattr(STDIN_FILENO, TCSANOW, &saved) == 0;
    if (!retired || !closed_slave || !closed_master || !restored ||
        (configured && (!clean_input || !drained))) return SESSION_UNSAFE;
    if (!relayed || status == 126 || status == 127) return SESSION_UNAVAILABLE;
    if (status == 128 + SIGINT) puts("Session interrupted.");
    else if (status) puts("Session crashed.");
    puts("Session closed.");
    return SESSION_CLOSED;
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
    if (geteuid() != 0 || local_account_validate_store() < 0) return 1;
    /* Physical Ctrl-C can arrive while the console is cooked between prompts
     * or during retry delay. The private session restores default handling. */
    if (signal(SIGINT, SIG_IGN) == SIG_ERR) return 1;
    unsigned failures = 0;
    for (;;) {
        char name[LOCAL_AUTH_NAME_MAX + 1] = {0};
        char password[LOCAL_AUTH_PASSWORD_MAX + 1] = {0};
        struct local_session_identity account;
        int named = local_auth_read_name("Login: ", name, sizeof(name));
        int entered = local_auth_read_secret("Password: ", password, sizeof(password));
        bool authenticated = local_account_authenticate(named == 0 ? name : NULL,
                                                        entered == 0 ? password : NULL,
                                                        strlen(password), &account);
        local_auth_wipe(password, sizeof(password));
        if (authenticated) {
            failures = 0;
            enum session_result result = run_session(&account);
            if (result == SESSION_UNAVAILABLE) puts("Session unavailable.");
            if (result == SESSION_UNSAFE) {
                local_auth_wipe(&account, sizeof(account));
                puts("Console could not be reset. Reboot to sign in.");
                fflush(stdout);
                return 1;
            }
        } else {
            if (failures < 3) failures++;
            if (!delay_failure(failures)) return 1;
            puts("Sign-in failed.");
        }
        local_auth_wipe(&account, sizeof(account));
    }
}
