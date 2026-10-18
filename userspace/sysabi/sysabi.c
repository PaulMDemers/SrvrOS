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

    struct gfx_info full;
    memset(&full, 0, sizeof(full));
    full.abi_version = SRV_ABI_VERSION;
    full.struct_size = sizeof(full);
    if (gfx_info(&full) != 0) {
        return fail("gfx-full");
    }
    if (full.width == 0 ||
        full.height == 0 ||
        full.pitch == 0 ||
        full.bpp == 0 ||
        full.scale_num == 0 ||
        full.scale_den == 0 ||
        full.accel_backend > GFX_ACCEL_INTEL_GEN8) {
        return fail("gfx-full-fields");
    }

    uint32_t pixels[4] = {
        0x1f6feb, 0x2ea043,
        0xd29922, 0xf85149,
    };
    if (blitrect(0, 0, 2, 2, pixels, 2) != 0) {
        return fail("gfx-blit");
    }
    return 0;
}

static int check_devices(void) {
    struct { struct srv_device_info info;uint64_t canary; } probe={.canary=SYSABI_CANARY};
    long index=0;unsigned count=0;
    while((index=srv_device_list(SRV_DEVICE_PCI,index,&probe.info))>0) {
        if(++count>128 || probe.info.version!=SRV_DEVICE_VERSION || probe.info.size!=sizeof(probe.info) ||
           probe.info.bus_kind!=SRV_DEVICE_PCI || probe.canary!=SYSABI_CANARY) return fail("device-list");
    }
    if(index<0 || !count || srv_device_list(0,0,&probe.info)!=-1 ||
       srv_syscall4(SYS_DEVICE_LIST,SRV_DEVICE_PCI,0,0,sizeof(probe.info))!=-1 ||
       srv_syscall4(SYS_DEVICE_LIST,SRV_DEVICE_PCI,0,(long)&probe.info,sizeof(probe.info)-1)!=-1 ||
       srv_device_list(SRV_DEVICE_USB,UINT64_MAX,&probe.info)!=0 || probe.canary!=SYSABI_CANARY) return fail("device-arguments");
    return 0;
}

static int check_smc(void) {
    struct { struct srv_smc_status value;uint64_t canary; } p={.canary=SYSABI_CANARY};
    if(srv_smc_status(&p.value) || p.value.version!=SRV_SMC_VERSION || p.canary!=SYSABI_CANARY) return fail("smc-query");
    if(srv_syscall2(SYS_SMC_STATUS,(long)&p.value,sizeof(p.value)-1)!=-1 ||
       srv_syscall2(SYS_SMC_STATUS,0,sizeof(p.value))!=-1 ||
       srv_syscall2(SYS_SMC_STATUS,-1,sizeof(p.value))!=-1 || p.canary!=SYSABI_CANARY) return fail("smc-buffer");
    return 0;
}

static int check_thermal(void) {
    struct { struct srv_thermal_status value;uint64_t canary; } p={.canary=SYSABI_CANARY};
    if(srv_thermal_status(&p.value) || p.value.version!=SRV_THERMAL_VERSION || p.canary!=SYSABI_CANARY) return fail("thermal-query");
    if(srv_syscall2(SYS_THERMAL_STATUS,(long)&p.value,sizeof(p.value)-1)!=-1 ||
       srv_syscall2(SYS_THERMAL_STATUS,0,sizeof(p.value))!=-1 ||
       srv_syscall2(SYS_THERMAL_STATUS,-1,sizeof(p.value))!=-1 || p.canary!=SYSABI_CANARY) return fail("thermal-buffer");
    return 0;
}

static int check_audio(void) {
    struct { struct srv_audio_status status;uint64_t canary; } probe={.canary=SYSABI_CANARY};
    if(gui_audio(SRV_AUDIO_QUERY,0,&probe.status) || probe.status.version!=SRV_AUDIO_VERSION ||
       probe.status.volume>100 || probe.canary!=SYSABI_CANARY) return fail("audio-query");
    uint64_t started=probe.status.tests_started;
    if(gui_audio(SRV_AUDIO_TEST,0,&probe.status)!=-1 || probe.status.tests_started!=started ||
       probe.canary!=SYSABI_CANARY) return fail("audio-owner");
    if(srv_syscall4(SYS_AUDIO,SRV_AUDIO_QUERY,0,(long)&probe.status,sizeof(probe.status)-1)!=-1 ||
       srv_syscall4(SYS_AUDIO,SRV_AUDIO_QUERY,0,0,sizeof(probe.status))!=-1 ||
       srv_syscall4(SYS_AUDIO,SRV_AUDIO_QUERY,0,(long)-1,sizeof(probe.status))!=-1) return fail("audio-buffer");
    if(gui_audio(SRV_AUDIO_QUERY,1,&probe.status)!=-1 || probe.canary!=SYSABI_CANARY) return fail("audio-argument");
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

    uint64_t surface_id = 0;
    uint32_t input[4] = {
        0x112233, 0x445566,
        0x778899, 0xaabbcc,
    };
    uint32_t output[4] = {0};
    if (gui_surface_create(2, 2, 0, &surface_id) != 0 || surface_id == 0) {
        return fail("gui-surface-create");
    }
    if (gui_surface_blit(surface_id, 0, 0, 2, 2, input, 2) != 0) {
        return fail("gui-surface-blit");
    }
    if (gui_surface_copy(surface_id, 0, 0, 2, 2, output, 2) != 0) {
        return fail("gui-surface-copy");
    }
    if (memcmp(input, output, sizeof(input)) != 0) {
        return fail("gui-surface-data");
    }
    if (gui_surface_destroy(surface_id) != 0) {
        return fail("gui-surface-destroy");
    }
    return 0;
}

int main(void) {
    if (check_stat() != 0 ||
        check_statfs() != 0 ||
        check_process() != 0 ||
        check_console() != 0 ||
        check_gfx() != 0 ||
        check_devices() != 0 ||
        check_audio() != 0 ||
        check_thermal() != 0 ||
        check_smc() != 0 ||
        check_gui() != 0) {
        return 1;
    }
    printf("sysabi: ok\n");
    return 0;
}
