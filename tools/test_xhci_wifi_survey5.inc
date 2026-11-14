/* Production calibration/survey/Stop/selection/parent-continuation paths.
 * HCD, air frames, clocks and the already-selected RF parent are modeled. */
static unsigned survey5_mode,survey5_target=1;
bool survey5_test_RF_parent(bool (*)(void),bool);
static bool survey5_native_inspect(void){return wifi_cal5_inspect(&wifi_channel_vco5);}
static void survey5_put16(uint8_t *p,unsigned n){p[0]=n;p[1]=n>>8;}
static void survey5_put32(uint8_t *p,unsigned n){survey5_put16(p,n);survey5_put16(p+2,n>>16);}
static unsigned survey5_frame(uint8_t *p,unsigned id,const char *name,unsigned security){
 memset(p,0,4096);uint8_t *wi=p+4,*h=p+36,*b=p+60;
 survey5_put16(h,0x80);memset(h+4,255,6);h[10]=h[16]=2;h[15]=h[21]=(uint8_t)id;
 survey5_put16(b+8,100);survey5_put16(b+10,1|(security?16:0));unsigned pos=12,n=(unsigned)strlen(name);
 b[pos++]=0;b[pos++]=(uint8_t)n;memcpy(b+pos,name,n);pos+=n;
 b[pos++]=1;b[pos++]=1;b[pos++]=0x8c;
 if(security)pos+=(unsigned)mt76_rsn_station_ie(b+pos,4096-pos-60);
 unsigned mpdu=24+pos,dma=(32+mpdu+3)&~3u;survey5_put16(p,dma);survey5_put32(wi+4,mpdu<<16);
 return dma+4;
}
static void survey5_test_command(void){
 if(!wifi_survey5.switching)return;
 struct xhci_device *d=cal5_mcu_test_device;
 if(survey5_mode==5)context_write32(d->output_context,9,1,1);
 if(survey5_mode==6)context_write32(d->output_context,9,2,context_read32(d->output_context,9,2)^1);
 if(survey5_mode==7)context_write32(d->output_context,9,0,1);
 if(survey5_mode==8){
  uint64_t p=wifi_survey5.prior[0].owner.plan.ring;
  early5_queue((struct xhci_trb){.parameter_low=(uint32_t)p,.parameter_high=p>>32,
   .status=(13u<<24)|4000,.control=(32u<<10)|(9u<<16)|((uint32_t)d->slot_id<<24)});
 }
 if(survey5_mode==9)((uint8_t *)wifi_survey5.prior[0].owner.plan.buffer[0])[0]^=1;
 if(survey5_mode==10)wifi_survey5.prior[0].owner.posts++;
 if(survey5_mode==16)prime5_test_fault=1;
 if(survey5_mode==17)prime5_test_fault=7;
 if(survey5_mode==18)survey5_test_drop_command=true;
}
static bool survey5_test_receive(void){
 unsigned i=wifi_cal5.posts-1;
 if(survey5_mode==3 || (survey5_mode==4 && wifi_survey5.window==1 && i>=3))return true;
 uint8_t *p=(uint8_t *)wifi_cal5.plan.buffer[i];
 const char *name=wifi_survey5.window>=survey5_target?"Demers5":"other";
 unsigned id=wifi_survey5.window<survey5_target?2:1,security=1;
 if(survey5_mode==1)name="other";
 if(survey5_mode==2)name="";
 if(survey5_mode==11 && wifi_survey5.window>=survey5_target)id=(i&1)+1;
 if(survey5_mode==12 && wifi_survey5.window>=survey5_target)security=0;
 if(survey5_mode==13 && !wifi_survey5.window){name="";id=1;}
 if(survey5_mode==14 && !wifi_survey5.window){name="";id=1;security=0;}
 unsigned size=survey5_frame(p,id,name,security);
 if(survey5_mode==15 && !wifi_survey5.window)p[0]=255;
 uint64_t trb=wifi_cal5.plan.ring+16*i;
 early5_queue((struct xhci_trb){.parameter_low=(uint32_t)trb,.parameter_high=trb>>32,
  .status=(13u<<24)|(4096-size),.control=(32u<<10)|(9u<<16)|((uint32_t)wifi_cal5.d->slot_id<<24)});
 return true;
}
static void survey5_whole_native(unsigned mode,unsigned limit){
 struct xhci_device *d=early5_base_setup();
 memset(&wifi_cal5,0,sizeof wifi_cal5);memset(&wifi_early5,0,sizeof wifi_early5);
 wifi_channel_vco5.d=d;context_write32(d->output_context,9,0,0);
 early5_test_active=survey5_test_active=true;early5_producer=(unsigned)xhci.event_dequeue;
 early5_mcu_calls=early5_control_calls=early5_rx_bells=0;early5_cycle=xhci.event_cycle;
 early5_control_inject=true;early5_timeout=early5_stop_race=early5_stop_length_invalid=false;
 early5_fault=early5_stop_fault=early5_owner_fault=early5_control_fault=0;
 early5_mac=0;early5_filter=0x17f97;early5_txop=0x583f;
 prime5_test_fault=prime5_test_commands=prime5_test_stops=prime5_test_new_posts=0;
 survey5_mode=mode;
 assert(!xhci_wifi_cal5_survey_enable());assert(xhci_wifi_cal5_cleanup_enable());
 assert(xhci_wifi_cal5_prime_enable());
 if(mode!=20)assert(xhci_wifi_cal5_survey_enable());
 else assert(!wifi_survey5.enabled);
 if(mode!=20){assert(!xhci_wifi_cal5_survey_enable());}wifi_discovery5_requested=true;
 survey5_test_drop_command=false;
 bool absent=mode==1 || mode==2 || mode==3 || mode==4 || mode==11 || mode==12 || mode==14 || mode==20;
 if(absent){
  /* The child/scheduler allocation is modeled here; its real abort is covered
   * separately by the native HTTP supervisor fixtures. No child is published. */
  wifi_http_reconnect5_reserved=wifi_http_reconnect5.requested=wifi_http_reconnect5.attempted=wifi_http_reconnect5.prepared=true;
  wifi_http_reconnect5.probe[0].self=(uintptr_t)&wifi_http_reconnect5.probe[0];
  wifi_power5_requested=true;
 }
 if(limit)fail_after=pool_used+50+limit-1;
 bool fault=(mode>=5 && mode<=10) || (mode>=16 && mode<=18) || limit;
 bool ok=survey5_test_RF_parent(survey5_native_inspect,!fault && mode!=20);
 if(fault){assert(!ok && wifi_survey5.failed && !freed);}
 else if(mode==20){
  assert(!ok && !wifi_survey5.enabled && wifi_cal5.discovery_status==MT76_DISC5_UNAVAILABLE &&
   wifi_cal5.window.completions==8 && wifi_power5.attempted && !wifi_power5.step);
 }
 else{
  assert(ok && wifi_survey5.complete && wifi_cal5.stopped && wifi_cal5_cleanup_held());
  bool selected=mode==0 || mode==13 || mode==15 || mode==19;
  assert(wifi_survey5.selected==selected && wifi_cal5.choice.selected==selected);
  assert(wifi_cal5.commands==5 && early5_mcu_calls==10);
  if(mode==1 || mode==2)assert(wifi_survey5.reason==2 && wifi_survey5.window==15);
  if(mode==3)assert(wifi_survey5.reason==1 && !wifi_cal5.window.completions && wifi_cal5.window.attempts==1);
  if(mode==4)assert(wifi_survey5.reason==1 && wifi_cal5.window.completions==3 && wifi_cal5.window.attempts==4);
  if(absent)assert(!wifi_http_reconnect5.complete && !wifi_http_reconnect5.connections && wifi_http_reconnect5_unattempted());
  if(mode==19){
   uint8_t *p=(uint8_t *)wifi_survey5.prior[0].owner.plan.buffer[0];
   p[0]^=1;assert(!wifi_survey5_held());p[0]^=1;
   assert(!wifi_survey5_held() && wifi_survey5.failed && !wifi_cal5_cleanup_held());
  }
 }
 assert((!wifi_power5.attempted || mode==20) && !wifi_join5.attempted && !freed);
 survey5_test_active=false;early5_finish();
}
static void test_wifi_survey5(void){
 for(unsigned mode=0;mode<=20;mode++)survey5_whole_native(mode,0);
 for(unsigned limit=1;limit<=17;limit++)survey5_whole_native(0,limit);
 puts("xhci-wifi-survey5: PASS38 native calibration/prime/fresh windows/Stop/selection/actual RF inverse cases; legacy eight-frame refusal reproduced; modeled HCD/RF, no physical acceptance");
}
