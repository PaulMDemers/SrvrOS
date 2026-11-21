/* The actual pure census recipe and boot dispatcher, with only process I/O
 * stubbed. No root is erased to make admission succeed. */
#define main stolen_adapter_fixture_main
#include "test_intel_stolen_probe.c"
#undef main
static unsigned spawns,reapers,waits,clocks;
static int64_t next_pid=42;
static bool ready=true;
static int64_t process_spawn_background_elf_args_quiet(const char *path,const char *args) {
 assert(!strcmp(path,"/init") && !strcmp(args,"--system"));spawns++;return next_pid;
}
static bool process_set_reaper(uint64_t pid){assert(pid==42);reapers++;return true;}
static bool process_wait_startup(uint64_t pid,uint64_t ticks){assert(pid==42 && ticks==300);waits++;return ready;}
/* The dispatcher reads the ordinary clock only when it starts user init. */
#define timer_monotonic_us boot_init_now
static uint64_t boot_init_now(void){return ++clocks;}
#ifdef SRVROS_STOLEN_BOOT_ORDINARY
#undef SRVROS_INTEL_STOLEN_CAPTURE
#endif
#ifndef SRVROS_STOLEN_BOOT_INIT_FILE
#define SRVROS_STOLEN_BOOT_INIT_FILE "../kernel/src/boot_system_init.inc"
#endif
#include SRVROS_STOLEN_BOOT_INIT_FILE
#undef timer_monotonic_us
static void fresh(void){reset();spawns=reapers=waits=clocks=0;next_pid=42;ready=true;}
static void normal(const char *s){fresh();assert(boot_system_init(s,true));assert(spawns==1 && reapers==1 && waits==1 && clocks==2 && !model.deferred);}
int main(int argc,char **argv){
 if(argc==2 && !strcmp(argv[1],"--report")){fresh();model.report=true;assert(!boot_system_init(command,true));return 0;}
 assert(intel_stolen_probe_recipe(command));fresh();
#ifdef SRVROS_STOLEN_BOOT_ORDINARY
 normal(command);
#else
 assert(!boot_system_init(command,true));
 assert(model.deferred==1 && !spawns && !reapers && !waits && !clocks);
 assert(!model.maps && !model.held && !stolen.attempted);
 fresh();assert(!boot_system_init(command,false));assert(model.deferred==1 && !spawns && !clocks);
#endif
 normal(NULL);normal("");normal("srvros.desktop=session");
 char bad[1024];unsigned tokens=0;
 for(const char *start=command;*start;){
  const char *end=start;while(*end && *end!=' ')end++;
  snprintf(bad,sizeof bad,"%.*s%s",(int)(start-command),command,*end?end+1:end);
  assert(!intel_stolen_probe_recipe(bad));normal(bad);
  snprintf(bad,sizeof bad,"%s %.*s",command,(int)(end-start),start);
  assert(!intel_stolen_probe_recipe(bad));normal(bad);
  tokens++;start=*end?end+1:end;
 }
 assert(tokens==9);
 const char *extra[]={"srvros.desktop=session","srvros.audio=hda","srvros.aml=probe","unknown=yes"};
 for(unsigned i=0;i<sizeof extra/sizeof extra[0];i++){
  snprintf(bad,sizeof bad,"%s %s",command,extra[i]);assert(!intel_stolen_probe_recipe(bad));normal(bad);
 }
 fresh();next_pid=-1;assert(!boot_system_init("",true));assert(spawns==1 && !reapers && !waits && !clocks);
 fresh();assert(!boot_system_init("",false));assert(spawns==1 && reapers==1 && !waits && clocks==2);
 fresh();ready=false;assert(!boot_system_init("",true));assert(spawns==1 && reapers==1 && waits==1 && clocks==2);
 puts("boot-stolen: PASS actual recipe/dispatcher; exact terminal deferral, malformed and ordinary init isolation");
}
