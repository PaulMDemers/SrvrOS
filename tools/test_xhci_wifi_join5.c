#include <srvros/mt76_join5.h>
#include <srvros/mt76_mac_cycle.h>
#include <srvros/mt76_rx.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <inttypes.h>
#define XHCI_TRB_COUNT 64
#define XHCI_TRB_TYPE_LINK 6
#define XHCI_TRB_TYPE_NORMAL 1
#define XHCI_TRB_TYPE_SETUP_STAGE 2
#define XHCI_TRB_IOC (1u<<5)
#define XHCI_TRB_ISP (1u<<2)
struct xhci_trb{uint32_t parameter_low,parameter_high,status,control;};
struct xhci_device{uint8_t slot_id;bool present,removed,retiring,ep0_quarantined,control_pending,control_failed;uint16_t vendor_id,product_id,device_bcd;void *input_context,*output_context;struct xhci_trb *ep0_ring;uint64_t input_context_phys,output_context_phys,ep0_ring_phys,descriptor_buffer_phys;struct {bool pending;uint64_t buffer_phys;struct {uint64_t ring_phys;}ep;struct {bool pending;uint64_t buffer_phys;struct {uint64_t ring_phys;}response;}mcu;}firmware;};
static struct {uint64_t command_ring_phys,command_enqueue;struct xhci_trb *command_ring;unsigned sync_users;bool stopping,command_pending,command_failed,command_cycle;}xhci;
static struct {struct xhci_device *device;uint64_t setup;uint32_t length;bool done,data_in;}strict_control;
static struct {bool cleanup_enabled;struct xhci_device *d;struct{void *ring;}ep;struct {uint64_t ring,physical[8];void *buffer[8];}plan;int window;}wifi_cal5;
static struct {struct {uint32_t chosen[9];}cycle;}wifi_power5;
static struct {struct {uint64_t new_ring;}plan;void *ring;}wifi_handoff5;
static struct {struct mt76_peer5 binding;}wifi_peer5;
static struct {uint64_t ring_phys;void *ring;bool active,input_restored,disabled;unsigned phase;}wifi_tx5;
static int wifi_channel_vco5;
static struct xhci_device device;
static bool wifi_join5_requested,firmware_command_failed,firmware_command_exact;
static struct {uint32_t input[128][8],output[32][8],initial_rx[8];uint8_t pages[128][4096];unsigned cleanup_frames,cleanup_left,cleanup_events,cleanup_fault,cleanup_mutate,ep0_mode,stop_span_fault,held_mutate;unsigned next,allocations,fail_alloc,alias_alloc,commands,controls,ownership,lose,fail_command,command_mutate,fail_control,fail_send,reply,sends,rx_events,mutate,event_fault,stop_fault,stop_fault_dci;uint64_t time;uint32_t mac,filter,txop;bool held_child_false,pending_tail,normal_at_stop,pre_disable,pre_completed,cleanup_duplicate,drain_busy,persistent_busy,cleanup_exhaust,cleanup_ep0,quarantine,trace,parent,empty,empty_assoc,reject,reject_assoc,noise,bad_guard,auth_extra,auth_bad_ie,eapol_missing,eapol_bad,eapol_fault,eapol_done,pad,skip_stop,alloc_fail,uncertain_command,wrong_slot,clear_disabled;}fake;
/* This legacy parent fixture has no cleanup allocations. Actual calibration
 * cleanup ownership is exercised by the controller fixture. */
static bool wifi_cal5_cleanup_alias(uint64_t p){(void)p;return false;}
static bool wifi_cal5_cleanup_held(void){return true;}
static bool wifi_handoff5_equal(const void *a,const void *b,size_t n){return !memcmp(a,b,n);}
static bool wifi_channel_vco5_owned(void *c){assert(c==&wifi_channel_vco5);return ++fake.ownership!=fake.lose && fake.parent && !fake.quarantine;}
static bool wifi_peer5_owned(void *c,void *w){assert(c==&wifi_peer5 && w==&wifi_cal5.window);return fake.parent && !fake.quarantine && !memcmp(fake.output[9],fake.initial_rx,32);}
static bool wifi_tx5_owned(void){return wifi_tx5.active && !(fake.output[8][0]&7) && !fake.quarantine;}
static bool wifi_mcu_fail(struct xhci_device *d){assert(d==&device);fake.quarantine=true;return false;}
static void *pmm_phys_to_virt(uint64_t p){assert(p && !(p&4095) && p/4096<128);return fake.pages[p/4096];}
static bool alloc_frame_zero(uint64_t *p,void **v){fake.allocations++;if(fake.alloc_fail || fake.allocations==fake.fail_alloc)return false;*p=4096*fake.next++;if(fake.allocations==fake.alias_alloc)*p=4096;*v=pmm_phys_to_virt(*p);memset(*v,0,4096);return true;}
static void write_link_trb(struct xhci_trb *r,uint64_t p,bool c){r[63]=(struct xhci_trb){p,p>>32,0,(6u<<10)|2u|(c?1u:0u)};}
static void zero_bytes(void *p,uint64_t n){memset(p,0,n);}
static void copy_bytes(uint8_t *d,const uint8_t *s,uint64_t n){memcpy(d,s,n);}
static uint32_t context_read32(void *p,unsigned i,unsigned w){return ((uint32_t (*)[8])p)[i][w];}
static void context_write32(void *p,unsigned i,unsigned w,uint32_t v){((uint32_t (*)[8])p)[i][w]=v;}
static uint64_t event_irq_save(void){return 0;}
static void event_irq_restore(uint64_t f){assert(!f);}
static uint64_t timer_monotonic_us(void){return fake.time;}
static bool timer_delay_us(unsigned us);
static void console_write(const char *s){if(fake.trace)fputs(s,stdout);}
static void console_printf(const char *s,...){if(!fake.trace)return;va_list ap;va_start(ap,s);for(;*s;s++){if(*s!='%'){putchar(*s);continue;}s++;uint64_t v=va_arg(ap,uint64_t);if(*s=='u')printf("%" PRIu64,v);else{assert(*s=='x');printf("0x%016" PRIx64,v);}}va_end(ap);}
static uint64_t ring_push(struct xhci_trb *r,uint64_t phys,uint64_t *enqueue,bool *cycle,const struct xhci_trb *td){assert(*enqueue<63 && *cycle);unsigned i=(*enqueue)++;r[i]=*td;r[i].control|=1;return phys+16*i;}
static void db_write32(unsigned slot,unsigned dci);
static bool poll_event(struct xhci_trb *e);
static void dispatch_transfer_event(const struct xhci_trb *e);
static bool submit_command_trb(const struct xhci_trb *t,unsigned type,uint64_t *slot);
static unsigned radio_reads,radio_fault,radio_entries;
static bool radio_unsupported;
static bool wifi_mac_private_usb(struct xhci_device *d,bool write,uint32_t a,uint32_t *v);
bool xhci_wifi_capture_tx5(void);
#include "../kernel/src/drivers/xhci_wifi_join5.inc"
static bool timer_delay_us(unsigned us){fake.time+=(wifi_eapol5.active && (fake.eapol_missing || fake.eapol_bad))?100000:us;return true;}
static void emit_stop(unsigned dci){
 if(fake.normal_at_stop && dci==9 && wifi_join5.rx_pending){
  uint64_t ptr=wifi_join5.rx_phys+16*(wifi_join5.posts-1);wifi_join5.buffer[wifi_join5.posts-1][4000]=0xbe;
  struct xhci_trb normal={ptr,ptr>>32,(13u<<24)|4092,(32u<<10)|(9u<<16)|((uint32_t)device.slot_id<<24)};fake.output[9][2]=(ptr+16)|1;
  assert(wifi_join5_event(&device,&normal));
 }

 unsigned count=dci==8?wifi_join5.sends:wifi_join5.posts;uint64_t ptr=(dci==8?wifi_join5.tx_phys:wifi_join5.rx_phys)+16*count;
 bool pending=dci==9 && wifi_join5.rx_pending;unsigned cc=pending?26:27,residue=pending?4096:0;if(pending)ptr-=16;
 fake.output[dci][2]=ptr|1;fake.output[dci][3]=ptr>>32;
 struct xhci_trb e={ptr,ptr>>32,(cc<<24)|residue,(32u<<10)|(dci<<16)|((uint32_t)device.slot_id<<24)};
 if(fake.stop_fault && dci==fake.stop_fault_dci){switch(fake.stop_fault){case 1:e.parameter_low+=16;break;case 2:e.status=25u<<24;break;case 3:e.status|=8192;break;case 4:e.control|=4;break;case 5:e.control^=1u<<24;break;case 8:e.status=27u<<24;e.parameter_low+=16;fake.output[dci][2]=e.parameter_low|1;break;default:break;}}
 if(fake.stop_fault==7 && dci==fake.stop_fault_dci)fake.output[dci][2]^=16;
 if(!fake.skip_stop){assert(wifi_join5_event(&device,&e));if(fake.stop_fault==6 && dci==fake.stop_fault_dci)assert(wifi_join5_event(&device,&e));}
}
static bool submit_command_trb(const struct xhci_trb *t,unsigned type,uint64_t *slot){
 assert(firmware_command_exact && !firmware_command_failed && !fake.quarantine);fake.commands++;unsigned dci=t->control>>16&31;assert(t->control>>24==device.slot_id && !t->status);*slot=fake.wrong_slot?device.slot_id+1:device.slot_id;
 if(wifi_join5.cleanup_enabled){unsigned i=xhci.command_enqueue;xhci.command_ring[i]=*t;xhci.command_ring[i].control|=xhci.command_cycle?1u:0u;xhci.command_enqueue=i==62?0:i+1;xhci.command_pending=true;xhci.sync_users=1;}
 bool fail=fake.commands==fake.fail_command;if(fail && !fake.uncertain_command)return false;
 if(type==12){assert(!fake.input[1][3]);if(fake.commands==1){assert(fake.input[0][0]==0 && fake.input[0][1]==257);memcpy(fake.output[8],fake.input[9],32);fake.output[8][0]=1;}else{assert(fake.commands==6 && fake.input[0][0]==256 && fake.input[0][1]==1);if(fake.clear_disabled)memset(fake.output[8],0,32);else fake.output[8][0]=0;}}
 else if(type==16){assert(dci==9 && (fake.output[9][0]&7)==3);fake.output[9][2]=t->parameter_low;fake.output[9][3]=t->parameter_high;}
 else {assert(type==15 && (dci==8 || dci==9));fake.output[dci][0]=3;
 if(fake.stop_span_fault){unsigned i=wifi_join5.cleanup_stop_index;switch(fake.stop_span_fault){case 1:xhci.command_ring[i].control^=1;break;case 2:xhci.command_ring[i].parameter_low=16;break;case 3:xhci.command_enqueue=(xhci.command_enqueue+1)%63;break;case 4:xhci.sync_users=2;break;case 5:xhci.command_pending=false;break;default:break;}}
 emit_stop(dci);}
 if(fake.commands==fake.command_mutate)fake.output[11][4]^=1;
 if(wifi_join5.cleanup_enabled){xhci.command_pending=false;xhci.sync_users=0;}
 return !fail;
}
static void db_write32(unsigned slot,unsigned dci){assert(slot==device.slot_id && !fake.quarantine);if(dci==9){fake.output[9][0]=1;}else{assert(dci==8);fake.sends++;fake.reply=wifi_join5.kinds[wifi_join5.sends-1]+1;}}
static void put16(uint8_t *p,unsigned v){p[0]=v;p[1]=v>>8;}
static void put32(uint8_t *p,uint32_t v){put16(p,v);put16(p+2,v>>16);}
static unsigned response(void){
 unsigned i=wifi_join5.posts-1;uint8_t *p=wifi_join5.buffer[i];memset(p,0,4096);bool auth=fake.reply==1;size_t n=auth?(fake.auth_extra?41:30):40;uint8_t *h=p+36,*body=h+24+(fake.pad?2:0);
 put32(p+4,fake.pad?1u<<14:0);put32(p+8,(uint32_t)n<<16);put16(h,auth?0xb0:0x10);memcpy(h+4,wifi_join5.peer.peer.station,6);memcpy(h+10,wifi_join5.peer.peer.bssid,6);memcpy(h+16,wifi_join5.peer.peer.bssid,6);
 if(auth){put16(body+2,2);put16(body+4,fake.reject?13:0);if(fake.auth_extra){body[6]=221;body[7]=fake.auth_bad_ie?10:9;for(unsigned k=0;k<9;k++)body[8+k]=(uint8_t)(k+1);}}else{put16(body,0x411);put16(body+2,(fake.reject || fake.reject_assoc)?13:0);put16(body+4,0xc007);body[6]=1;body[7]=8;memcpy(body+8,(uint8_t[]){0x8c,18,0x98,36,0xb0,72,96,108},8);}
 if(radio_unsupported && auth)put32(p+4,1u<<11);
 unsigned dma=(32+n+(fake.pad?2:0)+3)&~3u;put16(p,dma);if(fake.noise)h[10]^=2;else fake.reply=0;return dma+4;
}
static unsigned eapol_response(void){
 unsigned i=wifi_join5.posts-1;uint8_t *p=wifi_join5.buffer[i];memset(p,0,4096);
 unsigned mpdu=26+8+99,dma=(32+mpdu+2+3)&~3u;put16(p,dma);put32(p+4,2|(1u<<14));put32(p+8,mpdu<<16);
 uint8_t *h=p+36;put16(h,0x0288);memcpy(h+4,wifi_join5.peer.peer.station,6);memcpy(h+10,wifi_join5.peer.peer.bssid,6);memcpy(h+16,wifi_join5.peer.peer.bssid,6);h[24]=7;
 uint8_t *llc=h+28;memcpy(llc,(uint8_t[]){0xaa,0xaa,3,0,0,0,0x88,0x8e},8);uint8_t *m=llc+8;m[0]=2;m[1]=3;m[3]=95;m[4]=2;m[6]=0x8a;m[8]=16;m[16]=7;for(unsigned j=0;j<32;j++)m[17+j]=j+1;
 if(fake.eapol_bad)m[49]=1;
 return dma+4;
}
static bool poll_event(struct xhci_trb *e){
 if(wifi_join5.tx_pending && wifi_join5.sends!=fake.fail_send){uint64_t ptr=wifi_join5.tx_phys+16*(wifi_join5.sends-1);*e=(struct xhci_trb){ptr,ptr>>32,1u<<24,(32u<<10)|(8u<<16)|((uint32_t)device.slot_id<<24)};fake.output[8][2]=(ptr+16)|1;}
 else if(fake.drain_busy && wifi_join5.mac.mac_disabled && wifi_join5.rx_pending && fake.cleanup_left){
  uint64_t ptr=wifi_join5.rx_phys+16*(wifi_join5.posts-1);unsigned i=wifi_join5.posts-1;
  wifi_join5.buffer[i][4000]=(uint8_t)(0xa0+fake.cleanup_events);fake.cleanup_events++;fake.cleanup_left--;
  *e=(struct xhci_trb){ptr,ptr>>32,(13u<<24)|4092,(32u<<10)|(9u<<16)|((uint32_t)device.slot_id<<24)};fake.output[9][2]=(ptr+16)|1;
  if(fake.cleanup_fault){switch(fake.cleanup_fault){case 1:e->parameter_low+=16;break;case 2:e->control|=4;break;case 3:e->status=25u<<24;break;case 4:e->control^=1u<<24;break;case 5:e->status|=8192;break;default:break;}}
 }
 else if(wifi_join5.rx_pending && fake.reply && fake.reply<=2 && !fake.empty && !(fake.empty_assoc && fake.reply==2)){uint64_t ptr=wifi_join5.rx_phys+16*(wifi_join5.posts-1);unsigned n=response();*e=(struct xhci_trb){ptr,ptr>>32,(13u<<24)|(4096-n),(32u<<10)|(9u<<16)|((uint32_t)device.slot_id<<24)};fake.output[9][2]=(ptr+16)|1;fake.rx_events++;}
 else if(wifi_eapol5.active && wifi_join5.rx_pending && !fake.eapol_missing && !fake.eapol_done){uint64_t ptr=wifi_join5.rx_phys+16*(wifi_join5.posts-1);unsigned n=eapol_response();*e=(struct xhci_trb){ptr,ptr>>32,(13u<<24)|(4096-n),(32u<<10)|(9u<<16)|((uint32_t)device.slot_id<<24)};fake.output[9][2]=(ptr+16)|1;fake.eapol_done=true;if(fake.eapol_fault)e->control|=4;}
 else return false;
 if(fake.event_fault){switch(fake.event_fault){case 1:e->parameter_low+=16;break;case 2:e->control|=4;break;case 3:e->status^=1;break;case 4:e->control^=1u<<24;break;default:e->control|=1u<<5;break;}}
 return true;
}
static void dispatch_transfer_event(const struct xhci_trb *e){assert(wifi_join5_event(&device,e));if(fake.cleanup_duplicate && wifi_join5.cleanup_started)assert(wifi_join5_event(&device,e));}
static bool wifi_mac_private_usb(struct xhci_device *d,bool write,uint32_t a,uint32_t *v){
 assert(d==&device && !fake.quarantine);fake.controls++;
 if(a==0x1798 || a==0x1718){
  assert(!write && wifi_join5.radio_enabled && fake.sends<=1);
  assert(a==(radio_reads%2?0x1718u:0x1798u));
  unsigned entry=radio_reads/2;radio_reads++;
  *v=a==0x1798?0x1234:((entry%17)<radio_entries?1:0);
  if(radio_reads==radio_fault)return false;
 }

 if(write){if(a==0x1004){fake.mac=*v;if(!*v && fake.drain_busy)fake.cleanup_left=fake.cleanup_frames;}else if(a==0x1400)fake.filter=*v;else{assert(a==0x1340);fake.txop=*v;}}
 else if(a!=0x1798 && a!=0x1718){*v=a==0x1004?fake.mac:a==0x2000?0xf000f200:a==0x208?0x40000050:a==0x1400?fake.filter:a==0x1340?fake.txop:a==0x13b0?0x2f2f1313:a==0x2124?~0u:0;}
 if(fake.pending_tail && !write && a==0xa30 && wifi_join5.cleanup_posts==1 && wifi_join5.cleanup_completed==1)*v=0x500;
 if(fake.drain_busy && !write && a==0xa30 && wifi_join5.mac.mac_disabled && (fake.cleanup_left || fake.persistent_busy))*v=0x500;
 if((fake.pre_disable && (a==0x1798 || a==0x1718)) || (fake.cleanup_ep0 && wifi_join5.cleanup_started) || (fake.pre_disable && wifi_join5.sends==3 && !fake.pre_completed)){
  uint64_t setup=wifi_join5.cleanup_control_setup;device.control_pending=true;xhci.sync_users=true;strict_control=(typeof(strict_control)){.device=&device,.setup=device.ep0_ring_phys,.length=4,.data_in=!write};
  device.ep0_ring[0]=(struct xhci_trb){setup,setup>>32,8,2u<<10};
  if(fake.ep0_mode==1)device.ep0_ring[0].parameter_low^=1;
  if(fake.ep0_mode==2)strict_control.length=8;
  if(fake.ep0_mode==3)strict_control.data_in=write;
  if(fake.ep0_mode==4)device.ep0_ring[0].control=1u<<10;
  if(fake.ep0_mode==5)strict_control.setup+=16;
  struct xhci_trb e;
  if(fake.pre_disable && !fake.pre_completed && wifi_join5.rx_pending){uint64_t ptr=wifi_join5.rx_phys+16*(wifi_join5.posts-1);wifi_join5.buffer[wifi_join5.posts-1][4000]=0xc1;e=(struct xhci_trb){ptr,ptr>>32,(13u<<24)|4092,(32u<<10)|(9u<<16)|((uint32_t)device.slot_id<<24)};fake.output[9][2]=(ptr+16)|1;fake.pre_completed=true;dispatch_transfer_event(&e);}
  else if(poll_event(&e))dispatch_transfer_event(&e);
  device.control_pending=false;xhci.sync_users=0;strict_control=(typeof(strict_control)){0};
 }
 if(fake.cleanup_mutate && wifi_join5.cleanup_started && wifi_join5.cleanup_completed){switch(fake.cleanup_mutate){case 1:wifi_join5.buffer[wifi_join5.cleanup_first][4000]^=1;break;case 2:wifi_join5.rx[wifi_join5.cleanup_first].status^=1;break;case 3:wifi_join5.pointers[2+wifi_join5.cleanup_first]++;break;case 4:fake.output[11][4]^=1;break;default:break;}}
 if(fake.bad_guard && !write && a==0x1004 && fake.controls==1)*v=4;
 if(fake.controls==12){switch(fake.mutate){case 1:wifi_peer5.binding.generation++;break;case 2:fake.output[11][4]^=1;break;case 3:fake.input[5][0]^=1;break;case 4:wifi_cal5.plan.buffer[0]=fake.pages[90];break;case 5:wifi_join5.rx_phys+=4096;break;case 6:wifi_join5.rx[40].control=1;break;case 7:device.output_context_phys+=4096;break;case 8:wifi_join5.buffer[WIFI_JOIN5_RX][4000]=1;break;case 9:wifi_join5.buffer[47][4000]=1;break;case 10:((uint8_t *)wifi_cal5.plan.buffer[0])[0]=1;break;case 11:wifi_join5.pointers[2]++;break;case 12:device.slot_id++;break;default:break;}}
 return fake.controls!=fake.fail_control;
}
bool xhci_wifi_capture_tx5(void){return wifi_join5_inspect();}
static void setup(void){
 radio_reads=radio_fault=radio_entries=0;radio_unsupported=false;
 memset(&wifi_eapol5,0,sizeof wifi_eapol5);memset(&fake,0,sizeof fake);memset(&wifi_join5,0,sizeof wifi_join5);memset(&device,0,sizeof device);fake.next=30;fake.parent=true;fake.filter=0x17f97;fake.txop=0x583f;fake.time=1000;wifi_join5_requested=firmware_command_failed=firmware_command_exact=false;wifi_join5_after=0;
 xhci=(typeof(xhci)){.command_ring_phys=9*4096,.command_ring=(void *)fake.pages[9],.command_cycle=true};strict_control=(typeof(strict_control)){0};wifi_cal5.cleanup_enabled=false;
 device=(struct xhci_device){.slot_id=7,.present=true,.vendor_id=0x7392,.product_id=0xa711,.device_bcd=0x100,.input_context=fake.input,.output_context=fake.output,.input_context_phys=3*4096,.output_context_phys=4*4096,.ep0_ring=(void *)fake.pages[1],.ep0_ring_phys=4096,.descriptor_buffer_phys=2*4096};device.firmware.buffer_phys=5*4096;device.firmware.ep.ring_phys=6*4096;device.firmware.mcu.buffer_phys=7*4096;device.firmware.mcu.response.ring_phys=8*4096;
 wifi_cal5.d=&device;wifi_cal5.plan.ring=10*4096;wifi_cal5.ep.ring=fake.pages[10];for(unsigned i=0;i<8;i++){wifi_cal5.plan.physical[i]=(11+i)*4096;wifi_cal5.plan.buffer[i]=fake.pages[11+i];}
 wifi_handoff5.plan.new_ring=19*4096;wifi_handoff5.ring=fake.pages[19];wifi_tx5=(typeof(wifi_tx5)){.ring_phys=20*4096,.ring=fake.pages[20],.active=true,.input_restored=true,.disabled=true,.phase=7};
 wifi_peer5.binding=(struct mt76_peer5){.attempted=true,.complete=true,.generation=1,.observations=2,.rates=255,.peer={.station={2,1,2,3,4,5},.bssid={2,8,8,8,8,8},.ssid="Demers5",.ssid_size=7,.channel=161,.basic_rates=21}};wifi_peer5.binding.peer.rsn_size=mt76_rsn_station_ie(wifi_peer5.binding.peer.rsn,sizeof wifi_peer5.binding.peer.rsn);
 wifi_power5.cycle.chosen[0]=0x2f2f1313;fake.output[0][0]=16u<<27;fake.output[0][3]=(3u<<27)|7;memcpy(fake.output[9],(uint32_t[]){3,0x2000036,10*4096+129,0,4096,0,0,0},32);memcpy(fake.initial_rx,fake.output[9],32);for(unsigned i=0;i<128;i++)for(unsigned j=0;j<8;j++)fake.input[i][j]=i*8+j;
}
static bool mutate_held_child(void){
 assert(wifi_join5.cleanup_frozen);
 switch(fake.held_mutate){case 1:wifi_join5.rx_copied=49;break;case 2:wifi_join5.posts=49;break;case 3:wifi_join5.buffer[0]=0;break;case 4:wifi_join5.buffer[0]=(void *)1;break;case 5:wifi_join5.pointers[2]++;break;case 6:wifi_join5.pins[2]+=4096;break;case 7:wifi_join5.rx[0].status^=1;break;case 8:fake.input[5][0]^=1;break;case 9:((uint8_t *)wifi_cal5.plan.buffer[0])[0]^=1;break;case 10:wifi_join5.buffer[47][4000]=1;break;default:assert(0);}
 if(fake.held_mutate<=6)assert(!wifi_join5_cleanup_held());
 return !fake.held_child_false;
}
static bool late_child(void){
 struct xhci_trb e={wifi_join5.rx_phys,wifi_join5.rx_phys>>32,13u<<24,(32u<<10)|(9u<<16)|((uint32_t)device.slot_id<<24)};
 assert(wifi_join5.cleanup_frozen && wifi_join5.active);assert(wifi_join5_event(&device,&e));return true;
}
int main(int argc,char **argv){
 setup();if(argc==2 && (!strcmp(argv[1],"--radio-trace") || !strcmp(argv[1],"--radio-timeout-trace") || !strcmp(argv[1],"--radio-unsupported-trace"))){
  wifi_cal5.cleanup_enabled=true;fake.pre_disable=true;fake.trace=true;
  fake.empty=!strcmp(argv[1],"--radio-timeout-trace");radio_unsupported=!strcmp(argv[1],"--radio-unsupported-trace");
  assert(xhci_wifi_join5_radio_enable());bool ok=xhci_wifi_capture_join5();
  assert(ok==!(fake.empty || radio_unsupported) && fake.pre_completed && wifi_join5.restored && !fake.quarantine);
  assert(wifi_join5.radio[0].complete && wifi_join5.radio[1].complete);return 0;
 }
 setup();if(argc==2 && !strcmp(argv[1],"--cleanup-suite")){
  wifi_cal5.cleanup_enabled=true;fake.drain_busy=true;fake.cleanup_frames=3;wifi_eapol5.requested=true;
  assert(xhci_wifi_capture_join5());assert(wifi_join5.cleanup_posts==2 && wifi_join5.cleanup_completed==2 && wifi_join5.cleanup_forward==1 && wifi_join5.cleanup_frozen && wifi_join5.rx_copied==wifi_join5.posts && wifi_join5.mac.rx_rounds>=7);
  assert(wifi_join5.exchange.polls==2 && wifi_eapol5.frames==1 && fake.sends==3 && wifi_join5.posts==6);
  for(unsigned mode=0;mode<=5;mode++){setup();wifi_cal5.cleanup_enabled=true;fake.drain_busy=true;fake.cleanup_frames=3;fake.cleanup_ep0=true;fake.ep0_mode=mode;wifi_eapol5.requested=true;bool ok=xhci_wifi_capture_join5();assert(ok==!mode);if(mode)assert(fake.quarantine && !wifi_join5.restored);}
  for(unsigned fault=1;fault<=5;fault++){setup();wifi_cal5.cleanup_enabled=true;fake.drain_busy=true;fake.cleanup_frames=3;fake.cleanup_fault=fault;wifi_eapol5.requested=true;assert(!xhci_wifi_capture_join5() && fake.quarantine && !wifi_join5.restored);}
  for(unsigned fault=1;fault<=4;fault++){setup();wifi_cal5.cleanup_enabled=true;fake.drain_busy=true;fake.cleanup_frames=3;fake.cleanup_mutate=fault;wifi_eapol5.requested=true;assert(!xhci_wifi_capture_join5() && fake.quarantine && !wifi_join5.restored);}
  for(unsigned mode=0;mode<=5;mode++){setup();wifi_cal5.cleanup_enabled=true;fake.pre_disable=true;fake.ep0_mode=mode;wifi_eapol5.requested=true;bool ok=xhci_wifi_capture_join5();assert(ok==!mode);if(!mode)assert(wifi_join5.cleanup_forward==1 && wifi_join5.cleanup_posts==0);else assert(fake.quarantine);}
  setup();wifi_cal5.cleanup_enabled=true;fake.drain_busy=true;fake.cleanup_frames=2;fake.pending_tail=true;wifi_eapol5.requested=true;assert(xhci_wifi_capture_join5() && wifi_join5.cleanup_posts==2 && wifi_join5.cleanup_completed==1 && (wifi_join5.cleanup_stop_event[1].status>>24)==26 && wifi_join5.cleanup_frozen);
  setup();wifi_cal5.cleanup_enabled=true;fake.normal_at_stop=true;wifi_eapol5.requested=true;assert(xhci_wifi_capture_join5() && wifi_join5.cleanup_forward==1 && wifi_join5.cleanup_posts==0 && wifi_join5.cleanup_frozen);
  setup();wifi_cal5.cleanup_enabled=true;fake.stop_fault_dci=9;fake.stop_fault=8;wifi_eapol5.requested=true;assert(!xhci_wifi_capture_join5() && fake.quarantine && !wifi_join5.restored);
  setup();wifi_cal5.cleanup_enabled=true;fake.drain_busy=true;fake.cleanup_frames=3;fake.cleanup_duplicate=true;wifi_eapol5.requested=true;assert(!xhci_wifi_capture_join5() && fake.quarantine && !wifi_join5.restored);
  for(unsigned dci=8;dci<=9;dci++)for(unsigned fault=1;fault<=7;fault++){setup();wifi_cal5.cleanup_enabled=true;fake.drain_busy=true;fake.cleanup_frames=3;fake.stop_fault_dci=dci;fake.stop_fault=fault;wifi_eapol5.requested=true;assert(!xhci_wifi_capture_join5() && fake.quarantine && !wifi_join5.restored);}
  for(unsigned fault=1;fault<=5;fault++){setup();wifi_cal5.cleanup_enabled=true;fake.drain_busy=true;fake.cleanup_frames=3;fake.stop_span_fault=fault;wifi_eapol5.requested=true;assert(!xhci_wifi_capture_join5() && fake.quarantine && !wifi_join5.restored);}
  for(unsigned fault=1;fault<=10;fault++)for(unsigned child_false=0;child_false<2;child_false++){setup();wifi_cal5.cleanup_enabled=true;fake.held_child_false=child_false;fake.held_mutate=fault;wifi_eapol5.requested=true;wifi_join5_after=mutate_held_child;assert(!xhci_wifi_capture_join5() && fake.quarantine);}
  setup();assert(!wifi_join5_cleanup_event_span(0));
  setup();wifi_cal5.cleanup_enabled=true;fake.drain_busy=true;fake.cleanup_frames=3;wifi_eapol5.requested=true;wifi_join5_after=late_child;assert(!xhci_wifi_capture_join5() && fake.quarantine && !wifi_join5_cleanup_held());
  setup();wifi_cal5.cleanup_enabled=true;fake.drain_busy=true;fake.cleanup_frames=1;fake.persistent_busy=true;wifi_eapol5.requested=true;assert(!xhci_wifi_capture_join5() && !wifi_join5.mac.rx_drained && wifi_join5.mac.rx_rounds==200 && fake.quarantine);
  setup();wifi_cal5.cleanup_enabled=true;fake.drain_busy=true;fake.cleanup_frames=3;fake.noise=true;assert(!xhci_wifi_capture_join5() && wifi_join5.posts==48 && wifi_join5.cleanup_posts==0 && fake.quarantine && !wifi_join5.restored);
  setup();wifi_cal5.cleanup_enabled=true;fake.noise=true;assert(!xhci_wifi_capture_join5() && wifi_join5.posts==48 && wifi_join5.cleanup_posts==0 && wifi_join5.restored && !fake.quarantine);
  setup();wifi_cal5.cleanup_enabled=true;fake.drain_busy=true;fake.cleanup_frames=10;wifi_eapol5.requested=true;assert(!xhci_wifi_capture_join5() && wifi_join5.cleanup_posts==8 && fake.quarantine);
  setup();wifi_cal5.cleanup_enabled=true;wifi_eapol5.requested=true;assert(xhci_wifi_capture_join5() && wifi_join5.cleanup_posts==0 && wifi_join5.cleanup_frozen);
  struct xhci_trb late={wifi_join5.rx_phys,wifi_join5.rx_phys>>32,13u<<24,(32u<<10)|(9u<<16)|((uint32_t)device.slot_id<<24)};assert(wifi_join5_event(&device,&late) && fake.quarantine && !wifi_join5_cleanup_held());
  puts("cleanup-suite: PASS opt-in retained receive service,forward andscratch snapshot,no parsed cleanup,EP0boundaries,malformed events,mutation,persistentbusy,cap8,zero-post,late quarantine;modeled hardware only");return 0;
 }
 if((argc==2 || argc==11) && (!strcmp(argv[1],"--cleanup-trace") || !strcmp(argv[1],"--eapol-cleanup-trace") || !strcmp(argv[1],"--cleanup-zero-trace") || !strcmp(argv[1],"--cleanup-pending-trace") || !strcmp(argv[1],"--cleanup-stop-normal-trace"))){if(argc==11){for(unsigned i=0;i<8;i++)fake.initial_rx[i]=strtoull(argv[i+2],0,0);memcpy(fake.output[9],fake.initial_rx,32);device.slot_id=strtoul(argv[10],0,0);}wifi_cal5.cleanup_enabled=true;fake.drain_busy=true;fake.cleanup_frames=3;if(!strcmp(argv[1],"--cleanup-zero-trace"))fake.drain_busy=false;if(!strcmp(argv[1],"--cleanup-pending-trace")){fake.cleanup_frames=2;fake.pending_tail=true;}if(!strcmp(argv[1],"--cleanup-stop-normal-trace")){fake.drain_busy=false;fake.normal_at_stop=true;}fake.trace=true;wifi_eapol5.requested=true;assert(xhci_wifi_capture_join5());return 0;}
 if((argc==2 || argc==11) && (!strcmp(argv[1],"--trace") || !strcmp(argv[1],"--eapol-trace"))){
  if(argc==11){for(unsigned i=0;i<8;i++)fake.initial_rx[i]=strtoull(argv[i+2],0,0);memcpy(fake.output[9],fake.initial_rx,32);device.slot_id=strtoul(argv[10],0,0);}
  fake.auth_extra=true;fake.trace=true;wifi_eapol5.requested=!strcmp(argv[1],"--eapol-trace");assert(xhci_wifi_capture_join5());return 0;
 }assert(argc==1);
 uint8_t input[4096];memcpy(input,fake.input,4096);assert(xhci_wifi_capture_join5());assert(fake.sends==3 && fake.commands==6 && wifi_join5.restored && wifi_join5.exchange.aid==7 && !fake.mac && !memcmp(input,fake.input,4096));unsigned checks=fake.ownership,controls=fake.controls;
 assert(!xhci_wifi_capture_join5());
 setup();fake.pad=true;fake.clear_disabled=true;assert(xhci_wifi_capture_join5());
 setup();fake.auth_extra=true;fake.pad=true;assert(xhci_wifi_capture_join5() && wifi_join5.exchange.auth_matched && wifi_join5.exchange.assoc_matched && wifi_join5.restored);
 setup();fake.auth_extra=true;fake.auth_bad_ie=true;assert(!xhci_wifi_capture_join5() && wifi_join5.restored && !fake.quarantine && fake.sends==2 && !wifi_join5.exchange.assoc_sent);
 for(unsigned n=1;n<=6;n++)for(unsigned uncertain=0;uncertain<2;uncertain++){setup();fake.fail_command=n;fake.uncertain_command=uncertain;assert(!xhci_wifi_capture_join5() && fake.quarantine && fake.commands==n && !wifi_join5.restored);}
 for(unsigned n=1;n<=6;n++){setup();fake.command_mutate=n;assert(!xhci_wifi_capture_join5() && fake.quarantine && fake.commands==n);}
 for(unsigned n=1;n<=3;n++){setup();fake.fail_send=n;assert(!xhci_wifi_capture_join5() && fake.sends==n && fake.quarantine && !wifi_join5.restored);}
 for(unsigned n=1;n<=controls;n++){setup();fake.fail_control=n;assert(!xhci_wifi_capture_join5() && fake.quarantine && !wifi_join5.restored);}
 for(unsigned n=1;n<=checks;n++){setup();fake.lose=n;assert(!xhci_wifi_capture_join5() && fake.quarantine && !wifi_join5.restored);}
 for(unsigned n=1;n<=12;n++){setup();fake.mutate=n;assert(!xhci_wifi_capture_join5() && fake.quarantine);}
 for(unsigned n=1;n<=5;n++){setup();fake.event_fault=n;assert(!xhci_wifi_capture_join5() && fake.quarantine && !wifi_join5.restored);}
 setup();fake.bad_guard=true;assert(!xhci_wifi_capture_join5() && fake.quarantine && !wifi_join5.restored && !fake.sends);
 setup();fake.noise=true;assert(!xhci_wifi_capture_join5() && wifi_join5.restored && !fake.quarantine && wifi_join5.posts==WIFI_JOIN5_RX);
 setup();fake.empty_assoc=true;assert(!xhci_wifi_capture_join5() && wifi_join5.restored && !fake.quarantine && fake.sends==3 && wifi_join5.exchange.auth_matched && !wifi_join5.exchange.assoc_matched);
 setup();fake.reject_assoc=true;assert(!xhci_wifi_capture_join5() && wifi_join5.restored && !fake.quarantine && fake.sends==3 && wifi_join5.exchange.status==13);
 setup();fake.reject=true;assert(!xhci_wifi_capture_join5() && wifi_join5.restored && !fake.quarantine && fake.sends==2 && wifi_join5.exchange.leave_sent);
 setup();fake.empty=true;assert(!xhci_wifi_capture_join5() && wifi_join5.restored && !fake.quarantine && fake.sends==2 && wifi_join5.exchange.leave_sent);
 for(unsigned dci=8;dci<=9;dci++)for(unsigned fault=1;fault<=7;fault++){setup();fake.stop_fault=fault;fake.stop_fault_dci=dci;assert(!xhci_wifi_capture_join5() && fake.quarantine && !wifi_join5.restored);}
 for(unsigned n=1;n<=WIFI_JOIN5_RX+5;n++){setup();fake.fail_alloc=n;assert(!xhci_wifi_capture_join5() && !fake.commands);setup();fake.alias_alloc=n;assert(!xhci_wifi_capture_join5() && !fake.commands);}
 setup();fake.skip_stop=true;assert(!xhci_wifi_capture_join5() && fake.quarantine);
 setup();fake.alloc_fail=true;assert(!xhci_wifi_capture_join5() && !fake.commands);
 setup();fake.wrong_slot=true;assert(!xhci_wifi_capture_join5() && fake.quarantine && fake.commands==1);
 setup();assert(xhci_wifi_capture_eapol5() && wifi_eapol5.complete && wifi_eapol5.matches==1 && wifi_eapol5.frames==1 && wifi_join5.restored && fake.sends==3);assert(!xhci_wifi_capture_eapol5());
 setup();fake.eapol_missing=true;assert(!xhci_wifi_capture_eapol5() && wifi_join5.restored && !fake.quarantine && fake.sends==3 && wifi_eapol5.polls<30);
 setup();fake.eapol_bad=true;assert(!xhci_wifi_capture_eapol5() && wifi_join5.restored && !fake.quarantine && fake.sends==3 && wifi_eapol5.frames==1);
 setup();fake.eapol_fault=true;assert(!xhci_wifi_capture_eapol5() && fake.quarantine && !wifi_join5.restored && fake.sends==2);
 setup();radio_entries=17;assert(xhci_wifi_join5_radio_enable());
 assert(xhci_wifi_capture_join5() && wifi_join5.restored && radio_reads==68);
 assert(wifi_join5.radio[0].count==17 && wifi_join5.radio[1].count==17);
 /* Execute the real native send/receive/Stop path with the opt-in trace. */
 setup();assert(xhci_wifi_join5_radio_enable());assert(!xhci_wifi_join5_radio_enable());
 assert(xhci_wifi_capture_join5() && wifi_join5.restored);
 assert(wifi_join5.radio[0].complete && wifi_join5.radio[1].complete && wifi_join5.radio[0].count==1 && wifi_join5.radio[1].count==1);
 setup();fake.empty=true;assert(xhci_wifi_join5_radio_enable());
 assert(!xhci_wifi_capture_join5() && wifi_join5.restored && wifi_join5.exchange.response[0].reason==MT76_JOIN5_RESPONSE_DEADLINE && wifi_join5.radio[1].complete && fake.sends==2);
 for(unsigned n=1;n<=4;n++){
  setup();assert(xhci_wifi_join5_radio_enable());radio_fault=n;
  bool result=xhci_wifi_capture_join5();assert(!result && fake.quarantine && !wifi_join5.restored);
 }
 puts("xhci-join5: PASS actual fresh live RX/OUT owner,matched AUTH/ASSOC/leave,padded RX,all command/control/ownership boundaries,send uncertainty,no replay,semantic rejection/timeout cleanup,frame/event/pin/parent mutation quarantine,input and exact old RX restoration;mocked RF parent;no physical or WPA2/IP acceptance");return 0;
}
