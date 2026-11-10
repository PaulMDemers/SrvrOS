#define BOOT_SYSTEM_INIT_FIXTURE_ONLY
#include "test_boot_system_init.c"
#undef BOOT_SYSTEM_INIT_FIXTURE_ONLY
int main(void) {
 const char *recipe="srvros.storage=ram srvros.workspace-volume=37d8c5d0 srvros.desktop-release=0123456789abcdef srvros.capture=intel-display srvros.intel-backing=read-only srvros.intel-cache=read-only srvros.intel-window=read-only srvros.intel-handoff=qualify srvros.intel-gt-preflight=read-only srvros.intel-firmware=read-only srvros.intel-wake=trial srvros.capture-failure=reboot srvros.intel-engines=read-only srvros.intel-completion=read-only srvros.intel-takeover=preflight srvros.intel-engines-prepare=request-cancel srvros.intel-held-requester=read-only srvros.intel-bcs-reset=held-blitter-only srvros.intel-interval=read-only";
 assert(intel_gt_boot_held_interval_recipe(recipe) && !intel_gt_boot_held_reset_recipe(recipe));reset();
 assert(!boot_system_init(recipe,true) && deferred==1 && !spawns && !waits && !clocks);
 char bad[3000];unsigned tokens=0;
 for(const char *start=recipe;*start;) {
  const char *end=start;while(*end && *end!=' ')end++;
  snprintf(bad,sizeof bad,"%.*s%s",(int)(start-recipe),recipe,*end?end+1:end);
  assert(!intel_gt_boot_held_interval_recipe(bad));normal(bad);
  snprintf(bad,sizeof bad,"%s %.*s",recipe,(int)(end-start),start);
  assert(!intel_gt_boot_held_interval_recipe(bad));normal(bad);
  tokens++;start=*end?end+1:end;
 }
 assert(tokens==19);normal(NULL);normal("");normal("srvros.desktop=session");
 snprintf(bad,sizeof bad,"%s unknown=yes",recipe);assert(!intel_gt_boot_held_interval_recipe(bad));normal(bad);
 puts("boot-held-interval: PASS exact19-token preinit isolation, old/missing/duplicate/foreign recipes and ordinary readiness");
}
