#include <srvros/haswell_cache_context.h>
#include <assert.h>
#include <stdio.h>
static unsigned calls,mode;
static bool read_entry(void *ctx,uint64_t table,unsigned index,uint64_t *v){(void)ctx;(void)index;calls++;if(mode==1)return false;if(mode==5){*v=table==0x1000?0x2003:0xc0001083;return true;}if(mode==6){*v=table==0x1000?0x2003:table==0x2000?0x3003:0xfec01083;return true;}*v=mode==2?0:mode==3?0x83:mode==4?0x200083:table==0x1000?0x2003:table==0x2000?0x3003:table==0x3000?0x4003:0xfed90013;return true;}
int main(void){struct hcc_walk w,old;assert(hcc_walk(&w,0x1000,0xffff8000fed90000ull,39,read_entry,0));assert(w.state==2&&w.pa==0xfed90000&&w.count==4&&w.shift==12);old=w;assert(hcc_walk_equal(&old,&w));w.entry[3]^=32;assert(!hcc_walk_equal(&old,&w));
 for(mode=1;mode<=4;mode++){calls=0;bool ok=hcc_walk(&w,0x1000,0xffff8000fed90000ull,39,read_entry,0);assert(!ok || (mode==2&&w.state==1));assert(calls<=4);}
 for(mode=5;mode<=6;mode++){assert(hcc_walk(&w,0x1000,0xffff8000fed90000ull,39,read_entry,0));assert(w.state==2 && w.pa==0xfed90000 && w.shift==(mode==5?30:21));}
 calls=0;assert(!hcc_walk(&w,0x1001,0,39,read_entry,0)&&!calls);assert(!hcc_walk(&w,0x1000,0x1000000000000ull,39,read_entry,0)&&!calls);
 puts("haswell-cache-walk PASS raw preservation, finite walks, absent, failure, noncanonical/alignment, huge reserved bits");return 0;}
