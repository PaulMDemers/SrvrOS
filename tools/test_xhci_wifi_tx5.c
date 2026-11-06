#include <srvros/mt76_peer5.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#define XHCI_TRB_COUNT 64
#define XHCI_TRB_TYPE_LINK 6
#define XHCI_TRB_TYPE_CONFIGURE_ENDPOINT 12
struct xhci_trb{uint32_t parameter_low,parameter_high,status,control;};
struct xhci_device{uint8_t slot_id;void *input_context,*output_context;uint64_t input_context_phys,output_context_phys,ep0_ring_phys,descriptor_buffer_phys;struct {uint64_t buffer_phys;struct {uint64_t ring_phys;}ep;struct {uint64_t buffer_phys;struct {uint64_t ring_phys;}response;}mcu;}firmware;};
static struct {uint64_t command_ring_phys;}xhci;
static struct {struct xhci_device *d;struct {uint64_t ring,physical[8];}plan;int window;}wifi_cal5;
static struct {struct {uint64_t new_ring;}plan;}wifi_handoff5;
static struct {struct mt76_peer5 binding;}wifi_peer5;
static struct {struct {struct {uint32_t address;}write[9];}plan;struct {uint32_t chosen[9];}cycle;}wifi_power5;
static struct xhci_device device;
static bool wifi_tx5_requested,firmware_command_failed,firmware_command_exact,trace;
static struct {uint32_t input[128][8],output[32][8];uint8_t pages[64][4096];unsigned next,commands,reads,owned,lose,fail_command,fail_read,bad_read,mutate;bool parent,quarantine,uncertain,clear_disabled,allocate_fail,skip_stop_event;unsigned stop_fault;}fake;
static bool wifi_handoff5_equal(const void *a,const void *b,size_t n){return !memcmp(a,b,n);}
static bool wifi_peer5_owned(void *p,void *w){assert(p==&wifi_peer5 && w==&wifi_cal5.window);return ++fake.owned!=fake.lose && fake.parent && !fake.quarantine;}
static bool wifi_mcu_fail(struct xhci_device *d){assert(d==&device);fake.quarantine=true;return false;}
static void *pmm_phys_to_virt(uint64_t p){assert(p && !(p&4095) && p/4096<64);return fake.pages[p/4096];}
static bool alloc_frame_zero(uint64_t *p,void **v){if(fake.allocate_fail)return false;*p=4096*fake.next++;*v=pmm_phys_to_virt(*p);memset(*v,0,4096);return true;}
static void write_link_trb(struct xhci_trb *r,uint64_t p,bool c){r[63]=(struct xhci_trb){p,p>>32,0,(6u<<10)|2u|(c?1u:0u)};}
static void zero_bytes(void *p,uint64_t n){memset(p,0,n);}
static void copy_bytes(uint8_t *d,const uint8_t *s,uint64_t n){memcpy(d,s,n);}
static uint32_t context_read32(void *p,unsigned i,unsigned w){return ((uint32_t (*)[8])p)[i][w];}
static void context_write32(void *p,unsigned i,unsigned w,uint32_t v){((uint32_t (*)[8])p)[i][w]=v;}
static uint64_t event_irq_save(void){return 0;}
static void event_irq_restore(uint64_t f){assert(!f);}
static void console_write(const char *s){if(trace)fputs(s,stdout);}
static void console_printf(const char *s,...){if(!trace)return;va_list ap;va_start(ap,s);for(;*s;s++){if(*s!='%'){putchar(*s);continue;}s++;uint64_t v=va_arg(ap,uint64_t);if(*s=='u')printf("%" PRIu64,v);else{assert(*s=='x');printf("0x%016" PRIx64,v);}}va_end(ap);}
static void force_stop_event(void);
static bool submit_command_trb(const struct xhci_trb *t,unsigned type,uint64_t *slot){
 assert(firmware_command_exact && !firmware_command_failed && !fake.quarantine);fake.commands++;assert(fake.commands<=3 && t->control>>24==7 && !t->status);
 assert(type==(fake.commands==2?15:12) && (t->control>>10&63)==type);
 if(type==15)assert((t->control>>16&31)==8 && !t->parameter_low && !t->parameter_high);
 else{assert(t->parameter_low==device.input_context_phys && !t->parameter_high);assert(!fake.input[1][3]);assert(fake.input[0][0]==(fake.commands==3?1u<<8:0) && fake.input[0][1]==(fake.commands==1?1u|(1u<<8):1));}
 bool fail=fake.commands==fake.fail_command;
 if(!fail || fake.uncertain){
  if(fake.commands==1){memcpy(fake.output[8],fake.input[9],32);fake.output[8][0]=1;}
  if(fake.commands==2){fake.output[8][0]=3;if(!fake.skip_stop_event)force_stop_event();}
  if(fake.commands==3){if(fake.clear_disabled)memset(fake.output[8],0,32);else fake.output[8][0]=0;}
 }
 *slot=7;return !fail;
}
static bool wifi_mac_private_usb(struct xhci_device *,bool,uint32_t,uint32_t *);
bool xhci_wifi_capture_peer5(void);
#include "../kernel/src/drivers/xhci_wifi_tx5.inc"
static void force_stop_event(void){
 struct xhci_trb e={.parameter_low=(uint32_t)wifi_tx5.ring_phys,.parameter_high=wifi_tx5.ring_phys>>32,.status=27u<<24,.control=(32u<<10)|(8u<<16)|(7u<<24)};
 if(fake.stop_fault==1)e.parameter_low+=16;
 if(fake.stop_fault==2)e.status=26u<<24;
 if(fake.stop_fault==3)e.status|=1;
 if(fake.stop_fault==4)e.control|=4;
 if(fake.stop_fault==5)e.control^=1u<<24;
 if(fake.stop_fault==6)e.control|=1u<<5;
 assert(wifi_tx5_event(&device,&e));
 if(fake.stop_fault==7)assert(wifi_tx5_event(&device,&e));
}
static bool wifi_mac_private_usb(struct xhci_device *d,bool write,uint32_t a,uint32_t *v){
 assert(d==&device && !write && !fake.quarantine);unsigned i=fake.reads++,g=i<3?i:i>=12?i-12:3;assert(i<15);
 assert(a==(g==0?0x1004:g==1?0x2000:g==2?0x208:wifi_power5.plan.write[i-3].address));
 *v=g==0?0:g==1?0xf000f200:g==2?0x40000050:wifi_power5.cycle.chosen[i-3];if(i==fake.bad_read)*v^=1;
 if(i==0){switch(fake.mutate){case 1:wifi_peer5.binding.generation++;break;case 2:fake.input[5][0]^=1;break;case 3:fake.output[9][2]^=1;break;case 4:wifi_tx5.ring[0].control=1;break;case 5:wifi_tx5.ring_phys+=4096;break;case 6:fake.output[8][4]^=1;break;case 7:fake.output[16][2]^=1;break;case 8:fake.parent=false;break;default:break;}}
 return i!=fake.fail_read;
}
bool xhci_wifi_capture_peer5(void){return wifi_tx5_inspect();}
static void setup(void){
 memset(&fake,0,sizeof fake);memset(&wifi_tx5,0,sizeof wifi_tx5);fake.next=30;fake.parent=true;fake.fail_read=fake.bad_read=~0u;wifi_tx5_requested=firmware_command_failed=firmware_command_exact=false;
 memset(&device,0,sizeof device);device.slot_id=7;device.input_context=fake.input;device.output_context=fake.output;device.input_context_phys=3*4096;device.output_context_phys=4*4096;device.ep0_ring_phys=4096;device.descriptor_buffer_phys=2*4096;
 device.firmware.buffer_phys=5*4096;device.firmware.ep.ring_phys=6*4096;device.firmware.mcu.buffer_phys=7*4096;device.firmware.mcu.response.ring_phys=8*4096;xhci.command_ring_phys=9*4096;
 wifi_cal5.d=&device;wifi_cal5.plan.ring=10*4096;for(unsigned i=0;i<8;i++)wifi_cal5.plan.physical[i]=(11+i)*4096;wifi_handoff5.plan.new_ring=19*4096;
 wifi_peer5.binding=(struct mt76_peer5){.attempted=true,.complete=true,.generation=1};
 fake.output[0][0]=16u<<27;fake.output[0][3]=(3u<<27)|4;for(unsigned i=0;i<128;i++)for(unsigned j=0;j<8;j++)fake.input[i][j]=i*8+j;
 const uint32_t a[]={0x13b0,0x1314,0x1318,0x131c,0x1320,0x1324,0x13d4,0x13d8,0x13dc},v[]={0x2f2f1313,0x04050000,0x02020004,0x3e02,0x02020000,0x3e02,0x003e0000,0x3c3c0000,0x3e};for(unsigned i=0;i<9;i++){wifi_power5.plan.write[i].address=a[i];wifi_power5.cycle.chosen[i]=v[i];}
}
int main(int argc,char **argv){
 if(argc==2 && !strcmp(argv[1],"--trace")){setup();trace=true;assert(xhci_wifi_capture_tx5());return 0;}assert(argc==1);
 setup();uint8_t input[4096];memcpy(input,fake.input,4096);assert(xhci_wifi_capture_tx5() && fake.commands==3 && fake.reads==15 && wifi_tx5.disabled && wifi_tx5.input_restored && !fake.quarantine && !memcmp(input,fake.input,4096));unsigned checks=fake.owned;
 assert(!xhci_wifi_capture_tx5() && fake.commands==3);
 setup();fake.clear_disabled=true;assert(xhci_wifi_capture_tx5());
 for(unsigned i=1;i<=3;i++)for(unsigned after=0;after<2;after++){setup();fake.fail_command=i;fake.uncertain=after;assert(!xhci_wifi_capture_tx5() && fake.commands==i && fake.quarantine && !wifi_tx5.input_restored);}
 for(unsigned i=0;i<15;i++){
  setup();fake.fail_read=i;assert(!xhci_wifi_capture_tx5() && fake.quarantine && fake.commands==1 && fake.reads==i+1);
  setup();fake.bad_read=i;assert(!xhci_wifi_capture_tx5());if(i>=3 && i<12)assert(!fake.quarantine && wifi_tx5.input_restored && fake.commands==3);else assert(fake.quarantine && fake.commands==1);
 }
 for(unsigned i=1;i<=checks;i++){setup();fake.lose=i;assert(!xhci_wifi_capture_tx5());}
 for(unsigned i=1;i<=8;i++){setup();fake.mutate=i;assert(!xhci_wifi_capture_tx5() && fake.quarantine && fake.commands==1);}
 for(unsigned i=1;i<=7;i++){setup();fake.stop_fault=i;assert(!xhci_wifi_capture_tx5() && fake.quarantine && fake.commands==2 && !wifi_tx5.input_restored);}
 setup();fake.skip_stop_event=true;assert(!xhci_wifi_capture_tx5() && fake.commands==2 && fake.quarantine);
 setup();fake.allocate_fail=true;assert(!xhci_wifi_capture_tx5() && !fake.commands);
 setup();fake.parent=false;assert(!xhci_wifi_capture_tx5() && !fake.commands);
 setup();wifi_tx5.d=&device;wifi_tx5.active=true;struct xhci_trb event={.control=8u<<16};assert(wifi_tx5_event(&device,&event) && fake.quarantine);
 puts("xhci-tx5: PASS actual empty OUT04 owner,configure/stop/disable and input restoration,three uncertain command boundaries,15 read faults,semantic cleanup,all ownership boundaries,parent/input/endpoint/ring mutations,unexpected events,allocation and replay refusal;zero TD/doorbell/network OUT;modeled parent only");return 0;
}
