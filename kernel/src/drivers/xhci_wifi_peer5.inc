#include <srvros/mt76_peer5.h>
/* Current-address/read-only peer binding. The accepted empty handoff has
 * already restored DCI9. No TD,MAC start,network OUT or key is published. */
static struct {
 struct mt76_peer5 binding;
 unsigned controls;
 bool attempted,active,failed;
} wifi_peer5;
static bool wifi_peer5_owned(void *c,const struct mt76_rx_window *w){
 struct xhci_device *d=wifi_cal5.d;
 return c==&wifi_peer5 && w==&wifi_cal5.window && wifi_peer5_requested && wifi_peer5.active && !wifi_peer5.failed &&
 wifi_handoff5.op==7 && wifi_handoff5.result.complete && wifi_handoff5.result.restored && !wifi_handoff5.result.unsafe &&
 wifi_handoff5_owned(&wifi_handoff5,false) && wifi_power5_owned(&wifi_power5,false) && d &&
 d->firmware.mcu.mac_address_validate_mode && d->firmware.mcu.mac_address.complete && !d->firmware.mcu.mac_address_gate.failed;
}
static bool wifi_peer5_inspect(void){
 if(!wifi_peer5_requested || wifi_peer5.attempted)return false;
 wifi_peer5.attempted=wifi_peer5.active=true;
 if(!wifi_peer5_owned(&wifi_peer5,&wifi_cal5.window)){wifi_peer5.active=false;return false;}
 console_write("wifi-peer5-progress: private current station reads;rebuild retained target and rate binding;no TD/MAC start/network OUT\n");
 uint32_t word[4]={0};bool ok=true;
 for(unsigned i=0;i<10;i++){
  unsigned g=i<3?i:i>=7?i-7:3;uint32_t a=g==0?0x1004:g==1?0x2000:g==2?0x208:(i&1?0x1008:0x100c),v=0;
  uint32_t expected=g==0?0:g==1?0xf000f200:0x40000050;
  bool owned=wifi_peer5_owned(&wifi_peer5,&wifi_cal5.window);
  bool read=owned && wifi_mac_private_usb(wifi_cal5.d,false,a,&v);wifi_peer5.controls++;
  bool good=read && wifi_peer5_owned(&wifi_peer5,&wifi_cal5.window) && (g==3 || v==expected);
  console_printf("wifi-peer5-usb: op=%u address=%x value=%x private=%u ok=%u\n",(uint64_t)i,(uint64_t)a,(uint64_t)(g==3?0:v),(uint64_t)(g==3),(uint64_t)good);
  if(!good){wifi_peer5.failed=true;wifi_mcu_fail(wifi_cal5.d);ok=false;break;}
  if(g==3)word[i-3]=v;
 }
 uint8_t station[6]={0};
 bool stable=ok && word[0]==word[2] && word[1]==word[3] && !(word[1]&0xffff0000u);
 if(stable)for(unsigned i=0;i<6;i++)station[i]=(uint8_t)((i<4?word[0]:word[1])>>(8*(i<4?i:i-4)));
 const struct mt76_discovery_io io={&wifi_peer5,wifi_peer5_owned};
 enum mt76_peer5_status status=MT76_PEER5_INVALID;
 if(stable)status=mt76_peer5_bind(&wifi_peer5.binding,station,&wifi_cal5.choice,&wifi_cal5.discovery,&wifi_cal5.window,&io);
 uint8_t packet[124]={0};size_t sizes[3]={0};
 if(status==MT76_PEER5_OK)for(unsigned i=0;i<3;i++)sizes[i]=mt76_mgmt5_usb_request(&wifi_peer5.binding.peer,i,0,false,packet,sizeof packet);
 ok=stable && status==MT76_PEER5_OK && sizes[0]==60 && sizes[1]==100 && sizes[2]==56 && wifi_peer5_owned(&wifi_peer5,&wifi_cal5.window);
 for(unsigned i=0;i<sizeof word;i++)((volatile uint8_t *)word)[i]=0;
 for(unsigned i=0;i<sizeof station;i++)((volatile uint8_t *)station)[i]=0;
 for(unsigned i=0;i<sizeof packet;i++)((volatile uint8_t *)packet)[i]=0;
 console_printf("wifi-peer5: ok=%u status=%u complete=%u controls=%u station_reads=4 guards=6 stable=%u observations=%u rates=%u basic=%u generation=%u auth_bytes=%u assoc_bytes=%u leave_bytes=%u private=1 retained=1 posted=0 network_out=0 authenticated=0\n",(uint64_t)ok,(uint64_t)status,(uint64_t)wifi_peer5.binding.complete,(uint64_t)wifi_peer5.controls,(uint64_t)stable,(uint64_t)wifi_peer5.binding.observations,(uint64_t)wifi_peer5.binding.rates,(uint64_t)wifi_peer5.binding.peer.basic_rates,wifi_peer5.binding.generation,(uint64_t)sizes[0],(uint64_t)sizes[1],(uint64_t)sizes[2]);
 if(!ok)for(unsigned i=0;i<sizeof wifi_peer5.binding;i++)((volatile uint8_t *)&wifi_peer5.binding)[i]=0;
 wifi_peer5.active=false;return ok;
}
bool xhci_wifi_capture_peer5(void){
 if(wifi_peer5_requested || wifi_peer5.attempted)return false;
 wifi_peer5_requested=true;bool ok=xhci_wifi_capture_handoff5();wifi_peer5_requested=false;return ok;
}
