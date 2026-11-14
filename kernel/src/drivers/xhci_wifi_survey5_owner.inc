/* Explicit connection discovery continuation. Legacy eight-page profiles stay
 * unchanged. Every previous DMA allocation survives; only a proved stopped
 * endpoint can move to a fresh ring, preserving its USB toggle state. */
#define WIFI_SURVEY5_WINDOWS 16u
static struct {
 bool enabled,started,switching,complete,selected,failed;
 unsigned count,window,reason,fault;
 uint64_t start,deadline,previous;
 struct {
  struct wifi_cal5_state owner,guard;
  uint8_t ring[4096],pages[8][4096];
 } prior[WIFI_SURVEY5_WINDOWS-1];
} wifi_survey5;
static bool wifi_survey5_fail(unsigned reason){
 if(!wifi_survey5.fault)wifi_survey5.fault=reason;
 wifi_survey5.failed=wifi_cal5.failed=true;
 if(wifi_cal5.d)wifi_mcu_fail(wifi_cal5.d);
 return false;
}
static bool wifi_survey5_alias(uint64_t p){
 if(!p || !wifi_survey5.enabled)return false;
 for(unsigned n=0;n<wifi_survey5.count && n<WIFI_SURVEY5_WINDOWS-1;n++){
  const struct wifi_cal5_state *s=&wifi_survey5.prior[n].owner;
  if(p==s->plan.ring)return true;
  for(unsigned i=0;i<8;i++)if(p==s->plan.physical[i] || p==s->cleanup_physical[i])return true;
 }
 return false;
}
static bool wifi_survey5_held(void){
 if(!wifi_survey5.enabled)return true;
 if(wifi_survey5.failed)return false;
 if(wifi_survey5.count>=WIFI_SURVEY5_WINDOWS ||
    wifi_survey5.window!=wifi_survey5.count)return wifi_survey5_fail(10);
 if(wifi_survey5.started && (wifi_survey5.start>UINT64_MAX-10000000 ||
    wifi_survey5.deadline!=wifi_survey5.start+10000000 || wifi_survey5.previous<wifi_survey5.start))
  return wifi_survey5_fail(10);
 if(wifi_survey5.started && wifi_cal5.window.polled &&
    (wifi_cal5.window.deadline_us!=wifi_survey5.deadline || wifi_cal5.window.start_us<wifi_survey5.start ||
     wifi_cal5.window.start_us>=wifi_survey5.deadline ||
     wifi_cal5.window.elapsed_us>wifi_survey5.deadline-wifi_cal5.window.start_us+100000))return wifi_survey5_fail(10);
 for(unsigned n=0;n<wifi_survey5.count;n++){
  const struct wifi_cal5_state *s=&wifi_survey5.prior[n].owner;
  if(!wifi_prime5_equal(s,&wifi_survey5.prior[n].guard,sizeof *s) ||
     !s->configured || !s->stopped || s->pending || s->failed || !s->cleanup_frozen ||
     !s->window.complete || !s->window.stopped || s->window.failed ||
     !s->discovery.complete || s->discovery.failed || s->choice.selected ||
     s->discovery_status!=MT76_DISC5_UNAVAILABLE ||
     s->ep.ring_phys!=s->plan.ring || s->pin_ring!=s->plan.ring ||
     s->ep.ring!=pmm_phys_to_virt(s->plan.ring) || (uintptr_t)s->ep.ring!=s->pin_rptr ||
     !wifi_prime5_equal(s->ep.ring,wifi_survey5.prior[n].ring,4096))return wifi_survey5_fail(10);
  for(unsigned i=0;i<8;i++){
   if(s->plan.physical[i]!=s->pin_pages[i] ||
      s->plan.buffer[i]!=pmm_phys_to_virt(s->plan.physical[i]) ||
      (uintptr_t)s->plan.buffer[i]!=s->pin_v[i] ||
      !wifi_prime5_equal(s->plan.buffer[i],wifi_survey5.prior[n].pages[i],4096) ||
      s->cleanup_buffer[i]!=pmm_phys_to_virt(s->cleanup_physical[i]) ||
      (uintptr_t)s->cleanup_buffer[i]!=s->cleanup_pin[i] ||
      !wifi_prime5_equal(s->cleanup_buffer[i],s->cleanup_snapshot[i],4096))return wifi_survey5_fail(10);
  }
 }
 return true;
}
static bool wifi_survey5_event(const struct xhci_trb *e){
 if(!wifi_survey5.enabled)return false;
 uint64_t p=e->parameter_low|((uint64_t)e->parameter_high<<32);
 if(wifi_survey5.switching && ((e->control>>16)&31)==9 &&
    (e->control>>24)==wifi_cal5.d->slot_id){wifi_survey5_fail(1);return true;}
 for(unsigned n=0;n<wifi_survey5.count && n<WIFI_SURVEY5_WINDOWS-1;n++){
  uint64_t ring=wifi_survey5.prior[n].guard.plan.ring;
  if(p>=ring && p<ring+4096){wifi_survey5_fail(1);return true;}
 }
 return false;
}
bool xhci_wifi_cal5_survey_enable(void){
 if(wifi_survey5.enabled || !wifi_prime5.enabled || !wifi_cal5.cleanup_enabled ||
    wifi_cal5.attempted || wifi_cal5.active || wifi_cal5.configured)return false;
 wifi_survey5.enabled=true;return true;
}
static const char *wifi_cal5_tag(void){return wifi_survey5.window?"wifi-survey5-cal":"wifi-cal5";}
static bool wifi_survey5_partial(void){return wifi_survey5.enabled && wifi_survey5.started;}
