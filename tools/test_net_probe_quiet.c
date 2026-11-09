/* Explicit diagnostic logging boundary over the production stack. */
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "../kernel/src/net.c"
static const uint8_t station[6]={2,1,2,3,4,5};
static bool available=true;
static unsigned messages;
void console_write(const char *text){assert(text);messages++;}
void console_printf(const char *format,...){assert(format);messages++;}
bool net_device_select(void){return available;}
const uint8_t *net_device_mac(void){return available?station:NULL;}
const char *net_device_name(void){return "probe-fixture";}
bool net_device_send(const uint8_t *p,uint16_t n){(void)p;(void)n;assert(0);return false;}
static void reset(void){initialized=false;worker_started=false;dhcp_in_progress=false;dhcp_retry_active=false;net_probe_quiet=false;available=true;messages=0;}
static void ack(void){
 uint8_t p[250]={0};dhcp_in_progress=true;dhcp_retry_active=false;dhcp_xid=42;
 p[0]=2;p[1]=1;p[2]=6;p[7]=42;p[16]=10;p[19]=15;
 p[236]=0x63;p[237]=0x82;p[238]=0x53;p[239]=0x63;
 p[240]=53;p[241]=1;p[242]=5;p[243]=255;
 handle_dhcp_packet(p,sizeof p,0x0a000202);assert(dhcp_configured);
}
int main(void){
 reset();net_init();assert(initialized && messages==2);ack();assert(messages==3);
 reset();assert(net_init_bounded_probe() && initialized && net_probe_quiet && messages==0);ack();assert(messages==0);
 assert(!net_init_bounded_probe() && messages==0);
 reset();available=false;assert(!net_init_bounded_probe() && !initialized && !net_probe_quiet && messages==0);
 net_init();assert(messages==1);
 reset();worker_started=true;assert(!net_init_bounded_probe() && !net_probe_quiet && messages==0);
 reset();dhcp_in_progress=true;assert(!net_init_bounded_probe() && !net_probe_quiet && messages==0);
 puts("net-probe-quiet: PASS explicit first-init/ACK suppression,normal logging unchanged,missing provider and active-owner refusals;no USB/native acceptance");
}
