/* Actual empty-stack formatter and registry: no allocation or provider callback. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_NET_DEVICE_HOST_TEST
#include "../kernel/src/net_device.c"
#include "../kernel/src/net.c"
static unsigned callbacks;
static bool send_frame(void *c,const uint8_t *f,uint16_t n) {(void)c;(void)f;(void)n;callbacks++;return true;}
static uint64_t poll_frames(void *c,uint64_t b,bool v) {(void)c;(void)b;(void)v;callbacks++;return 0;}
static bool link_state(void *c,struct net_device_link *s) {(void)c;callbacks++;*s=(struct net_device_link){.up=true};return true;}
int main(void) {
    struct srv_net_status_info got,expected;
    assert(net_device_registry_empty());
    rx_frames=17;tcp_time_wait_reaped=23;
    assert(net_status_info(&expected)==0);
    memset(&got,0xa5,sizeof(got));
    assert(net_status_info_try_without_device(&got) && !memcmp(&got,&expected,sizeof(got)));
    assert(!got.initialized && !got.worker_started && !got.adapter_count && got.rx_frames==17 && got.tcp_time_wait_reaped==23);
    assert(!net_status_info_try_without_device(NULL));
    initialized=true;assert(!net_status_info_try_without_device(&got));initialized=false;
    worker_started=true;assert(!net_status_info_try_without_device(&got));worker_started=false;
    struct net_device_ops p={.name="refused-provider",.context=(void *)1,.mac={2,1,2,3,4,5},.send=send_frame,.poll=poll_frames,.link=link_state};
    assert(net_device_register(&p) && !net_device_registry_empty());
    memset(&got,0xa5,sizeof(got));expected=got;
    assert(!net_status_info_try_without_device(&got) && !callbacks && !memcmp(&got,&expected,sizeof(got)));
    assert(net_device_select() && !net_device_registry_empty());
    assert(!net_status_info_try_without_device(&got) && !callbacks);
    assert(net_status_info(&got)==0 && got.adapter_count==1 && callbacks==1);
    assert(net_device_retire(&p));callbacks=0;
    assert(!net_device_registry_empty() && !net_status_info_try_without_device(&got) && !callbacks);
    puts("net-empty-snapshot: PASS exact RAM formatter; initialized/worker/registered/selected/retired refusal without callbacks");
}
