/* Independent action-lifetime cases use the existing real GT/takeover/prepare
 * provider fixture. Run each mode in a fresh process: failure pins deliberately. */
#define INTEL_ENGINE_PREPARE_FIXTURE_ONLY
#include "test_intel_engine_prepare.c"
#undef INTEL_ENGINE_PREPARE_FIXTURE_ONLY
struct action_probe {
 struct prepare_model model;
 struct intel_engine_prepare_result *original;
 struct intel_gt_access *parent;
 const struct intel_takeover_result *preflight;
 struct intel_engine_prepare_io io;
 struct intel_engine_prepare_action action;
 struct intel_engine_prepare_result copied;
 unsigned calls;
 const char *mode;
};
static bool amode(const struct action_probe *a,const char *mode) {return !strcmp(a->mode,mode);}
static bool observe_ready(void *ctx,const struct intel_engine_prepare_result *view) {
 struct action_probe *a=ctx;struct prepare_model *m=&a->model;
 struct intel_engine_prepare_result *r=a->original;
 ++a->calls;
 assert(a->calls==1 && view==r && r->object==r && r->parent==a->parent);
 assert(r->preflight==a->preflight && r->epoch==a->parent->engine_epoch);
 assert(r->admitted && r->ready==31 && !r->cancelled && !r->returned);
 assert(r->borrow.object==&r->borrow && r->borrow.parent==a->parent && r->borrow.active);
 assert(a->parent->borrows==1 && a->parent->dependents==&r->borrow && !r->borrow.next);
 assert(r->snapshots==2 && r->counts[1]==6 && !r->counts[2] && m->samples==2);
 assert(m->stores==5 && r->writes==5 && r->attempts==5 && r->acks==5);
 assert(r->action_attempted && r->action_active && !r->action_completed && r->action_begin_us>=r->begin_us);
 unsigned writes=a->parent->write_attempts;
 assert(!intel_gt_access_close(a->parent) && a->parent->write_attempts==writes);
 /* Recursive calls, copies and later duplicate calls must not run an action. */
 if(amode(a,"reentry")) {
  struct intel_engine_prepare_result saved=*r;a->copied=*r;
  assert(!intel_engine_prepare_with_action(r,a->parent,a->preflight,true,a->io,a->action));
  assert(!memcmp(r,&saved,sizeof(saved)));
  assert(!intel_engine_prepare_with_action(&a->copied,a->parent,a->preflight,true,a->io,a->action));
  assert(!memcmp(&a->copied,&saved,sizeof(saved)) && a->calls==1 && m->stores==5);
 }
 if(amode(a,"post-guard")){m->mode="guard";m->point=m->guards+1;}
 if(amode(a,"post-time")){m->mode="guard-time";m->point=m->guards+1;}
 if(amode(a,"epoch"))a->parent->engine_epoch++;
 if(amode(a,"seal")){a->copied=*r;r->object=&a->copied;}
 if(amode(a,"parent"))r->parent=NULL;
 if(amode(a,"borrow-seal"))r->borrow.object=NULL;
 if(amode(a,"detach"))a->parent->dependents=NULL;
 if(amode(a,"deadline"))m->offset=INTEL_ENGINE_PREPARE_TOTAL_US+1;
 if(amode(a,"hold-drift"))m->control[2]=0;
 return !amode(a,"refuse");
}
int main(int argc,char **argv) {
 const char *mode=argc>1?argv[1]:"normal";
 struct action_probe a={.model={.base={.gt={.t=1000,.mode="normal"},.mode="normal"},.mode="normal"},.mode=mode};
 struct prepare_model *m=&a.model;
 struct display_access_token token;assert(display_access_acquire(27,&token));
 struct intel_gt_access g={0};assert(intel_gt_access_open(&g,&token,7,(struct intel_gt_io){&m->base.gt,guard,read_gt,write_gt,now,pause_gt,NULL}));m->base.parent=&g;
 struct intel_completion_result census={0};batch_parent=&g;
 assert(intel_completion_observe(&census,&g,(struct intel_completion_io){&m->base.gt,obs_guard,group,context,obs_now,batch_sample}));m->base.census=&census;
 struct intel_takeover_result pre={0};m->base.result=&pre;
 assert(intel_takeover_observe(&pre,&g,&census,(struct intel_takeover_io){&m->base,takeover_guard,takeover_sample,takeover_now}));
 struct intel_engine_prepare_result r={0};m->result=&r;
 a.original=&r;a.parent=&g;a.preflight=&pre;
 a.io=(struct intel_engine_prepare_io){m,prepare_guard,prepare_sample,prepare_control,prepare_transition,prepare_preserve,prepare_now,prepare_pause};
 a.action=(struct intel_engine_prepare_action){&a,observe_ready};
 bool no_action=amode(&a,"no-action");
 if(amode(&a,"missing"))a.action.observe=NULL;
 bool ok=no_action?intel_engine_prepare(&r,&g,&pre,true,a.io):intel_engine_prepare_with_action(&r,&g,&pre,true,a.io,a.action);
 if(amode(&a,"missing")) {
  assert(!ok && r.reason==IEP_INPUT && !a.calls && !m->stores && !g.borrows && !g.dependents);
  assert(intel_gt_access_close(&g));puts("engine-prepare-action: PASS missing action refuses before writes");return 0;
 }
 assert(!r.action_active);
 bool positive=amode(&a,"normal") || amode(&a,"reentry") || no_action;
 assert(ok==positive && a.calls==(no_action?0u:1u));
 if(positive) {
  assert(r.complete && !r.reason && !r.uncertain && r.ready==31 && r.cancelled==31);
  assert(r.held && r.preserved && r.returned && !r.borrow.active && !g.borrows && !g.dependents);
  assert(m->stores==10 && r.writes==10 && r.attempts==10 && r.acks==10 && r.reads==34 && r.snapshots==4);
  assert(r.action_attempted==!no_action && r.action_completed==!no_action);
  if(no_action)assert(!r.action_begin_us && !r.action_end_us);
  else assert(r.action_end_us>=r.action_begin_us && r.action_begin_us>=r.begin_us && r.action_end_us<=r.end_us);
  assert(intel_gt_access_close(&g));
 } else {
  assert(r.reason && !r.complete && r.uncertain && !r.returned && !r.cancelled);
  assert(m->stores==5 && r.writes==5 && r.attempts==5 && !m->preserves);
  if(amode(&a,"hold-drift")) {
   assert(!r.action_completed && r.reason&IEP_STATE && m->samples==3);
  } else {
   assert(!r.action_completed && r.reason&IEP_ACTION && m->samples==2);
  }
  if(amode(&a,"deadline"))assert(r.reason&IEP_CLOCK && r.clock_at-r.begin_us>INTEL_ENGINE_PREPARE_TOTAL_US);
  assert(!intel_gt_access_close(&g));
 }
 struct intel_engine_prepare_result saved=r,copy=r;unsigned calls=a.calls,stores=m->stores;
 assert(!intel_engine_prepare_with_action(&r,&g,&pre,true,a.io,a.action));
 assert(!intel_engine_prepare_with_action(&copy,&g,&pre,true,a.io,a.action));
 assert(!memcmp(&saved,&r,sizeof(r)) && !memcmp(&saved,&copy,sizeof(copy)) && a.calls==calls && m->stores==stores);
 puts("engine-prepare-action: PASS original held lifetime, one action, bounded failure and retirement");
 return 0;
}
