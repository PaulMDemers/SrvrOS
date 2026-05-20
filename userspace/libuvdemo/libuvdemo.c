#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <uv.h>

static int version_test(void) {
    unsigned int expected = (UV_VERSION_MAJOR << 16) | (UV_VERSION_MINOR << 8) | UV_VERSION_PATCH;
    if (uv_version() != expected || strcmp(uv_version_string(), "1.52.1") != 0) {
        puts("libuvdemo: version failed");
        return 1;
    }
    printf("libuvdemo: upstream version %s\n", uv_version_string());
    return 0;
}

static int error_test(void) {
    char name[32];
    char message[64];
    if (UV_EINVAL >= 0 || uv_translate_sys_error(EINVAL) != UV_EINVAL) {
        puts("libuvdemo: error translate failed");
        return 1;
    }
    if (strcmp(uv_err_name(UV_EINVAL), "EINVAL") != 0 ||
        strcmp(uv_strerror(UV_EINVAL), "invalid argument") != 0 ||
        strcmp(uv_strerror(UV_EOF), "end of file") != 0) {
        puts("libuvdemo: error strings failed");
        return 1;
    }
    if (uv_err_name_r(UV_EINVAL, name, sizeof(name)) != name ||
        uv_strerror_r(UV_EINVAL, message, sizeof(message)) != message ||
        strcmp(name, "EINVAL") != 0 ||
        strcmp(message, "invalid argument") != 0) {
        puts("libuvdemo: error reentrant failed");
        return 1;
    }
    puts("libuvdemo: errors ok");
    return 0;
}

static int platform_random_seen;
static int platform_random_status;

static void platform_random_cb(uv_random_t *request, int status, void *buffer, size_t buffer_length) {
    (void)request;
    platform_random_status = status;
    if (status == 0 && buffer != 0 && buffer_length == 8) {
        platform_random_seen = 1;
    }
}

static int random_has_data(const unsigned char *bytes, size_t length) {
    unsigned char combined = 0;
    for (size_t i = 0; i < length; i++) {
        combined |= bytes[i];
    }
    return combined != 0;
}

static int platform_test(void) {
    uv_loop_t loop;
    uv_random_t random_request;
    char buffer[96];
    char cwd[96];
    char original_cwd[96];
    unsigned char random_bytes[8];
    size_t size;
    uint64_t start;
    uint64_t end;
    uint64_t total_memory;
    uint64_t free_memory;
    size_t rss;
    double uptime_seconds;
    double loadavg[3];
    uv_rusage_t rusage;
    uv_cpu_info_t *cpu_infos;
    uv_interface_address_t *interfaces;
    uv_env_item_t *envitems;
    uv_passwd_t passwd_info;
    uv_group_t group_info;
    uv_utsname_t uname_info;
    int cpu_count;
    int interface_count;
    int env_count;
    int priority;

    size = sizeof(buffer);
    if (uv_os_setenv("UVDEMO_ENV", "srvros") != 0 ||
        uv_os_getenv("UVDEMO_ENV", buffer, &size) != 0 ||
        size != 6 ||
        strcmp(buffer, "srvros") != 0) {
        puts("libuvdemo: platform env failed");
        return 1;
    }
    size = 3;
    if (uv_os_getenv("UVDEMO_ENV", buffer, &size) != UV_ENOBUFS || size != 7 ||
        uv_os_unsetenv("UVDEMO_ENV") != 0 ||
        uv_os_getenv("UVDEMO_ENV", buffer, &size) != UV_ENOENT) {
        puts("libuvdemo: platform env failed");
        return 1;
    }

    if (uv_set_process_title("libuvdemo-title") != 0 ||
        uv_get_process_title(buffer, sizeof(buffer)) != 0 ||
        strcmp(buffer, "libuvdemo-title") != 0) {
        puts("libuvdemo: platform title failed");
        return 1;
    }
    size = sizeof(buffer);
    if (uv_os_homedir(buffer, &size) != 0 || size == 0 || buffer[0] != '/') {
        puts("libuvdemo: platform homedir failed");
        return 1;
    }
    size = sizeof(buffer);
    if (uv_os_tmpdir(buffer, &size) != 0 || strcmp(buffer, "/fat/tmp") != 0) {
        puts("libuvdemo: platform tmpdir failed");
        return 1;
    }
    if (uv_os_get_passwd(&passwd_info) != 0 ||
        strcmp(passwd_info.username, "root") != 0 ||
        strcmp(passwd_info.homedir, "/fat") != 0 ||
        strcmp(passwd_info.shell, "/fat/bin/sh") != 0) {
        puts("libuvdemo: platform passwd failed");
        return 1;
    }
    uv_os_free_passwd(&passwd_info);
    if (uv_os_get_group(&group_info, 0) != 0 ||
        strcmp(group_info.groupname, "root") != 0 ||
        group_info.members == 0 ||
        strcmp(group_info.members[0], "root") != 0) {
        puts("libuvdemo: platform group failed");
        return 1;
    }
    uv_os_free_group(&group_info);
    if (uv_os_environ(&envitems, &env_count) != 0 || env_count <= 0) {
        puts("libuvdemo: platform environ failed");
        return 1;
    }
    uv_os_free_environ(envitems, env_count);

    size = sizeof(original_cwd);
    if (uv_cwd(original_cwd, &size) != 0) {
        puts("libuvdemo: platform cwd initial failed");
        return 1;
    }
    if (uv_chdir("/fat") != 0) {
        puts("libuvdemo: platform cwd failed");
        return 1;
    }
    size = sizeof(cwd);
    if (uv_cwd(cwd, &size) != 0 || strcmp(cwd, "/fat") != 0 ||
        uv_chdir(original_cwd) != 0) {
        puts("libuvdemo: platform cwd failed");
        return 1;
    }

    size = sizeof(buffer);
    if (uv_exepath(buffer, &size) != 0 || size == 0 ||
        strcmp(buffer, "/fat/bin/libuvdemo") != 0) {
        puts("libuvdemo: platform exepath failed");
        return 1;
    }
    if (uv_os_getpid() <= 0 || uv_os_getppid() <= 0) {
        puts("libuvdemo: platform pid failed");
        return 1;
    }
    start = uv_hrtime();
    end = uv_hrtime();
    total_memory = uv_get_total_memory();
    free_memory = uv_get_free_memory();
    if (start == 0 || end < start || total_memory == 0 || free_memory == 0 || free_memory > total_memory) {
        puts("libuvdemo: platform info failed");
        return 1;
    }
    if (uv_resident_set_memory(&rss) != 0 ||
        uv_uptime(&uptime_seconds) != 0 ||
        uv_getrusage(&rusage) != 0 ||
        uv_getrusage_thread(&rusage) != 0 ||
        rss == 0 ||
        uptime_seconds < 0.0 ||
        uv_available_parallelism() == 0 ||
        uv_os_getpriority(0, &priority) != 0 ||
        priority != 0 ||
        uv_os_setpriority(0, priority) != 0) {
        puts("libuvdemo: platform resource failed");
        return 1;
    }
    if (uv_cpu_info(&cpu_infos, &cpu_count) != 0 ||
        cpu_count <= 0 ||
        cpu_infos[0].model == 0) {
        puts("libuvdemo: platform cpu failed");
        return 1;
    }
    uv_free_cpu_info(cpu_infos, cpu_count);
    if (uv_interface_addresses(&interfaces, &interface_count) != 0 ||
        interface_count <= 0 ||
        interfaces[0].name == 0) {
        puts("libuvdemo: platform interfaces failed");
        return 1;
    }
    uv_free_interface_addresses(interfaces, interface_count);
    if (uv_os_uname(&uname_info) != 0 ||
        strcmp(uname_info.sysname, "srvros") != 0 ||
        strcmp(uname_info.machine, "x86_64") != 0) {
        puts("libuvdemo: platform uname failed");
        return 1;
    }
    uv_loadavg(loadavg);
    if (loadavg[0] < 0.0 || loadavg[1] < 0.0 || loadavg[2] < 0.0) {
        puts("libuvdemo: platform loadavg failed");
        return 1;
    }

    memset(random_bytes, 0, sizeof(random_bytes));
    if (uv_random(0, 0, random_bytes, sizeof(random_bytes), 0, 0) != 0 ||
        !random_has_data(random_bytes, sizeof(random_bytes))) {
        puts("libuvdemo: platform random failed");
        return 1;
    }
    memset(random_bytes, 0, sizeof(random_bytes));
    platform_random_seen = 0;
    platform_random_status = -1;
    if (uv_loop_init(&loop) < 0 ||
        uv_random(&loop, &random_request, random_bytes, sizeof(random_bytes), 0, platform_random_cb) != 0) {
        puts("libuvdemo: platform random setup failed");
        return 1;
    }
    (void)uv_run(&loop, UV_RUN_DEFAULT);
    if (!platform_random_seen || platform_random_status != 0 ||
        !random_has_data(random_bytes, sizeof(random_bytes))) {
        puts("libuvdemo: platform random failed");
        return 1;
    }
    puts("libuvdemo: platform ok");
    return 0;
}

static int core_timer_seen;
static int core_close_seen;
static int core_walk_seen;
static int core_walk_arg_seen;

static void core_timer_cb(uv_timer_t *timer) {
    core_timer_seen++;
    uv_timer_stop(timer);
}

static void core_close_cb(uv_handle_t *handle) {
    if (uv_is_closing(handle)) {
        core_close_seen = 1;
    }
}

static void core_walk_cb(uv_handle_t *handle, void *arg) {
    int *token = arg;
    if (uv_handle_get_type(handle) == UV_TIMER && token != 0 && *token == 7) {
        core_walk_seen++;
        core_walk_arg_seen = 1;
    }
}

static int core_api_test(void) {
    uv_loop_t loop;
    uv_timer_t timer;
    uv_fs_t request;
    int loop_token = 1;
    int handle_token = 2;
    int req_token = 3;

    core_timer_seen = 0;
    core_close_seen = 0;
    core_walk_seen = 0;
    core_walk_arg_seen = 0;
    if (uv_loop_init(&loop) < 0 || uv_loop_close(&loop) != 0) {
        puts("libuvdemo: loop close failed");
        return 1;
    }
    if (uv_loop_init(&loop) < 0 || uv_timer_init(&loop, &timer) < 0) {
        puts("libuvdemo: core setup failed");
        return 1;
    }
    uv_loop_set_data(&loop, &loop_token);
    uv_handle_set_data((uv_handle_t *)&timer, &handle_token);
    if (uv_loop_get_data(&loop) != &loop_token ||
        uv_handle_get_data((uv_handle_t *)&timer) != &handle_token ||
        uv_handle_get_loop((uv_handle_t *)&timer) != &loop ||
        uv_handle_get_type((uv_handle_t *)&timer) != UV_TIMER ||
        strcmp(uv_handle_type_name(UV_TIMER), "timer") != 0 ||
        strcmp(uv_handle_type_name(UV_PREPARE), "prepare") != 0 ||
        strcmp(uv_handle_type_name(UV_CHECK), "check") != 0 ||
        strcmp(uv_handle_type_name(UV_IDLE), "idle") != 0 ||
        uv_handle_size(UV_TIMER) != sizeof(uv_timer_t) ||
        uv_handle_size(UV_PREPARE) != sizeof(uv_prepare_t) ||
        uv_handle_size(UV_CHECK) != sizeof(uv_check_t) ||
        uv_handle_size(UV_IDLE) != sizeof(uv_idle_t) ||
        uv_req_size(UV_FS) != sizeof(uv_fs_t) ||
        uv_req_size(UV_GETADDRINFO) != sizeof(uv_getaddrinfo_t) ||
        strcmp(uv_req_type_name(UV_FS), "fs") != 0 ||
        strcmp(uv_req_type_name(UV_GETADDRINFO), "getaddrinfo") != 0 ||
        uv_backend_fd(&loop) < 0 ||
        uv_is_active((uv_handle_t *)&timer) ||
        uv_is_closing((uv_handle_t *)&timer) ||
        !uv_has_ref((uv_handle_t *)&timer)) {
        puts("libuvdemo: core helpers failed");
        return 1;
    }
    if (uv_timer_start(&timer, core_timer_cb, 50, 0) < 0 ||
        !uv_is_active((uv_handle_t *)&timer) ||
        uv_loop_close(&loop) != UV_EBUSY ||
        uv_backend_timeout(&loop) < 0 ||
        uv_timer_get_due_in(&timer) > 50) {
        puts("libuvdemo: active helpers failed");
        return 1;
    }
    int walk_token = 7;
    uv_walk(&loop, core_walk_cb, &walk_token);
    uv_unref((uv_handle_t *)&timer);
    if (uv_loop_alive(&loop) || uv_has_ref((uv_handle_t *)&timer)) {
        puts("libuvdemo: unref failed");
        return 1;
    }
    uv_ref((uv_handle_t *)&timer);
    if (!uv_loop_alive(&loop) || !uv_has_ref((uv_handle_t *)&timer) ||
        core_walk_seen != 1 || !core_walk_arg_seen ||
        uv_fileno((uv_handle_t *)&timer, 0) != UV_EBADF) {
        puts("libuvdemo: ref/walk failed");
        return 1;
    }
    (void)uv_run(&loop, UV_RUN_DEFAULT);
    if (core_timer_seen != 1 || uv_is_active((uv_handle_t *)&timer)) {
        puts("libuvdemo: active state failed");
        return 1;
    }
    if (uv_loop_close(&loop) != UV_EBUSY) {
        puts("libuvdemo: loop active handle failed");
        return 1;
    }
    uv_close((uv_handle_t *)&timer, core_close_cb);
    if (!core_close_seen || !uv_is_closing((uv_handle_t *)&timer)) {
        puts("libuvdemo: close state failed");
        return 1;
    }
    if (uv_fs_stat(&loop, &request, "/fat", 0) < 0 ||
        uv_req_get_type((uv_req_t *)&request) != UV_FS) {
        puts("libuvdemo: request type failed");
        return 1;
    }
    uv_req_set_data((uv_req_t *)&request, &req_token);
    if (uv_req_get_data((uv_req_t *)&request) != &req_token) {
        puts("libuvdemo: request data failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_loop_close(&loop) != 0) {
        puts("libuvdemo: loop close failed");
        return 1;
    }
    puts("libuvdemo: core api ok");
    return 0;
}

static int timer_seen;

static void timer_cb(uv_timer_t *timer) {
    timer_seen++;
    uv_timer_stop(timer);
}

static int timer_test(void) {
    uv_loop_t loop;
    uv_timer_t timer;
    timer_seen = 0;
    if (uv_loop_init(&loop) < 0 ||
        uv_timer_init(&loop, &timer) < 0 ||
        uv_timer_start(&timer, timer_cb, 1, 0) < 0) {
        puts("libuvdemo: timer setup failed");
        return 1;
    }
    (void)uv_run(&loop, UV_RUN_DEFAULT);
    if (timer_seen != 1) {
        puts("libuvdemo: timer failed");
        return 1;
    }
    puts("libuvdemo: timer ok");
    return 0;
}

static uv_loop_t phase_loop;
static uv_prepare_t phase_prepare;
static uv_check_t phase_check;
static uv_idle_t phase_idle;
static int phase_prepare_seen;
static int phase_check_seen;
static int phase_idle_seen;

static void phase_idle_cb(uv_idle_t *handle) {
    phase_idle_seen++;
    if (phase_idle_seen >= 2) {
        uv_idle_stop(handle);
    }
}

static void phase_prepare_cb(uv_prepare_t *handle) {
    phase_prepare_seen++;
    if (phase_prepare_seen >= 2) {
        uv_prepare_stop(handle);
    }
}

static void phase_check_cb(uv_check_t *handle) {
    phase_check_seen++;
    if (phase_check_seen >= 2) {
        uv_check_stop(handle);
        uv_stop(&phase_loop);
    }
}

static int phase_test(void) {
    phase_prepare_seen = 0;
    phase_check_seen = 0;
    phase_idle_seen = 0;
    if (uv_loop_init(&phase_loop) < 0 ||
        uv_prepare_init(&phase_loop, &phase_prepare) < 0 ||
        uv_check_init(&phase_loop, &phase_check) < 0 ||
        uv_idle_init(&phase_loop, &phase_idle) < 0 ||
        uv_prepare_start(&phase_prepare, phase_prepare_cb) < 0 ||
        uv_check_start(&phase_check, phase_check_cb) < 0 ||
        uv_idle_start(&phase_idle, phase_idle_cb) < 0) {
        puts("libuvdemo: phase setup failed");
        return 1;
    }
    (void)uv_run(&phase_loop, UV_RUN_DEFAULT);
    if (phase_prepare_seen < 2 || phase_check_seen < 2 || phase_idle_seen < 2 ||
        uv_is_active((uv_handle_t *)&phase_prepare) ||
        uv_is_active((uv_handle_t *)&phase_check) ||
        uv_is_active((uv_handle_t *)&phase_idle)) {
        puts("libuvdemo: phase failed");
        return 1;
    }
    puts("libuvdemo: phases ok");
    return 0;
}

static uv_fs_t *fs_async_expected;
static uv_fs_t *fs_async_access_expected;
static uv_fs_t *fs_async_realpath_expected;
static int fs_async_seen;
static int fs_async_expected_count;

static void fs_async_cb(uv_fs_t *async_req) {
    if (async_req == fs_async_expected &&
        uv_fs_get_type(async_req) == UV_FS_STAT &&
        uv_fs_get_result(async_req) == 0) {
        fs_async_seen++;
    } else if (async_req == fs_async_access_expected &&
        uv_fs_get_type(async_req) == UV_FS_ACCESS &&
        uv_fs_get_result(async_req) == 0) {
        fs_async_seen++;
    } else if (async_req == fs_async_realpath_expected &&
        uv_fs_get_type(async_req) == UV_FS_REALPATH &&
        uv_fs_get_result(async_req) == 0 &&
        strcmp((const char *)uv_fs_get_ptr(async_req), uv_fs_get_path(async_req)) == 0) {
        fs_async_seen++;
    }
    if (fs_async_seen == fs_async_expected_count) {
        uv_stop(async_req->loop);
    }
}

static int fs_test(void) {
    uv_loop_t loop;
    uv_fs_t request;
    uv_fs_t async_request;
    uv_fs_t async_access_request;
    uv_fs_t async_realpath_request;
    const char *path = "/fat/libuvdemo.txt";
    const char *renamed = "/fat/libuvdemo-renamed.txt";
    const char *copy_path = "/fat/libuvdemo-copy.txt";
    const char *dir = "/fat/libuvdemo-dir";
    char payload[] = "libuv-fs-ok";
    char readback[32];
    uv_buf_t buffer;
    int fd;
    int src_fd;
    int copy_fd;
    int saw_bin = 0;
    int saw_echo = 0;
    fs_async_expected = 0;
    fs_async_access_expected = 0;
    fs_async_realpath_expected = 0;
    fs_async_seen = 0;
    fs_async_expected_count = 3;

    if (uv_loop_init(&loop) < 0) {
        return 1;
    }
    (void)uv_fs_unlink(&loop, &request, path, 0);
    uv_fs_req_cleanup(&request);
    (void)uv_fs_unlink(&loop, &request, renamed, 0);
    uv_fs_req_cleanup(&request);
    (void)uv_fs_unlink(&loop, &request, copy_path, 0);
    uv_fs_req_cleanup(&request);
    (void)uv_fs_rmdir(&loop, &request, dir, 0);
    uv_fs_req_cleanup(&request);

    if (uv_fs_open(&loop, &request, path, O_CREAT | O_TRUNC | O_RDWR, 0644, 0) < 0) {
        puts("libuvdemo: fs open failed");
        return 1;
    }
    fd = (int)request.result;
    uv_fs_req_cleanup(&request);
    buffer = uv_buf_init(payload, (unsigned int)strlen(payload));
    if (uv_fs_write(&loop, &request, fd, &buffer, 1, 0, 0) < 0) {
        puts("libuvdemo: fs write failed");
        (void)uv_fs_close(&loop, &request, fd, 0);
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_fsync(&loop, &request, fd, 0) < 0 ||
        uv_fs_fdatasync(&loop, &request, fd, 0) < 0 ||
        uv_fs_ftruncate(&loop, &request, fd, (int64_t)strlen(payload), 0) < 0 ||
        uv_fs_futime(&loop, &request, fd, 1.0, 1.0, 0) < 0) {
        puts("libuvdemo: fs sync failed");
        (void)uv_fs_close(&loop, &request, fd, 0);
        return 1;
    }
    uv_fs_req_cleanup(&request);
    (void)uv_fs_close(&loop, &request, fd, 0);
    uv_fs_req_cleanup(&request);

    if (uv_fs_open(&loop, &request, path, O_RDONLY, 0, 0) < 0) {
        puts("libuvdemo: fs reopen failed");
        return 1;
    }
    fd = (int)request.result;
    uv_fs_req_cleanup(&request);
    memset(readback, 0, sizeof(readback));
    buffer = uv_buf_init(readback, sizeof(readback) - 1);
    if (uv_fs_read(&loop, &request, fd, &buffer, 1, 0, 0) < 0) {
        puts("libuvdemo: fs read failed");
        (void)uv_fs_close(&loop, &request, fd, 0);
        return 1;
    }
    uv_fs_req_cleanup(&request);
    (void)uv_fs_close(&loop, &request, fd, 0);
    uv_fs_req_cleanup(&request);
    if (strcmp(readback, payload) != 0) {
        puts("libuvdemo: fs mismatch");
        return 1;
    }

    if (uv_fs_mkdir(&loop, &request, dir, 0755, 0) < 0) {
        puts("libuvdemo: fs metadata failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_rename(&loop, &request, path, renamed, 0) < 0) {
        puts("libuvdemo: fs metadata failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_stat(&loop, &request, renamed, 0) < 0 ||
        uv_fs_get_type(&request) != UV_FS_STAT ||
        uv_fs_get_result(&request) != 0 ||
        uv_fs_get_statbuf(&request)->st_size != (off_t)strlen(payload) ||
        uv_fs_get_statbuf(&request)->st_mtime != 1) {
        puts("libuvdemo: fs stat failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_open(&loop, &request, renamed, O_RDONLY, 0, 0) < 0) {
        puts("libuvdemo: fs fstat open failed");
        return 1;
    }
    fd = (int)request.result;
    uv_fs_req_cleanup(&request);
    if (uv_fs_fstat(&loop, &request, fd, 0) < 0 ||
        uv_fs_get_statbuf(&request)->st_size != (off_t)strlen(payload)) {
        puts("libuvdemo: fs fstat failed");
        (void)uv_fs_close(&loop, &request, fd, 0);
        return 1;
    }
    uv_fs_req_cleanup(&request);
    (void)uv_fs_close(&loop, &request, fd, 0);
    uv_fs_req_cleanup(&request);
    if (uv_fs_lstat(&loop, &request, renamed, 0) < 0 ||
        uv_fs_get_statbuf(&request)->st_size != (off_t)strlen(payload)) {
        puts("libuvdemo: fs lstat failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_access(&loop, &request, renamed, F_OK | R_OK, 0) < 0) {
        puts("libuvdemo: fs access failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_utime(&loop, &request, renamed, 2.0, 3.0, 0) < 0) {
        puts("libuvdemo: fs utime failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_stat(&loop, &request, renamed, 0) < 0 ||
        uv_fs_get_statbuf(&request)->st_atime != 2 ||
        uv_fs_get_statbuf(&request)->st_mtime != 3) {
        puts("libuvdemo: fs utime stat failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_open(&loop, &request, renamed, O_RDONLY, 0, 0) < 0) {
        puts("libuvdemo: fs sendfile open failed");
        return 1;
    }
    src_fd = (int)request.result;
    uv_fs_req_cleanup(&request);
    if (uv_fs_open(&loop, &request, copy_path, O_CREAT | O_TRUNC | O_RDWR, 0644, 0) < 0) {
        puts("libuvdemo: fs sendfile open failed");
        (void)uv_fs_close(&loop, &request, src_fd, 0);
        return 1;
    }
    copy_fd = (int)request.result;
    uv_fs_req_cleanup(&request);
    if (uv_fs_sendfile(&loop, &request, copy_fd, src_fd, 0, strlen(payload), 0) < 0 ||
        uv_fs_get_result(&request) != (ssize_t)strlen(payload)) {
        puts("libuvdemo: fs sendfile failed");
        (void)uv_fs_close(&loop, &request, src_fd, 0);
        (void)uv_fs_close(&loop, &request, copy_fd, 0);
        return 1;
    }
    uv_fs_req_cleanup(&request);
    (void)uv_fs_close(&loop, &request, src_fd, 0);
    uv_fs_req_cleanup(&request);
    (void)uv_fs_close(&loop, &request, copy_fd, 0);
    uv_fs_req_cleanup(&request);
    if (uv_fs_stat(&loop, &request, copy_path, 0) < 0 ||
        uv_fs_get_statbuf(&request)->st_size != (off_t)strlen(payload)) {
        puts("libuvdemo: fs sendfile stat failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_realpath(&loop, &request, renamed, 0) < 0 ||
        strcmp((const char *)uv_fs_get_ptr(&request), renamed) != 0 ||
        strcmp(uv_fs_get_path(&request), renamed) != 0) {
        puts("libuvdemo: fs realpath failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_scandir(&loop, &request, "/fat/bin", 0, 0) < 0 ||
        uv_fs_get_result(&request) <= 0) {
        puts("libuvdemo: fs scandir failed");
        return 1;
    }
    uv_dirent_t entry;
    while (uv_fs_scandir_next(&request, &entry) == 0) {
        if (strcmp(entry.name, "echo") == 0 && entry.type == UV_DIRENT_FILE) {
            saw_echo = 1;
        }
        if (strcmp(entry.name, "sh") == 0 && entry.type == UV_DIRENT_FILE) {
            saw_bin = 1;
        }
    }
    uv_fs_req_cleanup(&request);
    if (!saw_bin || !saw_echo) {
        puts("libuvdemo: fs scandir contents failed");
        return 1;
    }
    memset(&async_request, 0, sizeof(async_request));
    memset(&async_access_request, 0, sizeof(async_access_request));
    memset(&async_realpath_request, 0, sizeof(async_realpath_request));
    fs_async_expected = &async_request;
    fs_async_access_expected = &async_access_request;
    fs_async_realpath_expected = &async_realpath_request;
    if (uv_fs_stat(&loop, &async_request, renamed, fs_async_cb) < 0 ||
        uv_fs_access(&loop, &async_access_request, renamed, F_OK | R_OK, fs_async_cb) < 0 ||
        uv_fs_realpath(&loop, &async_realpath_request, renamed, fs_async_cb) < 0) {
        puts("libuvdemo: fs async setup failed");
        return 1;
    }
    (void)uv_run(&loop, UV_RUN_DEFAULT);
    uv_fs_req_cleanup(&async_request);
    uv_fs_req_cleanup(&async_access_request);
    uv_fs_req_cleanup(&async_realpath_request);
    if (fs_async_seen != fs_async_expected_count) {
        puts("libuvdemo: fs async failed");
        return 1;
    }
    if (uv_fs_unlink(&loop, &request, renamed, 0) < 0) {
        puts("libuvdemo: fs cleanup failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_unlink(&loop, &request, copy_path, 0) < 0) {
        puts("libuvdemo: fs cleanup failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    if (uv_fs_rmdir(&loop, &request, dir, 0) < 0) {
        puts("libuvdemo: fs cleanup failed");
        return 1;
    }
    uv_fs_req_cleanup(&request);
    puts("libuvdemo: fs ok");
    return 0;
}

static uv_loop_t fs_poll_loop;
static uv_fs_poll_t fs_poll_handle;
static uv_timer_t fs_poll_timer;
static const char *fs_poll_path = "/fat/libuvdemo-fspoll.txt";
static int fs_poll_seen;

static void fs_poll_timer_cb(uv_timer_t *timer) {
    (void)timer;
    int fd = open(fs_poll_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd >= 0) {
        (void)write(fd, "libuv-fs-poll-changed", 21);
        (void)close(fd);
    }
}

static void fs_poll_cb(uv_fs_poll_t *handle, int status, const uv_stat_t *previous, const uv_stat_t *current) {
    char path[96];
    size_t size = sizeof(path);
    if (handle == &fs_poll_handle &&
        status == 0 &&
        previous != 0 &&
        current != 0 &&
        current->st_size != previous->st_size &&
        uv_fs_poll_getpath(handle, path, &size) == 0 &&
        strcmp(path, fs_poll_path) == 0) {
        fs_poll_seen = 1;
        (void)uv_fs_poll_stop(handle);
        (void)uv_timer_stop(&fs_poll_timer);
        uv_stop(&fs_poll_loop);
    }
}

static int fs_poll_test(void) {
    int fd = open(fs_poll_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) {
        puts("libuvdemo: fs poll setup failed");
        return 1;
    }
    (void)write(fd, "seed", 4);
    (void)close(fd);

    fs_poll_seen = 0;
    if (uv_loop_init(&fs_poll_loop) < 0 ||
        uv_fs_poll_init(&fs_poll_loop, &fs_poll_handle) < 0 ||
        uv_timer_init(&fs_poll_loop, &fs_poll_timer) < 0 ||
        uv_fs_poll_start(&fs_poll_handle, fs_poll_cb, fs_poll_path, 10) < 0 ||
        uv_timer_start(&fs_poll_timer, fs_poll_timer_cb, 30, 0) < 0) {
        puts("libuvdemo: fs poll setup failed");
        return 1;
    }
    (void)uv_run(&fs_poll_loop, UV_RUN_DEFAULT);
    uv_close((uv_handle_t *)&fs_poll_handle, 0);
    uv_close((uv_handle_t *)&fs_poll_timer, 0);
    if (uv_loop_close(&fs_poll_loop) != 0) {
        puts("libuvdemo: fs poll loop close failed");
        return 1;
    }
    (void)unlink(fs_poll_path);
    if (!fs_poll_seen) {
        puts("libuvdemo: fs poll failed");
        return 1;
    }
    puts("libuvdemo: fs poll ok");
    return 0;
}

static uv_loop_t work_loop;
static uv_loop_t cancel_loop;
static uv_async_t async_handle;
static uv_work_t work_requests[6];
static uv_work_t cancel_blockers[4];
static uv_work_t cancel_request;
static uv_fs_t cancel_fs_request;
static uv_mutex_t work_mutex;
static uv_sem_t work_block_sem;
static int work_value;
static int work_done_count;
static int cancel_block_done_count;
static int cancel_seen;
static int cancel_fs_seen;
static int cancel_ran;
static int async_seen;
static int work_seen;

static void async_cb(uv_async_t *handle) {
    (void)handle;
    async_seen = 1;
}

static void work_cb(uv_work_t *request) {
    int value = (int)(uintptr_t)request->data;
    uv_mutex_lock(&work_mutex);
    work_value += value;
    uv_mutex_unlock(&work_mutex);
    (void)uv_async_send(&async_handle);
}

static void after_work_cb(uv_work_t *request, int status) {
    (void)request;
    if (status == 0) {
        work_done_count++;
    }
    if (work_done_count == (int)(sizeof(work_requests) / sizeof(work_requests[0]))) {
        work_seen = work_value == 21;
        uv_close((uv_handle_t *)&async_handle, 0);
        uv_stop(&work_loop);
    }
}

static void cancel_blocking_work_cb(uv_work_t *request) {
    (void)request;
    uv_sem_wait(&work_block_sem);
}

static void canceled_work_cb(uv_work_t *request) {
    (void)request;
    cancel_ran = 1;
}

static void cancel_after_work_cb(uv_work_t *request, int status) {
    if (request == &cancel_request) {
        cancel_seen = status == UV_ECANCELED;
    } else if (status == 0) {
        cancel_block_done_count++;
    }
    if (cancel_seen && cancel_fs_seen && cancel_block_done_count == (int)(sizeof(cancel_blockers) / sizeof(cancel_blockers[0]))) {
        uv_stop(&cancel_loop);
    }
}

static void cancel_fs_cb(uv_fs_t *request) {
    if (request == &cancel_fs_request && uv_fs_get_result(request) == UV_ECANCELED) {
        cancel_fs_seen = 1;
    }
    if (cancel_seen && cancel_fs_seen && cancel_block_done_count == (int)(sizeof(cancel_blockers) / sizeof(cancel_blockers[0]))) {
        uv_stop(&cancel_loop);
    }
}

static int work_test(void) {
    work_value = 0;
    work_done_count = 0;
    async_seen = 0;
    work_seen = 0;
    if (uv_mutex_init(&work_mutex) < 0) {
        puts("libuvdemo: work setup failed");
        return 1;
    }
    if (uv_loop_init(&work_loop) < 0 ||
        uv_async_init(&work_loop, &async_handle, async_cb) < 0) {
        puts("libuvdemo: work setup failed");
        return 1;
    }
    for (size_t i = 0; i < sizeof(work_requests) / sizeof(work_requests[0]); i++) {
        work_requests[i].data = (void *)(uintptr_t)(i + 1);
        if (uv_queue_work(&work_loop, &work_requests[i], work_cb, after_work_cb) < 0) {
            puts("libuvdemo: work setup failed");
            return 1;
        }
    }
    (void)uv_run(&work_loop, UV_RUN_DEFAULT);
    uv_mutex_destroy(&work_mutex);
    if (!async_seen || !work_seen) {
        puts("libuvdemo: work failed");
        return 1;
    }
    cancel_block_done_count = 0;
    cancel_seen = 0;
    cancel_fs_seen = 0;
    cancel_ran = 0;
    if (uv_loop_init(&cancel_loop) < 0 || uv_sem_init(&work_block_sem, 0) < 0) {
        puts("libuvdemo: cancel setup failed");
        return 1;
    }
    for (size_t i = 0; i < sizeof(cancel_blockers) / sizeof(cancel_blockers[0]); i++) {
        if (uv_queue_work(&cancel_loop, &cancel_blockers[i], cancel_blocking_work_cb, cancel_after_work_cb) < 0) {
            puts("libuvdemo: cancel setup failed");
            return 1;
        }
    }
    if (uv_queue_work(&cancel_loop, &cancel_request, canceled_work_cb, cancel_after_work_cb) < 0 ||
        uv_fs_stat(&cancel_loop, &cancel_fs_request, "/fat", cancel_fs_cb) < 0 ||
        uv_cancel((uv_req_t *)&cancel_request) < 0 ||
        uv_cancel((uv_req_t *)&cancel_fs_request) < 0) {
        puts("libuvdemo: cancel setup failed");
        return 1;
    }
    for (size_t i = 0; i < sizeof(cancel_blockers) / sizeof(cancel_blockers[0]); i++) {
        uv_sem_post(&work_block_sem);
    }
    (void)uv_run(&cancel_loop, UV_RUN_DEFAULT);
    uv_fs_req_cleanup(&cancel_fs_request);
    uv_sem_destroy(&work_block_sem);
    if (!cancel_seen || !cancel_fs_seen || cancel_ran ||
        cancel_block_done_count != (int)(sizeof(cancel_blockers) / sizeof(cancel_blockers[0]))) {
        puts("libuvdemo: cancel failed");
        return 1;
    }
    puts("libuvdemo: async ok");
    puts("libuvdemo: work ok");
    return 0;
}

static uv_mutex_t thread_mutex;
static uv_mutex_t thread_cond_mutex;
static uv_cond_t thread_cond;
static uv_sem_t thread_sem;
static uv_rwlock_t thread_rwlock;
static uv_barrier_t thread_barrier;
static uv_once_t thread_once_guard = UV_ONCE_INIT;
static uv_key_t thread_key;
static int thread_value;
static int thread_ready;
static int thread_go;
static int thread_once_count;
static int thread_barrier_serial_count;
static int thread_detached_value;

static void thread_once_cb(void) {
    thread_once_count++;
}

static void thread_worker(void *arg) {
    uv_thread_t self = uv_thread_self();
    if (!uv_thread_equal(&self, &self)) {
        thread_value = -100;
        return;
    }
    uv_key_set(&thread_key, arg);
    uv_mutex_lock(&thread_mutex);
    thread_value += (int)(uintptr_t)uv_key_get(&thread_key);
    uv_mutex_unlock(&thread_mutex);

    uv_mutex_lock(&thread_cond_mutex);
    thread_ready = 1;
    uv_cond_signal(&thread_cond);
    while (!thread_go) {
        uv_cond_wait(&thread_cond, &thread_cond_mutex);
    }
    uv_mutex_unlock(&thread_cond_mutex);
}

static void barrier_worker(void *arg) {
    (void)arg;
    if (uv_barrier_wait(&thread_barrier) != 0) {
        thread_barrier_serial_count++;
    }
}

static void detached_worker(void *arg) {
    thread_detached_value = (int)(uintptr_t)arg;
    uv_sem_post(&thread_sem);
}

static int thread_test(void) {
    uv_thread_t thread;
    uv_thread_t barrier_thread;
    uv_thread_t detached_thread;
    uv_thread_options_t options = {
        .flags = UV_THREAD_HAS_STACK_SIZE,
        .stack_size = PTHREAD_STACK_MIN,
    };
    thread_value = 0;
    thread_ready = 0;
    thread_go = 0;
    thread_once_count = 0;
    thread_barrier_serial_count = 0;
    thread_detached_value = 0;

    if (uv_mutex_init(&thread_mutex) < 0 ||
        uv_mutex_init(&thread_cond_mutex) < 0 ||
        uv_cond_init(&thread_cond) < 0 ||
        uv_sem_init(&thread_sem, 0) < 0 ||
        uv_rwlock_init(&thread_rwlock) < 0 ||
        uv_key_create(&thread_key) < 0) {
        puts("libuvdemo: thread setup failed");
        return 1;
    }

    uv_once(&thread_once_guard, thread_once_cb);
    uv_once(&thread_once_guard, thread_once_cb);
    if (thread_once_count != 1) {
        puts("libuvdemo: once failed");
        return 1;
    }

    uv_mutex_lock(&thread_mutex);
    if (uv_mutex_trylock(&thread_mutex) != UV_EBUSY) {
        puts("libuvdemo: mutex try failed");
        return 1;
    }
    uv_mutex_unlock(&thread_mutex);

    if (uv_sem_trywait(&thread_sem) != UV_EAGAIN) {
        puts("libuvdemo: sem empty failed");
        return 1;
    }
    uv_sem_post(&thread_sem);
    uv_sem_wait(&thread_sem);
    if (uv_sem_trywait(&thread_sem) != UV_EAGAIN) {
        puts("libuvdemo: sem wait failed");
        return 1;
    }

    uv_rwlock_rdlock(&thread_rwlock);
    if (uv_rwlock_trywrlock(&thread_rwlock) != UV_EBUSY) {
        puts("libuvdemo: rwlock read failed");
        return 1;
    }
    uv_rwlock_rdunlock(&thread_rwlock);
    uv_rwlock_wrlock(&thread_rwlock);
    if (uv_rwlock_tryrdlock(&thread_rwlock) != UV_EBUSY) {
        puts("libuvdemo: rwlock write failed");
        return 1;
    }
    uv_rwlock_wrunlock(&thread_rwlock);

    if (uv_thread_create(&thread, thread_worker, (void *)(uintptr_t)37) < 0) {
        puts("libuvdemo: thread create failed");
        return 1;
    }
    uv_mutex_lock(&thread_cond_mutex);
    while (!thread_ready) {
        uv_cond_wait(&thread_cond, &thread_cond_mutex);
    }
    thread_go = 1;
    uv_cond_signal(&thread_cond);
    uv_mutex_unlock(&thread_cond_mutex);
    if (uv_thread_join(&thread) < 0 || thread_value != 37) {
        puts("libuvdemo: thread join failed");
        return 1;
    }

    if (uv_thread_create_ex(&detached_thread, &options, detached_worker, (void *)(uintptr_t)11) < 0 ||
        uv_thread_detach(&detached_thread) < 0) {
        puts("libuvdemo: detached thread failed");
        return 1;
    }
    uv_sem_wait(&thread_sem);
    if (thread_detached_value != 11) {
        puts("libuvdemo: detached thread failed");
        return 1;
    }

    if (uv_barrier_init(&thread_barrier, 2) < 0 ||
        uv_thread_create(&barrier_thread, barrier_worker, 0) < 0) {
        puts("libuvdemo: barrier setup failed");
        return 1;
    }
    if (uv_barrier_wait(&thread_barrier) != 0) {
        thread_barrier_serial_count++;
    }
    if (uv_thread_join(&barrier_thread) < 0 || thread_barrier_serial_count != 1) {
        puts("libuvdemo: barrier failed");
        return 1;
    }

    uv_barrier_destroy(&thread_barrier);
    uv_key_delete(&thread_key);
    uv_rwlock_destroy(&thread_rwlock);
    uv_sem_destroy(&thread_sem);
    uv_cond_destroy(&thread_cond);
    uv_mutex_destroy(&thread_cond_mutex);
    uv_mutex_destroy(&thread_mutex);
    puts("libuvdemo: thread ok");
    return 0;
}

static uv_loop_t poll_loop;
static uv_poll_t poll_handle;
static int poll_fds[2] = {-1, -1};
static int poll_seen;

static void poll_cb(uv_poll_t *handle, int status, int events) {
    char buffer[32];
    ssize_t count;
    if (status < 0 || (events & UV_READABLE) == 0) {
        puts("libuvdemo: poll event failed");
        uv_stop(&poll_loop);
        return;
    }
    count = read(poll_fds[0], buffer, sizeof(buffer) - 1);
    if (count < 0) {
        puts("libuvdemo: poll read failed");
        uv_stop(&poll_loop);
        return;
    }
    buffer[count] = '\0';
    poll_seen = strcmp(buffer, "libuv-poll-ok") == 0;
    uv_poll_stop(handle);
    uv_stop(&poll_loop);
}

static int poll_test(void) {
    char message[] = "libuv-poll-ok";
    uv_os_fd_t fd = -1;
    poll_seen = 0;
    if (uv_loop_init(&poll_loop) < 0 ||
        pipe(poll_fds) < 0 ||
        uv_poll_init(&poll_loop, &poll_handle, poll_fds[0]) < 0 ||
        uv_poll_start(&poll_handle, UV_READABLE, poll_cb) < 0) {
        puts("libuvdemo: poll setup failed");
        return 1;
    }
    if (uv_fileno((uv_handle_t *)&poll_handle, &fd) < 0 || fd != poll_fds[0]) {
        puts("libuvdemo: poll fileno failed");
        uv_poll_stop(&poll_handle);
        close(poll_fds[0]);
        close(poll_fds[1]);
        return 1;
    }
    if (write(poll_fds[1], message, strlen(message)) != (ssize_t)strlen(message)) {
        puts("libuvdemo: poll write failed");
        uv_poll_stop(&poll_handle);
        close(poll_fds[0]);
        close(poll_fds[1]);
        return 1;
    }
    (void)uv_run(&poll_loop, UV_RUN_DEFAULT);
    close(poll_fds[0]);
    close(poll_fds[1]);
    if (!poll_seen) {
        puts("libuvdemo: poll failed");
        return 1;
    }
    puts("libuvdemo: poll ok");
    return 0;
}

static uv_loop_t pipe_loop;
static uv_pipe_t pipe_read_handle;
static uv_pipe_t pipe_write_handle;
static uv_write_t pipe_write_request;
static char pipe_output[64];
static int pipe_write_seen;
static int pipe_read_eof;
static int pipe_failed;

static void maybe_stop_pipe_loop(void) {
    if (pipe_write_seen && pipe_read_eof) {
        uv_stop(&pipe_loop);
    }
}

static void pipe_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer) {
    (void)handle;
    (void)suggested_size;
    size_t used = strlen(pipe_output);
    buffer->base = pipe_output + used;
    buffer->len = sizeof(pipe_output) - used - 1;
}

static void pipe_write_cb(uv_write_t *request, int status) {
    if (request != &pipe_write_request || status < 0) {
        pipe_failed = 1;
    }
    pipe_write_seen = 1;
    uv_close((uv_handle_t *)&pipe_write_handle, 0);
    maybe_stop_pipe_loop();
}

static void pipe_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buffer) {
    (void)buffer;
    if (nread > 0) {
        size_t offset = (size_t)(buffer->base - pipe_output);
        pipe_output[offset + (size_t)nread] = '\0';
        return;
    }
    if (nread == UV_EOF) {
        pipe_read_eof = 1;
        uv_close((uv_handle_t *)stream, 0);
        maybe_stop_pipe_loop();
        return;
    }
    pipe_failed = 1;
    uv_stop(&pipe_loop);
}

static int pipe_stream_test(void) {
    uv_file fds[2] = {-1, -1};
    uv_buf_t buffer = uv_buf_init("libuv-pipe-ok", 13);
    memset(pipe_output, 0, sizeof(pipe_output));
    pipe_write_seen = 0;
    pipe_read_eof = 0;
    pipe_failed = 0;
    if (uv_loop_init(&pipe_loop) < 0 ||
        uv_pipe(fds, UV_NONBLOCK_PIPE, UV_NONBLOCK_PIPE) < 0 ||
        uv_pipe_init(&pipe_loop, &pipe_read_handle, 0) < 0 ||
        uv_pipe_init(&pipe_loop, &pipe_write_handle, 0) < 0 ||
        uv_pipe_open(&pipe_read_handle, fds[0]) < 0 ||
        uv_pipe_open(&pipe_write_handle, fds[1]) < 0 ||
        uv_read_start((uv_stream_t *)&pipe_read_handle, pipe_alloc_cb, pipe_read_cb) < 0 ||
        uv_write(&pipe_write_request, (uv_stream_t *)&pipe_write_handle, &buffer, 1, pipe_write_cb) < 0) {
        puts("libuvdemo: pipe setup failed");
        if (fds[0] >= 0) {
            close(fds[0]);
        }
        if (fds[1] >= 0) {
            close(fds[1]);
        }
        return 1;
    }
    (void)uv_run(&pipe_loop, UV_RUN_DEFAULT);
    if (pipe_failed || !pipe_write_seen || !pipe_read_eof ||
        strcmp(pipe_output, "libuv-pipe-ok") != 0) {
        puts("libuvdemo: pipe failed");
        return 1;
    }
    puts("libuvdemo: pipe stream ok");
    return 0;
}

static uv_loop_t gai_loop;
static int gai_seen;
static int gai_failed;

static void gai_cb(uv_getaddrinfo_t *request, int status, struct addrinfo *result) {
    struct sockaddr_in *addr;
    if (uv_req_get_type((uv_req_t *)request) != UV_GETADDRINFO ||
        status != 0 || result == 0 || result->ai_addr == 0) {
        gai_failed = 1;
    } else {
        addr = (struct sockaddr_in *)result->ai_addr;
        gai_seen = result->ai_family == AF_INET &&
            result->ai_socktype == SOCK_STREAM &&
            ntohs(addr->sin_port) == 80;
    }
    uv_freeaddrinfo(result);
    uv_stop(&gai_loop);
}

static int getaddrinfo_test(void) {
    uv_getaddrinfo_t request;
    struct addrinfo hints;
    gai_seen = 0;
    gai_failed = 0;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    if (uv_loop_init(&gai_loop) < 0 ||
        uv_getaddrinfo(&gai_loop, &request, gai_cb, "10.0.2.15", "80", &hints) < 0) {
        puts("libuvdemo: getaddrinfo setup failed");
        return 1;
    }
    if (uv_req_get_data((uv_req_t *)&request) != 0) {
        puts("libuvdemo: getaddrinfo request data failed");
        return 1;
    }
    (void)uv_run(&gai_loop, UV_RUN_DEFAULT);
    if (gai_failed || !gai_seen) {
        puts("libuvdemo: getaddrinfo failed");
        return 1;
    }
    puts("libuvdemo: getaddrinfo ok");
    return 0;
}

static uv_loop_t tty_loop;
static uv_tty_t tty_out;
static uv_write_t tty_write_request;
static int tty_write_seen;
static int signal_seen;
static int signal_oneshot_seen;

static void tty_write_cb(uv_write_t *request, int status) {
    if (request == &tty_write_request && status == 0) {
        tty_write_seen = 1;
    }
}

static void signal_cb(uv_signal_t *handle, int signum) {
    if (signum == SIGTERM) {
        signal_seen = 1;
        uv_signal_stop(handle);
    } else if (signum == SIGINT) {
        signal_oneshot_seen = 1;
    }
}

static int tty_signal_test(void) {
    int width = 0;
    int height = 0;
    uv_os_fd_t fd = -1;
    uv_tty_vtermstate_t state = UV_TTY_UNSUPPORTED;
    uv_signal_t signal_handle;
    uv_buf_t buffer = uv_buf_init("libuvdemo: tty write ok\n", 24);
    tty_write_seen = 0;
    signal_seen = 0;
    signal_oneshot_seen = 0;
    if (uv_loop_init(&tty_loop) < 0) {
        puts("libuvdemo: tty setup failed");
        return 1;
    }
    if (uv_guess_handle(STDIN_FILENO) != UV_TTY ||
        uv_guess_handle(STDOUT_FILENO) != UV_TTY ||
        uv_tty_init(&tty_loop, &tty_out, STDOUT_FILENO, 0) < 0 ||
        uv_handle_get_type((uv_handle_t *)&tty_out) != UV_TTY ||
        strcmp(uv_handle_type_name(UV_TTY), "tty") != 0 ||
        uv_handle_size(UV_TTY) != sizeof(uv_tty_t) ||
        !uv_is_writable((uv_stream_t *)&tty_out) ||
        uv_fileno((uv_handle_t *)&tty_out, &fd) < 0 ||
        fd != STDOUT_FILENO) {
        puts("libuvdemo: tty helpers failed");
        return 1;
    }
    if (uv_tty_get_winsize(&tty_out, &width, &height) < 0 || width <= 0 || height <= 0 ||
        uv_tty_set_mode(&tty_out, UV_TTY_MODE_NORMAL) < 0 ||
        uv_tty_set_vterm_state(UV_TTY_SUPPORTED) < 0 ||
        uv_tty_get_vterm_state(&state) < 0 ||
        state != UV_TTY_SUPPORTED) {
        puts("libuvdemo: tty mode failed");
        return 1;
    }
    if (uv_write(&tty_write_request, (uv_stream_t *)&tty_out, &buffer, 1, tty_write_cb) < 0 ||
        uv_stream_get_write_queue_size((uv_stream_t *)&tty_out) != buffer.len) {
        puts("libuvdemo: tty write setup failed");
        return 1;
    }
    (void)uv_run(&tty_loop, UV_RUN_DEFAULT);
    if (!tty_write_seen || uv_stream_get_write_queue_size((uv_stream_t *)&tty_out) != 0) {
        puts("libuvdemo: tty write failed");
        return 1;
    }
    uv_close((uv_handle_t *)&tty_out, 0);

    if (uv_signal_init(&tty_loop, &signal_handle) < 0 ||
        strcmp(uv_handle_type_name(UV_SIGNAL), "signal") != 0 ||
        uv_handle_size(UV_SIGNAL) != sizeof(uv_signal_t) ||
        uv_signal_start(&signal_handle, signal_cb, SIGTERM) < 0 ||
        !uv_is_active((uv_handle_t *)&signal_handle) ||
        uv_kill((int)getpid(), SIGTERM) < 0) {
        puts("libuvdemo: signal helpers failed");
        return 1;
    }
    (void)uv_run(&tty_loop, UV_RUN_DEFAULT);
    if (!signal_seen || uv_is_active((uv_handle_t *)&signal_handle) ||
        uv_signal_start_oneshot(&signal_handle, signal_cb, SIGINT) < 0 ||
        uv_kill((int)getpid(), SIGINT) < 0) {
        puts("libuvdemo: signal helpers failed");
        return 1;
    }
    (void)uv_run(&tty_loop, UV_RUN_DEFAULT);
    if (!signal_oneshot_seen || uv_is_active((uv_handle_t *)&signal_handle)) {
        puts("libuvdemo: signal dispatch failed");
        return 1;
    }
    uv_close((uv_handle_t *)&signal_handle, 0);
    if (uv_loop_close(&tty_loop) != 0) {
        puts("libuvdemo: tty loop close failed");
        return 1;
    }
    puts("libuvdemo: tty ok");
    puts("libuvdemo: signal ok");
    return 0;
}

static uv_loop_t process_loop;
static uv_pipe_t process_stdin_pipe;
static uv_pipe_t process_stdout_pipe;
static uv_process_t process_handle;
static uv_write_t process_write_request;
static char process_output[128];
static int process_exit_seen;
static int process_stdout_eof;
static int process_failed;

static void process_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer) {
    (void)handle;
    (void)suggested_size;
    size_t used = strlen(process_output);
    buffer->base = process_output + used;
    buffer->len = sizeof(process_output) - used - 1;
}

static void maybe_stop_process_loop(void) {
    if (process_exit_seen && process_stdout_eof) {
        uv_stop(&process_loop);
    }
}

static void process_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buffer) {
    (void)buffer;
    if (nread > 0) {
        size_t offset = (size_t)(buffer->base - process_output);
        process_output[offset + (size_t)nread] = '\0';
        return;
    }
    if (nread == UV_EOF) {
        process_stdout_eof = 1;
        uv_close((uv_handle_t *)stream, 0);
        maybe_stop_process_loop();
        return;
    }
    process_failed = 1;
    uv_stop(&process_loop);
}

static void process_exit_cb(uv_process_t *process, int64_t exit_status, int term_signal) {
    if (process != &process_handle || exit_status != 0 || term_signal != 0) {
        process_failed = 1;
    }
    process_exit_seen = 1;
    maybe_stop_process_loop();
}

static void process_write_close_cb(uv_write_t *request, int status) {
    if (status < 0) {
        process_failed = 1;
    }
    if (request->handle != 0) {
        uv_close((uv_handle_t *)request->handle, 0);
    }
}

static void process_write_keep_cb(uv_write_t *request, int status) {
    (void)request;
    if (status < 0) {
        process_failed = 1;
    }
}

static int process_validation_test(void) {
    uv_loop_t loop;
    uv_process_t bad_process;
    uv_process_options_t options;
    uv_stdio_container_t stdio[1];
    char *valid_argv[] = {"echo", "ignored", 0};
    char *empty_argv[] = {0};
    memset(&options, 0, sizeof(options));
    memset(stdio, 0, sizeof(stdio));
    if (uv_loop_init(&loop) < 0) {
        puts("libuvdemo: process validation setup failed");
        return 1;
    }
    options.file = "";
    options.args = valid_argv;
    if (uv_spawn(&loop, &bad_process, &options) != UV_EINVAL) {
        puts("libuvdemo: process empty file failed");
        return 1;
    }
    options.file = "echo";
    options.args = empty_argv;
    if (uv_spawn(&loop, &bad_process, &options) != UV_EINVAL) {
        puts("libuvdemo: process empty args failed");
        return 1;
    }
    options.args = valid_argv;
    options.flags = UV_PROCESS_SETUID;
    if (uv_spawn(&loop, &bad_process, &options) != UV_ENOSYS) {
        puts("libuvdemo: process unsupported flags failed");
        return 1;
    }
    options.flags = 0;
    options.stdio_count = 1;
    options.stdio = 0;
    if (uv_spawn(&loop, &bad_process, &options) != UV_EINVAL) {
        puts("libuvdemo: process null stdio failed");
        return 1;
    }
    options.stdio = stdio;
    stdio[0].flags = UV_CREATE_PIPE | UV_INHERIT_FD;
    if (uv_spawn(&loop, &bad_process, &options) != UV_EINVAL) {
        puts("libuvdemo: process bad stdio flags failed");
        return 1;
    }
    stdio[0].flags = UV_INHERIT_FD;
    stdio[0].data.fd = -1;
    if (uv_spawn(&loop, &bad_process, &options) != UV_EBADF) {
        puts("libuvdemo: process bad fd failed");
        return 1;
    }
    options.file = "definitely-not-a-srvros-command";
    options.args = valid_argv;
    options.stdio_count = 0;
    options.stdio = 0;
    if (uv_spawn(&loop, &bad_process, &options) != UV_ENOENT) {
        puts("libuvdemo: process missing executable failed");
        return 1;
    }
    options.file = "echo";
    options.cwd = "/fat/does-not-exist";
    if (uv_spawn(&loop, &bad_process, &options) != UV_ENOENT) {
        puts("libuvdemo: process bad cwd failed");
        return 1;
    }
    if (uv_loop_close(&loop) != 0) {
        puts("libuvdemo: process validation loop close failed");
        return 1;
    }
    puts("libuvdemo: process validation ok");
    return 0;
}

static int process_inherit_fd_test(void) {
    const char *path = "/fat/libuvdemo-inherit-input.txt";
    char *argv[] = {"libuvdemo", "inherit-child", 0};
    uv_stdio_container_t stdio[3];
    uv_process_options_t options;
    int fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0644);
    if (fd < 0) {
        puts("libuvdemo: process inherit setup failed");
        return 1;
    }
    (void)write(fd, "uv-inherit-ok\n", 14);
    (void)lseek(fd, 0, SEEK_SET);
    memset(process_output, 0, sizeof(process_output));
    memset(stdio, 0, sizeof(stdio));
    memset(&options, 0, sizeof(options));
    process_exit_seen = 0;
    process_stdout_eof = 0;
    process_failed = 0;
    if (uv_loop_init(&process_loop) < 0) {
        puts("libuvdemo: process inherit setup failed");
        close(fd);
        return 1;
    }
    process_stdout_eof = 1;
    stdio[0].flags = UV_INHERIT_FD;
    stdio[0].data.fd = fd;
    stdio[1].flags = UV_IGNORE;
    stdio[2].flags = UV_IGNORE;
    options.exit_cb = process_exit_cb;
    options.file = "/libuvdemo";
    options.args = argv;
    options.stdio_count = 3;
    options.stdio = stdio;
    if (uv_spawn(&process_loop, &process_handle, &options) < 0 ||
        uv_process_get_pid(&process_handle) <= 0) {
        puts("libuvdemo: process inherit spawn failed");
        close(fd);
        return 1;
    }
    (void)uv_run(&process_loop, UV_RUN_DEFAULT);
    uv_close((uv_handle_t *)&process_handle, 0);
    if (process_failed || !process_exit_seen || !process_stdout_eof) {
        puts("libuvdemo: process inherit failed");
        close(fd);
        return 1;
    }
    close(fd);
    (void)unlink(path);
    puts("libuvdemo: process inherit ok");
    return 0;
}

static int process_test(void) {
    char *argv[] = {"cat", 0};
    uv_buf_t input = uv_buf_init("uv-process-ok\n", 14);
    uv_stdio_container_t stdio[3];
    uv_process_options_t options;
    memset(process_output, 0, sizeof(process_output));
    memset(stdio, 0, sizeof(stdio));
    memset(&options, 0, sizeof(options));
    process_exit_seen = 0;
    process_stdout_eof = 0;
    process_failed = 0;
    if (uv_loop_init(&process_loop) < 0 ||
        uv_pipe_init(&process_loop, &process_stdin_pipe, 0) < 0 ||
        uv_pipe_init(&process_loop, &process_stdout_pipe, 0) < 0) {
        puts("libuvdemo: process setup failed");
        return 1;
    }
    stdio[0].flags = UV_CREATE_PIPE | UV_READABLE_PIPE;
    stdio[0].data.stream = (uv_stream_t *)&process_stdin_pipe;
    stdio[1].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
    stdio[1].data.stream = (uv_stream_t *)&process_stdout_pipe;
    stdio[2].flags = UV_IGNORE;
    options.exit_cb = process_exit_cb;
    options.file = "cat";
    options.args = argv;
    options.stdio_count = 3;
    options.stdio = stdio;
    if (uv_spawn(&process_loop, &process_handle, &options) < 0 ||
        uv_process_get_pid(&process_handle) <= 0 ||
        uv_read_start((uv_stream_t *)&process_stdout_pipe, process_alloc_cb, process_read_cb) < 0 ||
        uv_write(&process_write_request,
            (uv_stream_t *)&process_stdin_pipe,
            &input,
            1,
            process_write_close_cb) < 0) {
        puts("libuvdemo: process spawn failed");
        return 1;
    }
    (void)uv_run(&process_loop, UV_RUN_DEFAULT);
    if (process_failed || !process_exit_seen || !process_stdout_eof ||
        strstr(process_output, "uv-process-ok") == 0) {
        puts("libuvdemo: process failed");
        return 1;
    }
    uv_close((uv_handle_t *)&process_handle, 0);
    puts("libuvdemo: pipe ok");
    puts("libuvdemo: process ok");
    return 0;
}

static int process_cwd_test(void) {
    char *argv[] = {"pwd", 0};
    uv_stdio_container_t stdio[3];
    uv_process_options_t options;
    memset(process_output, 0, sizeof(process_output));
    memset(stdio, 0, sizeof(stdio));
    memset(&options, 0, sizeof(options));
    process_exit_seen = 0;
    process_stdout_eof = 0;
    process_failed = 0;
    if (uv_loop_init(&process_loop) < 0 ||
        uv_pipe_init(&process_loop, &process_stdout_pipe, 0) < 0) {
        puts("libuvdemo: process cwd setup failed");
        return 1;
    }
    stdio[0].flags = UV_IGNORE;
    stdio[1].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
    stdio[1].data.stream = (uv_stream_t *)&process_stdout_pipe;
    stdio[2].flags = UV_IGNORE;
    options.exit_cb = process_exit_cb;
    options.file = "pwd";
    options.args = argv;
    options.cwd = "/fat/bin";
    options.stdio_count = 3;
    options.stdio = stdio;
    if (uv_spawn(&process_loop, &process_handle, &options) < 0 ||
        uv_read_start((uv_stream_t *)&process_stdout_pipe, process_alloc_cb, process_read_cb) < 0) {
        puts("libuvdemo: process cwd spawn failed");
        return 1;
    }
    (void)uv_run(&process_loop, UV_RUN_DEFAULT);
    if (process_failed || !process_exit_seen || !process_stdout_eof ||
        strstr(process_output, "/fat/bin") == 0) {
        puts("libuvdemo: process cwd failed");
        return 1;
    }
    uv_close((uv_handle_t *)&process_handle, 0);
    puts("libuvdemo: process cwd ok");
    return 0;
}

static int process_duplex_child(void) {
    char buffer[32];
    ssize_t count = read(0, buffer, sizeof(buffer));
    if (count < 0) {
        return 2;
    }
    if (write(0, "duplex:", 7) != 7) {
        return 3;
    }
    return write(0, buffer, (size_t)count) == count ? 0 : 4;
}

static int process_duplex_test(void) {
    char *argv[] = {"libuvdemo", "duplex-child", 0};
    uv_buf_t input = uv_buf_init("ping", 4);
    uv_stdio_container_t stdio[3];
    uv_process_options_t options;
    memset(process_output, 0, sizeof(process_output));
    memset(stdio, 0, sizeof(stdio));
    memset(&options, 0, sizeof(options));
    process_exit_seen = 0;
    process_stdout_eof = 0;
    process_failed = 0;
    if (uv_loop_init(&process_loop) < 0 ||
        uv_pipe_init(&process_loop, &process_stdout_pipe, 0) < 0) {
        puts("libuvdemo: process duplex setup failed");
        return 1;
    }
    stdio[0].flags = UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE;
    stdio[0].data.stream = (uv_stream_t *)&process_stdout_pipe;
    stdio[1].flags = UV_IGNORE;
    stdio[2].flags = UV_IGNORE;
    options.exit_cb = process_exit_cb;
    options.file = "/libuvdemo";
    options.args = argv;
    options.stdio_count = 3;
    options.stdio = stdio;
    if (uv_spawn(&process_loop, &process_handle, &options) < 0 ||
        uv_read_start((uv_stream_t *)&process_stdout_pipe, process_alloc_cb, process_read_cb) < 0 ||
        uv_write(&process_write_request,
            (uv_stream_t *)&process_stdout_pipe,
            &input,
            1,
            process_write_keep_cb) < 0) {
        puts("libuvdemo: process duplex spawn failed");
        return 1;
    }
    (void)uv_run(&process_loop, UV_RUN_DEFAULT);
    if (process_failed || !process_exit_seen || !process_stdout_eof ||
        strstr(process_output, "duplex:ping") == 0) {
        puts("libuvdemo: process duplex failed");
        return 1;
    }
    uv_close((uv_handle_t *)&process_handle, 0);
    puts("libuvdemo: process duplex ok");
    return 0;
}

static int process_inherit_child(void) {
    char buffer[32];
    ssize_t count = read(0, buffer, sizeof(buffer) - 1);
    if (count <= 0) {
        return 2;
    }
    buffer[count] = '\0';
    return strstr(buffer, "uv-inherit-ok") != 0 ? 0 : 3;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "duplex-child") == 0) {
        return process_duplex_child();
    }
    if (argc > 1 && strcmp(argv[1], "inherit-child") == 0) {
        return process_inherit_child();
    }

    puts("libuvdemo: srvros libuv port staging");
    if (version_test() != 0 ||
        error_test() != 0 ||
        platform_test() != 0 ||
        core_api_test() != 0 ||
        timer_test() != 0 ||
        phase_test() != 0 ||
        fs_test() != 0 ||
        fs_poll_test() != 0 ||
        work_test() != 0 ||
        thread_test() != 0 ||
        poll_test() != 0 ||
        pipe_stream_test() != 0 ||
        getaddrinfo_test() != 0 ||
        tty_signal_test() != 0 ||
        process_validation_test() != 0 ||
        process_test() != 0 ||
        process_cwd_test() != 0 ||
        process_duplex_test() != 0 ||
        process_inherit_fd_test() != 0) {
        puts("libuvdemo: failed");
        return 1;
    }
    puts("libuvdemo: ok");
    return 0;
}
