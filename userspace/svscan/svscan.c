#include <srvros/cli.h>
#include <srvros/sys.h>

#define SERVICE_DIR "/fat/etc/services"
#define SVSCAN_LOG "/fat/var/log/svscan.log"
#define SVSCAN_RELOAD "/fat/run/svscan.reload"
#define MAX_SERVICES 16
#define MAX_NAME 32
#define MAX_PATH 160
#define MAX_ARGS 256
#define RESTART_BACKOFF_TICKS 50
#define SCAN_INTERVAL_TICKS 50

struct service_config {
    char name[MAX_NAME];
    char command[MAX_PATH];
    char args[MAX_ARGS];
    char process_name[MAX_NAME];
    char log_path[MAX_PATH];
    char requires[MAX_NAME];
    char health[32];
    int enabled;
    char restart[16];
    uint64_t max_log_size;
};

struct service_state {
    int used;
    char name[MAX_NAME];
    int started_once;
    uint64_t last_start_tick;
    uint64_t backoff_until_tick;
    int backoff_logged;
    int unhealthy_logged;
};

static struct service_state states[MAX_SERVICES];

static void append_text(char *out, size_t capacity, size_t *length, const char *text) {
    while (*text != '\0' && *length + 1 < capacity) {
        out[(*length)++] = *text++;
    }
    out[*length] = '\0';
}

static void append_number(char *out, size_t capacity, size_t *length, uint64_t value) {
    char digits[21];
    size_t count = 0;
    if (value == 0) {
        append_text(out, capacity, length, "0");
        return;
    }
    while (value > 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }
    for (size_t i = 0; i < count; i++) {
        char text[2] = { digits[count - i - 1], '\0' };
        append_text(out, capacity, length, text);
    }
}

static void log_line(const char *text) {
    int fd = (int)srv_open_mode(SVSCAN_LOG, SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_APPEND);
    if (fd < 0) {
        return;
    }
    (void)srv_write(fd, text, cli_strlen(text));
    (void)srv_write(fd, "\n", 1);
    srv_close(fd);
}

static void log_pid_line(const char *service, const char *message, uint64_t pid) {
    char line[192];
    size_t length = 0;
    line[0] = '\0';
    append_text(line, sizeof(line), &length, "svscan: ");
    append_text(line, sizeof(line), &length, service);
    append_text(line, sizeof(line), &length, " ");
    append_text(line, sizeof(line), &length, message);
    append_text(line, sizeof(line), &length, " pid ");
    append_number(line, sizeof(line), &length, pid);
    log_line(line);
}

static void log_service_line(const char *service, const char *message) {
    char line[192];
    size_t length = 0;
    line[0] = '\0';
    append_text(line, sizeof(line), &length, "svscan: ");
    append_text(line, sizeof(line), &length, service);
    append_text(line, sizeof(line), &length, " ");
    append_text(line, sizeof(line), &length, message);
    log_line(line);
}

static char *trim(char *text) {
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    char *end = text + cli_strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        *--end = '\0';
    }
    return text;
}

static int immediate_child(const char *path, const char *directory, const char **name_out) {
    size_t dir_length = cli_strlen(directory);
    const char *name;
    if (!cli_starts_with(path, directory) || path[dir_length] != '/' || path[dir_length + 1] == '\0') {
        return 0;
    }
    name = path + dir_length + 1;
    for (const char *cursor = name; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            return 0;
        }
    }
    *name_out = name;
    return 1;
}

static uint64_t parse_u64(const char *text) {
    uint64_t value = 0;
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    for (; *text != '\0'; text++) {
        if (*text < '0' || *text > '9') {
            break;
        }
        value = value * 10 + (uint64_t)(*text - '0');
    }
    return value;
}

static int service_name_from_path(char *out, size_t capacity, const char *path) {
    const char *name;
    size_t length;
    if (!immediate_child(path, SERVICE_DIR, &name)) {
        return 0;
    }
    length = cli_strlen(name);
    if (length <= 4 ||
        name[length - 4] != '.' ||
        name[length - 3] != 's' ||
        name[length - 2] != 'v' ||
        name[length - 1] != 'c' ||
        length - 4 >= capacity) {
        return 0;
    }
    for (size_t i = 0; i < length - 4; i++) {
        out[i] = name[i];
    }
    out[length - 4] = '\0';
    return 1;
}

static void config_path(char *out, size_t capacity, const char *name) {
    size_t length = 0;
    out[0] = '\0';
    append_text(out, capacity, &length, SERVICE_DIR);
    append_text(out, capacity, &length, "/");
    append_text(out, capacity, &length, name);
    append_text(out, capacity, &length, ".svc");
}

static void default_config(struct service_config *config, const char *name) {
    size_t length = 0;
    for (size_t i = 0; i < sizeof(*config); i++) {
        ((char *)config)[i] = 0;
    }
    cli_copy(config->name, sizeof(config->name), name);
    cli_copy(config->process_name, sizeof(config->process_name), name);
    cli_copy(config->restart, sizeof(config->restart), "never");
    append_text(config->command, sizeof(config->command), &length, "/fat/bin/");
    append_text(config->command, sizeof(config->command), &length, name);
}

static void apply_config_line(struct service_config *config, char *line) {
    char *text = trim(line);
    char *equals;
    if (text[0] == '\0' || text[0] == '#') {
        return;
    }
    equals = text;
    while (*equals != '\0' && *equals != '=') {
        equals++;
    }
    if (*equals != '=') {
        return;
    }
    *equals++ = '\0';
    char *key = trim(text);
    char *value = trim(equals);
    if (cli_streq(key, "command") || cli_streq(key, "path")) {
        cli_copy(config->command, sizeof(config->command), value);
    } else if (cli_streq(key, "args")) {
        cli_copy(config->args, sizeof(config->args), value);
    } else if (cli_streq(key, "process") || cli_streq(key, "name")) {
        cli_copy(config->process_name, sizeof(config->process_name), value);
    } else if (cli_streq(key, "log") || cli_streq(key, "stdout")) {
        cli_copy(config->log_path, sizeof(config->log_path), value);
    } else if (cli_streq(key, "requires") || cli_streq(key, "after")) {
        cli_copy(config->requires, sizeof(config->requires), value);
    } else if (cli_streq(key, "health")) {
        cli_copy(config->health, sizeof(config->health), value);
    } else if (cli_streq(key, "max_log") || cli_streq(key, "log_max")) {
        config->max_log_size = parse_u64(value);
    } else if (cli_streq(key, "enabled")) {
        config->enabled =
            cli_streq(value, "1") ||
            cli_streq(value, "yes") ||
            cli_streq(value, "true") ||
            cli_streq(value, "on");
    } else if (cli_streq(key, "restart")) {
        cli_copy(config->restart, sizeof(config->restart), value);
    }
}

static int load_config(struct service_config *config, const char *name) {
    char path[MAX_PATH];
    char data[512];
    char line[MAX_ARGS];
    size_t line_length = 0;
    default_config(config, name);
    config_path(path, sizeof(path), name);
    int fd = (int)srv_open(path);
    if (fd < 0) {
        return 0;
    }
    long count = srv_read(fd, data, sizeof(data) - 1);
    srv_close(fd);
    if (count < 0) {
        return 0;
    }
    data[count] = '\0';
    for (long i = 0; i <= count; i++) {
        char c = data[i];
        if (c == '\r') {
            continue;
        }
        if (c == '\n' || c == '\0') {
            line[line_length] = '\0';
            apply_config_line(config, line);
            line_length = 0;
            continue;
        }
        if (line_length + 1 < sizeof(line)) {
            line[line_length++] = c;
        }
    }
    return config->command[0] != '\0' && config->process_name[0] != '\0';
}

static struct service_state *state_for(const char *name) {
    struct service_state *free_state = 0;
    for (size_t i = 0; i < MAX_SERVICES; i++) {
        if (states[i].used && cli_streq(states[i].name, name)) {
            return &states[i];
        }
        if (!states[i].used && free_state == 0) {
            free_state = &states[i];
        }
    }
    if (free_state == 0) {
        return 0;
    }
    free_state->used = 1;
    free_state->started_once = 0;
    free_state->last_start_tick = 0;
    free_state->backoff_until_tick = 0;
    free_state->backoff_logged = 0;
    free_state->unhealthy_logged = 0;
    cli_copy(free_state->name, sizeof(free_state->name), name);
    return free_state;
}

static int process_name_matches(const struct srv_process_info *info, const char *name) {
    const char *base = info->name;
    for (const char *cursor = info->name; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            base = cursor + 1;
        }
    }
    return cli_streq(base, name) || cli_streq(info->name, name);
}

static int find_process(const char *name, struct srv_process_info *out, int include_exited) {
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

static int reap_exited(const char *name) {
    int reaped = 0;
    struct srv_process_info info;
    uint64_t status = 0;
    for (;;) {
        uint64_t index = 0;
        int found = 0;
        for (;;) {
            long next = srv_proc_list(index, &info);
            if (next <= 0) {
                break;
            }
            if (process_name_matches(&info, name) && cli_streq(info.state, "exited")) {
                found = 1;
                break;
            }
            index = (uint64_t)next;
        }
        if (!found) {
            break;
        }
        if (srv_wait(info.pid, &status, SRV_WAIT_NOHANG) <= 0) {
            break;
        }
        (void)status;
        reaped++;
        log_pid_line(name, "reaped", info.pid);
    }
    return reaped;
}

static void stop_running_service(const struct service_config *config, const char *reason) {
    struct srv_process_info info;
    uint64_t status = 0;
    if (!find_process(config->process_name, &info, 0)) {
        return;
    }
    log_pid_line(config->name, reason, info.pid);
    (void)srv_kill(info.pid);
    (void)srv_wait(info.pid, &status, 0);
}

static int service_dependency_ready(const struct service_config *config) {
    struct srv_process_info info;
    if (config->requires[0] == '\0') {
        return 1;
    }
    if (cli_streq(config->requires, "network")) {
        struct srv_net_status_info net;
        return srv_net_status_info(&net) >= 0 && net.initialized != 0;
    }
    return find_process(config->requires, &info, 0);
}

static int parse_listen_health(const char *health, uint16_t *port_out) {
    const char *prefix = "listen:";
    uint64_t port;
    for (size_t i = 0; prefix[i] != '\0'; i++) {
        if (health[i] != prefix[i]) {
            return 0;
        }
    }
    port = parse_u64(health + 7);
    if (port == 0 || port > 65535) {
        return 0;
    }
    *port_out = (uint16_t)port;
    return 1;
}

static int service_health_ok(const struct service_config *config, const struct srv_process_info *process) {
    uint16_t port = 0;
    struct srv_net_info info;
    if (config->health[0] == '\0') {
        return 1;
    }
    if (!parse_listen_health(config->health, &port)) {
        return 1;
    }
    for (uint64_t index = 0;;) {
        long next = srv_net_list(index, &info);
        if (next <= 0) {
            break;
        }
        if (info.kind == SRV_NET_KIND_TCP_LISTENER &&
            info.local_port == port &&
            (process == 0 || info.pid == process->pid)) {
            return 1;
        }
        index = (uint64_t)next;
    }
    return 0;
}

static void append_suffix(char *out, size_t capacity, const char *path, const char *suffix) {
    size_t length = 0;
    out[0] = '\0';
    append_text(out, capacity, &length, path);
    append_text(out, capacity, &length, suffix);
}

static void rotate_log_if_needed(const struct service_config *config) {
    struct srv_stat stat;
    char rotated[MAX_PATH];
    int fd;
    if (config->log_path[0] == '\0' || config->max_log_size == 0) {
        return;
    }
    if (srv_stat(config->log_path, &stat) < 0 || stat.size <= config->max_log_size) {
        return;
    }
    append_suffix(rotated, sizeof(rotated), config->log_path, ".1");
    (void)srv_unlink(rotated);
    if (srv_rename(config->log_path, rotated) < 0) {
        log_service_line(config->name, "log rotate failed");
        return;
    }
    fd = (int)srv_open_mode(config->log_path, SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_TRUNC);
    if (fd >= 0) {
        srv_close(fd);
    }
    log_service_line(config->name, "log rotated");
}

static void start_service(const struct service_config *config, struct service_state *state) {
    int log_fd = -1;
    rotate_log_if_needed(config);
    if (config->log_path[0] != '\0') {
        log_fd = (int)srv_open_mode(config->log_path, SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_APPEND);
        if (log_fd < 0) {
            log_service_line(config->name, "log open failed");
            return;
        }
    }
    long pid = log_fd >= 0 ?
        srv_spawn_bg_args_fds(config->command, config->args, -1, log_fd) :
        srv_spawn_bg_args(config->command, config->args);
    if (log_fd >= 0) {
        srv_close(log_fd);
    }
    if (pid < 0) {
        log_service_line(config->name, "start failed");
        return;
    }
    if (state != 0) {
        state->started_once = 1;
        state->last_start_tick = (uint64_t)srv_ticks();
        state->backoff_logged = 0;
        state->unhealthy_logged = 0;
    }
    log_pid_line(config->name, "started", (uint64_t)pid);
}

static void scan_once(void) {
    uint64_t size = 0;
    char path[MAX_PATH];
    char name[MAX_NAME];
    struct service_config config;
    struct srv_process_info info;

    for (uint64_t index = 0;; index++) {
        long next = srv_list(index, path, sizeof(path), &size);
        if (next <= 0) {
            break;
        }
        if (!service_name_from_path(name, sizeof(name), path) || !load_config(&config, name)) {
            continue;
        }
        struct service_state *state = state_for(config.name);
        int reaped = reap_exited(config.process_name);
        rotate_log_if_needed(&config);
        if (!config.enabled) {
            stop_running_service(&config, "disabled stop");
            continue;
        }
        if (find_process(config.process_name, &info, 0)) {
            uint64_t now = (uint64_t)srv_ticks();
            if (state != 0 &&
                now - state->last_start_tick >= SCAN_INTERVAL_TICKS &&
                !service_health_ok(&config, &info)) {
                if (!state->unhealthy_logged) {
                    log_service_line(config.name, "health failed");
                    state->unhealthy_logged = 1;
                }
                stop_running_service(&config, "health stop");
            } else if (state != 0) {
                state->unhealthy_logged = 0;
            }
            continue;
        }
        int first_start = state == 0 || !state->started_once;
        int restart_always = cli_streq(config.restart, "always");
        if (!first_start && !restart_always) {
            continue;
        }
        if (!service_dependency_ready(&config)) {
            log_service_line(config.name, "waiting dependency");
            continue;
        }
        uint64_t now = (uint64_t)srv_ticks();
        if (!first_start && state != 0 && now - state->last_start_tick < RESTART_BACKOFF_TICKS) {
            state->backoff_until_tick = state->last_start_tick + RESTART_BACKOFF_TICKS;
            if (!state->backoff_logged) {
                log_service_line(config.name, "backoff");
                state->backoff_logged = 1;
            }
            continue;
        }
        if (state != 0 && state->backoff_until_tick != 0 && now >= state->backoff_until_tick) {
            state->backoff_until_tick = 0;
            state->backoff_logged = 0;
        }
        if (first_start) {
            log_service_line(config.name, "startup");
        } else if (reaped > 0) {
            log_service_line(config.name, "restarting exited");
        } else {
            log_service_line(config.name, "restarting missing");
        }
        start_service(&config, state);
    }
}

static void check_reload_request(void) {
    int fd = (int)srv_open(SVSCAN_RELOAD);
    if (fd < 0) {
        return;
    }
    srv_close(fd);
    (void)srv_unlink(SVSCAN_RELOAD);
    log_line("svscan: reload requested");
}

static int parse_cycles(const char *text, uint64_t *cycles_out) {
    uint64_t value = 0;
    if (text == 0 || text[0] == '\0') {
        return 0;
    }
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
        value = value * 10 + (uint64_t)(text[i] - '0');
    }
    *cycles_out = value;
    return 1;
}

int main(int argc, char **argv) {
    uint64_t cycles = 0;
    if (argc == 3 && cli_streq(argv[1], "--cycles")) {
        if (!parse_cycles(argv[2], &cycles)) {
            cli_puts("usage: svscan [--cycles n]\n");
            return 1;
        }
    } else if (argc != 1) {
        cli_puts("usage: svscan [--cycles n]\n");
        return 1;
    }

    log_line("svscan: started");
    for (uint64_t cycle = 0; cycles == 0 || cycle < cycles; cycle++) {
        check_reload_request();
        scan_once();
        srv_sleep_ticks(SCAN_INTERVAL_TICKS);
    }
    log_line("svscan: exiting");
    return 0;
}
