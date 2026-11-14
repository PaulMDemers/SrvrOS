#include <srvros/mt76_power5_cycle.h>
/* Explicit one-shot power-only child. No MAC enable,endpoint creation or OUT
 * network frame. The selected RF/calibration parent remains held throughout. */
static bool wifi_power5_requested;
static bool wifi_handoff5_requested;
static bool wifi_handoff5_inspect(void);
static struct {
 struct xhci_device *d;
 struct mt76_power5_capture fields;
 struct mt76_power5_gate map;
 struct mt76_power5_cycle cycle;
 struct mt76_power5_plan plan;
 uint32_t original[9];
 unsigned step,snapshot_mask,modified_mask,restored_mask,cleanup_phase,cleanup_steps;
 int cleanup_index;
 bool attempted,active,fields_active,inspected,failed,forward_failed,cleanup_started,cleanup_done;
} wifi_power5;
static bool wifi_power5_parent(void){
 struct xhci_device *d=wifi_power5.d;
 return wifi_power5_requested && wifi_discovery5_requested && d && d==wifi_cal5.d && !xhci.sync_users &&
 wifi_discovery5_owned(&wifi_cal5,&wifi_cal5.window) && wifi_cal5.choice.attempted && wifi_cal5.choice.selected &&
 wifi_cal5.choice.matches==1 && wifi_cal5.choice.generation==1 && wifi_cal5.choice.row.channel==161 &&
 !d->firmware.pending && !d->firmware.mcu.pending;
}
static bool wifi_power5_fail(void){wifi_power5.failed=true;wifi_power5.forward_failed=false;return wifi_power5.d?wifi_mcu_fail(wifi_power5.d):false;}
static bool wifi_power5_fields_owned(void *c){
 const struct mt76_power5_capture *f=&wifi_power5.fields;const struct mt76_power5_gate *g=&wifi_power5.map;
 return c==&wifi_power5 && wifi_power5.attempted && wifi_power5.active && wifi_power5.fields_active &&
 !wifi_power5.failed && wifi_power5_parent() && f->attempted && !f->complete && !wifi_power5.cycle.attempted &&
 !g->failed && !g->pending && g->submitted==f->reads+f->writes;
}
static bool wifi_power5_fields_transfer(void *c,bool write,uint32_t a,uint32_t *v){
 struct mt76_power5_gate *g=&wifi_power5.map;unsigned block=g->block,phase=g->phase;
 if(!v || !wifi_power5_fields_owned(c) || !mt76_power5_begin(g,write,a,*v))return wifi_power5_fail();
 bool ok=wifi_mac_private_usb(wifi_power5.d,write,a,v) && mt76_power5_finish(g,true,*v);
 if(!ok)return wifi_power5_fail();
 /* Compact,source-gated block receipts keep the worst-case1024-poll failure
  * within the existing full-prefix capture budget. Never log raw block data. */
 if(phase==14)console_printf("wifi-power5-map-block: index=%u submitted=%u polls=%u guards=1 mode=0 raw=0\n",(uint64_t)block,(uint64_t)g->submitted,(uint64_t)g->polls);
 return true;
}
static bool wifi_power5_fields_read(void *c,uint32_t a,uint32_t *v){if(!v)return false;*v=0;return wifi_power5_fields_transfer(c,false,a,v);}
static bool wifi_power5_fields_write(void *c,uint32_t a,uint32_t v){return wifi_power5_fields_transfer(c,true,a,&v);}
static bool wifi_power5_fields_wait(void *c,uint32_t us){
 if(!wifi_power5_fields_owned(c) || !mt76_power5_delay(&wifi_power5.map,us) || !timer_delay_us(us))return wifi_power5_fail();
 return true;
}
static bool wifi_power5_owned(void *c,bool cleanup){
 const struct mt76_power5_capture *f=&wifi_power5.fields;const struct mt76_power5_gate *g=&wifi_power5.map;
 if(c!=&wifi_power5 || !wifi_power5.active || wifi_power5.fields_active || (wifi_power5.failed && !(cleanup && wifi_power5.forward_failed)) || !wifi_power5_parent() ||
    !mt76_power5_capture_valid(f) || g->failed || g->pending || g->phase!=15 || g->block!=5 || g->polls!=f->polls ||
    g->submitted!=f->reads+f->writes || f->fields.generation!=wifi_cal5.choice.generation ||
    f->fields.nic0!=wifi_power5.d->firmware.mcu.ant_fields.nic0 || f->fields.nic1!=wifi_power5.d->firmware.mcu.ant_fields.nic1)return false;
 const uint8_t us[2]={'U','S'};struct mt76_power5_plan p={0};
 if(mt76_power5_plan(&f->fields,us,161,20,30,&p)!=MT76_POWER5_OK || !wifi_power5.plan.valid ||
    p.generation!=wifi_power5.plan.generation || p.factory_base!=wifi_power5.plan.factory_base || p.limit_half_dbm!=wifi_power5.plan.limit_half_dbm || p.maximum_half_dbm!=wifi_power5.plan.maximum_half_dbm)return false;
 for(unsigned i=0;i<9;i++)if(p.write[i].address!=wifi_power5.plan.write[i].address || p.write[i].mask!=wifi_power5.plan.write[i].mask || p.write[i].value!=wifi_power5.plan.write[i].value)return false;
 return true;
}
/* Independent ledger retains private originals and the pre-submission dirty
 * mask. A completed USB readback mismatch ends forward admission but preserves
 * only exact original restoration under fresh disabled-MAC guards. Transfer,
 * guard,request or restoration failures still quarantine;no DMA is reclaimed. */
static bool wifi_power5_cleanup_start(void){
 struct mt76_power5_cycle *s=&wifi_power5.cycle;
 if(wifi_power5.cleanup_started)return true;
 if(!s->snapshot_complete || s->selected || s->saved_mask!=511 || wifi_power5.snapshot_mask!=511 ||
    s->modified_mask!=wifi_power5.modified_mask || s->generation!=wifi_power5.plan.generation ||
    !((wifi_power5.step==84 && wifi_power5.inspected && !wifi_power5.failed) ||
      (wifi_power5.forward_failed && wifi_power5.step>=41 && wifi_power5.step<=81)))return false;
 for(unsigned i=0;i<9;i++)if(s->saved[i]!=wifi_power5.original[i])return false;
 wifi_power5.cleanup_started=true;wifi_power5.cleanup_index=8;
 while(wifi_power5.cleanup_index>=0 && !(wifi_power5.modified_mask&(1u<<wifi_power5.cleanup_index)))wifi_power5.cleanup_index--;
 return true;
}
static bool wifi_power5_transfer(void *c,bool write,uint32_t a,uint32_t *v,bool cleanup){
 unsigned step=wifi_power5.step,index=0,part=0;bool want_write=false;uint32_t want_address=0,want_value=0;bool exact=true;
 if(!v || !wifi_power5_owned(c,cleanup) || step>=132 || wifi_power5.cycle.reads+wifi_power5.cycle.writes!=step+1)return wifi_power5_fail();
 if(cleanup){
  if(!wifi_power5_cleanup_start() || wifi_power5.cleanup_done || wifi_power5.cleanup_steps>=48 || wifi_power5.cleanup_index < -1 || wifi_power5.cleanup_index>8 || wifi_power5.cleanup_phase>4)return wifi_power5_fail();
  part=wifi_power5.cleanup_phase;
  if(wifi_power5.cleanup_index>=0){
   index=(unsigned)wifi_power5.cleanup_index;
   if(part>=3){want_address=wifi_power5.plan.write[index].address;want_write=part==3;want_value=wifi_power5.original[index];}
  }else if(part>=3)return wifi_power5_fail();
 }else{
  if(wifi_power5.cleanup_started || step>=84)return wifi_power5_fail();
  if(step<36){index=step/4;part=step%4;if(part==3){want_address=wifi_power5.plan.write[index].address;exact=index==0;if(exact)want_value=wifi_cal5.cal.saved[0];}}
  else if(step<81){index=(step-36)/5;part=(step-36)%5;
   if(part>=3){want_address=wifi_power5.plan.write[index].address;want_write=part==3;want_value=(wifi_power5.original[index]&~wifi_power5.plan.write[index].mask)|wifi_power5.plan.write[index].value;}}
  else part=step-81;
 }
 if(part<3){const uint32_t addresses[]={0x1004,0x2000,0x208},values[]={0,0xf000f200,0x40000050};want_address=addresses[part];want_value=values[part];}
 if(a!=want_address || write!=want_write || (write && *v!=want_value))return wifi_power5_fail();
 if(!cleanup && write)wifi_power5.modified_mask|=1u<<index;
 wifi_power5.step++;
 bool transport=wifi_mac_private_usb(wifi_power5.d,write,a,v);
 bool matched=transport && (exact?*v==want_value:*v!=UINT32_MAX);
 if(matched && !cleanup && step<36 && part==3){wifi_power5.original[index]=*v;wifi_power5.snapshot_mask|=1u<<index;}
 console_printf("wifi-power5-usb: op=%u cleanup=%u write=%u address=%x value=%x ok=%u\n",(uint64_t)step,(uint64_t)cleanup,(uint64_t)write,(uint64_t)a,(uint64_t)*v,(uint64_t)matched);
 if(!matched){
  /* Only a completed selected-word readback with intact snapshot/dirty proof
   * permits cleanup. It never permits another forward word or a MAC start. */
  if(transport && !cleanup && step>=36 && step<81 && part==4 && !write && wifi_power5.snapshot_mask==511){
   wifi_power5.failed=wifi_power5.forward_failed=true;return false;
  }
  return wifi_power5_fail();
 }
 if(cleanup){
  wifi_power5.cleanup_steps++;
  if(wifi_power5.cleanup_index>=0 && part==4){
   wifi_power5.restored_mask|=1u<<index;wifi_power5.cleanup_phase=0;wifi_power5.cleanup_index--;
   while(wifi_power5.cleanup_index>=0 && !(wifi_power5.modified_mask&(1u<<wifi_power5.cleanup_index)))wifi_power5.cleanup_index--;
  }else{
   wifi_power5.cleanup_phase++;
   if(wifi_power5.cleanup_index<0 && wifi_power5.cleanup_phase==3)wifi_power5.cleanup_done=wifi_power5.restored_mask==wifi_power5.modified_mask;
  }
 }
 return true;
}
static bool wifi_power5_read(void *c,uint32_t a,uint32_t *v,bool clean){if(!v)return false;*v=0;return wifi_power5_transfer(c,false,a,v,clean);}
static bool wifi_power5_write(void *c,uint32_t a,uint32_t v,bool clean){return wifi_power5_transfer(c,true,a,&v,clean);}
static bool wifi_power5_selected(void *c){
 const struct mt76_power5_cycle *s=&wifi_power5.cycle;
 if(!wifi_power5_owned(c,false) || wifi_power5.step!=84 || wifi_power5.cleanup_started || wifi_power5.snapshot_mask!=511 || wifi_power5.modified_mask!=511 || wifi_power5.inspected || !s->selected || !s->snapshot_complete ||
    s->saved_mask!=511 || s->modified_mask!=511 || s->verified_mask!=511 || s->generation!=wifi_power5.plan.generation)return wifi_power5_fail();
 for(unsigned i=0;i<9;i++)if(s->saved[i]!=wifi_power5.original[i] || s->chosen[i]!=((wifi_power5.original[i]&~wifi_power5.plan.write[i].mask)|wifi_power5.plan.write[i].value))return wifi_power5_fail();
 wifi_power5.inspected=true;console_write("wifi-power5-selected: words=9 readbacks=9 mac_disabled=1 retained=1 network_out=0\n");
 return !wifi_handoff5_requested || wifi_handoff5_inspect();
}
static bool wifi_power5_inspect(void){
 if(!wifi_power5_requested || wifi_power5.attempted)return false;
 wifi_power5.d=wifi_cal5.d;wifi_power5.attempted=wifi_power5.active=wifi_power5.fields_active=true;
 if(!wifi_power5_parent()){wifi_power5.active=wifi_power5.fields_active=false;console_write("wifi-power5: admission=refused network_out=0\n");return false;}
 console_write("wifi-power5-progress: fresh mapped fields;guarded nine-word snapshot/select/readback;disabled-MAC inspection;reverse original words;no network OUT\n");
 const struct mt76_efuse_map_io fields={{&wifi_power5,wifi_power5_fields_read,wifi_power5_fields_write,wifi_power5_fields_wait},wifi_power5_fields_owned};
 enum mt76_capture_status result=mt76_power5_capture_run(&wifi_power5.fields,&fields,&wifi_power5.d->wifi_identity,wifi_cal5.choice.generation);
 wifi_power5.fields_active=false;const struct mt76_power5_fields *f=&wifi_power5.fields.fields;
 console_printf("wifi-power5-fields: status=%u complete=%u blocks=%u reads=%u writes=%u polls=%u phase=%u submitted=%u chip=%x nic0=%x nic1=%x pair=%x cck=%x ofdm_lo=%x ofdm_hi=%x ht_lo=%x ht_hi=%x vht=%x generation=%u raw=0\n",(uint64_t)result,(uint64_t)wifi_power5.fields.complete,(uint64_t)wifi_power5.fields.blocks,(uint64_t)wifi_power5.fields.reads,(uint64_t)wifi_power5.fields.writes,(uint64_t)wifi_power5.fields.polls,(uint64_t)wifi_power5.map.phase,(uint64_t)wifi_power5.map.submitted,(uint64_t)f->chip,(uint64_t)f->nic0,(uint64_t)f->nic1,(uint64_t)f->channel_pair,(uint64_t)f->cck,(uint64_t)f->ofdm_lo,(uint64_t)f->ofdm_hi,(uint64_t)f->ht_lo,(uint64_t)f->ht_hi,(uint64_t)f->vht,f->generation);
 const uint8_t us[2]={'U','S'};bool ok=result==MT76_CAPTURE_OK && mt76_power5_plan(f,us,161,20,30,&wifi_power5.plan)==MT76_POWER5_OK;
 const struct mt76_power5_cycle_io cycle={&wifi_power5,wifi_power5_owned,wifi_power5_read,wifi_power5_write,wifi_power5_selected};
 if(ok)ok=wifi_power5_owned(&wifi_power5,false);
 if(ok)ok=mt76_power5_cycle_run(&wifi_power5.cycle,&cycle,&wifi_power5.fields,30);
 struct mt76_power5_cycle *s=&wifi_power5.cycle;ok=ok && wifi_power5.step==132 && wifi_power5.inspected && wifi_power5.cleanup_done && wifi_power5.modified_mask==511 && wifi_power5.restored_mask==511 && !wifi_power5.failed && wifi_power5_owned(&wifi_power5,true);
 console_printf("wifi-power5: ok=%u complete=%u snapshot=%u inspected=%u selected=%u cleanup=%u saved=%u modified=%u verified=%u restored=%u reads=%u writes=%u submitted=%u generation=%u base=%u limit=%u maximum=%u retained=1 network_out=0 authenticated=0\n",(uint64_t)ok,(uint64_t)s->complete,(uint64_t)s->snapshot_complete,(uint64_t)s->inspected,(uint64_t)s->selected,(uint64_t)s->cleanup_complete,(uint64_t)s->saved_mask,(uint64_t)s->modified_mask,(uint64_t)s->verified_mask,(uint64_t)s->restored_mask,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)wifi_power5.step,s->generation,(uint64_t)wifi_power5.plan.factory_base,(uint64_t)wifi_power5.plan.limit_half_dbm,(uint64_t)wifi_power5.plan.maximum_half_dbm);
 wifi_power5.active=false;return ok;
}
bool xhci_wifi_capture_power5(void){
 if(wifi_power5_requested || wifi_power5.attempted)return false;
 wifi_power5_requested=true;bool ok=xhci_wifi_capture_discovery5();wifi_power5_requested=false;return ok;
}
