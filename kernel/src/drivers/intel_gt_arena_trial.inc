/* CPU storage belongs to the original full-GT new epoch, not the legacy BCS
 * source. Allocation/zeroing and final cleanup stay outside GPU exclusion. */
#include "intel_command_memory_trial.inc"
static bool command_lease_identity(const struct intel_command_lease *l) {
 const struct intel_gt_access *g=gt_bootstrap_native.g;
 return l==&command_native.owner.lease && command_native.parent==&gt_bootstrap_native &&
  gt_bootstrap_native.object==&gt_bootstrap_native && g && g==joint.scope.gpu &&
  l->owner==g && l->provider==&gt_bootstrap_native && l->lifetime==&gt_bootstrap_native.borrow &&
  l->root==gt_bootstrap_native.root && l->root==command_native.owner.root &&
  l->epoch==g->engine_epoch && l->epoch==gt_bootstrap_native.epoch+1 && l->sequence==1 &&
  l->prepare==gt_bootstrap_native.begin && l->gt==gt_bootstrap_native.gt_origin &&
  l->hda==gt_bootstrap_native.hda_origin && l->outer==gt_bootstrap_native.outer_origin &&
  vmm_current_address_space()==l->root;
}
static bool command_outer_retired(void) {
 const struct intel_gt_access *g=gt_bootstrap_native.g;
 const struct wake_adapter *a=gt_bootstrap_native.a;
 return command_native.outer_safe && command_lease_identity(&command_native.owner.lease) &&
  !command_native.clock_lost && !command_native.clock_at && !command_native.clock_previous &&
  command_native.outer_end && command_native.outer_end>=joint.scope.end_us &&
  command_native.outer_end>=joint.scope.hda.end_us && command_native.outer_end>=g->last &&
  gt_bootstrap_native.complete && gt_bootstrap_native.state==2 && !gt_bootstrap_native.failed &&
  gt_bootstrap_native.returned && !gt_bootstrap_native.borrow.active &&
  gt_bootstrap_native.closing==3 && gt_bootstrap_native.arena_original==&command_native.owner &&
  !gt_bootstrap_native.arena_child && gt_bootstrap_native.arena_borrows==1 && gt_bootstrap_native.arena_returns==1 &&
  !g->pinned && g->state==GT_ACCESS_CLOSED && !g->dependents && !g->borrows && !g->reason &&
  a && a==joint.wake && !a->uncertain && !a->fw.uncertain && !display_access_held(&a->fw.gt.token) &&
  a->maps==WAKE_MAPS && a->retired==WAKE_MAPS && !trial_mapping_fault &&
  joint.scope.state==IHG_CLOSED && !joint.scope.reason && !joint_terminal() &&
  joint.scope.hda.complete && bootstrap_memory_returned();
}
/* No resource/cache/alias callback follows an identity loss. During cleanup
 * use the independent retired parent, never a completed descriptor as a lease. */
static bool command_context_live(void) {
 const struct intel_command_memory *o=&command_native.owner;
 if(o->state==COMMAND_PREPARING)return !gt_bootstrap_native.object && !command_native.parent;
 if(o->state==COMMAND_RETURNED && command_native.outer_safe)return command_outer_retired();
 return o->state==COMMAND_HELD && command_lease_identity(&o->lease) &&
  scanout_memory_hold_live(&o->hold) && o->hold.lifetime==o && o->hold.dependent==o && o->hold.registered==o &&
  bootstrap_parent() && gt_bootstrap_native.command_writes==1 &&
  gt_bootstrap_native.arena_original==o && gt_bootstrap_native.arena_child==o &&
  gt_bootstrap_native.arena_borrows==1 && !gt_bootstrap_native.arena_returns &&
  bootstrap_memory_returned() && wake_irq_off();
}
static bool command_parent_guard(void *ctx,const struct intel_command_lease *l) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_command_guard();
#endif
 return ctx==&command_native && command_lease_identity(l) && command_context_live() &&
  x86_cache_equal(&command_native.cache,&gt_bootstrap_native.cache) &&
  bootstrap_guard() && command_context_live()
#ifdef SRVROS_INTEL_BCS_CONTEXT
  && bcs_context_held()
#endif
  ;
}
static bool command_parent_retired(void *ctx,const struct intel_command_lease *l) {
 if(ctx!=&command_native)return false;
 ++command_native.retire_checks;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 if(!intel_display_test_command_retire())return false;
#endif
 return command_lease_identity(l) && command_outer_retired();
}
static bool command_run(void) {
 struct intel_command_memory *o=&command_native.owner;
 if(o->state!=COMMAND_PREPARED || command_native.parent || gt_bootstrap_native.arena_original ||
  !bootstrap_parent() || !bootstrap_memory_returned() || !bootstrap_tick() ||
  !x86_cache_equal(&command_native.cache,&gt_bootstrap_native.cache))return bootstrap_fail(1024);
 command_native.parent=&gt_bootstrap_native;
 gt_bootstrap_native.arena_original=gt_bootstrap_native.arena_child=o;++gt_bootstrap_native.arena_borrows;
 struct intel_command_lease l={gt_bootstrap_native.g,&gt_bootstrap_native,&gt_bootstrap_native.borrow,
  gt_bootstrap_native.root,gt_bootstrap_native.g->engine_epoch,1,gt_bootstrap_native.begin,
  gt_bootstrap_native.gt_origin,gt_bootstrap_native.hda_origin,gt_bootstrap_native.outer_origin};
 if(!intel_command_memory_run(o,&l) || !bootstrap_tick() || o->end>gt_bootstrap_native.last ||
  !command_lease_identity(&o->lease))return bootstrap_fail(1024);
 gt_bootstrap_native.arena_child=NULL;++gt_bootstrap_native.arena_returns;
 return command_returned()?true:bootstrap_fail(1024);
}
static bool command_returned(void) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_command_return();
#endif
 const struct intel_command_memory *o=&command_native.owner;
 return command_lease_identity(&o->lease) && intel_command_memory_returned(o) &&
  !command_native.clock_lost && !command_native.clock_at && !command_native.clock_previous &&
  !command_native.outer_end &&
  gt_bootstrap_native.arena_original==o && !gt_bootstrap_native.arena_child &&
  gt_bootstrap_native.arena_borrows==1 && gt_bootstrap_native.arena_returns==1 &&
  gt_memory.d.end<=o->begin && o->end<=gt_bootstrap_native.last &&
  command_native.cache_checks==3 && command_native.alias_checks==3 && command_native.zeros==1 &&
  command_native.allocated==4 && !command_native.released && !command_native.word_checks &&
  x86_cache_equal(&command_native.cache,&gt_bootstrap_native.cache);
}
static void arena_report(void) {
 command_report();
#ifdef SRVROS_INTEL_BCS_CONTEXT
 bcs_context_report();
#endif
 console_printf("intel-arena-binding: v=1 owner=%u provider=%u lifetime=%u original=%u child=%u borrows=%u returns=%u parent_retired=%u clock=%u at=%u previous=%u outer_end=%u\n",
  (uint64_t)(command_native.owner.lease.owner && command_native.owner.lease.owner==gt_bootstrap_native.g),
  (uint64_t)(command_native.owner.lease.provider==&gt_bootstrap_native),
  (uint64_t)(command_native.owner.lease.lifetime==&gt_bootstrap_native.borrow),
  (uint64_t)(gt_bootstrap_native.arena_original==&command_native.owner),
  (uint64_t)(gt_bootstrap_native.arena_child!=NULL),(uint64_t)gt_bootstrap_native.arena_borrows,
  (uint64_t)gt_bootstrap_native.arena_returns,(uint64_t)command_outer_retired(),
  (uint64_t)command_native.clock_lost,command_native.clock_at,command_native.clock_previous,command_native.outer_end);
}
