#include <srvros/intel_gt_boot.h>
static bool equal(const char *p,unsigned n,const char *s) {
 unsigned i=0;for(;i<n && s[i];i++)if(p[i]!=s[i])return false;return i==n && !s[i];
}
static bool suffix_hex(const char *p,unsigned n,const char *prefix,unsigned hex) {
 unsigned k=0;while(prefix[k]){if(k>=n || p[k]!=prefix[k])return false;k++;}
 if(n!=k+hex)return false;
 for(;k<n;k++)if(!((p[k]>='0' && p[k]<='9') || (p[k]>='a' && p[k]<='f')))return false;
 return true;
}
static bool recipe(const char *p,unsigned mode) {
 if(!p)return false;
 static const char *const names[]={"srvros.storage=ram","srvros.capture=intel-display","srvros.intel-backing=read-only","srvros.intel-cache=read-only","srvros.intel-window=read-only","srvros.intel-handoff=qualify","srvros.intel-gt-preflight=read-only","srvros.intel-firmware=read-only","srvros.intel-wake=trial","srvros.capture-failure=reboot"};
 unsigned seen=0,total=0;
 while(*p) {
  if(++total>2048)return false;
  if(*p==' '){p++;continue;}
  const char *start=p;unsigned n=0,bit=16;
  while(*p && *p!=' '){p++;if(++n>80 || ++total>2048)return false;}
  for(unsigned i=0;i<10;i++)if(equal(start,n,i==8 && (mode==5 || mode==6)?"srvros.intel-hda=lease-trial":i==8 && mode==4?"srvros.intel-hda=owner-trial":mode==3 && i==8?"srvros.intel-hda=read-only":names[i]))bit=i;
  if(suffix_hex(start,n,"srvros.workspace-volume=",8))bit=10;
  if(suffix_hex(start,n,"srvros.desktop-release=",16))bit=11;
  if((mode==1 || mode==2 || mode==7 || mode==8 || mode==9 || mode==10 || mode==11 || mode==12) && equal(start,n,"srvros.intel-engines=read-only"))bit=12;
  if((mode==4 || mode==5 || mode==6) && equal(start,n,"srvros.intel-hda-policy=cooperative-efi-v1"))bit=12;
  if((mode==7 || mode==8 || mode==9 || mode==11 || mode==12) && equal(start,n,"srvros.intel-completion=read-only"))bit=13;
  if(mode==6 && equal(start,n,"srvros.intel-readers=read-only"))bit=13;
  if((mode==2 || mode==10) && equal(start,n,"srvros.intel-blitter=retire"))bit=13;
  if(mode==10 && equal(start,n,"srvros.intel-shutdown=qualify"))bit=14;
  if((mode==9 || mode==11) && equal(start,n,"srvros.intel-bcs-reset=blitter-only"))bit=14;
  if(mode==8 && equal(start,n,"srvros.intel-reset-ready=request-cancel"))bit=14;
  if(mode==11 && equal(start,n,"srvros.intel-shutdown=observe-disabled"))bit=15;
  if(mode==12 && equal(start,n,"srvros.intel-takeover=preflight"))bit=14;
  if(bit==16 || (seen&(1u<<bit)))return false;
  seen|=1u<<bit;
 }
 return seen==(mode==11?0xffffu:(mode==8 || mode==9 || mode==10 || mode==11 || mode==12)?0x7fffu:(mode==2 || mode==6 || mode==7)?0x3fffu:(mode==1 || mode==4 || mode==5)?0x1fffu:0xfffu);
}
uint32_t intel_gt_boot_grants(const struct intel_gt_boot_facts *f) {
 if(!f || !f->compiled || !f->recipe || !f->loader || !f->efi64 || !f->profile || !f->serialized || !f->excluded || !f->prerequisites || !f->firmware_quiet || !f->requesters_quiet)return 0;
 return GT_ACCESS_REQUIRED;
}

bool intel_gt_boot_recipe(const char *p){return recipe(p,false);}
bool intel_gt_boot_engine_recipe(const char *p){return recipe(p,1);}

bool intel_gt_boot_blitter_recipe(const char *p){return recipe(p,2);}

bool intel_gt_boot_hda_recipe(const char *p){return recipe(p,3);}

bool intel_gt_boot_hda_owner_recipe(const char *p){return recipe(p,4);}

bool intel_gt_boot_hda_lease_recipe(const char *p){return recipe(p,5);}

bool intel_gt_boot_reader_recipe(const char *p){return recipe(p,6);}

bool intel_gt_boot_completion_recipe(const char *p){return recipe(p,7);}

bool intel_gt_boot_reset_ready_recipe(const char *p){return recipe(p,8);}

bool intel_gt_boot_bcs_reset_recipe(const char *p){return recipe(p,9);}

bool intel_gt_boot_shutdown_recipe(const char *p){return recipe(p,10);}

bool intel_gt_boot_bcs_shutdown_recipe(const char *p){return recipe(p,11);}

bool intel_gt_boot_takeover_recipe(const char *p){return recipe(p,12);}
