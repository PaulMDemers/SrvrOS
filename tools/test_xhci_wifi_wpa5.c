#include <srvros/mt76_join5.h>
#include <srvros/mt76_wpa5.h>
#include <srvros/initramfs.h>
#include "fixtures/wpa2-handshake-synthetic.h"
#include "fixtures/mt76-data5-synthetic.h"
#include <srvros/mt76_mac_cycle.h>
#include <srvros/mt76_rx.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <inttypes.h>
#define SRVROS_NET_DEVICE_HOST_TEST
#include "../kernel/src/net_device.c"
void net_notify_rx(void){}
void net_handle_ethernet_frame(const uint8_t *frame,uint16_t length){(void)frame;(void)length;assert(0);}
#define XHCI_TRB_COUNT 64
#define XHCI_TRB_TYPE_LINK 6
#define XHCI_TRB_TYPE_NORMAL 1
#define XHCI_TRB_IOC (1u<<5)
#define XHCI_TRB_ISP (1u<<2)
struct usb_setup_packet{uint8_t request_type,request;uint16_t value,index,length;};
struct xhci_trb{uint32_t parameter_low,parameter_high,status,control;};
struct xhci_device{uint8_t slot_id;bool present,removed,retiring,ep0_quarantined,control_pending,control_failed;uint16_t vendor_id,product_id,device_bcd;void *input_context,*output_context;uint64_t input_context_phys,output_context_phys,ep0_ring_phys,descriptor_buffer_phys;struct {bool pending,stop_attempted,stop_ok;unsigned stop_event_cc;uint64_t buffer_phys;struct {uint64_t ring_phys;}ep;struct {bool pending;uint64_t buffer_phys;struct {uint64_t ring_phys;}response;}mcu;}firmware;};
static struct {uint64_t command_ring_phys;bool stopping,sync_users,command_pending,command_failed;}xhci;
static struct {void *device;}strict_control;
static struct {struct xhci_device *d;struct{void *ring;}ep;struct {uint64_t ring,physical[8];void *buffer[8];}plan;int window;}wifi_cal5;
static struct {struct {uint32_t chosen[9];}cycle;}wifi_power5;
static struct {struct {uint64_t new_ring;}plan;void *ring;}wifi_handoff5;
static struct {struct mt76_peer5 binding;}wifi_peer5;
static struct {uint64_t ring_phys;void *ring;bool active,input_restored,disabled;unsigned phase;}wifi_tx5;
static int wifi_channel_vco5;
static struct xhci_device device;
static bool wifi_join5_requested,firmware_command_failed,firmware_command_exact;
static struct {uint32_t input[128][8],output[32][8],initial_rx[8];uint8_t pages[256][4096];unsigned next,allocations,fail_alloc,alias_alloc,commands,controls,ownership,lose,fail_command,command_mutate,fail_control,fail_send,reply,sends,rx_events,mutate,event_fault,stop_fault,stop_fault_dci,wpa_m3_mutate,child_first,wpa_mutate,toggle_count,toggle_actual,data_mutate,drain_queue,drain_fault;uint64_t time;unsigned slow_console,data_console_calls,provider_fault;bool provider_fault_done;uint32_t mac,filter,txop;bool data_wapi_once,data_wapi,data_flood,data_mode,data_empty,data_bad_mic,data_spoof,data_nonce_fail,quarantine,trace,parent,empty,empty_assoc,reject,reject_assoc,noise,bad_guard,auth_extra,auth_bad_ie,eapol_missing,eapol_bad,eapol_fault,eapol_done,pad,skip_stop,alloc_fail,uncertain_command,wrong_slot,clear_disabled,wpa_m1_done,wpa_m3_done,wpa_nonce_fail,wpa_no_m3,toggle_fail,toggle_lie,host_toggle,device_toggle;}fake;
static bool wifi_handoff5_equal(const void *a,const void *b,size_t n){return !memcmp(a,b,n);}
static bool child_active(void);
static bool wifi_channel_vco5_owned(void *c){assert(c==&wifi_channel_vco5);fake.ownership++;if(child_active() && !fake.child_first)fake.child_first=fake.ownership;return fake.ownership!=fake.lose && fake.parent && !fake.quarantine;}
static bool wifi_peer5_owned(void *c,void *w){assert(c==&wifi_peer5 && w==&wifi_cal5.window);return fake.parent && !fake.quarantine && !memcmp(fake.output[9],fake.initial_rx,32);}
static bool wifi_tx5_owned(void){return wifi_tx5.active && !(fake.output[8][0]&7) && !fake.quarantine;}
static bool wifi_mcu_fail(struct xhci_device *d){assert(d==&device);fake.quarantine=true;return false;}
static void *pmm_phys_to_virt(uint64_t p){assert(p && !(p&4095) && p/4096<256);return fake.pages[p/4096];}
static bool alloc_frame_zero(uint64_t *p,void **v){fake.allocations++;if(fake.alloc_fail || fake.allocations==fake.fail_alloc)return false;*p=4096*fake.next++;if(fake.allocations==fake.alias_alloc)*p=4096;*v=pmm_phys_to_virt(*p);memset(*v,0,4096);return true;}
static void write_link_trb(struct xhci_trb *r,uint64_t p,bool c){r[63]=(struct xhci_trb){p,p>>32,0,(6u<<10)|2u|(c?1u:0u)};}
static void zero_bytes(void *p,uint64_t n){memset(p,0,n);}
static void copy_bytes(uint8_t *d,const uint8_t *s,uint64_t n){memcpy(d,s,n);}
static uint32_t context_read32(void *p,unsigned i,unsigned w){return ((uint32_t (*)[8])p)[i][w];}
static void context_write32(void *p,unsigned i,unsigned w,uint32_t v){((uint32_t (*)[8])p)[i][w]=v;}
static uint64_t event_irq_save(void){return 0;}
static void event_irq_restore(uint64_t f){assert(!f);}
uint64_t timer_monotonic_us(void){return fake.time;}
static bool timer_delay_us(unsigned us);
static void console_account(void);
static void console_write(const char *s){console_account();if(fake.trace)fputs(s,stdout);}
static void console_printf(const char *s,...){console_account();if(!fake.trace)return;va_list ap;va_start(ap,s);for(;*s;s++){if(*s!='%'){putchar(*s);continue;}s++;uint64_t v=va_arg(ap,uint64_t);if(*s=='u')printf("%" PRIu64,v);else{assert(*s=='x');printf("0x%016" PRIx64,v);}}va_end(ap);}
static uint64_t ring_push(struct xhci_trb *r,uint64_t phys,uint64_t *enqueue,bool *cycle,const struct xhci_trb *td){assert(*enqueue<63 && *cycle);unsigned i=(*enqueue)++;r[i]=*td;r[i].control|=1;return phys+16*i;}
static void db_write32(unsigned slot,unsigned dci);
static bool poll_event(struct xhci_trb *e);
static void dispatch_transfer_event(const struct xhci_trb *e);
static bool submit_command_trb(const struct xhci_trb *t,unsigned type,uint64_t *slot);
static bool wifi_mac_private_usb(struct xhci_device *d,bool write,uint32_t a,uint32_t *v);
bool xhci_wifi_capture_tx5(void);
static bool wifi_fw_control_once(struct xhci_device *,const struct usb_setup_packet *,uint64_t,uint32_t,bool,int *);
#include "../kernel/src/drivers/xhci_wifi_join5.inc"
#include "../kernel/src/drivers/xhci_wifi_wpa5.inc"
static void console_account(void){
 if(wifi_wpa5.data_active && wifi_wpa5.ip.attempted){fake.data_console_calls++;fake.time+=fake.slow_console;}
 /* Deferred receipt emission must occur after the actual MAC-cycle engine
  * has returned,even on refusal. It must not overlap forward or drain I/O. */
 if(wifi_wpa5.data_reported)assert(!wifi_wpa5.data_active && wifi_wpa5.mac.attempted && (wifi_wpa5.mac.complete || wifi_wpa5.mac.failed));
}
static bool wifi_fw_control_once(struct xhci_device *d,const struct usb_setup_packet *p,uint64_t buffer,uint32_t length,bool in,int *actual){
 assert(d==&device && p && !buffer && !length && !in && actual && !fake.quarantine);
 assert(p->request_type==2 && p->request==1 && !p->value && p->index==4 && !p->length);
 assert(wifi_wpa5.active && wifi_wpa5.toggle_attempted && !wifi_wpa5.toggle_synced && !wifi_wpa5.phase && !wifi_wpa5.posts && !wifi_wpa5.sends && !wifi_wpa5.commands && !wifi_wpa5.mac.attempted && fake.sends==3);
 assert(!wifi_wpa5_toggle()); /* In-flight reentry cannot reissue EP0. */
 fake.toggle_count++;assert(fake.toggle_count==1);*actual=fake.toggle_actual;
 if(!fake.toggle_fail && !fake.toggle_lie)fake.device_toggle=false;
 return !fake.toggle_fail;
}
static bool child_active(void){return wifi_wpa5.active;}
#define LIVE(field) (*(wifi_wpa5.active?&wifi_wpa5.field:&wifi_join5.field))
static bool timer_delay_us(unsigned us){fake.time+=(wifi_eapol5.active && (fake.eapol_missing || fake.eapol_bad))?100000:us;return true;}
static void emit_stop(unsigned dci){
 unsigned count=dci==8?LIVE(sends):LIVE(posts);uint64_t ptr=(dci==8?LIVE(tx_phys):LIVE(rx_phys))+16*count;
 bool pending=dci==9 && LIVE(rx_pending);unsigned cc=pending?26:27,residue=pending?4096:0;if(pending)ptr-=16;
 fake.output[dci][2]=ptr|1;fake.output[dci][3]=ptr>>32;
 struct xhci_trb e={ptr,ptr>>32,(cc<<24)|residue,(32u<<10)|(dci<<16)|((uint32_t)device.slot_id<<24)};
 if(fake.stop_fault && dci==fake.stop_fault_dci){switch(fake.stop_fault){case 1:e.parameter_low+=16;break;case 2:e.status=25u<<24;break;case 3:e.status|=8192;break;case 4:e.control|=4;break;case 5:e.control^=1u<<24;break;default:break;}}
 if(fake.stop_fault==7 && dci==fake.stop_fault_dci)fake.output[dci][2]^=16;
 if(!fake.skip_stop){assert((wifi_wpa5.active?wifi_wpa5_event:wifi_join5_event)(&device,&e));if(fake.stop_fault==6 && dci==fake.stop_fault_dci)assert((wifi_wpa5.active?wifi_wpa5_event:wifi_join5_event)(&device,&e));}
}
static bool submit_command_trb(const struct xhci_trb *t,unsigned type,uint64_t *slot){
 assert(firmware_command_exact && !firmware_command_failed && !fake.quarantine);fake.commands++;unsigned dci=t->control>>16&31;assert(t->control>>24==device.slot_id && !t->status);*slot=fake.wrong_slot?device.slot_id+1:device.slot_id;
 bool fail=fake.commands==fake.fail_command;if(fail && !fake.uncertain_command)return false;
 if(type==12){assert(!fake.input[1][3]);if((fake.commands-1)%6==0){assert(fake.input[0][0]==0 && fake.input[0][1]==257);memcpy(fake.output[8],fake.input[9],32);fake.output[8][0]=1;fake.host_toggle=false;fake.reply=0;}else{assert(fake.commands%6==0 && fake.input[0][0]==256 && fake.input[0][1]==1);if(fake.clear_disabled)memset(fake.output[8],0,32);else fake.output[8][0]=0;}}
 else if(type==16){assert(dci==9 && (fake.output[9][0]&7)==3);fake.output[9][2]=t->parameter_low;fake.output[9][3]=t->parameter_high;}
 else {assert(type==15 && (dci==8 || dci==9));fake.output[dci][0]=3;emit_stop(dci);}
 if(fake.commands==fake.command_mutate)fake.output[11][4]^=1;
 return !fail;
}
static void db_write32(unsigned slot,unsigned dci){assert(slot==device.slot_id && !fake.quarantine);if(dci==9){fake.output[9][0]=1;}else{assert(dci==8);fake.sends++;fake.reply=0;if(wifi_wpa5.data_active){unsigned i=wifi_wpa5.data_sends;assert(i<6);assert(LIVE(lengths[LIVE(sends)-1])==data5_fixture[i].tx_size);assert(!memcmp(LIVE(buffer[WIFI_WPA5_RX+LIVE(sends)-1]),data5_fixture[i].tx,data5_fixture[i].tx_size));}if(fake.host_toggle==fake.device_toggle){fake.reply=LIVE(kinds[LIVE(sends)-1])+1;fake.device_toggle=!fake.device_toggle;}fake.host_toggle=!fake.host_toggle;}}
static void put16(uint8_t *p,unsigned v){p[0]=v;p[1]=v>>8;}
static void put32(uint8_t *p,uint32_t v){put16(p,v);put16(p+2,v>>16);}
static unsigned response(void){
 unsigned i=LIVE(posts)-1;uint8_t *p=LIVE(buffer[i]);memset(p,0,4096);bool auth=fake.reply==1;size_t n=auth?(fake.auth_extra?41:30):40;uint8_t *h=p+36,*body=h+24+(fake.pad?2:0);
 put32(p+4,fake.pad?1u<<14:0);put32(p+8,(uint32_t)n<<16);put16(h,auth?0xb0:0x10);memcpy(h+4,LIVE(peer.peer.station),6);memcpy(h+10,LIVE(peer.peer.bssid),6);memcpy(h+16,LIVE(peer.peer.bssid),6);
 if(auth){put16(body+2,2);put16(body+4,fake.reject?13:0);if(fake.auth_extra){body[6]=221;body[7]=fake.auth_bad_ie?10:9;for(unsigned k=0;k<9;k++)body[8+k]=(uint8_t)(k+1);}}else{put16(body,0x411);put16(body+2,(fake.reject || fake.reject_assoc)?13:0);put16(body+4,0xc007);body[6]=1;body[7]=8;memcpy(body+8,(uint8_t[]){0x8c,18,0x98,36,0xb0,72,96,108},8);}
 unsigned dma=(32+n+(fake.pad?2:0)+3)&~3u;put16(p,dma);if(fake.noise)h[10]^=2;else fake.reply=0;return dma+4;
}
static unsigned eapol_response(void){
 unsigned i=LIVE(posts)-1;uint8_t *p=LIVE(buffer[i]);memset(p,0,4096);
 unsigned mpdu=26+8+99,dma=(32+mpdu+2+3)&~3u;put16(p,dma);put32(p+4,2|(1u<<14));put32(p+8,mpdu<<16);
 uint8_t *h=p+36;put16(h,0x0288);memcpy(h+4,LIVE(peer.peer.station),6);memcpy(h+10,LIVE(peer.peer.bssid),6);memcpy(h+16,LIVE(peer.peer.bssid),6);h[24]=7;
 uint8_t *llc=h+28;memcpy(llc,(uint8_t[]){0xaa,0xaa,3,0,0,0,0x88,0x8e},8);uint8_t *m=llc+8;m[0]=2;m[1]=3;m[3]=95;m[4]=2;m[6]=0x8a;m[8]=16;m[16]=7;for(unsigned j=0;j<32;j++)m[17+j]=j+1;
 if(fake.eapol_bad)m[49]=1;
 return dma+4;
}
static unsigned wpa_response(void){
 bool m1=!wifi_wpa5.exchange.m1_matched;const uint8_t *message=m1?hs_m1:hs_m3;size_t size=m1?sizeof hs_m1:sizeof hs_m3;
 unsigned mpdu=24+8+size,pad=fake.pad?2:0,dma=(32+mpdu+pad+3)&~3u;
 uint8_t *p=wifi_wpa5.buffer[wifi_wpa5.posts-1];memset(p,0,4096);put16(p,dma);put32(p+4,2|(pad?1u<<14:0));put32(p+8,mpdu<<16);
 uint8_t *h=p+36;put16(h,0x0a08);memcpy(h+4,hs_spa,6);memcpy(h+10,hs_aa,6);memcpy(h+16,hs_aa,6);
 memcpy(h+24+pad,(uint8_t[]){0xaa,0xaa,3,0,0,0,0x88,0x8e},8);memcpy(h+32+pad,message,size);
 if(!m1 && fake.wpa_m3_mutate)h[32+pad+fake.wpa_m3_mutate-1]^=1;
 if(m1)fake.wpa_m1_done=true;else fake.wpa_m3_done=true;return dma+4;
}
static unsigned data_response(void){
 unsigned index=wifi_wpa5.sends-5;assert(index<6);const uint8_t *mpdu=data5_fixture[index].rx;unsigned size=data5_fixture[index].rx_size;
 unsigned pad=fake.pad?2:0,dma=(32+size+pad+3)&~3u;
 uint8_t *p=wifi_wpa5.buffer[wifi_wpa5.posts-1];memset(p,0,4096);put16(p,dma);put32(p+4,2|(pad?1u<<14:0));put32(p+8,size<<16);
 memcpy(p+36,mpdu,24);memcpy(p+60+pad,mpdu+24,size-24);
 if(fake.data_bad_mic)p[36+pad+size-1]^=1;
 if(fake.data_spoof)p[46]^=2;
 if(fake.data_wapi || fake.data_wapi_once)p[6]|=4;
 switch(fake.data_mutate){
 case 1:wifi_wpa5.exchange.keys.peer.aa[0]^=2;break;
 case 2:wifi_wpa5.exchange.keys.peer.spa[0]^=2;break;
 case 3:wifi_wpa5.exchange.keys.generation++;break;
 case 4:wifi_wpa5.exchange.keys.tx_pn++;break;
 case 5:wifi_wpa5.exchange.handshake.key_owner++;break;
 case 6:wifi_wpa5.exchange.keys.phase=WPA2_CCMP_EMPTY;break;
 case 7:wifi_wpa5.exchange.handshake.phase=WPA2_HS_EMPTY;break;
 case 8:wifi_wpa5.data_events=WIFI_DATA5_EVENTS;break;
 case 9:wifi_wpa5.data_events=WIFI_DATA5_EVENTS+1;break;
 default:break;
 }
 fake.data_wapi_once=false;fake.reply=0;return dma+4;
}
static bool poll_event(struct xhci_trb *e){
 if(LIVE(tx_pending) && fake.sends!=fake.fail_send){uint64_t ptr=LIVE(tx_phys)+16*(LIVE(sends)-1);*e=(struct xhci_trb){ptr,ptr>>32,1u<<24,(32u<<10)|(8u<<16)|((uint32_t)device.slot_id<<24)};fake.output[8][2]=(ptr+16)|1;}
 else if(LIVE(rx_pending) && fake.reply && fake.reply<=2 && !fake.empty && !(fake.empty_assoc && fake.reply==2)){uint64_t ptr=LIVE(rx_phys)+16*(LIVE(posts)-1);unsigned n=response();*e=(struct xhci_trb){ptr,ptr>>32,(13u<<24)|(4096-n),(32u<<10)|(9u<<16)|((uint32_t)device.slot_id<<24)};fake.output[9][2]=(ptr+16)|1;fake.rx_events++;}
 else if(wifi_wpa5.active && LIVE(rx_pending) && wifi_wpa5.exchange.assoc_matched &&
 ((!wifi_wpa5.exchange.m1_matched && !fake.wpa_m1_done) || (wifi_wpa5.exchange.m2_sent && !fake.wpa_m3_done && !fake.wpa_no_m3))){
  uint64_t ptr=LIVE(rx_phys)+16*(LIVE(posts)-1);unsigned n=wpa_response();*e=(struct xhci_trb){ptr,ptr>>32,(13u<<24)|(4096-n),(32u<<10)|(9u<<16)|((uint32_t)device.slot_id<<24)};fake.output[9][2]=(ptr+16)|1;
 }
 else if(wifi_wpa5.active && LIVE(rx_pending) && ((fake.data_flood && wifi_wpa5.data_active) || (wifi_wpa5.mac.mac_disabled && fake.drain_queue))){
  uint64_t ptr=LIVE(rx_phys)+16*(LIVE(posts)-1);memset(wifi_wpa5.buffer[wifi_wpa5.posts-1],0,4096);
  *e=(struct xhci_trb){ptr,ptr>>32,(13u<<24)|(4096-472),(32u<<10)|(9u<<16)|((uint32_t)device.slot_id<<24)};fake.output[9][2]=(ptr+16)|1;
  if(wifi_wpa5.mac.mac_disabled){fake.drain_queue--;if(fake.drain_fault)wifi_wpa5.rx[0].control^=1;}
 }
 else if(wifi_wpa5.active && wifi_wpa5.data_active && LIVE(rx_pending) && fake.reply==6 && !fake.data_empty){
  uint64_t ptr=LIVE(rx_phys)+16*(LIVE(posts)-1);unsigned n=data_response();*e=(struct xhci_trb){ptr,ptr>>32,(13u<<24)|(4096-n),(32u<<10)|(9u<<16)|((uint32_t)device.slot_id<<24)};fake.output[9][2]=(ptr+16)|1;
 }
 else if(wifi_eapol5.active && LIVE(rx_pending) && !fake.eapol_missing && !fake.eapol_done){uint64_t ptr=LIVE(rx_phys)+16*(LIVE(posts)-1);unsigned n=eapol_response();*e=(struct xhci_trb){ptr,ptr>>32,(13u<<24)|(4096-n),(32u<<10)|(9u<<16)|((uint32_t)device.slot_id<<24)};fake.output[9][2]=(ptr+16)|1;fake.eapol_done=true;if(fake.eapol_fault)e->control|=4;}
 else return false;
 if(wifi_provider5_requested && fake.provider_fault && !fake.provider_fault_done && wifi_wpa5.tx_pending && wifi_wpa5.sends>4 && ((e->control>>16)&31)==8){
  fake.provider_fault_done=true;
  if(fake.provider_fault==1)wifi_provider5.token++;
  if(fake.provider_fault==2)wifi_provider5.epoch++;
  if(fake.provider_fault==3)wifi_provider5.net.pending_token++;
 }
 if(fake.event_fault){switch(fake.event_fault){case 1:e->parameter_low+=16;break;case 2:e->control|=4;break;case 3:e->status^=1;break;case 4:e->control^=1u<<24;break;default:e->control|=1u<<5;break;}}
 return true;
}
static void dispatch_transfer_event(const struct xhci_trb *e){assert((wifi_wpa5.active?wifi_wpa5_event:wifi_join5_event)(&device,e));}
static bool wifi_mac_private_usb(struct xhci_device *d,bool write,uint32_t a,uint32_t *v){
 assert(d==&device && !fake.quarantine);fake.controls++;
 if(write){if(a==0x1004)fake.mac=*v;else if(a==0x1400)fake.filter=*v;else{assert(a==0x1340);fake.txop=*v;}}
 else {*v=a==0x1004?fake.mac:a==0x2000?0xf000f200:a==0x208?0x40000050:a==0x1400?fake.filter:a==0x1340?fake.txop:a==0x13b0?0x2f2f1313:a==0x2124?~0u:0;}
 if(!write && a==0x430 && wifi_wpa5.active && wifi_wpa5.mac.mac_disabled && fake.drain_queue)*v=1u<<16;
 if(fake.bad_guard && !write && a==0x1004 && fake.controls==1)*v=4;
 if(wifi_wpa5.active && fake.controls==65){switch(fake.wpa_mutate){
 case 1:wifi_join5.exchange.aid++;break;
 case 2:wifi_join5.rx[0].control^=1;break;
 case 3:wifi_join5.buffer[0][0]^=1;break;
 case 4:wifi_wpa5.buffer[47][4095]^=1;break;
 case 5:wifi_wpa5.buffer[WIFI_WPA5_RX][4095]^=1;break;
 case 6:fake.input[5][0]^=1;break;
 case 7:fake.output[11][4]^=1;break;
 case 8:wifi_wpa5.pointers[2]++;break;
 case 9:wifi_wpa5.toggle_synced=false;break;
 default:break;}}
 if(fake.controls==12){switch(fake.mutate){case 1:wifi_peer5.binding.generation++;break;case 2:fake.output[11][4]^=1;break;case 3:fake.input[5][0]^=1;break;case 4:wifi_cal5.plan.buffer[0]=fake.pages[90];break;case 5:LIVE(rx_phys)+=4096;break;case 6:LIVE(rx)[40].control=1;break;case 7:device.output_context_phys+=4096;break;case 8:LIVE(buffer[WIFI_JOIN5_RX])[4000]=1;break;case 9:LIVE(buffer[47])[4000]=1;break;case 10:((uint8_t *)wifi_cal5.plan.buffer[0])[0]=1;break;case 11:LIVE(pointers[2])++;break;case 12:device.slot_id++;break;default:break;}}
 return fake.controls!=fake.fail_control;
}
bool xhci_wifi_capture_tx5(void){return wifi_join5_inspect();}
static void setup(void){
 memset(&wifi_provider5,0,sizeof wifi_provider5);wifi_provider5_requested=false;
 memset(providers,0,sizeof providers);memset(identities,0,sizeof identities);memset(names,0,sizeof names);memset(retiring,0,sizeof retiring);memset(references,0,sizeof references);provider_count=0;selected=NULL;sealed=false;
 memset(&wifi_wpa5,0,sizeof wifi_wpa5);wifi_wpa5_requested=wifi_wpa5_data_requested=false;wifi_join5_after=0;memset(&wifi_eapol5,0,sizeof wifi_eapol5);memset(&fake,0,sizeof fake);memset(&wifi_join5,0,sizeof wifi_join5);memset(&device,0,sizeof device);fake.next=30;fake.parent=true;fake.filter=0x17f97;fake.txop=0x583f;fake.time=1000;wifi_join5_requested=firmware_command_failed=firmware_command_exact=false;
 xhci=(typeof(xhci)){.command_ring_phys=9*4096};strict_control.device=0;
 device=(struct xhci_device){.slot_id=7,.present=true,.vendor_id=0x7392,.product_id=0xa711,.device_bcd=0x100,.input_context=fake.input,.output_context=fake.output,.input_context_phys=3*4096,.output_context_phys=4*4096,.ep0_ring_phys=4096,.descriptor_buffer_phys=2*4096};device.firmware.buffer_phys=5*4096;device.firmware.ep.ring_phys=6*4096;device.firmware.mcu.buffer_phys=7*4096;device.firmware.mcu.response.ring_phys=8*4096;
 wifi_cal5.d=&device;wifi_cal5.plan.ring=10*4096;wifi_cal5.ep.ring=fake.pages[10];for(unsigned i=0;i<8;i++){wifi_cal5.plan.physical[i]=(11+i)*4096;wifi_cal5.plan.buffer[i]=fake.pages[11+i];}
 wifi_handoff5.plan.new_ring=19*4096;wifi_handoff5.ring=fake.pages[19];wifi_tx5=(typeof(wifi_tx5)){.ring_phys=20*4096,.ring=fake.pages[20],.active=true,.input_restored=true,.disabled=true,.phase=7};
 wifi_peer5.binding=(struct mt76_peer5){.attempted=true,.complete=true,.generation=1,.observations=2,.rates=255,.peer={.station={2,1,2,3,4,5},.bssid={2,8,8,8,8,8},.ssid="Demers5",.ssid_size=7,.channel=161,.basic_rates=21}};wifi_peer5.binding.peer.rsn_size=mt76_rsn_station_ie(wifi_peer5.binding.peer.rsn,sizeof wifi_peer5.binding.peer.rsn);
 memcpy(wifi_peer5.binding.peer.station,hs_spa,6);memcpy(wifi_peer5.binding.peer.bssid,hs_aa,6);
 wifi_power5.cycle.chosen[0]=0x2f2f1313;fake.output[0][0]=16u<<27;fake.output[0][3]=(3u<<27)|7;memcpy(fake.output[9],(uint32_t[]){3,0x2000036,10*4096+129,0,4096,0,0,0},32);memcpy(fake.initial_rx,fake.output[9],32);for(unsigned i=0;i<128;i++)for(unsigned j=0;j<8;j++)fake.input[i][j]=i*8+j;
}

static uint8_t credential[73];
static struct initramfs_file credential_file={.path="/cfg/wifi-wpa5.pmk",.data=credential,.size=73,.mode=0600};
const struct initramfs_file *initramfs_find(const char *p){assert(!strcmp(p,"/cfg/wifi-wpa5.pmk"));return &credential_file;}
uint64_t initramfs_file_count(void){return 1;}
const struct initramfs_file *initramfs_file_at(uint64_t i){assert(!i);return &credential_file;}
bool wpa2_nonce_generate(uint8_t out[32]){memcpy(out,hs_sn,32);return !fake.wpa_nonce_fail && !(fake.data_nonce_fail && out!=wifi_wpa5.exchange.snonce);}
static void begin(void){setup();memset(credential,0,sizeof credential);memcpy(credential,"SRWPA5\0\1",8);credential[8]=7;memcpy(credential+9,"Demers5",7);memcpy(credential+41,hs_pmk,32);credential_file.size=73;credential_file.mode=0600;}
static bool run(void){bool good=fake.data_mode?xhci_wifi_capture_data5():xhci_wifi_capture_wpa5();for(unsigned i=0;i<32;i++)assert(!wifi_wpa5.pmk[i]);assert(!wifi_wpa5_requested && !wifi_wpa5_data_requested && !wifi_join5_after);return good;}
int main(int argc,char **argv){
 begin();if((argc==2 || argc==11) && !strcmp(argv[1],"--trace")){
  if(argc==11){for(unsigned i=0;i<8;i++)fake.initial_rx[i]=strtoull(argv[i+2],0,0);memcpy(fake.output[9],fake.initial_rx,32);device.slot_id=strtoul(argv[10],0,0);}
  fake.auth_extra=true;fake.trace=true;assert(run());return 0;}assert(argc==1);
 assert(run() && wifi_wpa5.restored && wifi_wpa5.exchange.complete && wifi_wpa5.exchange.m3_mic && wifi_wpa5.exchange.keys_activated && wifi_wpa5.exchange.keys_erased && fake.sends==8 && fake.commands==12 && !fake.mac && !fake.quarantine);
 assert(fake.toggle_count==1 && wifi_wpa5.toggle_synced);
 unsigned controls=fake.controls,checks=fake.ownership,child_first=fake.child_first;assert(!run());
 begin();fake.toggle_lie=true;assert(!run() && wifi_wpa5.restored && !fake.quarantine && !wifi_wpa5.exchange.auth_matched && !wifi_wpa5.exchange.assoc_sent && wifi_wpa5.exchange.leave_sent && fake.sends==5);
 begin();fake.toggle_fail=true;assert(!run() && fake.quarantine && fake.commands==6 && fake.sends==3 && !wifi_wpa5.sends);
 begin();fake.toggle_actual=1;assert(!run() && fake.quarantine && fake.commands==6 && fake.sends==3);
 begin();fake.pad=true;fake.clear_disabled=true;assert(run());
 for(unsigned n=7;n<=12;n++)for(unsigned uncertain=0;uncertain<2;uncertain++){begin();fake.fail_command=n;fake.uncertain_command=uncertain;assert(!run() && fake.quarantine && fake.commands==n && !wifi_wpa5.restored);}
 for(unsigned n=4;n<=8;n++){begin();fake.fail_send=n;assert(!run() && fake.quarantine && !wifi_wpa5.restored && fake.sends==n);assert(wifi_wpa5.exchange.keys_erased);}
 // Include all parent and child control failures without repeating old sweeps.
 for(unsigned n=controls-52;n<=controls;n++){begin();fake.fail_control=n;assert(!run() && fake.quarantine && !wifi_wpa5.restored);}
 begin();fake.wpa_nonce_fail=true;assert(!run() && wifi_wpa5.restored && !fake.quarantine && fake.sends==3);
 begin();fake.wpa_m3_mutate=82;assert(!run() && wifi_wpa5.restored && !fake.quarantine && fake.sends==7 && !wifi_wpa5.exchange.keys_installed);
 begin();fake.wpa_no_m3=true;assert(!run() && wifi_wpa5.restored && !fake.quarantine && fake.sends==7);
 for(unsigned i=0;i<73;i++){begin();credential[i]^=1;if(i>=41){assert(!run() && wifi_wpa5.restored && !fake.quarantine && !wifi_wpa5.exchange.m3_mic);}else assert(!run() && fake.commands==6 && !wifi_wpa5.active);}
 for(unsigned n=54;n<=108;n++){
  begin();fake.fail_alloc=n;assert(!run() && fake.commands==6);
  begin();fake.alias_alloc=n;assert(!run() && fake.commands==6);
 }
 for(unsigned n=1;n<=9;n++){begin();fake.wpa_mutate=n;assert(!run() && fake.quarantine && !wifi_wpa5.restored);}
 begin();credential_file.mode=0644;assert(!run() && fake.commands==6);
 begin();credential_file.size=72;assert(!run() && fake.commands==6);
 for(unsigned n=child_first;n<=checks;n++){begin();fake.lose=n;assert(!run() && fake.quarantine && !wifi_wpa5.restored);}
 printf("xhci-wpa5: PASS independent child after real M1 parent,synthetic four-way,software key erasure,retained distinct DMA,12 commands and %u register controls,one guarded device toggle reset,duplicate-ACK/auth-timeout reproduction,padded RX,child command/control/send/ownership failures,credential/MIC/nonce/deadline refusal and restoration;mocked RF/AP/entropy,no physical data acceptance\n",controls);
 return 0;
}
