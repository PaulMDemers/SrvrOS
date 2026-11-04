#include <srvros/conio.h>
#include <srvros/gfx.h>
#include <srvros/gui.h>
#include <srvros/audio.h>
#include <srvros/sys.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

static int check_user_address(void) {
    uint8_t byte = 0;
    void *alias = (void *)((uintptr_t)&byte | (UINT64_C(1) << 48));
    if (srv_random(&byte, 1, 0) != 1 || srv_random(alias, 1, 0) >= 0)
        return fail("noncanonical-buffer");
    puts("sysabi: noncanonical buffer refused");
    return 0;
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

static int check_audio_stream(void) {
    struct { struct srv_pcm_status status;uint64_t canary; } probe={.canary=SYSABI_CANARY};
    if(srv_syscall4(SYS_AUDIO_STREAM,UINT64_MAX,0,0,0)!=-1 ||
       srv_syscall4(SYS_AUDIO_STREAM,SRV_PCM_OPEN,1,0,0)!=-1 ||
       srv_syscall4(SYS_AUDIO_STREAM,SRV_PCM_WRITE,0,0,4)!=-1 ||
       srv_syscall4(SYS_AUDIO_STREAM,SRV_PCM_WRITE,0,(long)-1,4)!=-1 ||
       srv_syscall4(SYS_AUDIO_STREAM,SRV_PCM_WRITE,0,(long)&probe,3)!=-1 ||
       srv_syscall4(SYS_AUDIO_STREAM,SRV_PCM_QUERY,0,(long)&probe.status,sizeof(probe.status)-1)!=-1 ||
       srv_syscall4(SYS_AUDIO_STREAM,SRV_PCM_QUERY,0,(long)-1,sizeof(probe.status))!=-1)
        return fail("pcm-invalid-request");
    for(unsigned token=1;token<=8;token++) {
        if(srv_pcm_query(token,&probe.status)!=-1 ||
           srv_pcm_write(token,&probe.canary,4)!=-1 || srv_pcm_close(token)!=-1)
            return fail("pcm-foreign-owner");
    }
    long handle=srv_pcm_open();
    if(handle>0) {
        if(srv_pcm_query(handle,&probe.status) || probe.status.version!=SRV_PCM_VERSION ||
           probe.status.state!=SRV_PCM_BUFFERING || probe.status.rate!=48000 ||
           probe.status.handle!=(uint64_t)handle || probe.canary!=SYSABI_CANARY ||
           srv_pcm_query(handle+1,&probe.status)!=-1 || srv_pcm_open()!=-1 || srv_pcm_close(handle))
            return fail("pcm-owner-snapshot");
    }
    return probe.canary==SYSABI_CANARY?0:fail("pcm-canary");
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

extern char **environ;

static void *invalid_instruction(void *unused) {
    (void)unused;
    __asm__ volatile ("ud2");
    return NULL;
}

static void *delayed_invalid_instruction(void *unused) {
    usleep(50000);
    return invalid_instruction(unused);
}

static int check_fault_exit(const char *mode, int expected_signal) {
    char *args[] = {"sysabi", (char *)mode, NULL};
    pid_t child;
    int status;
    if (posix_spawn(&child, "/fat/bin/sysabi", NULL, NULL, args, environ) ||
        waitpid(child, &status, 0) != child || !WIFSIGNALED(status) ||
        WTERMSIG(status) != expected_signal) return fail(mode);
    return 0;
}

int namespace_test(void);
int salvage_consumer_test(int reopen);
int dirty_salvage_test(int reopen);
int destination_salvage_test(int reopen);
int destination_open_test(void);
int namespace_workspace_hold(const char *mode);
int namespace_workspace_launch(const char *mode);
int namespace_workspace_verify(int system_only);
int namespace_test_persistence(int verify);

int main(int argc, char **argv) {
    if(argc==2 && !strcmp(argv[1],"--destination-open")) return destination_open_test();
    if(argc==2 && !strcmp(argv[1],"--destination-salvage-save")) return destination_salvage_test(0);
    if(argc==2 && !strcmp(argv[1],"--destination-salvage-reopen")) return destination_salvage_test(1);
    if(argc==2 && !strcmp(argv[1],"--dirty-salvage-save")) return dirty_salvage_test(0);
    if(argc==2 && !strcmp(argv[1],"--dirty-salvage-reopen")) return dirty_salvage_test(1);
    if(argc==2 && !strcmp(argv[1],"--salvage-cli-save")) return salvage_consumer_test(0);
    if(argc==2 && !strcmp(argv[1],"--salvage-cli-reopen")) return salvage_consumer_test(1);
    if (argc == 2 && !strncmp(argv[1], "--workspace-hold-",17)) return namespace_workspace_hold(argv[1]+17);
    if (argc == 2 && !strncmp(argv[1], "--workspace-launch-",19)) return namespace_workspace_launch(argv[1]+19);
    if (argc == 2 && !strcmp(argv[1], "--workspace-system")) return namespace_workspace_verify(1);
    if (argc == 2 && !strcmp(argv[1], "--workspace-verify")) return namespace_workspace_verify(0);
    if (argc == 2 && !strcmp(argv[1], "--namespace")) return namespace_test();
    if (argc == 2 && !strcmp(argv[1], "--namespace-save")) return namespace_test_persistence(0);
    if (argc == 2 && !strcmp(argv[1], "--namespace-reopen")) return namespace_test_persistence(1);
    if (argc == 2 && !strcmp(argv[1], "--fault-retired-main")) {
        pthread_t worker;
        if (pthread_create(&worker, NULL, delayed_invalid_instruction, NULL)) return 1;
        pthread_exit(NULL);
    }
    if (argc == 2 && !strcmp(argv[1], "--fault-worker")) {
        pthread_t worker;
        if (pthread_create(&worker, NULL, invalid_instruction, NULL)) return 1;
        pthread_join(worker, NULL);
        return fail("faulting worker returned");
    }
    if (argc == 2 && !strcmp(argv[1], "--fault-main")) {
        __asm__ volatile ("movb $0, (%0)" : : "r" ((uintptr_t)1) : "memory");
        return fail("unmapped user write returned");
    }
    if (check_user_address() || check_fault_exit("--fault-main", SIGSEGV) ||
        check_fault_exit("--fault-worker", SIGILL) ||
        check_fault_exit("--fault-retired-main", SIGILL)) return 1;
    puts("sysabi: faulting process and worker reaped");
    if (check_stat() != 0 ||
        check_statfs() != 0 ||
        check_process() != 0 ||
        check_console() != 0 ||
        check_gfx() != 0 ||
        check_devices() != 0 ||
        check_audio() != 0 ||
        check_audio_stream() != 0 ||
        check_thermal() != 0 ||
        check_smc() != 0 ||
        check_gui() != 0) {
        return 1;
    }
    printf("sysabi: ok\n");
    return 0;
}
