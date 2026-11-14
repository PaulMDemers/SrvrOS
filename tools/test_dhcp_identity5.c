#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <srvros/dhcp_identity5.h>
static void p16(uint8_t *p,unsigned v){p[0]=v>>8;p[1]=v;}
static void p32(uint8_t *p,uint32_t v){p16(p,v>>16);p16(p+2,v);}
static const uint8_t mac[6]={2,3,4,5,6,7};
static struct dhcp_identity5 see(const uint8_t *p,size_t n){return dhcp_identity5_observe(p,n,mac,123,0x0a00020f,0x0a000202);}
int main(void){
 uint8_t p[342]={0};memset(p,255,6);memcpy(p+6,mac,6);p16(p+12,0x800);
 p[14]=0x45;p16(p+16,328);p16(p+20,0x4000);p[22]=64;p[23]=17;p32(p+30,UINT32_MAX);
 p16(p+24,~dhcp_identity5_sum(p+14,20,0));p16(p+34,68);p16(p+36,67);p16(p+38,308);
 uint8_t *b=p+42;b[0]=1;b[1]=1;b[2]=6;p32(b+4,123);p16(b+10,0x8000);memcpy(b+28,mac,6);p32(b+236,0x63825363);
 unsigned at=240;b[at++]=53;b[at++]=1;b[at++]=3;b[at++]=61;b[at++]=7;b[at++]=1;
 memcpy(b+at,mac,6);at+=6;b[at++]=50;b[at++]=4;p32(b+at,0x0a00020f);at+=4;
 b[at++]=54;b[at++]=4;p32(b+at,0x0a000202);at+=4;b[at++]=55;b[at++]=4;
 memcpy(b+at,"\1\3\6\17",4);at+=4;b[at++]=255;
 uint8_t original[342];memcpy(original,p,sizeof p);assert(see(p,sizeof p).mask==DHCP_IDENTITY5_ALL);
 unsigned udp=~dhcp_identity5_sum(p+34,308,dhcp_identity5_sum(p+26,8,17+308))&65535;
 p16(p+40,udp?udp:65535);assert(see(p,sizeof p).mask==DHCP_IDENTITY5_ALL);memcpy(p,original,sizeof p);
 const unsigned offsets[]={6,46,70,288,299,305,52,30,0};
 const unsigned bits[]={2,4,8,16,32,64,128,256,512};
 for(unsigned i=0;i<sizeof offsets/sizeof *offsets;i++){
  p[offsets[i]]^=1;assert(!(see(p,sizeof p).mask&bits[i]));memcpy(p,original,sizeof p);
 }
 p[24]^=1;assert(!(see(p,sizeof p).mask&1));memcpy(p,original,sizeof p);
 p16(p+40,0x1234);assert(!(see(p,sizeof p).mask&1));memcpy(p,original,sizeof p);
 b[at]=1;assert(!see(p,sizeof p).mask);memcpy(p,original,sizeof p);
 b[at-1]=61;b[at]=255;assert(!see(p,sizeof p).mask);memcpy(p,original,sizeof p);
 long page=sysconf(_SC_PAGESIZE);uint8_t *base=mmap(NULL,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
 assert(base!=MAP_FAILED && !mprotect(base+page,page,PROT_NONE));
 for(unsigned n=0;n<=sizeof p;n++){uint8_t *v=base+page-n;memcpy(v,p,n);struct dhcp_identity5 r=see(v,n);assert(n==sizeof p?r.mask==DHCP_IDENTITY5_ALL:!r.mask);}
 for(unsigned n=42;n<=sizeof p;n++){
  uint8_t *v=base+page-n;memcpy(v,p,n);p16(v+16,n-14);p16(v+38,n-34);p16(v+24,0);
  p16(v+24,~dhcp_identity5_sum(v+14,20,0));assert(see(v,n).mask<=DHCP_IDENTITY5_ALL);
 }
 assert(!see(NULL,sizeof p).mask && !see(p,1515).mask && !memcmp(p,original,sizeof p));
 assert(!munmap(base,page*2));puts("dhcp-identity5: PASS guarded truncations, private scalar mismatch masks, malformed options/checksums and immutable input;acceptance=0");
}
