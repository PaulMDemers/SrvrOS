#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <srvros/gfx.h>
#include <srvros/gui.h>
#include <srvros/mouse.h>
#include <srvros/process_credentials.h>
#include <srvros/sys.h>

#define CHECK(expr) do { if (!(expr)) { printf("desktop-scope: FAIL line=%d\n", __LINE__); return 1; } } while (0)

static char path[] = "/fat/bin/desktop-scope-test";

static int check_display(const char *mode) {
    bool second = !strcmp(mode, "crash");
    unsigned id = second ? 1002 : 1001;
    CHECK(srv_syscall3(SYS_PROCESS_CREDENTIALS, SRV_PROCESS_CREDENTIALS_DROP, id, id) == 0);
    CHECK(getuid() == id && getgid() == id);
    struct gfx_info info = {0};
    if (!strcmp(mode, "ungranted")) {
        CHECK(gfx_info(&info) == SRV_ERR_PERM && gui_register_server() == SRV_ERR_PERM);
        return 0;
    }
    CHECK(gfx_info(&info) == 0 && info.width >= 640 && info.height >= 480);
    CHECK(gfx_console_mute(1) == SRV_ERR_PERM);
    CHECK(gui_register_server() == 0 && gfx_console_mute(1) == 0);
    CHECK(gfx_console_mute(0) == SRV_ERR_PERM);
    CHECK(gui_keyboard_mode(1) == 0 && gui_keyboard_mode(0) == SRV_ERR_PERM);
    struct srv_key_event key = {0};
    struct mouse_event pointer = {0};
    CHECK(gui_keyboard_read(&key) >= 0 && mouse_scan(&pointer) >= 0);
    CHECK(gfx_mode_count() == SRV_ERR_PERM);
    uint32_t pixel = second ? 0x00804020 : 0x00208040;
    CHECK(blitrect(0, 0, 1, 1, &pixel, 1) == 0);

    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) {
        _exit(gfx_info(&info) == SRV_ERR_PERM && gui_register_server() == SRV_ERR_PERM &&
              gfx_console_mute(0) == SRV_ERR_PERM && gui_keyboard_read(&key) == SRV_ERR_PERM &&
              mouse_scan(&pointer) == SRV_ERR_PERM && blitrect(0, 0, 1, 1, &pixel, 1) == SRV_ERR_PERM ? 0 : 1);
    }
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    printf("desktop-scope: uid=%u display and input granted; child refused\n", id);
    fflush(stdout);
    if (second) __asm__ volatile ("ud2");
    return 0;
}

static int run_scope(char *mode, unsigned operation, bool crash) {
    uint64_t scope = 0;
    CHECK(srv_session_scope(SRV_SCOPE_CREATE, 0, &scope) == 0);
    char *args[] = {path, mode, NULL};
    struct srv_exec_request request = {.path = path, .argv = args, .flags = SRV_EXEC_BACKGROUND,
        .stdin_fd = 0, .stdout_fd = 1, .stderr_fd = 2};
    struct srv_scope_exec scoped = {.version = SRV_SCOPE_VERSION, .size = sizeof(scoped), .request = &request};
    long child = srv_session_scope(operation, scope, &scoped);
    CHECK(child > 0);
    uint64_t status = 0;
    CHECK(srv_wait(child, &status, 0) == child && status == (crash ? 128u + SIGILL : 0u));
    CHECK(srv_session_scope(SRV_SCOPE_REVOKE, scope, NULL) == 0);
    CHECK(srv_session_scope(SRV_SCOPE_RELEASE, scope, NULL) == 0);
    CHECK(gfx_console_muted() == 0);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2) return check_display(argv[1]);
    CHECK(argc == 1 && geteuid() == 0);
    CHECK(run_scope("ungranted", SRV_SCOPE_EXEC, false) == 0);
    CHECK(run_scope("normal", SRV_SCOPE_EXEC_DISPLAY, false) == 0);
    CHECK(run_scope("crash", SRV_SCOPE_EXEC_DISPLAY, true) == 0);
    CHECK(run_scope("normal", SRV_SCOPE_EXEC_DISPLAY, false) == 0);
    puts("desktop-scope: PASS grant, descendant refusal, exit, crash and reacquisition");
    return 0;
}
