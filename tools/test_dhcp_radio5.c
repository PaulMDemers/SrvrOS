#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <srvros/dhcp_radio5.h>
static void wire(uint8_t b[96],unsigned sequence){
 for(unsigned i=0;i<96;i++)b[i]=(uint8_t)(i*17+3);
 b[6]=0;b[7]=32;b[8]=1;b[9]=255;b[23]=128;b[24]=8;b[25]=65;
 b[46]=(uint8_t)(sequence<<4);b[47]=(uint8_t)(sequence>>4);
}
int main(void){
 struct packet_observation p={.role=4,.message=1};uint8_t b[96],before[96];
 for(unsigned slot=0;slot<2;slot++){
  struct dhcp_radio5 s={.enabled=1,.slot=slot,.epoch=slot+1};
  for(unsigned i=0;i<3;i++){
   wire(b,9+i);memcpy(before,b,sizeof b);p.message=i==2?3:1;
   unsigned n=dhcp_radio5_tag(&s,&p,9+i,b,sizeof b);assert(n==i+1 && b[23]==3+slot*3+i);
   for(unsigned j=0;j<sizeof b;j++)assert(j==23 || b[j]==before[j]);
   dhcp_radio5_bind(&s,n,100+i,20+i);dhcp_radio5_complete(&s,b[23],p.message,9+i,100+i,20+i,1000+i);
   assert(s.tx[i].completed && s.tx[i].finished==1000+i && !s.binding_error);
  }
  wire(b,12);memcpy(before,b,sizeof b);assert(!dhcp_radio5_tag(&s,&p,12,b,sizeof b));assert(s.overflow==1 && !memcmp(b,before,sizeof b));
  dhcp_radio5_complete(&s,s.tx[0].pid,1,9,100,20,2000);assert(s.binding_error==1 && s.tx[0].finished==1000);
 }
 struct dhcp_radio5 s={.enabled=1,.epoch=1};wire(b,9);p.message=1;
 unsigned n=dhcp_radio5_tag(&s,&p,9,b,sizeof b);assert(n==1);
 dhcp_radio5_complete(&s,3,1,9,100,20,1000);assert(!s.tx[0].completed && s.binding_error==1);
 dhcp_radio5_bind(&s,1,100,20);dhcp_radio5_complete(&s,3,1,10,100,20,1000);assert(!s.tx[0].completed && s.binding_error==2);
 dhcp_radio5_complete(&s,128,1,9,100,20,1000);assert(s.binding_error==2);
 for(unsigned changed=0;changed<5;changed++){
  s=(struct dhcp_radio5){.enabled=1,.epoch=1};wire(b,9);unsigned at[]={6,8,9,23,46};b[at[changed]]^=1;memcpy(before,b,sizeof b);
  assert(!dhcp_radio5_tag(&s,&p,9,b,sizeof b) && !memcmp(b,before,sizeof b) && s.binding_error==1 && !s.count);
 }
 long page=sysconf(_SC_PAGESIZE);assert(page>96);uint8_t *map=mmap(NULL,(size_t)page*2,PROT_READ|PROT_WRITE,MAP_ANONYMOUS|MAP_PRIVATE,-1,0);assert(map!=MAP_FAILED && !mprotect(map+page,(size_t)page,PROT_NONE));
 for(unsigned size=0;size<48;size++){
  s=(struct dhcp_radio5){.enabled=1,.epoch=1};uint8_t *end=map+page-size;if(size)memset(end,0,size);
  assert(!dhcp_radio5_tag(&s,&p,9,end,size) && s.binding_error==1);
 }
 assert(!munmap(map,(size_t)page*2));
 s=(struct dhcp_radio5){.enabled=1,.epoch=1};p.role=2;wire(b,9);memcpy(before,b,sizeof b);assert(!dhcp_radio5_tag(&s,&p,9,b,sizeof b) && !memcmp(b,before,sizeof b) && !s.count && !s.binding_error);
 puts("dhcp-radio5: PASS tag-only byte changes; disjoint contexts; exact native binding, duplicate/unbound/wrong-sequence refusal; bounded spans; no callbacks or packet/key retention");
}
