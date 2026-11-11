/* Original-owner read-only child; only snapshot data crosses admission. */
#include <srvros/intel_entry_admit.h>
struct entry_adapter {
 struct intel_entry_result result;
 struct wake_adapter *a;
 struct intel_gt_access *g;
 const struct intel_completion_result *c;
};
static struct entry_adapter entry;
static bool entry_guard(void *ctx) {
 struct entry_adapter *e=ctx;
 return e==&entry && e->a==joint.wake && e->g==joint.scope.gpu &&
  e->result.object==&e->result && e->result.parent==e->g && e->result.census==e->c &&
  e->g->io.context==e->a && engine_guard(e->a);
}
static bool entry_read(void *ctx,uint32_t out[INTEL_BCS_WORDS]) {
 struct entry_adapter *e=ctx;
 if(!out || !e->a->mapped[7] || !entry_guard(e))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_entry_read(out);
#else
 for(unsigned i=0;i<6;i++)out[i]=*(volatile const uint32_t *)(WAKE_VA+7*4096+intel_engine_offsets[i]);
 out[6]=*(volatile const uint32_t *)(WAKE_VA+7*4096+0x9c);
 out[7]=*(volatile const uint32_t *)(WAKE_VA+7*4096+0x29c);return true;
#endif
}
static bool entry_run(struct wake_adapter *a,struct intel_gt_access *g,const struct intel_completion_result *c,bool policy) {
 if(entry.result.object)return false;
 entry.a=a;entry.g=g;entry.c=c;
 bool ok=intel_entry_admit(&entry.result,g,c,policy,(struct intel_entry_io){&entry,entry_guard,entry_read,now});
 if(entry.result.borrow.active)a->uncertain=true;
 return ok;
}
static void entry_report(void) {
 const struct intel_entry_result *r=&entry.result;const struct intel_blitter_result *b=&r->bcs;
 for(unsigned n=0;n<b->samples;n++) {
  const uint32_t *w=b->words[n];
  console_printf("intel-compose-entry-row: v=1 n=%u d=%u,%u,%u,%u,%u,%u,%u,%u\n",(uint64_t)n,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7]);
 }
 console_printf("intel-compose-entry-summary: v=1 policy=%u borrowed=%u returned=%u blocked=%u bound=%u complete=%u retained=%u reason=%u checks=%u begin=%u end=%u last=%u source_begin=%u source_end=%u source_last=%u epoch=%u epoch_after=%u parent_attempts_before=%u parent_attempts_after=%u clock_at=%u clock_prev=%u samples=%u reads=%u bcs_admitted=%u bcs_disabled=%u bcs_entry_disabled=%u bcs_attempts=%u bcs_writes=%u bcs_uncertain=%u bcs_reason=%u bcs_begin=%u bcs_end=%u reset_writes=0 managed_pages=0\n",
 (uint64_t)r->policy,(uint64_t)(r->borrow.parent!=0),(uint64_t)r->returned,(uint64_t)r->blocked,(uint64_t)r->bound,(uint64_t)r->complete,(uint64_t)r->borrow.active,(uint64_t)r->reason,(uint64_t)r->checks,r->begin_us,r->end_us,r->last_us,r->source_begin,r->source_end,r->source_last,(uint64_t)r->epoch,(uint64_t)r->epoch_after,(uint64_t)r->parent_attempts_before,(uint64_t)r->parent_attempts_after,r->clock_at,r->clock_prev,(uint64_t)b->samples,(uint64_t)b->reads,(uint64_t)b->admitted,(uint64_t)b->disabled,(uint64_t)b->entry_disabled,(uint64_t)b->attempts,(uint64_t)b->writes,(uint64_t)b->uncertain,(uint64_t)b->reason,b->begin_us,b->end_us);
}
