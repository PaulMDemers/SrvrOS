#include <srvros/intel_gt_boot.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char recipe[]="srvros.storage=ram srvros.workspace-volume=37d8c5d0 srvros.desktop-release=0123456789abcdef srvros.capture=intel-display srvros.intel-backing=read-only srvros.intel-cache=read-only srvros.intel-window=read-only srvros.intel-handoff=qualify srvros.intel-gt-preflight=read-only srvros.intel-firmware=read-only srvros.intel-wake=trial srvros.capture-failure=reboot";
int main(void) {
 struct intel_gt_boot_facts f={true,true,true,true,true,true,true,true,true,true};
 assert(intel_gt_boot_grants(&f)==7 && !intel_gt_boot_grants(NULL));
 bool *fields[]={&f.compiled,&f.recipe,&f.loader,&f.efi64,&f.profile,&f.serialized,&f.excluded,&f.prerequisites,&f.firmware_quiet,&f.requesters_quiet};
 for(unsigned i=0;i<10;i++){*fields[i]=false;assert(!intel_gt_boot_grants(&f));*fields[i]=true;}
 assert(intel_gt_boot_recipe(recipe));assert(!intel_gt_boot_recipe(NULL) && !intel_gt_boot_recipe(" "));
 char bad[4096];
 for(unsigned i=0;recipe[i];i++)if(recipe[i]==' '){strcpy(bad,recipe+i+1);assert(!intel_gt_boot_recipe(bad));}
 const char *suffix[]={" srvros.aml=probe"," srvros.audio=hda"," srvros.storage=ram"," srvros.ap=probe","\t"};
 for(unsigned i=0;i<5;i++){snprintf(bad,sizeof(bad),"%s%s",recipe,suffix[i]);assert(!intel_gt_boot_recipe(bad));}
 strcpy(bad,recipe);strstr(bad,"37d8c5d0")[0]='G';assert(!intel_gt_boot_recipe(bad));
 memset(bad,' ',sizeof(bad)-1);bad[sizeof(bad)-1]=0;assert(!intel_gt_boot_recipe(bad));
 puts("intel-gt-boot: PASS explicit cooperative policy, all ten independent facts, strict complete recipe and untrusted/duplicate/long token refusal; not SMM retirement");
}
