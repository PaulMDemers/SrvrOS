/* Actual read-only native peer child;held-parent ownership is modeled. */
#include <srvros/mt76_peer5.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
struct xhci_device {struct {struct {bool mac_address_validate_mode;struct {bool complete;}mac_address;struct {bool failed;}mac_address_gate;}mcu;}firmware;};
static struct xhci_device device;
static struct {struct xhci_device *d;struct mt76_rx_window window;struct mt76_discovery5_choice choice;struct mt76_discovery discovery;}wifi_cal5;
static struct {unsigned op;struct {bool complete,restored,unsafe;}result;}wifi_handoff5;
static int wifi_power5;
static bool wifi_peer5_requested;
static struct {unsigned reads,fail,bad,owned,lose;bool parent,quarantined,replay;uint32_t low,high;}fake;
static bool wifi_handoff5_owned(void *p,bool cleanup){(void)cleanup;assert(p==&wifi_handoff5);fake.owned++;return fake.parent && !fake.quarantined && fake.owned!=fake.lose;}
static bool wifi_power5_owned(void *p,bool cleanup){(void)cleanup;assert(p==&wifi_power5);return fake.parent && !fake.quarantined;}
static bool wifi_mcu_fail(struct xhci_device *d){assert(d==&device);fake.quarantined=true;return false;}
static bool trace;
static void console_write(const char *s){if(trace)fputs(s,stdout);}
static void console_printf(const char *s,...){
 if(!trace)return;
 va_list ap;va_start(ap,s);
 for(;*s;s++){
  if(*s!='%'){putchar(*s);continue;}
  s++;uint64_t v=va_arg(ap,uint64_t);
  if(*s=='u')printf("%" PRIu64,v);
  else {assert(*s=='x');printf("0x%016" PRIx64,v);}
 }
 va_end(ap);
}
static bool wifi_mac_private_usb(struct xhci_device *d,bool write,uint32_t a,uint32_t *v){
 assert(d==&device && !write && !fake.quarantined);unsigned i=fake.reads++;
 const uint32_t address[]={0x1004,0x2000,0x208,0x1008,0x100c,0x1008,0x100c,0x1004,0x2000,0x208};assert(i<10 && a==address[i]);
 *v=a==0x1004?0:a==0x2000?0xf000f200:a==0x208?0x40000050:a==0x1008?fake.low:fake.high;
 if(i==fake.bad)*v^=1;
 return i!=fake.fail;
}
bool xhci_wifi_capture_handoff5(void);
#include "../kernel/src/drivers/xhci_wifi_peer5.inc"
enum mt76_peer5_status mt76_peer5_bind(struct mt76_peer5 *out,const uint8_t station[6],const struct mt76_discovery5_choice *choice,const struct mt76_discovery *cache,const struct mt76_rx_window *w,const struct mt76_discovery_io *io){
 assert(choice==&wifi_cal5.choice && cache==&wifi_cal5.discovery && w==&wifi_cal5.window && io->context==&wifi_peer5);
 assert(station[0]==2 && station[1]==1 && station[2]==2 && station[3]==3 && station[4]==4 && station[5]==5);
 if(!io->owned(io->context,w))return MT76_PEER5_OWNERSHIP;
 *out=(struct mt76_peer5){.attempted=true,.complete=true,.generation=1,.observations=3,.rates=255,.peer={.station={2,1,2,3,4,5},.bssid={2,7,7,7,7,7},.ssid="Demers5",.ssid_size=7,.channel=161,.basic_rates=21}};
 out->peer.rsn_size=mt76_rsn_station_ie(out->peer.rsn,sizeof out->peer.rsn);
 return MT76_PEER5_OK;
}
bool xhci_wifi_capture_handoff5(void){return wifi_peer5_inspect();}
static void setup(void){
 memset(&wifi_peer5,0,sizeof wifi_peer5);memset(&fake,0,sizeof fake);fake.parent=true;fake.fail=fake.bad=~0u;fake.low=0x03020102;fake.high=0x0504;wifi_peer5_requested=false;
 memset(&device,0,sizeof device);device.firmware.mcu.mac_address_validate_mode=device.firmware.mcu.mac_address.complete=true;wifi_cal5.d=&device;
 wifi_handoff5.op=7;wifi_handoff5.result.complete=wifi_handoff5.result.restored=true;wifi_handoff5.result.unsafe=false;
}
int main(int argc,char **argv){
 if(argc==2 && !strcmp(argv[1],"--trace")){setup();trace=true;assert(xhci_wifi_capture_peer5());return 0;}
 assert(argc==1);
 setup();assert(xhci_wifi_capture_peer5() && wifi_peer5.binding.complete && fake.reads==10 && !fake.quarantined && !wifi_peer5.active);unsigned checks=fake.owned;
 struct mt76_peer5 saved=wifi_peer5.binding;assert(!xhci_wifi_capture_peer5() && !memcmp(&saved,&wifi_peer5.binding,sizeof saved));
 for(unsigned i=0;i<10;i++){
  setup();fake.fail=i;assert(!xhci_wifi_capture_peer5() && fake.quarantined && fake.reads==i+1 && !wifi_peer5.binding.complete);
  setup();fake.bad=i;assert(!xhci_wifi_capture_peer5() && !wifi_peer5.binding.complete);assert(fake.quarantined==(i<3 || i>=7));
 }
 for(unsigned i=1;i<=checks;i++){setup();fake.lose=i;assert(!xhci_wifi_capture_peer5() && !wifi_peer5.binding.complete && !wifi_peer5.active);}
 for(unsigned i=0;i<7;i++){
  setup();if(i==0)fake.parent=false;if(i==1)wifi_handoff5.op=6;if(i==2)wifi_handoff5.result.complete=false;if(i==3)wifi_handoff5.result.restored=false;if(i==4)wifi_handoff5.result.unsafe=true;if(i==5)device.firmware.mcu.mac_address_validate_mode=false;if(i==6)device.firmware.mcu.mac_address_gate.failed=true;
  assert(!xhci_wifi_capture_peer5() && !fake.reads && !fake.quarantined);
 }
 setup();fake.high|=0xff0000;assert(!xhci_wifi_capture_peer5() && fake.reads==10 && !fake.quarantined && !wifi_peer5.binding.complete);
 puts("xhci-peer5: PASS actual read-only native child,10 transport/guard/address fault boundaries,all ownership boundaries,stable captured address controls,exact parent/replay refusal,private copied codec binding;no TD/MAC/network OUT;modeled parent only");return 0;
}
