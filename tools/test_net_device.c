#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_NET_DEVICE_HOST_TEST
#include "../kernel/src/net_device.c"
static int link_down,link_unknown;
static bool wake_ready;
static bool wake(void *context) { assert(context==(void *)2);return wake_ready; }
static unsigned sends,polls;static uint64_t last_budget;static void *last_context;
static bool send(void *context,const uint8_t *frame,uint16_t size) { last_context=context;sends++;return frame[0]==7 && size==64; }
static uint64_t poll(void *context,uint64_t budget,bool verbose) { last_context=context;polls++;last_budget=budget;return verbose?2:1; }
static bool link(void *context,struct net_device_link *out) { last_context=context;*out=(struct net_device_link){!link_down,true,true,1000};return !link_unknown; }
static struct net_device_ops release_identity;
static unsigned release_kind,release_calls;
static bool release_nested;
static void release_callback(void *context,unsigned kind) {
    unsigned *live=context;assert(*live==0x1234);release_calls++;
    if(release_kind!=kind)return;
    release_kind=0;
    assert(!net_device_retire(&release_identity)); /* This callback still owns it. */
    uint8_t frame[64]={7};struct net_device_link state;
    assert(!net_device_send(frame,64) && !net_device_poll(1,false) && !net_device_link(&state));
    assert(!net_device_rx_wake_ready());
    struct srv_net_adapter_info rows[2];unsigned before=release_calls;
    assert(net_device_inventory(rows,2)==1 && release_calls==before);
}
static bool release_send(void *c,const uint8_t *f,uint16_t n) {
    (void)f;(void)n;
    if(release_nested) {release_nested=false;struct net_device_link state;
        assert(net_device_link(&state));assert(!net_device_retire(&release_identity));}
    release_callback(c,1);return true;
}
static uint64_t release_poll(void *c,uint64_t n,bool v) { (void)n;(void)v;release_callback(c,2);return 1; }
static bool release_link(void *c,struct net_device_link *out) { release_callback(c,3);*out=(struct net_device_link){.up=true};return true; }
static bool release_wake(void *c) { release_callback(c,4);return true; }
static bool alias_send(void *c,const uint8_t *f,uint16_t n) { return release_send(c,f,n); }
static void test_shared_release(void) {
    selected=NULL;sealed=false;provider_count=0;release_kind=0;release_nested=false;
    unsigned *context=malloc(sizeof(*context));assert(context);*context=0x1234;
    struct net_device_ops a={.name="first",.context=context,.mac={2,1,2,3,4,5},.priority=2,
        .send=release_send,.poll=release_poll,.link=release_link};
    struct net_device_ops b=a;b.name="second";b.send=alias_send;b.priority=1;
    assert(net_device_register(&a) && net_device_register(&b) && net_device_select());
    assert(net_device_retire(&a));
    unsigned calls=release_calls;struct srv_net_adapter_info rows[2];
    assert(net_device_inventory(rows,2)==2 && release_calls==calls+1); /* Second still owns context. */
    assert(net_device_retire(&b));free(context);calls=release_calls;
    assert(net_device_inventory(rows,2)==2 && release_calls==calls);
    assert(!strcmp(net_device_name(),"first") && !net_device_poll(1,false)); /* No failover. */
}
static void test_release(void) {
    for(unsigned kind=1;kind<=6;kind++) {
        selected=NULL;sealed=false;provider_count=0;
        unsigned *context=malloc(sizeof(*context));assert(context);*context=0x1234;
        char *name=malloc(32);assert(name);strcpy(name,"retirable");
        release_identity=(struct net_device_ops){.name=name,.context=context,.mac={2,1,2,3,4,5},
            .send=release_send,.poll=release_poll,.link=release_link,.rx_wake_ready=release_wake};
        assert(net_device_register(&release_identity) && net_device_select());
        memset(name,'x',9);assert(!strcmp(net_device_name(),"retirable"));
        struct net_device_ops unknown=release_identity;unknown.context=&kind;
        assert(!net_device_retire(&unknown));
        release_nested=kind==6;release_kind=kind>=5?3:kind;release_calls=0;
        uint8_t frame[64]={7};struct net_device_link state;struct srv_net_adapter_info rows[2];
        if(kind==1 || kind==6)assert(net_device_send(frame,64));
        if(kind==2)assert(net_device_poll(1,false)==1);
        if(kind==3)assert(net_device_link(&state));
        if(kind==4)assert(net_device_rx_wake_ready());
        if(kind==5)assert(net_device_inventory(rows,2)==1);
        assert(!release_kind && release_calls==(kind==6?2u:1u) && net_device_retire(&release_identity));
        free(context);free(name); /* No registry reference may dereference these. */
        for(unsigned retry=0;retry<4;retry++) {
            assert(net_device_retire(&release_identity));
            assert(!net_device_send(frame,64) && !net_device_poll(1,false));
            assert(!net_device_link(&state) && !net_device_rx_wake_ready());
            assert(net_device_inventory(rows,2)==1 && !strcmp(rows[0].driver,"retirable"));
            assert(!strcmp(net_device_name(),"retirable") && net_device_mac()[5]==5);
        }
        assert(release_calls==(kind==6?2u:1u) && !net_device_register(&release_identity));
    }
    assert(!net_device_retire(NULL));
    test_shared_release();
    puts("net-device-retire: PASS nested send/poll/link/wake/inventory retirement, context release and owned metadata");
}
int main(void) {
    assert(!net_device_rx_wake_ready());
    uint8_t frame[64]={7};struct net_device_link status;
    assert(!net_device_mac() && !net_device_send(frame,64) && !net_device_poll(1,0) && !net_device_link(&status));
    struct net_device_ops usb={.name="usb-model",.context=(void *)1,.mac={2,1,2,3,4,5},.priority=50,.send=send,.poll=poll,.link=link};
    assert(!net_device_register(NULL));struct net_device_ops bad=usb;bad.mac[0]=1;assert(!net_device_register(&bad));
    bad=usb;memset(bad.mac,0,6);assert(!net_device_register(&bad));bad=usb;bad.send=NULL;assert(!net_device_register(&bad));
    assert(net_device_register(&usb));assert(!net_device_register(&usb));
    struct net_device_ops pci=usb;pci.name="pci-model";pci.context=(void *)2;pci.priority=100;pci.mac[5]=6;pci.rx_wake_ready=wake;
    assert(net_device_register(&pci));assert(net_device_select());assert(net_device_select());
    assert(!strcmp(net_device_name(),"pci-model") && net_device_mac()[5]==6);
    assert(!net_device_register(&usb));
    assert(!net_device_rx_wake_ready());wake_ready=true;assert(net_device_rx_wake_ready());wake_ready=false;assert(!net_device_rx_wake_ready());
    struct srv_net_adapter_info inventory[SRV_NET_ADAPTER_MAX];
    memset(inventory,0xff,sizeof(inventory));
    assert(net_device_inventory(inventory,4)==2);
    assert(inventory[0].id==1 && !inventory[0].flags && !strcmp(inventory[0].driver,"usb-model"));
    assert(inventory[1].id==2 && inventory[1].flags==SRV_NET_ADAPTER_ACTIVE && inventory[1].mac[5]==6);
    assert(inventory[1].link_speed_mbps==1000 && (inventory[1].link_flags&SRV_NET_LINK_UP));
    assert(!sends && !polls); /* Inventory never transmits or polls receive queues. */
    link_down=1;net_device_inventory(inventory,2);assert(inventory[1].link_flags==SRV_NET_LINK_KNOWN && !inventory[1].link_speed_mbps);
    link_unknown=1;net_device_inventory(inventory,2);assert(!inventory[1].link_flags && !inventory[1].link_speed_mbps);
    link_unknown=link_down=0;
    assert(!net_device_inventory(NULL,4) && !net_device_inventory(inventory,0));
    inventory[1].id=99;assert(net_device_inventory(inventory,1)==1 && inventory[1].id==99);
    assert(net_device_send(frame,64) && last_context==(void *)2 && sends==1);
    assert(!net_device_send(NULL,64) && !net_device_send(frame,13) && !net_device_send(frame,1515) && sends==1);
    assert(net_device_poll(1000,1)==2 && last_budget==32 && polls==1);
    assert(!net_device_poll(0,0) && polls==1);
    assert(net_device_link(&status) && status.up && status.full_duplex && status.speed_mbps==1000);
    assert(!net_device_link(NULL));
    selected=NULL;sealed=false;provider_count=0;
    assert(!net_device_select() && !net_device_register(&usb));
    sealed=false;
    for(unsigned i=0;i<SRV_NET_ADAPTER_MAX;i++) { usb.context=(void *)(uintptr_t)(i+1);usb.mac[5]=i;assert(net_device_register(&usb)); }
    usb.context=(void *)99;assert(!net_device_register(&usb));
    assert(net_device_select());assert(!net_device_rx_wake_ready());net_device_inventory(inventory,4);
    assert(inventory[0].flags==SRV_NET_ADAPTER_ACTIVE && !inventory[3].flags); /* Stable tie winner. */
    test_release();
    puts("net-device: PASS validation, selection priority, immutable provider, frame bounds and poll budget");
}
