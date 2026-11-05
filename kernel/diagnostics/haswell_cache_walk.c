#include <srvros/haswell_cache_context.h>
bool hcc_walk(struct hcc_walk *w,uint64_t root,uint64_t va,unsigned bits,
             bool (*read)(void *,uint64_t,unsigned,uint64_t *),void *ctx) {
 if(!w)return false;
 *w=(struct hcc_walk){.va=va};
 if(!read || bits<36 || bits>52 || !root || (root&4095) || root>=(1ull<<bits) ||
    (va>>47!=0 && va>>47!=0x1ffff))return false;
 uint64_t addr=((1ull<<bits)-1)&~4095ull,table=root;
 const unsigned shifts[]={39,30,21,12};
 for(unsigned i=0;i<4;i++) {
  uint64_t e;if(!read(ctx,table,(unsigned)((va>>shifts[i])&511),&e))return false;
  w->table[i]=table;w->entry[i]=e;w->count++;
  if(!(e&1)){w->state=1;return true;}
  if(e&0x000ffffffffff000ull&~addr)return false;
  if((e&128) && i==0)return false;
  if(i==3 || ((e&128) && (i==1 || i==2))) {
   unsigned shift=shifts[i];uint64_t low=(1ull<<shift)-1;
   /* Huge-leaf bit12 is PAT, not a physical-address bit. */
   if(i!=3 && (e&addr&low&~4096ull))return false;
   w->pa=(e&addr&~low)|(va&low);w->shift=shift;w->state=2;return true;
  }
  table=e&addr;if(!table)return false;
 }
 return false;
}
bool hcc_walk_equal(const struct hcc_walk *a,const struct hcc_walk *b) {
 if(!a || !b || !a->state || a->va!=b->va || a->pa!=b->pa || a->count!=b->count || a->shift!=b->shift || a->state!=b->state)return false;
 for(unsigned i=0;i<a->count;i++)if(a->table[i]!=b->table[i] || a->entry[i]!=b->entry[i])return false;
 return true;
}
