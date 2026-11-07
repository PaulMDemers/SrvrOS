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
#define XHCI_TRB_IOC (1u<<5)
#define XHCI_TRB_ISP (1u<<2)
struct xhci_trb{uint32_t parameter_low,parameter_high,status,control;};
struct xhci_device{uint8_t slot_id;bool present,removed,retiring,ep0_quarantined,control_pending,control_failed;uint16_t vendor_id,product_id,device_bcd;void *input_context,*output_context;uint64_t input_context_phys,output_context_phys,ep0_ring_phys,descriptor_buffer_phys;struct {bool pending;uint64_t buffer_phys;struct {uint64_t ring_phys;}ep;struct {bool pending;uint64_t buffer_phys;struct {uint64_t ring_phys;}response;}mcu;}firmware;};
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
static struct {uint32_t input[128][8],output[32][8],initial_rx[8];uint8_t pages[128][4096];unsigned next,allocations,fail_alloc,alias_alloc,commands,controls,ownership,lose,fail_command,command_mutate,fail_control,fail_send,reply,sends,rx_events,mutate,event_fault,stop_fault,stop_fault_dci;uint64_t time;uint32_t mac,filter,txop;bool quarantine,trace,parent,empty,empty_assoc,reject,reject_assoc,noise,bad_guard,auth_extra,auth_bad_ie,pad,skip_stop,alloc_fail,uncertain_command,wrong_slot,clear_disabled;}fake;
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
static bool timer_delay_us(unsigned us){fake.time+=us;return true;}
static void console_write(const char *s){if(fake.trace)fputs(s,stdout);}
static void console_printf(const char *s,...){if(!fake.trace)return;va_list ap;va_start(ap,s);for(;*s;s++){if(*s!='%'){putchar(*s);continue;}s++;uint64_t v=va_arg(ap,uint64_t);if(*s=='u')printf("%" PRIu64,v);else{assert(*s=='x');printf("0x%016" PRIx64,v);}}va_end(ap);}
static uint64_t ring_push(struct xhci_trb *r,uint64_t phys,uint64_t *enqueue,bool *cycle,const struct xhci_trb *td){assert(*enqueue<63 && *cycle);unsigned i=(*enqueue)++;r[i]=*td;r[i].control|=1;return phys+16*i;}
static void db_write32(unsigned slot,unsigned dci);
static bool poll_event(struct xhci_trb *e);
static void dispatch_transfer_event(const struct xhci_trb *e);
static bool submit_command_trb(const struct xhci_trb *t,unsigned type,uint64_t *slot);
static bool wifi_mac_private_usb(struct xhci_device *d,bool write,uint32_t a,uint32_t *v);
bool xhci_wifi_capture_tx5(void);
#include "../kernel/src/drivers/xhci_wifi_join5.inc"
static void emit_stop(unsigned dci){
 unsigned count=dci==8?wifi_join5.sends:wifi_join5.posts;uint64_t ptr=(dci==8?wifi_join5.tx_phys:wifi_join5.rx_phys)+16*count;
 bool pending=dci==9 && wifi_join5.rx_pending;unsigned cc=pending?26:27,residue=pending?4096:0;if(pending)ptr-=16;
 fake.output[dci][2]=ptr|1;fake.output[dci][3]=ptr>>32;
 struct xhci_trb e={ptr,ptr>>32,(cc<<24)|residue,(32u<<10)|(dci<<16)|((uint32_t)device.slot_id<<24)};
 if(fake.stop_fault && dci==fake.stop_fault_dci){switch(fake.stop_fault){case 1:e.parameter_low+=16;break;case 2:e.status=25u<<24;break;case 3:e.status|=8192;break;case 4:e.control|=4;break;case 5:e.control^=1u<<24;break;default:break;}}
 if(fake.stop_fault==7 && dci==fake.stop_fault_dci)fake.output[dci][2]^=16;
 if(!fake.skip_stop){assert(wifi_join5_event(&device,&e));if(fake.stop_fault==6 && dci==fake.stop_fault_dci)assert(wifi_join5_event(&device,&e));}
}
static bool submit_command_trb(const struct xhci_trb *t,unsigned type,uint64_t *slot){
 assert(firmware_command_exact && !firmware_command_failed && !fake.quarantine);fake.commands++;unsigned dci=t->control>>16&31;assert(t->control>>24==device.slot_id && !t->status);*slot=fake.wrong_slot?device.slot_id+1:device.slot_id;
 bool fail=fake.commands==fake.fail_command;if(fail && !fake.uncertain_command)return false;
 if(type==12){assert(!fake.input[1][3]);if(fake.commands==1){assert(fake.input[0][0]==0 && fake.input[0][1]==257);memcpy(fake.output[8],fake.input[9],32);fake.output[8][0]=1;}else{assert(fake.commands==6 && fake.input[0][0]==256 && fake.input[0][1]==1);if(fake.clear_disabled)memset(fake.output[8],0,32);else fake.output[8][0]=0;}}
 else if(type==16){assert(dci==9 && (fake.output[9][0]&7)==3);fake.output[9][2]=t->parameter_low;fake.output[9][3]=t->parameter_high;}
 else {assert(type==15 && (dci==8 || dci==9));fake.output[dci][0]=3;emit_stop(dci);}
 if(fake.commands==fake.command_mutate)fake.output[11][4]^=1;
 return !fail;
}
static void db_write32(unsigned slot,unsigned dci){assert(slot==device.slot_id && !fake.quarantine);if(dci==9){fake.output[9][0]=1;}else{assert(dci==8);fake.sends++;fake.reply=wifi_join5.kinds[wifi_join5.sends-1]+1;}}
static void put16(uint8_t *p,unsigned v){p[0]=v;p[1]=v>>8;}
static void put32(uint8_t *p,uint32_t v){put16(p,v);put16(p+2,v>>16);}
static unsigned response(void){
 unsigned i=wifi_join5.posts-1;uint8_t *p=wifi_join5.buffer[i];memset(p,0,4096);bool auth=fake.reply==1;size_t n=auth?(fake.auth_extra?41:30):40;uint8_t *h=p+36,*body=h+24+(fake.pad?2:0);
 put32(p+4,fake.pad?1u<<14:0);put32(p+8,(uint32_t)n<<16);put16(h,auth?0xb0:0x10);memcpy(h+4,wifi_join5.peer.peer.station,6);memcpy(h+10,wifi_join5.peer.peer.bssid,6);memcpy(h+16,wifi_join5.peer.peer.bssid,6);
 if(auth){put16(body+2,2);put16(body+4,fake.reject?13:0);if(fake.auth_extra){body[6]=221;body[7]=fake.auth_bad_ie?10:9;for(unsigned k=0;k<9;k++)body[8+k]=(uint8_t)(k+1);}}else{put16(body,0x411);put16(body+2,(fake.reject || fake.reject_assoc)?13:0);put16(body+4,0xc007);body[6]=1;body[7]=8;memcpy(body+8,(uint8_t[]){0x8c,18,0x98,36,0xb0,72,96,108},8);}
 unsigned dma=(32+n+(fake.pad?2:0)+3)&~3u;put16(p,dma);if(fake.noise)h[10]^=2;else fake.reply=0;return dma+4;
}
static bool poll_event(struct xhci_trb *e){
 if(wifi_join5.tx_pending && wifi_join5.sends!=fake.fail_send){uint64_t ptr=wifi_join5.tx_phys+16*(wifi_join5.sends-1);*e=(struct xhci_trb){ptr,ptr>>32,1u<<24,(32u<<10)|(8u<<16)|((uint32_t)device.slot_id<<24)};fake.output[8][2]=(ptr+16)|1;}
 else if(wifi_join5.rx_pending && fake.reply && fake.reply<=2 && !fake.empty && !(fake.empty_assoc && fake.reply==2)){uint64_t ptr=wifi_join5.rx_phys+16*(wifi_join5.posts-1);unsigned n=response();*e=(struct xhci_trb){ptr,ptr>>32,(13u<<24)|(4096-n),(32u<<10)|(9u<<16)|((uint32_t)device.slot_id<<24)};fake.output[9][2]=(ptr+16)|1;fake.rx_events++;}
 else return false;
 if(fake.event_fault){switch(fake.event_fault){case 1:e->parameter_low+=16;break;case 2:e->control|=4;break;case 3:e->status^=1;break;case 4:e->control^=1u<<24;break;default:e->control|=1u<<5;break;}}
 return true;
}
static void dispatch_transfer_event(const struct xhci_trb *e){assert(wifi_join5_event(&device,e));}
static bool wifi_mac_private_usb(struct xhci_device *d,bool write,uint32_t a,uint32_t *v){
 assert(d==&device && !fake.quarantine);fake.controls++;
 if(write){if(a==0x1004)fake.mac=*v;else if(a==0x1400)fake.filter=*v;else{assert(a==0x1340);fake.txop=*v;}}
 else {*v=a==0x1004?fake.mac:a==0x2000?0xf000f200:a==0x208?0x40000050:a==0x1400?fake.filter:a==0x1340?fake.txop:a==0x13b0?0x2f2f1313:a==0x2124?~0u:0;}
 if(fake.bad_guard && !write && a==0x1004 && fake.controls==1)*v=4;
 if(fake.controls==12){switch(fake.mutate){case 1:wifi_peer5.binding.generation++;break;case 2:fake.output[11][4]^=1;break;case 3:fake.input[5][0]^=1;break;case 4:wifi_cal5.plan.buffer[0]=fake.pages[90];break;case 5:wifi_join5.rx_phys+=4096;break;case 6:wifi_join5.rx[40].control=1;break;case 7:device.output_context_phys+=4096;break;case 8:wifi_join5.buffer[WIFI_JOIN5_RX][4000]=1;break;case 9:wifi_join5.buffer[47][4000]=1;break;case 10:((uint8_t *)wifi_cal5.plan.buffer[0])[0]=1;break;case 11:wifi_join5.pointers[2]++;break;case 12:device.slot_id++;break;default:break;}}
 return fake.controls!=fake.fail_control;
}
bool xhci_wifi_capture_tx5(void){return wifi_join5_inspect();}
static void setup(void){
 memset(&fake,0,sizeof fake);memset(&wifi_join5,0,sizeof wifi_join5);memset(&device,0,sizeof device);fake.next=30;fake.parent=true;fake.filter=0x17f97;fake.txop=0x583f;fake.time=1000;wifi_join5_requested=firmware_command_failed=firmware_command_exact=false;
 xhci=(typeof(xhci)){.command_ring_phys=9*4096};strict_control.device=0;
 device=(struct xhci_device){.slot_id=7,.present=true,.vendor_id=0x7392,.product_id=0xa711,.device_bcd=0x100,.input_context=fake.input,.output_context=fake.output,.input_context_phys=3*4096,.output_context_phys=4*4096,.ep0_ring_phys=4096,.descriptor_buffer_phys=2*4096};device.firmware.buffer_phys=5*4096;device.firmware.ep.ring_phys=6*4096;device.firmware.mcu.buffer_phys=7*4096;device.firmware.mcu.response.ring_phys=8*4096;
 wifi_cal5.d=&device;wifi_cal5.plan.ring=10*4096;wifi_cal5.ep.ring=fake.pages[10];for(unsigned i=0;i<8;i++){wifi_cal5.plan.physical[i]=(11+i)*4096;wifi_cal5.plan.buffer[i]=fake.pages[11+i];}
 wifi_handoff5.plan.new_ring=19*4096;wifi_handoff5.ring=fake.pages[19];wifi_tx5=(typeof(wifi_tx5)){.ring_phys=20*4096,.ring=fake.pages[20],.active=true,.input_restored=true,.disabled=true,.phase=7};
 wifi_peer5.binding=(struct mt76_peer5){.attempted=true,.complete=true,.generation=1,.observations=2,.rates=255,.peer={.station={2,1,2,3,4,5},.bssid={2,8,8,8,8,8},.ssid="Demers5",.ssid_size=7,.channel=161,.basic_rates=21}};wifi_peer5.binding.peer.rsn_size=mt76_rsn_station_ie(wifi_peer5.binding.peer.rsn,sizeof wifi_peer5.binding.peer.rsn);
 wifi_power5.cycle.chosen[0]=0x2f2f1313;fake.output[0][0]=16u<<27;fake.output[0][3]=(3u<<27)|7;memcpy(fake.output[9],(uint32_t[]){3,0x2000036,10*4096+129,0,4096,0,0,0},32);memcpy(fake.initial_rx,fake.output[9],32);for(unsigned i=0;i<128;i++)for(unsigned j=0;j<8;j++)fake.input[i][j]=i*8+j;
}
int main(int argc,char **argv){
 setup();if((argc==2 || argc==11) && !strcmp(argv[1],"--trace")){
  if(argc==11){for(unsigned i=0;i<8;i++)fake.initial_rx[i]=strtoull(argv[i+2],0,0);memcpy(fake.output[9],fake.initial_rx,32);device.slot_id=strtoul(argv[10],0,0);}
  fake.auth_extra=true;fake.trace=true;assert(xhci_wifi_capture_join5());return 0;
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
 puts("xhci-join5: PASS actual fresh live RX/OUT owner,matched AUTH/ASSOC/leave,padded RX,all command/control/ownership boundaries,send uncertainty,no replay,semantic rejection/timeout cleanup,frame/event/pin/parent mutation quarantine,input and exact old RX restoration;mocked RF parent;no physical or WPA2/IP acceptance");return 0;
}
