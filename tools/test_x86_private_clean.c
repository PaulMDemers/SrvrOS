#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/drivers/x86_private_clean.inc"
static struct private_clean_result result;
static struct private_clean_span spans[2];
static uint64_t ticks;static unsigned lines,fences,guards,clocks,fail_line,fail_fence,fail_guard,fail_clock,mutation,reentry,persistent_fence_failure,invalidations,fail_invalidation;
static uint8_t seen[2*4096*64];
static bool guard(void *ctx){assert(ctx==&result);++guards;return guards!=fail_guard;}
static uint64_t now(void *ctx){assert(ctx==&result);++clocks;if(clocks==fail_clock)return 0;if(clocks==mutation)result.span[0].physical^=4096;return ++ticks;}
static bool fence(void *ctx){assert(ctx==&result);++fences;return fences!=fail_fence && !(persistent_fence_failure && fences>=2);}
static bool invalidate(void *ctx,uint64_t va){
 assert(ctx==&result);unsigned b=lines/(spans[0].pages*64),off=b?lines-spans[0].pages*64:lines;
 assert(b<2 && !(off%64) && va==spans[b].virtual_base+(uint64_t)off*64);++invalidations;return invalidations!=fail_invalidation;
}
static bool line(void *ctx,uint64_t va){
 assert(ctx==&result);unsigned b=lines/(spans[0].pages*64),off=b?lines-spans[0].pages*64:lines;
 assert(b<2 && off<spans[b].pages*64 && va==spans[b].virtual_base+(uint64_t)off*64);assert(invalidations==lines/64+1);assert(!seen[lines]);seen[lines]=1;++lines;
 if(reentry){struct private_clean_io io={ctx,guard,line,fence,invalidate,now};assert(!private_clean_run(&result,spans,0x306d4,8u<<8,1u<<19,1,io));}
 return lines!=fail_line;
}
static void reset(unsigned pages){
 memset(&result,0,sizeof(result));memset(seen,0,sizeof(seen));ticks=100;lines=fences=guards=clocks=0;fail_line=fail_fence=fail_guard=fail_clock=mutation=reentry=persistent_fence_failure=invalidations=fail_invalidation=0;
 spans[0]=(struct private_clean_span){0xffff800001000000ull,0x1000000,pages};spans[1]=(struct private_clean_span){0xffff800005000000ull,0x5000000,pages};
}
static bool run(void){return private_clean_run(&result,spans,0x306d4,8u<<8,1u<<19,100,(struct private_clean_io){&result,guard,line,fence,invalidate,now});}
int main(void){
 reset(1356);assert(run() && result.complete && result.completed==2 && fences==4 && lines==2712*64 && result.lines[0]==1356*64 && result.lines[1]==1356*64);
 for(unsigned i=0;i<lines;i++)assert(seen[i]==1);
 assert(!run());
 reset(4096);assert(run() && lines==4096*128);
 reset(1);assert(run());unsigned nc=clocks,ng=guards;
 for(unsigned i=1;i<=128;i++){reset(1);fail_line=i;assert(!run() && !result.complete && result.reason==5 && lines==i);assert(!run());}
 for(unsigned i=1;i<=4;i++){reset(1);fail_fence=i;assert(!run() && result.reason==4 && fences==i+(lines!=0));}
 for(unsigned i=1;i<=ng;i++){reset(1);fail_guard=i;assert(!run() && !result.complete);}
 for(unsigned i=1;i<=nc;i++){reset(1);fail_clock=i;assert(!run() && !result.complete);reset(1);mutation=i;assert(!run() && !result.complete);}
 for(unsigned i=1;i<=2;i++){reset(1);fail_invalidation=i;assert(!run() && result.reason==7 && lines==(i-1)*64);}
 reset(1);persistent_fence_failure=1;assert(!run() && result.attempted && !result.drained);
 reset(1);reentry=1;assert(run());
 for(unsigned feature=0;feature<3;feature++){reset(1);assert(!private_clean_run(&result,spans,feature==0?0x306d5:0x306d4,feature==1?0:8u<<8,feature==2?0:1u<<19,100,(struct private_clean_io){&result,guard,line,fence,invalidate,now}));assert(!lines && !fences);}
 for(unsigned bad=0;bad<8;bad++){
  reset(1);
  if(bad==0)spans[0].pages=0;
  if(bad==1)spans[1].pages=4097;
  if(bad==2)spans[1].physical=spans[0].physical;
  if(bad==3)spans[1].virtual_base=spans[0].virtual_base;
  if(bad==4)spans[0].virtual_base=0x800000000000ull;
  if(bad==5)spans[0].physical=UINT64_MAX-4095;
  if(bad==6)spans[0].virtual_base++;
  if(bad==7)ticks=100101;
  assert(!run() && !lines && !fences);
  if(bad==7)assert(result.reason==3 && result.end>100100);
 }
 puts("private-clean: PASS full exact coverage, bounds, every small-span failure, no replay/reentry or false completion");
}
