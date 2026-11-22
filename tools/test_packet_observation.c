#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <srvros/packet_observation.h>
static void p16(unsigned char *p,unsigned v){p[0]=v>>8;p[1]=v;}
int main(void){
 unsigned char p[342]={0};p16(p+12,0x800);p[14]=0x45;p16(p+16,sizeof p-14);p[23]=17;
 p16(p+34,68);p16(p+36,67);p16(p+38,sizeof p-34);p[42]=1;p[43]=1;p[44]=6;
 memcpy(p+278,"\x63\x82\x53\x63",4);p[282]=53;p[283]=1;p[284]=3;p[285]=255;
 struct packet_observation r=packet_observe(p,sizeof p);assert(r.role==4 && r.message==3 && r.bytes==sizeof p);
 unsigned char before[sizeof p];memcpy(before,p,sizeof p);
 long page=sysconf(_SC_PAGESIZE);unsigned char *base=mmap(NULL,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(base!=MAP_FAILED && !mprotect(base+page,page,PROT_NONE));
 for(unsigned n=0;n<=sizeof p;n++){unsigned char *at=base+page-n;memcpy(at,p,n);r=packet_observe(at,n);if(n<sizeof p)assert(!r.message);}
 assert(!memcmp(before,p,sizeof p));assert(!packet_observe(NULL,sizeof p).role);assert(!packet_observe(p,1515).role);
 p16(p+34,67);p16(p+36,68);p[42]=2;p[284]=5;r=packet_observe(p,sizeof p);assert(r.role==5 && r.message==5);
 p[285]=53;p[286]=1;p[287]=5;p[288]=255;assert(!packet_observe(p,sizeof p).message);
 p[285]=255;p[283]=255;assert(!packet_observe(p,sizeof p).message);
 p[20]=0x20;assert(packet_observe(p,sizeof p).role==2);
 assert(!munmap(base,page*2));puts("packet-observation: PASS guarded truncation, malformed TLV, reflected/client/server roles and input immutability;acceptance=0");
}
