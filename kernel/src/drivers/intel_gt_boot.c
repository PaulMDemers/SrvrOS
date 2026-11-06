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
bool intel_gt_boot_recipe(const char *p) {
 if(!p)return false;
 static const char *const names[]={"srvros.storage=ram","srvros.capture=intel-display","srvros.intel-backing=read-only","srvros.intel-cache=read-only","srvros.intel-window=read-only","srvros.intel-handoff=qualify","srvros.intel-gt-preflight=read-only","srvros.intel-firmware=read-only","srvros.intel-wake=trial","srvros.capture-failure=reboot"};
 unsigned seen=0,total=0;
 while(*p) {
  if(++total>2048)return false;
  if(*p==' '){p++;continue;}
  const char *start=p;unsigned n=0,bit=12;
  while(*p && *p!=' '){p++;if(++n>80 || ++total>2048)return false;}
  for(unsigned i=0;i<10;i++)if(equal(start,n,names[i]))bit=i;
  if(suffix_hex(start,n,"srvros.workspace-volume=",8))bit=10;
  if(suffix_hex(start,n,"srvros.desktop-release=",16))bit=11;
  if(bit==12 || (seen&(1u<<bit)))return false;
  seen|=1u<<bit;
 }
 return seen==0xfffu;
}
uint32_t intel_gt_boot_grants(const struct intel_gt_boot_facts *f) {
 if(!f || !f->compiled || !f->recipe || !f->loader || !f->efi64 || !f->profile || !f->serialized || !f->excluded || !f->prerequisites || !f->firmware_quiet || !f->requesters_quiet)return 0;
 return GT_ACCESS_REQUIRED;
}
