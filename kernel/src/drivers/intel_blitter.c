#include <srvros/intel_blitter.h>
static bool clock_ok(struct intel_blitter_result *r,struct intel_blitter_io io) {
 uint64_t t=io.now(io.context);
 if(!t || t<r->last_us || t<r->begin_us || t-r->begin_us>100000){r->reason=2;return false;}
 r->last_us=t;return true;
}
static bool sample(struct intel_blitter_result *r,struct intel_blitter_io io,bool disabled) {
 if(!clock_ok(r,io) || !io.guard(io.context)) {if(!r->reason)r->reason=6;return false;}
 uint32_t *w=r->words[r->samples];
 if(!io.read(io.context,w)){r->reason=3;return false;}
 r->reads+=INTEL_BCS_WORDS;
 if(!clock_ok(r,io))return false;
 for(unsigned i=0;i<INTEL_BCS_WORDS;i++)if(w[i]==UINT32_MAX){r->reason=3;return false;}
 r->samples++;
 /* Exact captured legacy ring, no pending/active execlist and parser idle.
  * Upper MI_MODE mask bits are read-only observations, never written. */
 if(w[0] || w[1] || w[2]!=0x3eef000 || w[3]!=(disabled?0xf000:0xf001) ||
    w[4]!=1 || w[5] || (w[6]&0xffffu)!=0x200 || (w[7]&0xfffeu)) {r->reason=4;return false;}
 if((r->samples==2 || r->samples==4))for(unsigned i=0;i<INTEL_BCS_WORDS;i++)
  if(w[i]!=r->words[r->samples-2][i]){r->reason=5;return false;}
 return true;
}
bool intel_blitter_disable(struct intel_blitter_result *r,struct intel_gt_access *g,
 bool handoff,struct intel_blitter_io io) {
 if(!r || r->object)return false;
 *r=(struct intel_blitter_result){.object=r};
 if(!g || !handoff || !io.guard || !io.read || !io.write || !io.now || !intel_gt_access_check(g)){r->reason=1;return false;}
 r->begin_us=r->last_us=io.now(io.context);
 if(!r->begin_us){r->reason=2;return false;}
 if(!sample(r,io,false) || !sample(r,io,false))return false;
 if(!intel_gt_access_check(g) || !io.guard(io.context)){r->reason=6;return false;}
 if(!clock_ok(r,io) || r->last_us<=r->begin_us){r->reason=2;return false;}
 r->admitted=true;r->attempts=1;
 enum intel_gt_write w=io.write(io.context,INTEL_BCS_DISABLED);
 if(w==GT_WRITE_NONE){r->reason=7;return false;}
 r->uncertain=true;
 if(w!=GT_WRITE_DONE){r->reason=8;return false;}
 r->writes=1;
 if(!sample(r,io,true) || !sample(r,io,true))return false;
 for(unsigned i=0;i<INTEL_BCS_WORDS;i++)if(i!=3 && r->words[0][i]!=r->words[2][i]){r->reason=5;return false;}
 if(!intel_gt_access_check(g)){r->reason=6;return false;}
 r->end_us=io.now(io.context);
 if(r->end_us<r->last_us || r->end_us<=r->begin_us || r->end_us-r->begin_us>100000){r->reason=2;return false;}
 r->disabled=true;r->uncertain=false;return true;
}
