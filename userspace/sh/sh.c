#include <srvros/cli.h>
#include <srvros/conio.h>
#include <srvros/sys.h>

#define LINE_MAX 256
#define PATH_MAX_ENTRIES 8
#define PIPELINE_MAX_COMMANDS 6

static char path_entries[PATH_MAX_ENTRIES][CLI_PATH_MAX] = {
    "/fat/bin",
    "/",
    "/fat",
};
static size_t path_count = 3;

static void run_line(char *line, char *cwd);

static int read_line(char *line, size_t capacity) {
    size_t length = 0;
    for (;;) {
        char c;
        if (srv_read(SRV_STDIN, &c, 1) != 1) {
            line[length] = '\0';
            return 0;
        }
        if (c == '\n') {
            cli_puts("\n");
            line[length] = '\0';
            return 1;
        }
        if (c == '\b') {
            if (length > 0) {
                length--;
                cli_puts("\b");
            }
            continue;
        }
        if (c >= 32 && c < 127 && length + 1 < capacity) {
            line[length++] = c;
            srv_write(SRV_STDOUT, &c, 1);
        }
    }
}

static int resolve_command(char *out, size_t capacity, const char *command) {
    if (cli_contains_slash(command)) {
        cli_copy(out, capacity, command);
        return 1;
    }
    for (size_t i = 0; i < path_count; i++) {
        if (!cli_join_path(out, capacity, path_entries[i], command)) {
            continue;
        }
        for (uint64_t index = 0;; index++) {
            char listed[CLI_PATH_MAX];
            uint64_t size = 0;
            long result = srv_list(index, listed, sizeof(listed), &size);
            if (result <= 0) {
                break;
            }
            if (cli_streq(listed, out)) {
                (void)size;
                return 1;
            }
        }
        long fd = srv_open(out);
        if (fd >= 0) {
            srv_close((int)fd);
            return 1;
        }
    }
    return 0;
}

static void print_help(void) {
    cli_puts("builtins: help exit source . path cd pwd clear echo jobs wait service dhcp net dns rmdir\n");
    cli_puts("commands: ls cat write cp rm mkdir mv tap wc grep head stat ps kill hello webd spin fpdemo desktop calcgui notesgui textedit imgedit posixdemo zlibdemo lua\n");
    cli_puts("syntax: command [args], quote args with ' or \", use ; between commands, append & for background\n");
    cli_puts("redirection: command > file, command >> file; pipeline: command | command [...]\n");
}

static void print_ipv4(uint64_t ip) {
    cli_putn((ip >> 24) & 0xff);
    cli_puts(".");
    cli_putn((ip >> 16) & 0xff);
    cli_puts(".");
    cli_putn((ip >> 8) & 0xff);
    cli_puts(".");
    cli_putn(ip & 0xff);
}

static void dhcp_command(void) {
    long ip = srv_net_dhcp();
    if (ip < 0) {
        cli_puts("dhcp: failed\n");
        return;
    }
    cli_puts("dhcp: ");
    print_ipv4((uint64_t)ip);
    cli_puts("\n");
}

static void net_command(void) {
    if (srv_net_status() < 0) {
        cli_puts("net: failed\n");
    }
}

static void dns_command(const char *args) {
    char work[LINE_MAX];
    char *name;
    uint32_t ip = 0;
    cli_copy(work, sizeof(work), args);
    name = cli_trim(work);
    if (name[0] == '\0') {
        cli_puts("usage: dns <name>\n");
        return;
    }
    if (srv_net_dns(name, &ip) < 0) {
        cli_puts("dns: failed: ");
        cli_puts(name);
        cli_puts("\n");
        return;
    }
    cli_puts(name);
    cli_puts(" ");
    print_ipv4(ip);
    cli_puts("\n");
}

static void rmdir_command(const char *args) {
    char work[LINE_MAX];
    char *path;
    cli_copy(work, sizeof(work), args);
    path = cli_trim(work);
    if (path[0] == '\0') {
        cli_puts("usage: rmdir <path>\n");
        return;
    }
    if (srv_rmdir(path) < 0) {
        cli_puts("rmdir: failed: ");
        cli_puts(path);
        cli_puts("\n");
    }
}

static uint64_t parse_u64(const char *text) {
    uint64_t value = 0;
    while (*text >= '0' && *text <= '9') {
        value = value * 10 + (uint64_t)(*text - '0');
        text++;
    }
    return value;
}

static void print_jobs(void) {
    uint64_t index = 0;
    struct srv_process_info info;
    cli_puts("PID STATE      NAME\n");
    for (;;) {
        long next = srv_proc_list(index, &info);
        if (next <= 0) {
            break;
        }
        if (!cli_streq(info.state, "foreground")) {
            cli_putn(info.pid);
            cli_puts("   ");
            cli_puts(info.state);
            cli_puts(" ");
            cli_puts(info.name);
            cli_puts("\n");
        }
        index = (uint64_t)next;
    }
}

static void wait_for_job(const char *args) {
    uint64_t status = 0;
    uint64_t pid = 0;
    long waited;

    if (args != 0 && args[0] != '\0') {
        pid = parse_u64(args);
        if (pid == 0) {
            cli_puts("usage: wait [pid]\n");
            return;
        }
    }

    waited = srv_wait(pid, &status, 0);
    if (waited < 0) {
        cli_puts("wait: no matching background process\n");
        return;
    }
    cli_puts("[done] pid ");
    cli_putn((uint64_t)waited);
    cli_puts(" status ");
    cli_putn(status);
    cli_puts("\n");
}

static int process_name_matches(const struct srv_process_info *info, const char *name) {
    char base[CLI_PATH_MAX];
    cli_basename(base, sizeof(base), info->name);
    return cli_streq(info->name, name) || cli_streq(base, name);
}

static int find_service_process(const char *name, struct srv_process_info *out, int include_exited) {
    uint64_t index = 0;
    struct srv_process_info info;
    for (;;) {
        long next = srv_proc_list(index, &info);
        if (next <= 0) {
            break;
        }
        if (process_name_matches(&info, name) &&
            (include_exited || !cli_streq(info.state, "exited"))) {
            *out = info;
            return 1;
        }
        index = (uint64_t)next;
    }
    return 0;
}

static void reap_exited_service(const char *name) {
    struct srv_process_info info;
    uint64_t status = 0;
    while (find_service_process(name, &info, 1) && cli_streq(info.state, "exited")) {
        if (srv_wait(info.pid, &status, SRV_WAIT_NOHANG) <= 0) {
            return;
        }
    }
}

static void service_command(const char *args) {
    char work[LINE_MAX];
    char *name;
    char *action;
    char *action_args;
    struct srv_process_info info;
    uint64_t status = 0;

    cli_copy(work, sizeof(work), args);
    name = cli_trim(work);
    action = name;
    while (*action != '\0' && *action != ' ' && *action != '\t') {
        action++;
    }
    if (*action != '\0') {
        *action++ = '\0';
        action = cli_trim(action);
    }

    if (!cli_streq(name, "webd")) {
        cli_puts("usage: service webd [start [root]|stop|status]\n");
        return;
    }
    if (action[0] == '\0') {
        action = "status";
    }
    action_args = action;
    while (*action_args != '\0' && *action_args != ' ' && *action_args != '\t') {
        action_args++;
    }
    if (*action_args != '\0') {
        *action_args++ = '\0';
        action_args = cli_trim(action_args);
    }

    reap_exited_service("webd");
    if (cli_streq(action, "status")) {
        if (find_service_process("webd", &info, 1)) {
            cli_puts("webd ");
            cli_puts(info.state);
            cli_puts(" pid ");
            cli_putn(info.pid);
            cli_puts("\n");
        } else {
            cli_puts("webd stopped\n");
        }
        return;
    }

    if (cli_streq(action, "start")) {
        if (find_service_process("webd", &info, 0)) {
            cli_puts("webd already running pid ");
            cli_putn(info.pid);
            cli_puts("\n");
            return;
        }
        long pid = srv_spawn_bg_args("/fat/bin/webd", action_args);
        if (pid < 0) {
            cli_puts("webd start failed\n");
            return;
        }
        cli_puts("webd started pid ");
        cli_putn((uint64_t)pid);
        cli_puts("\n");
        return;
    }

    if (cli_streq(action, "stop")) {
        if (!find_service_process("webd", &info, 0)) {
            cli_puts("webd stopped\n");
            return;
        }
        if (srv_kill(info.pid) < 0) {
            cli_puts("webd stop failed\n");
            return;
        }
        long waited = srv_wait(info.pid, &status, 0);
        cli_puts("webd stopped pid ");
        cli_putn(info.pid);
        if (waited > 0) {
            cli_puts(" status ");
            cli_putn(status);
        }
        cli_puts("\n");
        return;
    }

    cli_puts("usage: service webd [start [root]|stop|status]\n");
}

static void print_path(void) {
    for (size_t i = 0; i < path_count; i++) {
        cli_puts(path_entries[i]);
        cli_puts("\n");
    }
}

static void add_path(const char *directory) {
    if (directory == 0 || directory[0] == '\0') {
        cli_puts("usage: path [add <dir>]\n");
        return;
    }
    if (path_count >= PATH_MAX_ENTRIES) {
        cli_puts("path: full\n");
        return;
    }
    cli_copy(path_entries[path_count++], CLI_PATH_MAX, directory);
}

static char *find_argument_tail(char *command) {
    char quote = '\0';
    while (*command != '\0') {
        if (quote != '\0') {
            if (*command == quote) {
                quote = '\0';
            } else if (*command == '\\' && command[1] != '\0') {
                command++;
            }
            command++;
            continue;
        }
        if (*command == '\'' || *command == '"') {
            quote = *command++;
            continue;
        }
        if (*command == ' ' || *command == '\t') {
            break;
        }
        command++;
    }
    return command;
}

static void run_script(const char *path, char *cwd) {
    int fd = (int)srv_open(path);
    char buffer[128];
    char line[LINE_MAX];
    size_t length = 0;
    if (fd < 0) {
        cli_puts("source: cannot open ");
        cli_puts(path);
        cli_puts("\n");
        return;
    }
    for (;;) {
        long count = srv_read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            cli_puts("source: read failed\n");
            break;
        }
        if (count == 0) {
            break;
        }
        for (long i = 0; i < count; i++) {
            char c = buffer[i];
            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                line[length] = '\0';
                run_line(line, cwd);
                length = 0;
            } else if (length + 1 < sizeof(line)) {
                line[length++] = c;
            }
        }
    }
    if (length > 0) {
        line[length] = '\0';
        run_line(line, cwd);
    }
    srv_close(fd);
}

static void split_redirection(char *args, char **redirect_path, int *append) {
    char quote = '\0';
    *redirect_path = 0;
    *append = 0;
    for (char *cursor = args; *cursor != '\0'; cursor++) {
        if (quote != '\0') {
            if (*cursor == quote) {
                quote = '\0';
            } else if (*cursor == '\\' && cursor[1] != '\0') {
                cursor++;
            }
            continue;
        }
        if (*cursor == '\'' || *cursor == '"') {
            quote = *cursor;
            continue;
        }
        if (*cursor == '>') {
            *cursor++ = '\0';
            if (*cursor == '>') {
                *append = 1;
                *cursor++ = '\0';
            }
            *redirect_path = cli_trim(cursor);
            args = cli_trim(args);
            return;
        }
    }
}

static int split_pipeline_segments(char *line, char **segments, size_t capacity) {
    char quote = '\0';
    size_t count = 1;
    segments[0] = cli_trim(line);
    for (char *cursor = line; *cursor != '\0'; cursor++) {
        if (quote != '\0') {
            if (*cursor == quote) {
                quote = '\0';
            } else if (*cursor == '\\' && cursor[1] != '\0') {
                cursor++;
            }
            continue;
        }
        if (*cursor == '\'' || *cursor == '"') {
            quote = *cursor;
            continue;
        }
        if (*cursor == '|') {
            if (count >= capacity) {
                return -1;
            }
            *cursor = '\0';
            segments[count++] = cli_trim(cursor + 1);
        }
    }
    return (int)count;
}

static int prepare_external_command(char *line,
    char *path,
    size_t path_capacity,
    char **args_out,
    char **redirect_path,
    int *redirect_append,
    int allow_redirect) {
    char *command = cli_trim(line);
    char *args = find_argument_tail(command);

    *redirect_path = 0;
    *redirect_append = 0;
    if (*command == '\0' || *command == '#') {
        return 0;
    }
    if (*args != '\0') {
        *args++ = '\0';
        args = cli_trim(args);
    }
    split_redirection(args, redirect_path, redirect_append);
    if (*redirect_path != 0 && !allow_redirect) {
        cli_puts("sh: pipeline redirection unsupported\n");
        return 0;
    }
    args = cli_trim(args);
    if (!resolve_command(path, path_capacity, command)) {
        cli_puts("sh: command not found: ");
        cli_puts(command);
        cli_puts("\n");
        return 0;
    }
    *args_out = args;
    return 1;
}

struct pipeline_command {
    char path[CLI_PATH_MAX];
    char *args;
    char *redirect_path;
    int redirect_append;
};

static void close_pipeline_fds(int pipes[][2], size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (pipes[i][0] >= 0) {
            srv_close(pipes[i][0]);
            pipes[i][0] = -1;
        }
        if (pipes[i][1] >= 0) {
            srv_close(pipes[i][1]);
            pipes[i][1] = -1;
        }
    }
}

static void wait_pipeline_pids(long *pids, size_t count, uint64_t *last_status) {
    for (size_t i = 0; i < count; i++) {
        uint64_t status = 0;
        if (pids[i] >= 0) {
            (void)srv_wait((uint64_t)pids[i], &status, 0);
            if (i + 1 == count && last_status != 0) {
                *last_status = status;
            }
        }
    }
}

static void run_pipeline(char **segments, size_t segment_count) {
    struct pipeline_command commands[PIPELINE_MAX_COMMANDS];
    int pipes[PIPELINE_MAX_COMMANDS - 1][2];
    long pids[PIPELINE_MAX_COMMANDS];
    uint64_t final_status = 0;
    int output_fd = -1;

    if (segment_count < 2 || segment_count > PIPELINE_MAX_COMMANDS) {
        cli_puts("sh: pipeline too long\n");
        return;
    }

    for (size_t i = 0; i < PIPELINE_MAX_COMMANDS; i++) {
        pids[i] = -1;
        if (i + 1 < PIPELINE_MAX_COMMANDS) {
            pipes[i][0] = -1;
            pipes[i][1] = -1;
        }
    }

    for (size_t i = 0; i < segment_count; i++) {
        if (!prepare_external_command(segments[i],
                commands[i].path,
                sizeof(commands[i].path),
                &commands[i].args,
                &commands[i].redirect_path,
                &commands[i].redirect_append,
                i + 1 == segment_count)) {
            return;
        }
    }

    if (commands[segment_count - 1].redirect_path != 0) {
        uint64_t flags = SRV_OPEN_WRITE | SRV_OPEN_CREATE |
            (commands[segment_count - 1].redirect_append ? SRV_OPEN_APPEND : SRV_OPEN_TRUNC);
        output_fd = (int)srv_open_mode(commands[segment_count - 1].redirect_path, flags);
        if (output_fd < 0) {
            cli_puts("sh: pipeline redirect failed\n");
            return;
        }
    }

    for (size_t i = 0; i + 1 < segment_count; i++) {
        if (srv_pipe(pipes[i]) < 0) {
            cli_puts("sh: pipe failed\n");
            close_pipeline_fds(pipes, segment_count - 1);
            if (output_fd >= 0) {
                srv_close(output_fd);
            }
            return;
        }
    }

    for (size_t i = 0; i < segment_count; i++) {
        int stdin_fd = i == 0 ? -1 : pipes[i - 1][0];
        int stdout_fd = i + 1 == segment_count ? output_fd : pipes[i][1];
        pids[i] = srv_spawn_bg_args_fds(commands[i].path, commands[i].args, stdin_fd, stdout_fd);
        if (pids[i] < 0) {
            cli_puts("sh: pipeline spawn failed\n");
            close_pipeline_fds(pipes, segment_count - 1);
            if (output_fd >= 0) {
                srv_close(output_fd);
            }
            wait_pipeline_pids(pids, i, 0);
            return;
        }
    }

    close_pipeline_fds(pipes, segment_count - 1);
    if (output_fd >= 0) {
        srv_close(output_fd);
    }
    wait_pipeline_pids(pids, segment_count, &final_status);
    cli_puts("status ");
    cli_putn(final_status);
    cli_puts("\n");
}

static void echo_text(const char *text, const char *redirect_path, int append) {
    char buffer[LINE_MAX];
    size_t out = 0;
    for (size_t i = 0; text[i] != '\0' && out + 1 < sizeof(buffer); i++) {
        if (text[i] == '\'' || text[i] == '"') {
            continue;
        }
        if (text[i] == '\\' && text[i + 1] != '\0') {
            i++;
        }
        buffer[out++] = text[i];
    }
    buffer[out++] = '\n';
    if (redirect_path != 0 && redirect_path[0] != '\0') {
        long result = append ?
            srv_fs_append(redirect_path, buffer, out) :
            srv_fs_write(redirect_path, buffer, out);
        if (result < 0) {
            cli_puts("echo: redirect failed\n");
        }
        return;
    }
    srv_write(SRV_STDOUT, buffer, out);
}

static void run_command(char *line, char *cwd, int background) {
    char *command = cli_trim(line);
    char *args = command;
    char *pipeline_segments[PIPELINE_MAX_COMMANDS];
    int pipeline_count;
    char path[CLI_PATH_MAX];
    char *redirect_path = 0;
    int redirect_append = 0;
    long status;

    if (*command == '\0' || *command == '#') {
        return;
    }

    pipeline_count = split_pipeline_segments(command, pipeline_segments, PIPELINE_MAX_COMMANDS);
    if (pipeline_count < 0) {
        cli_puts("sh: pipeline too long\n");
        return;
    }
    if (pipeline_count > 1) {
        if (background) {
            cli_puts("sh: background pipeline unsupported\n");
            return;
        }
        run_pipeline(pipeline_segments, (size_t)pipeline_count);
        return;
    }

    args = find_argument_tail(command);
    if (*args != '\0') {
        *args++ = '\0';
        args = cli_trim(args);
    }
    split_redirection(args, &redirect_path, &redirect_append);
    args = cli_trim(args);

    if (cli_streq(command, "help")) {
        print_help();
        return;
    }
    if (cli_streq(command, "exit")) {
        srv_exit(0);
    }
    if (cli_streq(command, "path")) {
        if (cli_starts_with(args, "add ")) {
            add_path(cli_trim(args + 4));
        } else {
            print_path();
        }
        return;
    }
    if (cli_streq(command, "source") || cli_streq(command, ".")) {
        if (args[0] == '\0') {
            cli_puts("usage: source <file>\n");
        } else {
            run_script(args, cwd);
        }
        return;
    }
    if (cli_streq(command, "pwd")) {
        cli_puts(cwd);
        cli_puts("\n");
        return;
    }
    if (cli_streq(command, "cd")) {
        char next[CLI_PATH_MAX];
        cli_normalize_path(next, sizeof(next), cwd, args[0] != '\0' ? args : "/");
        cli_copy(cwd, CLI_PATH_MAX, next);
        return;
    }
    if (cli_streq(command, "clear")) {
        clrscr();
        return;
    }
    if (cli_streq(command, "echo")) {
        echo_text(args, redirect_path, redirect_append);
        return;
    }
    if (cli_streq(command, "jobs")) {
        print_jobs();
        return;
    }
    if (cli_streq(command, "wait")) {
        wait_for_job(args);
        return;
    }
    if (cli_streq(command, "service")) {
        service_command(args);
        return;
    }
    if (cli_streq(command, "dhcp")) {
        dhcp_command();
        return;
    }
    if (cli_streq(command, "net")) {
        net_command();
        return;
    }
    if (cli_streq(command, "dns")) {
        dns_command(args);
        return;
    }
    if (cli_streq(command, "rmdir")) {
        rmdir_command(args);
        return;
    }
    if (!resolve_command(path, sizeof(path), command)) {
        cli_puts("sh: command not found: ");
        cli_puts(command);
        cli_puts("\n");
        return;
    }

    if (background) {
        if (redirect_path != 0) {
            cli_puts("sh: background redirection unsupported\n");
            return;
        }
        status = srv_spawn_bg_args(path, args);
        if (status < 0) {
            cli_puts("sh: background spawn failed\n");
        } else {
            cli_puts("[bg] pid ");
            cli_putn((uint64_t)status);
            cli_puts("\n");
        }
        return;
    }

    status = redirect_path != 0 ?
        srv_spawn_args_redirect(path, args, redirect_path, redirect_append) :
        srv_spawn_args(path, args);
    if (status < 0) {
        cli_puts("sh: exec failed\n");
        return;
    }
    cli_puts("status ");
    cli_putn((uint64_t)status);
    cli_puts("\n");
}

static void run_line(char *line, char *cwd) {
    char quote = '\0';
    char *segment = line;

    for (char *cursor = line; ; cursor++) {
        char c = *cursor;
        if (c == '\0') {
            run_command(segment, cwd, 0);
            return;
        }
        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
            } else if (c == '\\' && cursor[1] != '\0') {
                cursor++;
            }
        } else if (c == '\'' || c == '"') {
            quote = c;
        } else if (c == '#' && (cursor == segment || cursor[-1] == ' ' || cursor[-1] == '\t')) {
            *cursor = '\0';
            run_command(segment, cwd, 0);
            return;
        } else if (c == ';' || c == '&') {
            int background = c == '&';
            *cursor = '\0';
            run_command(segment, cwd, background);
            segment = cursor + 1;
        }
    }
}

int main(int argc, char **argv) {
    char line[LINE_MAX];
    char cwd[CLI_PATH_MAX] = "/";
    int login = argc > 1 && cli_streq(argv[1], "--login");

    cli_puts("srvsh: interactive shell\n");
    print_help();
    if (login) {
        run_script("/fat/etc/init.sh", cwd);
    }
    for (;;) {
        cli_puts(cwd);
        cli_puts(" $ ");
        if (!read_line(line, sizeof(line))) {
            break;
        }
        run_line(line, cwd);
    }
    return 0;
}
