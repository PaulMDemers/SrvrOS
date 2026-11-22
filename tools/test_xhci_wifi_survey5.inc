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
/* Run survey and the real generation-bound power child against the same
 * modeled controller. Only the adapter's register values are synthesized. */
static const unsigned survey5_power_addresses[]={0x13b0,0x1314,0x1318,0x131c,0x1320,0x1324,0x13d4,0x13d8,0x13dc};
static struct {
 uint32_t control,words[9],original[9];
 uint8_t efuse[512];
} survey5_power;
static uint32_t survey5_test_power_register(unsigned address,bool in,uint32_t word){
 if(address==0x1004 || address==0x2000 || address==0x208){
  assert(in);
  return address==0x1004?0:address==0x2000?0xf000f200:0x40000050;
 }
 if(address==0x24){
  if(!in)survey5_power.control=word&~(1u<<30);
  return survey5_power.control;
 }
 if(address>=0x28 && address<=0x34){
  unsigned offset=((survey5_power.control>>16)&0x3ff)+address-0x28;
  assert(in && offset+4<=sizeof survey5_power.efuse);
  uint32_t value=0;
  for(unsigned i=0;i<4;i++)value|=(uint32_t)survey5_power.efuse[offset+i]<<(8*i);
  return value;
 }
 unsigned index=0;
 while(index<9 && survey5_power_addresses[index]!=address)index++;
 assert(index<9);
 if(!index && !wifi_power5.snapshot_mask && !wifi_power5.cycle.snapshot_complete)
  survey5_power.words[0]=survey5_power.original[0]=wifi_cal5.cal.saved[0];
 if(!in)survey5_power.words[index]=word;
 return survey5_power.words[index];
}
static void survey5_test_power_setup(struct xhci_device *d){
 memset(&survey5_power,0,sizeof survey5_power);
 survey5_power.control=0x80000000;
 const unsigned offsets[]={0,0x34,0x36,0x98,0xde,0x120,0x122,0x124,0x126,0x12c};
 const uint16_t values[]={0x7610,0xfd11,0x1004,19u<<8,0x0201,0x0405,4,0x0607,0x0203,0x0809};
 for(unsigned i=0;i<10;i++)survey5_put16(survey5_power.efuse+offsets[i],values[i]);
 d->firmware.mcu.ant_fields.nic0=values[1];d->firmware.mcu.ant_fields.nic1=values[2];
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
 memset(&wifi_desktop5,0,sizeof wifi_desktop5);
 assert(!xhci_wifi_cal5_survey_enable());assert(xhci_wifi_cal5_cleanup_enable());
 assert(xhci_wifi_cal5_prime_enable());
 if(mode!=20)assert(xhci_wifi_cal5_survey_enable());
 else assert(!wifi_survey5.enabled);
 if(mode!=20){assert(!xhci_wifi_cal5_survey_enable());}wifi_discovery5_requested=true;
 survey5_test_drop_command=false;
 bool desktop_absent=mode==21;
 bool replacement_power=mode==22;
 if(replacement_power){
  survey5_test_power_setup(d);
  wifi_power5_requested=true;
 }
 bool absent=mode==1 || mode==2 || mode==3 || mode==4 || mode==11 || mode==12 || mode==14 || mode==20;
 if(desktop_absent){
  survey5_mode=1;
  wifi_desktop5.requested=true;
  wifi_power5_requested=true;
 }else if(absent){
  /* Discovery runs after admission and before child/worker allocation. */
  wifi_http_reconnect5_reserved=wifi_http_reconnect5.requested=wifi_http_reconnect5.attempted=wifi_http_reconnect5.admitted=true;
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
  bool selected=mode==0 || mode==13 || mode==15 || mode==19 || replacement_power;
  assert(wifi_survey5.selected==selected && wifi_cal5.choice.selected==selected);
  if(selected)assert(wifi_cal5.choice.generation==wifi_radio5_bank+1);
  assert(wifi_cal5.commands==5 && early5_mcu_calls==10);
  if(mode==1 || mode==2)assert(wifi_survey5.reason==2 && wifi_survey5.window==15);
  if(mode==3)assert(wifi_survey5.reason==1 && !wifi_cal5.window.completions && wifi_cal5.window.attempts==1);
  if(mode==4)assert(wifi_survey5.reason==1 && wifi_cal5.window.completions==3 && wifi_cal5.window.attempts==4);
  if(absent)assert(!wifi_http_reconnect5.complete && !wifi_http_reconnect5.connections && wifi_http_reconnect5_unattempted());
  if(desktop_absent)assert(wifi_desktop5_unattempted() && !wifi_power5.attempted &&
   !wifi_survey5.selected && wifi_survey5.window==15 && !wifi_http_reconnect5_reserved);
  if(replacement_power){
   assert(wifi_power5.cycle.complete && wifi_power5.cycle.generation==2 && wifi_power5.step==132 &&
          wifi_power5.cycle.restored_mask==511);
   assert(!memcmp(survey5_power.words,survey5_power.original,sizeof survey5_power.words));
  }
  if(mode==19){
   uint8_t *p=(uint8_t *)wifi_survey5.prior[0].owner.plan.buffer[0];
   p[0]^=1;assert(!wifi_survey5_held());p[0]^=1;
   assert(!wifi_survey5_held() && wifi_survey5.failed && !wifi_cal5_cleanup_held());
  }
 }
 assert((!wifi_power5.attempted || mode==20 || replacement_power) && !wifi_join5.attempted && !freed);
 survey5_test_active=false;early5_finish();
}
static void test_wifi_survey5(void){
 for(unsigned mode=0;mode<=20;mode++)survey5_whole_native(mode,0);
 for(unsigned limit=1;limit<=17;limit++)survey5_whole_native(0,limit);
 survey5_whole_native(21,0);
 assert(wifi_survey5_banks[0].complete && !wifi_survey5_banks[0].selected);
 wifi_radio5_bank=1;
 survey5_whole_native(22,0);
 assert(wifi_survey5_banks[0].complete && !wifi_survey5_banks[0].selected);
 assert(wifi_survey5_banks[1].complete && wifi_survey5_banks[1].selected);
 wifi_radio5_bank=0;
 puts("xhci-wifi-survey5: PASS 40 native calibration/prime/fresh windows/Stop/selection/actual RF inverse cases, including replacement discovery through the power cycle and desktop missing-target cleanup; modeled HCD/RF");
}
