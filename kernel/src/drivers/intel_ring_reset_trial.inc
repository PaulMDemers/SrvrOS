#include <srvros/intel_ring_reset.h>
struct ring_reset_adapter {struct intel_ring_reset result;struct wake_adapter *a;
 struct intel_gt_access *g;struct intel_takeover_result *takeover;};
static struct ring_reset_adapter ring_reset_native;
static bool ring_reset_guard(void *ctx) {
 struct ring_reset_adapter *p=ctx;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_ring_reset_guard(p);
#endif
 return p==&ring_reset_native && p->a==joint.wake && p->g==joint.scope.gpu &&
  p->result.entry==&ring_native.result && p->result.source==&ring_native.result.post &&
  p->result.parent==p->g && p->result.takeover==p->takeover && p->g->io.context==p->a && engine_guard(p->a);
}
static bool ring_reset_sample(void *ctx,uint32_t out[INTEL_TAKEOVER_WORDS],unsigned *count) {
 struct ring_reset_adapter *p=ctx;
 return ring_reset_guard(p) && takeover_sample(p->a,out,count);
}
static uint64_t ring_reset_now(void *ctx) {
 struct ring_reset_adapter *p=ctx;
 if(p!=&ring_reset_native || p->a!=joint.wake)return 0;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 uint64_t at;if(intel_display_test_ring_reset_clock(&at))return at;
#endif
 return now(p->a);
}
static bool ring_reset_run(struct wake_adapter *a,struct intel_gt_access *g,struct intel_takeover_result *t,bool policy) {
 if(ring_reset_native.result.object)return false;
 ring_reset_native.a=a;ring_reset_native.g=g;ring_reset_native.takeover=t;
 bool ok=intel_ring_reset_admit(&ring_reset_native.result,g,&ring_native.result,t,policy,
  (struct intel_takeover_io){&ring_reset_native,ring_reset_guard,ring_reset_sample,ring_reset_now});
 if(ring_reset_native.result.uncertain || t->borrow.active)a->uncertain=true;
 return ok;
}
static void ring_reset_report(void) {
 const struct intel_ring_reset *r=&ring_reset_native.result;
 console_printf("intel-reset-admit: v=1 eligible=%u bound=%u complete=%u uncertain=%u reason=%u checks=%u begin=%u end=%u last=%u source_begin=%u source_end=%u source_last=%u entry_end=%u epoch=%u clock_at=%u clock_prev=%u take_begin=%u take_end=%u take_last=%u\n",
 (uint64_t)r->eligible,(uint64_t)r->bound,(uint64_t)r->complete,(uint64_t)r->uncertain,(uint64_t)r->reason,(uint64_t)r->checks,
 r->begin,r->end,r->last,r->source_begin,r->source_end,r->source_last,r->entry_end,(uint64_t)r->epoch,r->clock_at,r->clock_prev,r->take_begin,r->take_end,r->take_last);
}
