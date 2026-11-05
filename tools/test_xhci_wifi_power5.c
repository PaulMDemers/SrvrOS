/* The actual native power child under a modeled already-held parent. These
 * fixtures do not accept physical controller ownership or firmware behavior. */
#include <srvros/mt76_discovery5.h>
#include <srvros/mt76_power5_cycle.h>
#include <srvros/mt76_calibration.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
struct xhci_device {
 struct mt76_identity wifi_identity;
 struct {bool pending;struct {bool pending;struct mt76_ant_fields ant_fields;} mcu;} firmware;
 bool quarantined;
};
static struct {uint64_t sync_users;} xhci;
static struct {struct xhci_device *d;struct mt76_rx_window window;struct mt76_discovery5_choice choice;struct mt76_calibration cal;} wifi_cal5;
static bool wifi_discovery5_requested;
static struct {
 unsigned calls,owners,fail,deny,bad,busy;
 bool uncertain,lost,injected;
 uint32_t control,words[9],original[9];uint8_t logical[512];
 char log[65536];size_t used;
} fake;
static const uint32_t addresses[]={0x13b0,0x1314,0x1318,0x131c,0x1320,0x1324,0x13d4,0x13d8,0x13dc};
static bool wifi_discovery5_owned(void *c,const struct mt76_rx_window *w){
 assert(c==&wifi_cal5 && w==&wifi_cal5.window);fake.owners++;
 if(fake.deny && fake.owners>=fake.deny)fake.lost=true;
 return !fake.lost && !wifi_cal5.d->quarantined;
}
static bool wifi_mcu_fail(struct xhci_device *d){d->quarantined=true;return false;}
static bool timer_delay_us(uint32_t us){assert(us==2 || us==1000);return ++fake.calls!=fake.fail;}
static void console_printf(const char *fmt,...){
 /* Kernel %u/%x consume uint64_t. Avoid a host va_arg format mismatch by
  * recording only format tags;production formatting is checked by the log
  * parser and exact-image VM composition. Never print modeled identifiers. */
 size_t n=strcspn(fmt,":");if(fake.used+n+2<sizeof fake.log){memcpy(fake.log+fake.used,fmt,n);fake.used+=n;fake.log[fake.used++]='\n';fake.log[fake.used]=0;}
}
static void console_write(const char *s){(void)s;}
static bool wifi_mac_private_usb(struct xhci_device *,bool,uint32_t,uint32_t *);
bool xhci_wifi_capture_discovery5(void);
#include "../kernel/src/drivers/xhci_wifi_power5.inc"
static bool wifi_mac_private_usb(struct xhci_device *d,bool write,uint32_t a,uint32_t *v){
 assert(d==wifi_cal5.d && !d->quarantined && !fake.lost && !xhci.sync_users);
 bool fail=++fake.calls==fake.fail;
 if(write){
  if(a==0x24){unsigned b=(*v>>16)&0x3ff;assert(b==0 || b==0x30 || b==0x90 || b==0xd0 || b==0x120);assert((*v&(3u<<6))==0 && (*v&(1u<<30)));if(!fail || fake.uncertain)fake.control=*v&~(1u<<30);}
  else {unsigned i=0;while(i<9 && addresses[i]!=a)i++;assert(i<9 && wifi_power5.cycle.snapshot_complete);if(!fail || fake.uncertain)fake.words[i]=*v;}
 }else if(!fail){
  if(a==0x1004)*v=0;
  else if(a==0x2000)*v=0xf000f200;
  else if(a==0x208)*v=0x40000050;
  else if(a==0x24){*v=fake.control;if(wifi_power5.fields_active && wifi_power5.map.phase==5 && fake.busy){*v|=1u<<30;if(fake.busy!=~0u)fake.busy--;}}
  else if(a>=0x28 && a<=0x34){unsigned n=((fake.control>>16)&0x3ff)+a-0x28;assert(n+4<=512);*v=fake.logical[n]|(uint32_t)fake.logical[n+1]<<8|(uint32_t)fake.logical[n+2]<<16|(uint32_t)fake.logical[n+3]<<24;}
  else {unsigned i=0;while(i<9 && addresses[i]!=a)i++;assert(i<9);*v=fake.words[i];}
  if(fake.calls==fake.bad && (a==0x1004 || a==0x2000 || a==0x208 || wifi_power5.cycle.snapshot_complete)){*v^=1;fake.injected=true;}
 }
 return !fail;
}
bool xhci_wifi_capture_discovery5(void){wifi_discovery5_requested=true;bool ok=wifi_power5_inspect();wifi_discovery5_requested=false;return ok;}
static struct xhci_device device;
static void put16(unsigned offset,uint16_t value){fake.logical[offset]=value;fake.logical[offset+1]=value>>8;}
static void setup(void){
 /* Test-only independent device lifetimes. Production never resets this owner. */
 wifi_power5=(typeof(wifi_power5)){0};wifi_power5_requested=wifi_discovery5_requested=false;fake=(typeof(fake)){.control=0x80000000};device=(struct xhci_device){.wifi_identity={.asic=0x76100002,.mac=0x76502000}};
 device.firmware.mcu.ant_fields.nic0=0xfd11;device.firmware.mcu.ant_fields.nic1=0x1004;
 wifi_cal5=(typeof(wifi_cal5)){.d=&device,.cal={.saved={0x2f2f1b1b,0xffffffff}},.choice={.attempted=true,.selected=true,.generation=1,.matches=1,.row={.channel=161}}};xhci.sync_users=0;
 const unsigned offsets[]={0,0x34,0x36,0x98,0xde,0x120,0x122,0x124,0x126,0x12c};
 const uint16_t values[]={0x7610,0xfd11,0x1004,19u<<8,0x0201,0x0405,4,0x0607,0x0203,0x0809};
 for(unsigned i=0;i<10;i++)put16(offsets[i],values[i]);
 memset(fake.logical+4,0xa5,6);
 for(unsigned i=0;i<9;i++)fake.words[i]=fake.original[i]=i?0x02020202:0x2f2f1b1b;
}
int main(void){
 setup();assert(xhci_wifi_capture_power5());unsigned calls=fake.calls,owners=fake.owners;assert(calls==202 && wifi_power5.step==132 && wifi_power5.cycle.complete && wifi_power5.cycle.restored_mask==511 && !wifi_power5.active && !wifi_power5_requested);
 assert(!memcmp(fake.words,fake.original,sizeof fake.words));assert(strstr(fake.log,"wifi-power5-fields") && strstr(fake.log,"wifi-power5-usb") && strstr(fake.log,"wifi-power5\n"));
 assert(!xhci_wifi_capture_power5() && calls==fake.calls);assert(!wifi_power5_inspect() && calls==fake.calls);
 for(unsigned n=1;n<=calls;n++)for(unsigned uncertain=0;uncertain<2;uncertain++){
  setup();fake.fail=n;fake.uncertain=uncertain;assert(!xhci_wifi_capture_power5());assert(fake.calls==n && !wifi_power5.active && !wifi_power5_requested && !wifi_power5.cycle.complete);
 }
 for(unsigned n=1;n<=owners;n++){
  setup();fake.deny=n;assert(!xhci_wifi_capture_power5());assert(!wifi_power5.active && (!wifi_power5.cycle.complete || wifi_power5.step==132));
  if(wifi_power5.cycle.complete)assert(!memcmp(fake.words,fake.original,sizeof fake.words));
 }
 for(unsigned n=1;n<=calls;n++){
  setup();fake.bad=n;bool ok=xhci_wifi_capture_power5();if(fake.injected)assert(!ok && device.quarantined && !wifi_power5.cycle.complete);
 }
 for(unsigned variant=0;variant<9;variant++){
  setup();
  if(variant==0)xhci.sync_users=1;
  if(variant==1)device.firmware.pending=true;
  if(variant==2)device.firmware.mcu.pending=true;
  if(variant==3)wifi_cal5.choice.selected=false;
  if(variant==4)wifi_cal5.choice.generation=2;
  if(variant==5)wifi_cal5.choice.row.channel=1;
  if(variant==6)wifi_cal5.choice.matches=2;
  if(variant==7)device.quarantined=true;
  if(variant==8)wifi_cal5.d=NULL;
  assert(!xhci_wifi_capture_power5() && !fake.calls);
 }
 setup();device.firmware.mcu.ant_fields.nic0^=1;assert(!xhci_wifi_capture_power5() && fake.calls==70 && !wifi_power5.cycle.attempted);
 setup();fake.busy=3;assert(xhci_wifi_capture_power5() && fake.calls==208 && wifi_power5.fields.polls==8 && wifi_power5.map.polls==8);
 setup();fake.busy=~0u;assert(!xhci_wifi_capture_power5() && fake.calls==2053 && wifi_power5.fields.polls==1024 && !wifi_power5.cycle.attempted);
 setup();wifi_cal5.cal.saved[0]^=1;assert(!xhci_wifi_capture_power5() && fake.calls==74 && !wifi_power5.cycle.modified_mask && device.quarantined);
 printf("xhci-power5: PASS actual native child;202 before/after-uncertain I/O faults;%u ownership boundaries;guard/readback/quarantine/replay/parent-generation/USB-busy and1024-poll refusals;modeled parent only,no hardware acceptance\n",owners);
 return 0;
}
