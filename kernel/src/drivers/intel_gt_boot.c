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
  const char *start=p;unsigned n=0,bit=22;
  while(*p && *p!=' '){p++;if(++n>80 || ++total>2048)return false;}
  for(unsigned i=0;i<10;i++)if(equal(start,n,i==8 && (mode==5 || mode==6)?"srvros.intel-hda=lease-trial":i==8 && mode==4?"srvros.intel-hda=owner-trial":mode==3 && i==8?"srvros.intel-hda=read-only":names[i]))bit=i;
  if(suffix_hex(start,n,"srvros.workspace-volume=",8))bit=10;
  if(suffix_hex(start,n,"srvros.desktop-release=",16))bit=11;
  if((mode==24 || mode==23 || mode==1 || mode==2 || mode==7 || mode==8 || mode==9 || mode==10 || mode==11 || mode==12 || mode==13 || mode==14 || mode==15 || (mode==16 || mode==17 || mode==18 || mode==19 || mode==20 || mode==22 || mode==25)) && equal(start,n,"srvros.intel-engines=read-only"))bit=12;
  if((mode==4 || mode==5 || mode==6) && equal(start,n,"srvros.intel-hda-policy=cooperative-efi-v1"))bit=12;
  if((mode==24 || mode==23 || mode==7 || mode==8 || mode==9 || mode==11 || mode==12 || mode==13 || mode==14 || mode==15 || (mode==16 || mode==17 || mode==18 || mode==19 || mode==20 || mode==22 || mode==25)) && equal(start,n,"srvros.intel-completion=read-only"))bit=13;
  if(mode==6 && equal(start,n,"srvros.intel-readers=read-only"))bit=13;
  if((mode==2 || mode==10 || mode==21) && equal(start,n,"srvros.intel-blitter=retire"))bit=13;
  if((mode==10 || mode==21) && equal(start,n,"srvros.intel-shutdown=qualify"))bit=14;
  if((mode==9 || mode==11) && equal(start,n,"srvros.intel-bcs-reset=blitter-only"))bit=14;
  if(mode==8 && equal(start,n,"srvros.intel-reset-ready=request-cancel"))bit=14;
  if(mode==11 && equal(start,n,"srvros.intel-shutdown=observe-disabled"))bit=15;
  if((mode==12 || mode==13 || mode==14 || mode==15 || (mode==16 || mode==17 || mode==18 || mode==19 || mode==20 || mode==22 || mode==25)) && equal(start,n,"srvros.intel-takeover=preflight"))bit=14;
  if((mode==13 || mode==15 || (mode==16 || mode==17 || mode==18 || mode==19 || mode==20 || mode==22 || mode==25)) && equal(start,n,"srvros.intel-engines-prepare=request-cancel"))bit=15;
  if(mode==14 && equal(start,n,"srvros.intel-requester=read-only"))bit=15;
  if((mode==15 || (mode==16 || mode==17 || mode==18 || mode==19 || mode==20 || mode==22 || mode==25)) && equal(start,n,"srvros.intel-held-requester=read-only"))bit=16;
  if((mode==16 || mode==17 || mode==18 || mode==19 || mode==20 || mode==22 || mode==25) && equal(start,n,"srvros.intel-bcs-reset=held-blitter-only"))bit=17;
  if((mode==17 || mode==18 || mode==19 || mode==20 || mode==22 || mode==25) && equal(start,n,"srvros.intel-interval=read-only"))bit=18;
  if((mode==18 || mode==19 || mode==20 || mode==22 || mode==25) && equal(start,n,"srvros.intel-rehold=request-cancel"))bit=19;
  if((mode==19 || mode==20 || mode==22 || mode==25) && equal(start,n,"srvros.intel-joint=hda-claim"))bit=20;
  if(mode==20 && equal(start,n,"srvros.intel-gpu-requester=hold"))bit=21;
  if(mode==22 && equal(start,n,"srvros.intel-admission=disabled-bcs-v1"))bit=21;
  if(mode==25 && equal(start,n,"srvros.intel-ring=reset-v1"))bit=21;
  if(mode==23 && equal(start,n,"srvros.intel-ring=handoff-v1"))bit=14;
  if(mode==24 && equal(start,n,"srvros.intel-ring=entry-v1"))bit=14;
  if(mode==21 && equal(start,n,"srvros.intel-engines=read-only"))bit=12;
  if(mode==21 && equal(start,n,"srvros.intel-hda-bcs=joint-owner-v1"))bit=15;
  if(bit==22 || (seen&(1u<<bit)))return false;
  seen|=1u<<bit;
 }
 return seen==((mode==23 || mode==24)?0x7fffu:(mode==22 || mode==25)?0x3fffffu:mode==21?0xffffu:mode==20?0x3fffffu:mode==19?0x1fffffu:mode==18?0xfffffu:mode==17?0x7ffffu:(mode==16 || mode==17 || mode==18 || mode==19 || mode==20)?0x3ffffu:mode==15?0x1ffffu:(mode==11 || mode==13 || mode==14 || mode==15 || (mode==16 || mode==17 || mode==18 || mode==19 || mode==20 || mode==22 || mode==25))?0xffffu:(mode==8 || mode==9 || mode==10 || mode==11 || mode==12 || mode==13 || mode==14 || mode==15 || (mode==16 || mode==17 || mode==18 || mode==19 || mode==20 || mode==22 || mode==25))?0x7fffu:(mode==2 || mode==6 || mode==7)?0x3fffu:(mode==1 || mode==4 || mode==5)?0x1fffu:0xfffu);
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

bool intel_gt_boot_engine_prepare_recipe(const char *p){return recipe(p,13);}

bool intel_gt_boot_requester_recipe(const char *p){return recipe(p,14);}

bool intel_gt_boot_held_requester_recipe(const char *p){return recipe(p,15);}

bool intel_gt_boot_held_reset_recipe(const char *p){return recipe(p,16);}

bool intel_gt_boot_held_interval_recipe(const char *p){return recipe(p,17);}

bool intel_gt_boot_rehold_recipe(const char *p){return recipe(p,18);}

bool intel_gt_boot_joint_recipe(const char *p){return recipe(p,19);}

bool intel_gt_boot_gpu_hold_recipe(const char *p){return recipe(p,20);}

bool intel_gt_boot_hda_shutdown_recipe(const char *p){return recipe(p,21);}

bool intel_gt_boot_joint_admit_recipe(const char *p){return recipe(p,22);}

bool intel_gt_boot_ring_transition_recipe(const char *p){return recipe(p,23);}

bool intel_gt_boot_ring_entry_recipe(const char *p){return recipe(p,24);}

bool intel_gt_boot_ring_reset_recipe(const char *p){return recipe(p,25);}
