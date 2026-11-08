/* Separate image: prove front-end shutdown before expanding reset admission.
 * No reset, Stop Ring, address/translation/cache writer or backing grant. */
struct shutdown_trial {
 struct intel_gt_borrow borrow;
 struct intel_engine_census post;
 uint64_t begin,end;
 uint32_t epoch_before,epoch_after;
 bool blocked,stable,complete;
};
struct shutdown_io {struct wake_adapter *a;struct intel_gt_access *g;struct shutdown_trial *t;};
static bool shutdown_guard(void *ctx) {
 struct shutdown_io *s=ctx;
 return s->g->object==s->g && s->t->borrow.object==&s->t->borrow && s->t->borrow.active && s->t->borrow.parent==s->g &&
  s->g->state==GT_ACCESS_AWAKE && s->g->pinned && s->g->borrows==1 &&
  s->g->dependents==&s->t->borrow && s->g->engine_epoch==s->t->epoch_after && engine_guard(s->a);
}
static bool shutdown_read(void *ctx,uint32_t out[INTEL_BCS_WORDS]) {
 struct shutdown_io *s=ctx;return shutdown_guard(s) && blitter_group(s->a,out);
}
static enum intel_gt_write shutdown_write(void *ctx,uint32_t value) {
 struct shutdown_io *s=ctx;
 if(value!=INTEL_BCS_DISABLED || !shutdown_guard(s) || s->t->epoch_after!=s->t->epoch_before || s->g->engine_epoch==UINT32_MAX)return GT_WRITE_NONE;
 /* Invalidate pre-write observations before a possibly issued store, even NONE. */
 s->t->epoch_after=++s->g->engine_epoch;
 return blitter_store(s->a,value);
}
static bool shutdown_reader_binding(const struct reader_audit *r,unsigned n) {
 for(unsigned p=0;p<3;p++) {
  const uint32_t *old=result.sample[1].pipe[p],*v=r->words[n][p];
  if(v[0]!=old[2] || v[1]!=old[4] || v[2]!=old[6] || v[3]!=old[7] || v[4]!=old[8])return false;
 }
 const struct package_pci_snapshot *a=&r->audio[n];
 return a->state==1 && a->id==0x160c8086u && a->class_code==0x040300u;
}
static void shutdown_run(struct wake_adapter *a,struct intel_gt_access *g,
 const struct intel_engine_census *pre,struct intel_blitter_result *b,struct reader_audit *r,
 struct shutdown_trial *t,bool policy) {
 bool off=pre->enabled==0;
 if(!policy || !pre->complete || !pre->stable || pre->pending || (pre->enabled!=16 && !off) ||
    g->engine_epoch==UINT32_MAX || g->borrows || g->dependents || !intel_gt_access_borrow(g,&t->borrow))return;
 t->epoch_before=t->epoch_after=g->engine_epoch;t->begin=timer_monotonic_us();
 unsigned writes=g->writes;
 t->blocked=!intel_gt_access_close(g) && g->writes==writes && g->state==GT_ACCESS_AWAKE && g->pinned && t->borrow.active;
 if(!t->blocked){a->uncertain=true;return;}
 if(!reader_pass(a,g,r,0) || !shutdown_reader_binding(r,0))goto done;
 struct shutdown_io ctx={a,g,t};
 if(off)(void)intel_blitter_observe_disabled(b,g,policy,(struct intel_blitter_io){&ctx,shutdown_guard,shutdown_read,0,now});
 else (void)intel_blitter_disable(b,g,policy,(struct intel_blitter_io){&ctx,shutdown_guard,shutdown_read,shutdown_write,now});
 if(b->uncertain){a->uncertain=true;return;}
 if(!b->disabled)goto done;
 if(!intel_engine_observe(&t->post,g,(struct intel_engine_io){a,engine_guard,engine_group,now}) ||
    !reader_pass(a,g,r,1) || !shutdown_reader_binding(r,1))goto done;
 reader_finish(r);
 t->stable=t->post.complete && t->post.stable && !t->post.enabled && !t->post.pending && r->complete;
 /* All six streamer words survive, except the single captured enable bit. */
 for(unsigned n=0;n<2;n++)for(unsigned e=0;e<5;e++)for(unsigned i=0;i<6;i++) {
  uint32_t expected=pre->words[n][e][i];if(e==4 && i==3)expected&=~1u;
  if(t->post.words[n][e][i]!=expected)t->stable=false;
  if(e==4 && (pre->words[n][e][i]!=b->words[n][i] || t->post.words[n][e][i]!=b->words[n+2][i]))t->stable=false;
 }
 const struct package_pci_snapshot *x=&r->audio[0],*y=&r->audio[1];
 if(x->link_cap!=y->link_cap || x->link!=y->link || x->ltr_cap!=y->ltr_cap || x->ltr!=y->ltr)t->stable=false;
 t->complete=t->stable;
 done:
 t->end=timer_monotonic_us();
 if(!t->begin || t->end<=t->begin || t->end-t->begin>150000 || g->engine_epoch!=t->epoch_after)t->complete=false;
 /* A possibly issued disable is not retired until every postcondition holds. */
 if(b->uncertain || (b->writes && !t->complete)){a->uncertain=true;return;}
 if(!intel_gt_access_return(&t->borrow)){a->uncertain=true;t->complete=false;}
}
static void shutdown_report(const struct shutdown_trial *t,const struct intel_gt_access *g,const struct reader_audit *r,bool retired) {
 for(unsigned n=0;n<r->samples;n++) {
  const struct package_pci_snapshot *a=&r->audio[n];
  console_printf("intel-shutdown-audio: v=1 sample=%u link_cap=%u link=%u ltr_cap=%u ltr=%u\n",(uint64_t)n,(uint64_t)a->link_cap,(uint64_t)a->link,(uint64_t)a->ltr_cap,(uint64_t)a->ltr);
 }
 engine_report_named(&t->post,retired,"intel-shutdown-post");
 console_printf("intel-shutdown-summary: v=1 borrowed=%u returned=%u blocked=%u stable=%u begin=%u end=%u epoch_before=%u epoch_after=%u owner_begin=%u owner_end=%u complete=%u reset_writes=0 managed_pages=0\n",
  (uint64_t)(t->borrow.parent!=0),(uint64_t)(t->borrow.parent && !t->borrow.active),(uint64_t)t->blocked,(uint64_t)t->stable,t->begin,t->end,(uint64_t)t->epoch_before,(uint64_t)t->epoch_after,g->begin,g->last,(uint64_t)(t->complete && g->state==GT_ACCESS_CLOSED));
}
