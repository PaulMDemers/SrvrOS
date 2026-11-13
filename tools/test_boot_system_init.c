#include <srvros/intel_gt_boot.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned spawns, reapers, waits, clocks, deferred;
static int64_t next_pid=42;
static bool startup_ready=true;
static int64_t process_spawn_background_elf_args_quiet(const char *path,const char *args) {
    assert(!strcmp(path,"/init") && !strcmp(args,"--system"));spawns++;return next_pid;
}
static bool process_set_reaper(uint64_t pid) {assert(pid==42);reapers++;return true;}
static bool process_wait_startup(uint64_t pid,uint64_t ticks) {
    assert(pid==42 && ticks==300);waits++;return startup_ready;
}
static uint64_t timer_monotonic_us(void) {return ++clocks;}
static void console_printf(const char *fmt,...) {(void)fmt;}
static void console_write(const char *s) {if(strstr(s,"deferred=startup-transfer-preinit") || strstr(s,"deferred=startup-buffers-preinit") || strstr(s,"deferred=startup-source-preinit") || strstr(s,"deferred=nonzero-source-preinit") || strstr(s,"deferred=nonzero-reset-preinit") || strstr(s,"deferred=entry-requester-preinit") || strstr(s,"deferred=ring-reset-preinit") || strstr(s,"deferred=ring-entry-preinit") || strstr(s,"deferred=ring-transition-preinit") || strstr(s,"deferred=joint-admit-preinit") || strstr(s,"deferred=hda-bcs-preinit") || strstr(s,"deferred=gpu-hold-preinit") || strstr(s,"deferred=joint-preinit") || strstr(s,"deferred=rehold-preinit") || strstr(s,"deferred=held-reset-preinit") || strstr(s,"deferred=held-interval-preinit"))deferred++;}
#include "../kernel/src/boot_system_init.inc"

static void reset(void) {
    spawns=reapers=waits=clocks=deferred=0;next_pid=42;startup_ready=true;
}
static void normal(const char *cmdline) {
    reset();assert(boot_system_init(cmdline,true));
    assert(spawns==1 && reapers==1 && waits==1 && clocks==2 && !deferred);
}
#ifndef BOOT_SYSTEM_INIT_FIXTURE_ONLY
int main(void) {
    const char *recipe="srvros.storage=ram srvros.workspace-volume=37d8c5d0 srvros.desktop-release=0123456789abcdef srvros.capture=intel-display srvros.intel-backing=read-only srvros.intel-cache=read-only srvros.intel-window=read-only srvros.intel-handoff=qualify srvros.intel-gt-preflight=read-only srvros.intel-firmware=read-only srvros.intel-wake=trial srvros.capture-failure=reboot srvros.intel-engines=read-only srvros.intel-completion=read-only srvros.intel-takeover=preflight srvros.intel-engines-prepare=request-cancel srvros.intel-held-requester=read-only srvros.intel-bcs-reset=held-blitter-only";
    assert(intel_gt_boot_held_reset_recipe(recipe));reset();
#ifdef SRVROS_INTEL_PREPARE_RESET
    assert(!boot_system_init(recipe,true));
    assert(deferred==1 && !spawns && !reapers && !waits && !clocks);
#else
    normal(recipe);
#endif
    normal(NULL);normal("");normal("srvros.desktop=session");
    char bad[3000];unsigned tokens=0;
    for(const char *start=recipe;*start;) {
        const char *end=start;while(*end && *end!=' ')end++;
        snprintf(bad,sizeof(bad),"%.*s%s",(int)(start-recipe),recipe,*end?end+1:end);
        assert(!intel_gt_boot_held_reset_recipe(bad));normal(bad);
        snprintf(bad,sizeof(bad),"%s %.*s",recipe,(int)(end-start),start);
        assert(!intel_gt_boot_held_reset_recipe(bad));normal(bad);
        tokens++;start=*end?end+1:end;
    }
    assert(tokens==18);
    const char *extras[]={"srvros.desktop=session","srvros.aml=probe","srvros.intel-bcs-reset=blitter-only","unknown=yes"};
    for(unsigned i=0;i<sizeof(extras)/sizeof(extras[0]);i++) {
        snprintf(bad,sizeof(bad),"%s %s",recipe,extras[i]);normal(bad);
    }
    reset();next_pid=-1;assert(!boot_system_init("",true));
    assert(spawns==1 && !reapers && !waits && !clocks && !deferred);
    reset();assert(!boot_system_init("",false));
    assert(spawns==1 && reapers==1 && !waits && clocks==2 && !deferred);
    reset();startup_ready=false;assert(!boot_system_init("",true));
    assert(spawns==1 && reapers==1 && waits==1 && clocks==2 && !deferred);
    puts("boot-system-init: PASS exact diagnostic isolation, missing/duplicate/foreign recipes and ordinary readiness/failure semantics");
}
#endif
