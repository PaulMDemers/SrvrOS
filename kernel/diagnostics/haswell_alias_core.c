#include <srvros/haswell_alias_context.h>
#define ADDR (((1ull<<39)-1)&~4095ull)
static bool room(struct hac_result *r,const struct hac_io *io) {
 uint64_t t=io->now(io->ctx);
 if(!t || t<r->begin || t-r->begin>=100000){r->status=2;return false;}return true;
}
static bool add(struct hac_result *r,uint64_t pa,unsigned level) {
 if(!pa || (pa&4095) || pa>ADDR){r->status=4;return false;}
 for(unsigned i=0;i<r->pages;i++)if(r->table[i].pa==pa){if(r->table[i].level!=level){r->status=4;return false;}return true;}
 if(r->pages==HAC_TABLE_CAP){r->status=2;return false;}
 r->table[r->pages].pa=pa;r->table[r->pages++].level=level;return true;
}
static bool emit(struct hac_result *r,unsigned pass,uint64_t v,unsigned n) {
 if(n>HAC_DATA_CAP-r->bytes[pass]){r->status=2;return false;}
 for(unsigned i=0;i<n;i++)r->data[pass][r->bytes[pass]++]=(uint8_t)(v>>(8*i));
 return true;
}
static bool encode(struct hac_result *r,unsigned pass,const struct hac_page *p) {
 if(!emit(r,pass,p->pa,8) || !emit(r,pass,p->level,1))return false;
 for(unsigned i=0;i<512;) {
  unsigned count=1;uint64_t first=p->word[i],step=0;
  if(i+1<512){step=p->word[i+1]-first;count=2;while(i+count<512 && p->word[i+count]==first+step*count)count++;}
  if(!emit(r,pass,count,2) || !emit(r,pass,first,8) || !emit(r,pass,step,8))return false;
  i+=count;
 }return true;
}
bool hac_capture(struct hac_result *r,const struct hac_io *io,uint64_t root) {
 if(!r)return false;
 /* Large table backing is static in callers; reset metadata, not via a stack copy. */
 r->status=3;r->pages=r->target_leaves=0;r->reads[0]=r->reads[1]=r->bytes[0]=r->bytes[1]=0;r->begin=r->end=r->cursor_pa=r->cursor_expected=r->cursor_observed=0;r->cursor_index=0;
 if(!io || !io->read || !io->now)return false;
 r->begin=io->now(io->ctx);if(!room(r,io) || !add(r,root,0))return false;
 for(unsigned pass=0;pass<2;pass++)for(unsigned i=0;i<r->pages;i++) {
  struct hac_page *p=&r->table[i];
  for(unsigned j=0;j<512;j++) {
   r->cursor_pa=p->pa;r->cursor_index=j;r->cursor_expected=pass?p->word[j]:0;r->cursor_observed=0;
   uint64_t v;if(!room(r,io))return false;
   if(!io->read(io->ctx,p->pa,j,&v)){r->status=3;return false;}r->reads[pass]++;r->cursor_observed=v;
   if(pass){if(v!=p->word[j]){r->status=5;return false;}continue;}
   p->word[j]=v;if(!(v&1))continue;
   if(v&0x000ffffffffff000ull&~ADDR || (!p->level && v&128)){r->status=4;return false;}
   if(p->level==3 || (v&128 && (p->level==1 || p->level==2))) {
    unsigned shift=p->level==1?30:p->level==2?21:12;uint64_t low=(1ull<<shift)-1;
    if(p->level!=3 && (v&ADDR&low&~4096ull)){r->status=4;return false;}
    uint64_t base=v&ADDR&~low;if(base<=0xfed90000ull && 0xfed90000ull-base<=low)r->target_leaves++;
   }else if(!add(r,v&ADDR,p->level+1))return false;
  }
  if(!encode(r,pass,p))return false;
 }
 r->end=io->now(io->ctx);
 if(!r->end || r->end<=r->begin || r->end-r->begin>=100000){r->status=2;return false;}
 r->status=1;return true;
}
