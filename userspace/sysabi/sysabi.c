#include <srvros/conio.h>
#include <srvros/gfx.h>
#include <srvros/gui.h>
#include <srvros/sys.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SYSABI_CANARY 0x6b5a4938271605f4ull

struct small_stat {
    uint64_t abi_version;
    uint64_t struct_size;
    uint64_t size;
    uint64_t type;
};

struct small_fsinfo {
    uint64_t abi_version;
    uint64_t struct_size;
    uint64_t block_size;
};

struct small_process_info {
    uint64_t abi_version;
    uint64_t struct_size;
    uint64_t pid;
};

struct small_console_info {
    uint64_t abi_version;
    uint64_t struct_size;
    uint64_t columns;
};

struct small_gfx_info {
    uint64_t abi_version;
    uint64_t struct_size;
    uint64_t width;
};

struct small_gui_message {
    uint64_t abi_version;
    uint64_t struct_size;
    uint64_t type;
};

static void prepare(uint64_t *abi_version, uint64_t *struct_size, uint64_t size) {
    *abi_version = SRV_ABI_VERSION;
    *struct_size = size;
}

static int fail(const char *step) {
    printf("sysabi: fail %s\n", step);
    return 1;
}

static int check_stat(void) {
    struct {
        struct small_stat info;
        uint64_t canary;
    } probe;
    memset(&probe, 0, sizeof(probe));
    probe.canary = SYSABI_CANARY;
    prepare(&probe.info.abi_version, &probe.info.struct_size, sizeof(probe.info));
    if (srv_syscall2(SYS_STAT, (long)"/fat/bin/sh", (long)&probe.info) != 0) {
        return fail("stat");
    }
    if (probe.canary != SYSABI_CANARY ||
        probe.info.struct_size < sizeof(probe.info) ||
        probe.info.type != 0 ||
        probe.info.size == 0) {
        return fail("stat-copy");
    }
    return 0;
}

static int check_statfs(void) {
    struct {
        struct small_fsinfo info;
        uint64_t canary;
    } probe;
    memset(&probe, 0, sizeof(probe));
    probe.canary = SYSABI_CANARY;
    prepare(&probe.info.abi_version, &probe.info.struct_size, sizeof(probe.info));
    if (srv_syscall2(SYS_STATFS, (long)"/fat", (long)&probe.info) != 0) {
        return fail("statfs");
    }
    if (probe.canary != SYSABI_CANARY ||
        probe.info.struct_size < sizeof(probe.info) ||
        probe.info.block_size == 0) {
        return fail("statfs-copy");
    }
    return 0;
}

static int check_process(void) {
    struct {
        struct small_process_info info;
        uint64_t canary;
    } probe;
    memset(&probe, 0, sizeof(probe));
    probe.canary = SYSABI_CANARY;
    prepare(&probe.info.abi_version, &probe.info.struct_size, sizeof(probe.info));
    long next = srv_syscall2(SYS_PROC_LIST, 0, (long)&probe.info);
    if (next <= 0) {
        return fail("proc");
    }
    if (probe.canary != SYSABI_CANARY ||
        probe.info.struct_size < sizeof(probe.info) ||
        probe.info.pid == 0) {
        return fail("proc-copy");
    }
    return 0;
}

static int check_console(void) {
    struct {
        struct small_console_info info;
        uint64_t canary;
    } probe;
    memset(&probe, 0, sizeof(probe));
    probe.canary = SYSABI_CANARY;
    prepare(&probe.info.abi_version, &probe.info.struct_size, sizeof(probe.info));
    if (srv_syscall1(SYS_CONSOLE_INFO, (long)&probe.info) != 0) {
        return fail("console");
    }
    if (probe.canary != SYSABI_CANARY ||
        probe.info.struct_size < sizeof(probe.info) ||
        probe.info.columns == 0) {
        return fail("console-copy");
    }
    return 0;
}

static int check_gfx(void) {
    struct {
        struct small_gfx_info info;
        uint64_t canary;
    } probe;
    memset(&probe, 0, sizeof(probe));
    probe.canary = SYSABI_CANARY;
    prepare(&probe.info.abi_version, &probe.info.struct_size, sizeof(probe.info));
    if (srv_syscall1(SYS_GFX_INFO, (long)&probe.info) != 0) {
        return fail("gfx");
    }
    if (probe.canary != SYSABI_CANARY ||
        probe.info.struct_size < sizeof(probe.info) ||
        probe.info.width == 0) {
        return fail("gfx-copy");
    }
    return 0;
}

static int check_gui(void) {
    if (gui_register_server() != 0) {
        return fail("gui-register");
    }
    struct gui_message outgoing;
    memset(&outgoing, 0, sizeof(outgoing));
    outgoing.type = GUI_MSG_EVENT_CLICK;
    outgoing.target_pid = (uint64_t)srv_getpid();
    outgoing.control_id = 7;
    if (gui_send(&outgoing) != 0) {
        return fail("gui-send");
    }

    struct {
        struct small_gui_message info;
        uint64_t canary;
    } probe;
    memset(&probe, 0, sizeof(probe));
    probe.canary = SYSABI_CANARY;
    prepare(&probe.info.abi_version, &probe.info.struct_size, sizeof(probe.info));
    if (srv_syscall1(SYS_GUI_RECV, (long)&probe.info) != 1) {
        return fail("gui-recv");
    }
    if (probe.canary != SYSABI_CANARY ||
        probe.info.struct_size < sizeof(probe.info) ||
        probe.info.type != GUI_MSG_EVENT_CLICK) {
        return fail("gui-copy");
    }
    return 0;
}

int main(void) {
    if (check_stat() != 0 ||
        check_statfs() != 0 ||
        check_process() != 0 ||
        check_console() != 0 ||
        check_gfx() != 0 ||
        check_gui() != 0) {
        return 1;
    }
    printf("sysabi: ok\n");
    return 0;
}
