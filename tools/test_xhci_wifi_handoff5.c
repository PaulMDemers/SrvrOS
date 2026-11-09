/* Actual native empty-handoff child under a modeled already-held power/cal
 * parent. These fixtures do not accept real controller/firmware ownership. */
#include <srvros/mt76_handoff5.h>
#include <srvros/mt76_power5_cycle.h>
#include <srvros/mt76_rx_window.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <inttypes.h>
#define XHCI_TRB_COUNT 64
#define XHCI_TRB_TYPE_LINK 6
struct xhci_trb {uint32_t parameter_low,parameter_high,status,control;};
struct xhci_device {
 bool present,removed,retiring,ep0_quarantined,control_pending,control_failed;
 uint16_t vendor_id,product_id,device_bcd;uint8_t slot_id;
 uint64_t input_context_phys,output_context_phys,ep0_ring_phys,descriptor_buffer_phys;void *output_context;
 struct {bool pending;uint64_t buffer_phys;struct {uint64_t ring_phys;}ep;struct {bool pending;uint64_t buffer_phys;struct{uint64_t ring_phys;}response;}mcu;}firmware;
};
static struct {bool stopping,command_pending,command_failed;unsigned sync_users;uint64_t command_ring_phys;}xhci;
static bool firmware_command_failed,firmware_command_exact,wifi_handoff5_requested;
static struct {void *device;}strict_control;
static struct {struct xhci_device *d;struct {uint64_t ring_phys;struct xhci_trb *ring;}ep;struct mt76_rx_window_plan plan;uint64_t pin_ring,stop_dequeue,cleanup_physical[8];unsigned posts,cleanup_posts;bool cleanup_enabled;}wifi_cal5;
static struct {struct xhci_device *d;unsigned step;bool inspected;struct mt76_power5_plan plan;struct mt76_power5_cycle cycle;}wifi_power5;
static unsigned wifi_channel_vco5;
static struct {
 uint8_t pages[64][4096];unsigned next,calls,commands,fail,guard_bad,power_bad,mutate,bad_slot,corrupt_command,quarantines;
 bool parent,allocation_fail,uncertain,cleanup_held;uint32_t context[8];
}fake;
static struct xhci_device device;
static uint64_t physical_bias;
static bool trace;
static void *pmm_phys_to_virt(uint64_t p){assert(p>physical_bias && !(p&4095) && (p-physical_bias)/4096<64);return fake.pages[(p-physical_bias)/4096];}
static bool alloc_frame_zero(uint64_t *p,void **v){if(fake.allocation_fail)return false;assert(fake.next<64);*p=physical_bias+(uint64_t)fake.next++*4096;*v=pmm_phys_to_virt(*p);memset(*v,0,4096);return true;}
static void write_link_trb(struct xhci_trb *r,uint64_t p,bool c){r[XHCI_TRB_COUNT-1]=(struct xhci_trb){.parameter_low=p,.parameter_high=p>>32,.control=(6u<<10)|2u|(c?1u:0u)};}
static uint32_t context_read32(void *p,unsigned dci,unsigned word){assert(p==fake.context && dci==9 && word<8);return fake.context[word];}
static uint64_t event_irq_save(void){return 0;}
static void event_irq_restore(uint64_t f){assert(!f);}
static bool wifi_mcu_fail(struct xhci_device *d){assert(d==&device);d->ep0_quarantined=true;fake.quarantines++;return false;}
static void copy_bytes(uint8_t *d,const uint8_t *s,uint64_t n){memcpy(d,s,n);}
/* Native console varargs are uint64_t, including %u and %x. */
static void console_printf(const char *f,...){
 if(!trace)return;
 va_list ap;va_start(ap,f);
 for(;*f;f++){
  if(*f!='%'){putchar(*f);continue;}
  f++;uint64_t v=va_arg(ap,uint64_t);
  if(*f=='u')printf("%" PRIu64,v);
  else {assert(*f=='x');printf("0x%016" PRIx64,v);}
 }
 va_end(ap);
}
static void console_write(const char *f){if(trace)fputs(f,stdout);}
static bool wifi_cal5_cleanup_held(void){return !wifi_cal5.cleanup_enabled || fake.cleanup_held;}
static bool wifi_channel_vco5_owned(void *p){return p==&wifi_channel_vco5 && fake.parent;}
static bool wifi_power5_owned(void *p,bool cleanup){(void)cleanup;return p==&wifi_power5 && fake.parent && !device.ep0_quarantined && wifi_power5.step==84 && (fake.context[2]|((uint64_t)fake.context[3]<<32))==wifi_cal5.stop_dequeue;}
static void mutate(void);
static bool submit_command_trb(const struct xhci_trb *c,unsigned type,uint64_t *slot){
 assert(type==16 && firmware_command_exact && !device.ep0_quarantined && !firmware_command_failed);fake.calls++;fake.commands++;
 assert(c->control==((16u<<10)|(9u<<16)|((uint32_t)device.slot_id<<24)) && !c->status && fake.commands<=2);
 bool fail=fake.calls==fake.fail;
 if(!fail || fake.uncertain){fake.context[2]=c->parameter_low;fake.context[3]=c->parameter_high;}
 if(fake.commands==fake.corrupt_command)fake.context[4]^=1;
 *slot=device.slot_id+(fake.commands==fake.bad_slot);if(fake.commands==1)mutate();return !fail;
}
static bool wifi_mac_private_usb(struct xhci_device *d,bool write,uint32_t a,uint32_t *v);
bool xhci_wifi_capture_power5(void);
#include "../kernel/src/drivers/xhci_wifi_handoff5.inc"
static bool wifi_peer5_inspect(void){assert(!"unexpected peer child");return false;}
static void mutate(void){
 switch(fake.mutate){case 1:wifi_power5.step++;break;case 2:wifi_cal5.posts--;break;case 3:wifi_cal5.ep.ring[0].status^=1;break;case 4:((uint8_t *)wifi_cal5.plan.buffer[0])[0]^=1;break;case 5:xhci.command_ring_phys+=4096;break;case 6:device.slot_id++;break;case 7:device.input_context_phys+=4096;break;case 8:device.removed=true;break;case 9:device.vendor_id^=1;break;case 10:fake.parent=false;break;case 11:wifi_handoff5.plan.old_dequeue^=16;break;case 12:wifi_handoff5.plan.old_ring+=4096;break;case 13:wifi_handoff5.plan.protected_pages[0]=0x30000;break;case 14:wifi_handoff5.plan.new_ring+=4096;break;case 15:wifi_handoff5.plan.generation++;break;case 16:wifi_handoff5.new_physical+=4096;break;case 17:wifi_handoff5.new_pointer+=4096;break;case 18:wifi_cal5.cleanup_physical[0]^=4096;break;case 19:fake.cleanup_held=false;break;default:break;}
}
static bool wifi_mac_private_usb(struct xhci_device *d,bool write,uint32_t a,uint32_t *v){
 assert(d==&device && !write && !device.ep0_quarantined && wifi_handoff5.op==3);fake.calls++;
 unsigned i=wifi_handoff5.controls-1,g=i<3?i:i>=12?i-12:3;
 uint32_t address=g==0?0x1004:g==1?0x2000:g==2?0x208:wifi_power5.plan.write[i-3].address;
 assert(a==address);if(fake.calls==fake.fail)return false;
 *v=g==0?0:g==1?0xf000f200:g==2?0x40000050:wifi_power5.cycle.chosen[i-3];
 if((g<3 && i==fake.guard_bad) || (g==3 && i-3==fake.power_bad))*v^=1;
 return true;
}
bool xhci_wifi_capture_power5(void){return wifi_handoff5_inspect();}
static void allocate(uint64_t *p){void *v;assert(alloc_frame_zero(p,&v));}
static void setup(void){
 memset(&fake,0,sizeof fake);fake.next=1;fake.parent=true;fake.guard_bad=fake.power_bad=~0u;
 memset(&wifi_handoff5,0,sizeof wifi_handoff5);wifi_handoff5_requested=false;firmware_command_failed=firmware_command_exact=false;strict_control.device=NULL;
 memset(&device,0,sizeof device);device.present=true;device.vendor_id=0x7392;device.product_id=0xa711;device.device_bcd=0x100;device.slot_id=7;device.output_context=fake.context;
 memset(&xhci,0,sizeof xhci);allocate(&xhci.command_ring_phys);
 allocate(&device.input_context_phys);allocate(&device.output_context_phys);allocate(&device.ep0_ring_phys);allocate(&device.descriptor_buffer_phys);
 allocate(&device.firmware.buffer_phys);allocate(&device.firmware.ep.ring_phys);allocate(&device.firmware.mcu.buffer_phys);allocate(&device.firmware.mcu.response.ring_phys);
 memset(&wifi_cal5,0,sizeof wifi_cal5);wifi_cal5.d=&device;allocate(&wifi_cal5.plan.ring);wifi_cal5.ep.ring_phys=wifi_cal5.pin_ring=wifi_cal5.plan.ring;wifi_cal5.ep.ring=pmm_phys_to_virt(wifi_cal5.plan.ring);write_link_trb(wifi_cal5.ep.ring,wifi_cal5.plan.ring,true);wifi_cal5.posts=8;wifi_cal5.stop_dequeue=wifi_cal5.plan.ring+129;
 for(unsigned i=0;i<8;i++){allocate(&wifi_cal5.plan.physical[i]);void *buffer=pmm_phys_to_virt(wifi_cal5.plan.physical[i]);wifi_cal5.plan.buffer[i]=buffer;memset(buffer,i+1,4096);}
 fake.context[0]=3;fake.context[1]=(3u<<1)|(6u<<3)|(512u<<16);fake.context[2]=wifi_cal5.stop_dequeue;fake.context[3]=wifi_cal5.stop_dequeue>>32;fake.context[4]=4096;
 memset(&wifi_power5,0,sizeof wifi_power5);wifi_power5.d=&device;wifi_power5.step=84;wifi_power5.inspected=true;wifi_power5.plan.generation=1;wifi_power5.plan.valid=true;
 wifi_power5.cycle.selected=true;wifi_power5.cycle.verified_mask=wifi_power5.cycle.modified_mask=511;
 const uint32_t address[]={0x13b0,0x1314,0x1318,0x131c,0x1320,0x1324,0x13d4,0x13d8,0x13dc};
 const uint32_t value[]={0x2f2f1313,0x04050000,0x02020004,0x00003e02,0x02020000,0x00003e02,0x003e0000,0x3c3c0000,0x3e};
 for(unsigned i=0;i<9;i++){wifi_power5.plan.write[i].address=address[i];wifi_power5.cycle.chosen[i]=value[i];}
}
int main(int argc,char **argv){
 if(argc==5 && !strcmp(argv[1],"--trace")){
  char *end;physical_bias=strtoull(argv[2],&end,0);assert(!*end && !(physical_bias&4095));
  setup();wifi_cal5.stop_dequeue=strtoull(argv[3],&end,0);assert(!*end);
  fake.context[2]=wifi_cal5.stop_dequeue;fake.context[3]=wifi_cal5.stop_dequeue>>32;
  unsigned long slot=strtoul(argv[4],&end,0);assert(!*end && slot && slot<=255);device.slot_id=slot;
  trace=true;assert(xhci_wifi_capture_handoff5() && wifi_handoff5.result.complete);return 0;
 }
 assert(argc==1);
 physical_bias=UINT64_C(0x100000000);setup();
 assert(xhci_wifi_capture_handoff5() && wifi_handoff5.result.complete && fake.context[3]==1);
 physical_bias=0;
 setup();assert(xhci_wifi_capture_handoff5());assert(fake.calls==17 && fake.commands==2 && !fake.quarantines && wifi_handoff5.op==7 && wifi_handoff5.controls==15 && wifi_handoff5.result.complete && wifi_handoff5.result.restored && !wifi_handoff5.active);
 assert(!xhci_wifi_capture_handoff5() && !wifi_handoff5_inspect() && fake.calls==17);
 for(unsigned n=1;n<=17;n++)for(unsigned after=0;after<2;after++){
  setup();fake.fail=n;fake.uncertain=after;assert(!xhci_wifi_capture_handoff5() && fake.calls==n && fake.quarantines==1 && !wifi_handoff5.result.complete && !wifi_handoff5.active);
 }
 for(unsigned i=0;i<9;i++){
  setup();fake.power_bad=i;assert(!xhci_wifi_capture_handoff5() && fake.calls==17 && fake.commands==2 && !fake.quarantines && wifi_handoff5.result.restored && !wifi_handoff5.result.complete && !wifi_handoff5.result.unsafe);
  assert((fake.context[2]|((uint64_t)fake.context[3]<<32))==wifi_cal5.stop_dequeue);
 }
 const unsigned guards[]={0,1,2,12,13,14};
 for(unsigned n=0;n<6;n++){setup();fake.guard_bad=guards[n];assert(!xhci_wifi_capture_handoff5() && fake.quarantines==1 && fake.commands==1 && wifi_handoff5.result.unsafe);}
 for(unsigned n=1;n<=17;n++){setup();fake.mutate=n;assert(!xhci_wifi_capture_handoff5() && fake.calls==1 && fake.quarantines==1 && !wifi_handoff5.result.restored);}
 for(unsigned n=1;n<=2;n++){
  setup();fake.bad_slot=n;assert(!xhci_wifi_capture_handoff5() && fake.quarantines==1 && fake.commands==n && firmware_command_failed);
  setup();fake.corrupt_command=n;assert(!xhci_wifi_capture_handoff5() && fake.quarantines==1 && fake.commands==n);
 }
 for(unsigned n=0;n<10;n++){
  setup();
  if(n==0)fake.parent=false;
  if(n==1)wifi_power5.step=83;
  if(n==2)wifi_power5.inspected=false;
  if(n==3)wifi_power5.cycle.selected=false;
  if(n==4)wifi_power5.cycle.verified_mask=510;
  if(n==5)wifi_cal5.posts=7;
  if(n==6)wifi_cal5.pin_ring^=4096;
  if(n==7)fake.context[0]=1;
  if(n==8)fake.allocation_fail=true;
  if(n==9)device.firmware.mcu.buffer_phys=device.firmware.buffer_phys;
  assert(!xhci_wifi_capture_handoff5() && !fake.calls && !wifi_handoff5.active && !fake.quarantines);
 }
 for(unsigned n=0;n<4;n++){
  setup();wifi_cal5.cleanup_enabled=true;wifi_cal5.cleanup_posts=6;fake.cleanup_held=true;
  for(unsigned i=0;i<8;i++)allocate(&wifi_cal5.cleanup_physical[i]);
  wifi_cal5.stop_dequeue=wifi_cal5.plan.ring+16*14+1;fake.context[2]=wifi_cal5.stop_dequeue;fake.context[3]=wifi_cal5.stop_dequeue>>32;
  if(n==1)fake.mutate=18;
  if(n==2)fake.mutate=19;
  if(n==3)wifi_cal5.stop_dequeue=wifi_cal5.plan.ring+129;
  bool ok=xhci_wifi_capture_handoff5();assert(ok==(n==0));
  if(ok)assert(wifi_handoff5.plan.old_posts==8 && wifi_handoff5.plan.cleanup_posts==6 && wifi_handoff5.plan.protected_count==25);
 }
 puts("xhci-handoff5: PASS actual empty native child;17 before/after uncertain I/O positions,9 semantic selected-power mismatches with exact context restoration,6 guard failures,17 frozen-parent/identity/page/plan mutations,command slot/context corruption,admission/allocation/alias/replay refusal;no posted TD/MAC start/network OUT;modeled parent only,no native acceptance");return 0;
}
