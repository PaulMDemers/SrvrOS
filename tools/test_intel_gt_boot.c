#include <srvros/intel_gt_boot.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char recipe[]="srvros.storage=ram srvros.workspace-volume=37d8c5d0 srvros.desktop-release=0123456789abcdef srvros.capture=intel-display srvros.intel-backing=read-only srvros.intel-cache=read-only srvros.intel-window=read-only srvros.intel-handoff=qualify srvros.intel-gt-preflight=read-only srvros.intel-firmware=read-only srvros.intel-wake=trial srvros.capture-failure=reboot";
int main(void) {
 char bad_engine[2048];
 struct intel_gt_boot_facts f={true,true,true,true,true,true,true,true,true,true};
 assert(intel_gt_boot_grants(&f)==7 && !intel_gt_boot_grants(NULL));
 bool *fields[]={&f.compiled,&f.recipe,&f.loader,&f.efi64,&f.profile,&f.serialized,&f.excluded,&f.prerequisites,&f.firmware_quiet,&f.requesters_quiet};
 for(unsigned i=0;i<10;i++){*fields[i]=false;assert(!intel_gt_boot_grants(&f));*fields[i]=true;}
 snprintf(bad_engine,sizeof(bad_engine),"%s srvros.intel-engines=read-only",recipe);
 assert(intel_gt_boot_engine_recipe(bad_engine));assert(!intel_gt_boot_recipe(bad_engine));
 assert(!intel_gt_boot_engine_recipe(recipe));
 char blitter[2200];snprintf(blitter,sizeof(blitter),"%s srvros.intel-blitter=retire",bad_engine);
 assert(intel_gt_boot_blitter_recipe(blitter));assert(!intel_gt_boot_engine_recipe(blitter) && !intel_gt_boot_recipe(blitter));assert(!intel_gt_boot_blitter_recipe(bad_engine));
 char shutdown[2400];snprintf(shutdown,sizeof(shutdown),"%s srvros.intel-shutdown=qualify",blitter);
 assert(intel_gt_boot_shutdown_recipe(shutdown) && !intel_gt_boot_blitter_recipe(shutdown) && !intel_gt_boot_shutdown_recipe(blitter));
 char missing[3000];
 for(const char *start=shutdown;*start;) {
  const char *end=start;while(*end && *end!=' ')end++;
  snprintf(missing,sizeof(missing),"%.*s%s",(int)(start-shutdown),shutdown,*end?end+1:end);assert(!intel_gt_boot_shutdown_recipe(missing));
  snprintf(missing,sizeof(missing),"%s %.*s",shutdown,(int)(end-start),start);assert(!intel_gt_boot_shutdown_recipe(missing));start=*end?end+1:end;
 }
 const char *blit=strstr(shutdown,"srvros.intel-blitter=retire");
 snprintf(missing,sizeof(missing),"%.*ssrvros.intel-completion=read-only%s",(int)(blit-shutdown),shutdown,blit+strlen("srvros.intel-blitter=retire"));assert(!intel_gt_boot_shutdown_recipe(missing));
 strcat(blitter," srvros.intel-blitter=retire");assert(!intel_gt_boot_blitter_recipe(blitter));
 strcat(bad_engine," srvros.intel-engines=read-only");assert(!intel_gt_boot_engine_recipe(bad_engine));
 assert(intel_gt_boot_recipe(recipe));assert(!intel_gt_boot_recipe(NULL) && !intel_gt_boot_recipe(" "));
 char bad[4096];
 for(unsigned i=0;recipe[i];i++)if(recipe[i]==' '){strcpy(bad,recipe+i+1);assert(!intel_gt_boot_recipe(bad));}
 const char *suffix[]={" srvros.aml=probe"," srvros.audio=hda"," srvros.storage=ram"," srvros.ap=probe","\t"};
 for(unsigned i=0;i<5;i++){snprintf(bad,sizeof(bad),"%s%s",recipe,suffix[i]);assert(!intel_gt_boot_recipe(bad));}
 strcpy(bad,recipe);strstr(bad,"37d8c5d0")[0]='G';assert(!intel_gt_boot_recipe(bad));
 memset(bad,' ',sizeof(bad)-1);bad[sizeof(bad)-1]=0;assert(!intel_gt_boot_recipe(bad));
 char hda[2048];const char *wake=strstr(recipe,"srvros.intel-wake=trial");
 snprintf(hda,sizeof(hda),"%.*ssrvros.intel-hda=read-only%s",(int)(wake-recipe),recipe,wake+strlen("srvros.intel-wake=trial"));
 assert(intel_gt_boot_hda_recipe(hda) && !intel_gt_boot_recipe(hda) && !intel_gt_boot_hda_recipe(recipe));
 for(const char *start=hda;*start;) {
  const char *end=start;while(*end && *end!=' ')end++;
  snprintf(bad,sizeof(bad),"%.*s%s",(int)(start-hda),hda,*end?end+1:end);
  assert(!intel_gt_boot_hda_recipe(bad));
  snprintf(bad,sizeof(bad),"%s %.*s",hda,(int)(end-start),start);
  assert(!intel_gt_boot_hda_recipe(bad));
  start=*end?end+1:end;
 }
 const char *foreign[]={"srvros.intel-wake=trial","srvros.intel-engines=read-only","srvros.intel-blitter=retire","srvros.audio=hda","srvros.aml=probe","srvros.ap=probe"};
 for(unsigned i=0;i<6;i++){snprintf(bad,sizeof(bad),"%s %s",hda,foreign[i]);assert(!intel_gt_boot_hda_recipe(bad));}
 char owner[2048];
 snprintf(owner,sizeof(owner),"%.*ssrvros.intel-hda=owner-trial%s srvros.intel-hda-policy=cooperative-efi-v1",(int)(wake-recipe),recipe,wake+strlen("srvros.intel-wake=trial"));
 assert(intel_gt_boot_hda_owner_recipe(owner) && !intel_gt_boot_hda_recipe(owner) && !intel_gt_boot_recipe(owner));
 assert(!intel_gt_boot_hda_owner_recipe(hda) && !intel_gt_boot_hda_owner_recipe(recipe));
 for(const char *start=owner;*start;) {
  const char *end=start;while(*end && *end!=' ')end++;
  snprintf(bad,sizeof(bad),"%.*s%s",(int)(start-owner),owner,*end?end+1:end);
  assert(!intel_gt_boot_hda_owner_recipe(bad));
  snprintf(bad,sizeof(bad),"%s %.*s",owner,(int)(end-start),start);
  assert(!intel_gt_boot_hda_owner_recipe(bad));start=*end?end+1:end;
 }
 for(unsigned i=0;i<6;i++){snprintf(bad,sizeof(bad),"%s %s",owner,foreign[i]);assert(!intel_gt_boot_hda_owner_recipe(bad));}
 char lease[2048];const char *mode=strstr(owner,"srvros.intel-hda=owner-trial");
 snprintf(lease,sizeof(lease),"%.*ssrvros.intel-hda=lease-trial%s",(int)(mode-owner),owner,mode+strlen("srvros.intel-hda=owner-trial"));
 assert(intel_gt_boot_hda_lease_recipe(lease) && !intel_gt_boot_hda_owner_recipe(lease) && !intel_gt_boot_hda_lease_recipe(owner));
 for(const char *start=lease;*start;) {
  const char *end=start;while(*end && *end!=' ')end++;
  snprintf(bad,sizeof(bad),"%.*s%s",(int)(start-lease),lease,*end?end+1:end);assert(!intel_gt_boot_hda_lease_recipe(bad));
  snprintf(bad,sizeof(bad),"%s %.*s",lease,(int)(end-start),start);assert(!intel_gt_boot_hda_lease_recipe(bad));start=*end?end+1:end;
 }
 for(unsigned i=0;i<6;i++){snprintf(bad,sizeof(bad),"%s %s",lease,foreign[i]);assert(!intel_gt_boot_hda_lease_recipe(bad));}
 puts("intel-gt-boot: PASS explicit cooperative policy, all ten independent facts, strict complete recipe and untrusted/duplicate/long token refusal; not SMM retirement");
 char readers[2200];snprintf(readers,sizeof(readers),"%s srvros.intel-readers=read-only",lease);
 assert(intel_gt_boot_reader_recipe(readers) && !intel_gt_boot_hda_lease_recipe(readers) && !intel_gt_boot_reader_recipe(lease));
 for(const char *start=readers;*start;) {
  const char *end=start;while(*end && *end!=' ')end++;
  snprintf(bad,sizeof(bad),"%.*s%s",(int)(start-readers),readers,*end?end+1:end);assert(!intel_gt_boot_reader_recipe(bad));
  snprintf(bad,sizeof(bad),"%s %.*s",readers,(int)(end-start),start);assert(!intel_gt_boot_reader_recipe(bad));start=*end?end+1:end;
 }
 for(unsigned i=0;i<6;i++){snprintf(bad,sizeof(bad),"%s %s",readers,foreign[i]);assert(!intel_gt_boot_reader_recipe(bad));}
 char completion[2200];snprintf(completion,sizeof(completion),"%s srvros.intel-engines=read-only srvros.intel-completion=read-only",recipe);
 assert(intel_gt_boot_completion_recipe(completion) && !intel_gt_boot_engine_recipe(completion) && !intel_gt_boot_completion_recipe(recipe));
 for(const char *start=completion;*start;) {
  const char *end=start;while(*end && *end!=' ')end++;
  snprintf(bad,sizeof(bad),"%.*s%s",(int)(start-completion),completion,*end?end+1:end);assert(!intel_gt_boot_completion_recipe(bad));
  snprintf(bad,sizeof(bad),"%s %.*s",completion,(int)(end-start),start);assert(!intel_gt_boot_completion_recipe(bad));start=*end?end+1:end;
 }
 for(unsigned j=0;j<6;j++){snprintf(bad,sizeof(bad),"%s %s",completion,foreign[j]);assert(!intel_gt_boot_completion_recipe(bad));}
 char ready[2400];snprintf(ready,sizeof(ready),"%s srvros.intel-reset-ready=request-cancel",completion);
 assert(intel_gt_boot_reset_ready_recipe(ready) && !intel_gt_boot_completion_recipe(ready) && !intel_gt_boot_reset_ready_recipe(completion));
 for(const char *start=ready;*start;) {
  const char *end=start;while(*end && *end!=' ')end++;
  snprintf(bad,sizeof(bad),"%.*s%s",(int)(start-ready),ready,*end?end+1:end);assert(!intel_gt_boot_reset_ready_recipe(bad));
  snprintf(bad,sizeof(bad),"%s %.*s",ready,(int)(end-start),start);assert(!intel_gt_boot_reset_ready_recipe(bad));start=*end?end+1:end;
 }
 for(unsigned i=0;i<6;i++){snprintf(bad,sizeof(bad),"%s %s",ready,foreign[i]);assert(!intel_gt_boot_reset_ready_recipe(bad));}
 char reset_bcs[2400];snprintf(reset_bcs,sizeof(reset_bcs),"%s srvros.intel-bcs-reset=blitter-only",completion);
 assert(intel_gt_boot_bcs_reset_recipe(reset_bcs) && !intel_gt_boot_reset_ready_recipe(reset_bcs) && !intel_gt_boot_completion_recipe(reset_bcs) && !intel_gt_boot_bcs_reset_recipe(ready));
 for(const char *start=reset_bcs;*start;) {
  const char *end=start;while(*end && *end!=' ')end++;
  snprintf(bad,sizeof(bad),"%.*s%s",(int)(start-reset_bcs),reset_bcs,*end?end+1:end);assert(!intel_gt_boot_bcs_reset_recipe(bad));
  snprintf(bad,sizeof(bad),"%s %.*s",reset_bcs,(int)(end-start),start);assert(!intel_gt_boot_bcs_reset_recipe(bad));start=*end?end+1:end;
 }
 for(unsigned i=0;i<6;i++){snprintf(bad,sizeof(bad),"%s %s",reset_bcs,foreign[i]);assert(!intel_gt_boot_bcs_reset_recipe(bad));}

}
