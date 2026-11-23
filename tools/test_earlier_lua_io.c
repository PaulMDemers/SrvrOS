/* Exercise ordinary stream failures through the shipped Lua launcher. */
#include <srvros/sys.h>
#include <stdio.h>

static char *environment[] = {"PATH=/fat/bin", NULL};
static char *stdin_args[] = {"/fat/bin/lua", NULL};
static char *print_args[] = {"/fat/bin/lua", "-e", "print('lua-output')", NULL};

static long launch(char **args, int input, int output) {
    struct srv_exec_request request = {
        .path = args[0], .argv = args, .envp = environment,
        .flags = SRV_EXEC_BACKGROUND, .stdin_fd = input, .stdout_fd = output,
        .stderr_fd = 2, .process_group = UINT64_MAX,
    };
    return srv_exec(&request);
}

static int check_failure(long pid) {
    if (pid <= 0)
        return 0;
    uint64_t status = 0;
    for (int i = 0; i < 500; i++) {
        long waited = srv_wait((uint64_t)pid, &status, SRV_WAIT_NOHANG);
        if (waited != 0)
            return waited == pid && status == 1;
        srv_sleep_ticks(1);
    }
    (void)srv_kill_signal(pid, SRV_SIGNAL_KILL);
    return 0;
}

int main(void) {
    int read_only = (int)srv_open("/fat/earlier-cli/lua-stdin.lua");
    if (read_only < 0)
        return 1;
    long writer = launch(print_args, 0, read_only);
    srv_close(read_only);
    if (!check_failure(writer))
        return 1;
    puts("lua-io: read-only stdout refused");

    int input[2];
    if (srv_pipe(input) != 0)
        return 1;
    const char chunk[] = "print('lua-read-error-chunk-ran')\n";
    long flags = srv_fcntl(input[0], SRV_F_GETFL, 0);
    if (flags < 0 || srv_fcntl(input[0], SRV_F_SETFL, (uint64_t)flags | SRV_FD_NONBLOCK) < 0 ||
            srv_write(input[1], chunk, sizeof(chunk) - 1) != (long)(sizeof(chunk) - 1)) {
        srv_close(input[0]);
        srv_close(input[1]);
        return 1;
    }
    long reader = launch(stdin_args, input[0], 1);
    srv_close(input[0]);
    int refused = check_failure(reader);
    srv_close(input[1]);
    if (!refused)
        return 1;
    puts("lua-io: incomplete stdin refused with preloaded input");
    return 0;
}
