#define BOOT_SYSTEM_INIT_FIXTURE_ONLY
#include "test_boot_system_init.c"
int main(void) {
 const char *recipe="srvros.storage=ram srvros.workspace-volume=37d8c5d0 srvros.desktop-release=0123456789abcdef srvros.capture=intel-display srvros.intel-backing=read-only srvros.intel-cache=read-only srvros.intel-window=read-only srvros.intel-handoff=qualify srvros.intel-gt-preflight=read-only srvros.intel-firmware=read-only srvros.intel-wake=trial srvros.capture-failure=reboot srvros.intel-engines=read-only srvros.intel-completion=read-only srvros.intel-ring=nonzero-reset-v1";
 assert(intel_gt_boot_nonzero_reset_recipe(recipe) && !intel_gt_boot_shutdown_recipe(recipe));reset();
 assert(!boot_system_init(recipe,true) && deferred==1 && !spawns && !reapers && !waits && !clocks);
 char bad[3000];unsigned tokens=0;
 for(const char *start=recipe;*start;) {
  const char *end=start;while(*end && *end!=' ')end++;
  snprintf(bad,sizeof bad,"%.*s%s",(int)(start-recipe),recipe,*end?end+1:end);
  assert(!intel_gt_boot_nonzero_reset_recipe(bad));normal(bad);
  snprintf(bad,sizeof bad,"%s %.*s",recipe,(int)(end-start),start);
  assert(!intel_gt_boot_nonzero_reset_recipe(bad));normal(bad);
  tokens++;start=*end?end+1:end;
 }
 assert(tokens==15);assert(!intel_gt_boot_ring_transition_recipe(recipe));assert(!intel_gt_boot_ring_entry_recipe(recipe));normal(NULL);normal("");normal("srvros.desktop=session");
 const char *extras[]={"srvros.intel-joint=hda-claim","srvros.intel-bcs-reset=blitter-only","srvros.intel-shutdown=qualify","srvros.aml=probe","unknown=yes"};
 for(unsigned i=0;i<sizeof extras/sizeof extras[0];i++) {
  snprintf(bad,sizeof bad,"%s %s",recipe,extras[i]);assert(!intel_gt_boot_nonzero_reset_recipe(bad));normal(bad);
 }
 puts("boot-nonzero-reset: PASS exact15 preinit isolation and ordinary readiness for missing/duplicate/foreign recipes");
}
