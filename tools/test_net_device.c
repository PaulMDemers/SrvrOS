#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/net_device.c"
static int link_down,link_unknown;
static unsigned sends,polls;static uint64_t last_budget;static void *last_context;
static bool send(void *context,const uint8_t *frame,uint16_t size) { last_context=context;sends++;return frame[0]==7 && size==64; }
static uint64_t poll(void *context,uint64_t budget,bool verbose) { last_context=context;polls++;last_budget=budget;return verbose?2:1; }
static bool link(void *context,struct net_device_link *out) { last_context=context;*out=(struct net_device_link){!link_down,true,true,1000};return !link_unknown; }
int main(void) {
    uint8_t frame[64]={7};struct net_device_link status;
    assert(!net_device_mac() && !net_device_send(frame,64) && !net_device_poll(1,0) && !net_device_link(&status));
    struct net_device_ops usb={.name="usb-model",.context=(void *)1,.mac={2,1,2,3,4,5},.priority=50,.send=send,.poll=poll,.link=link};
    assert(!net_device_register(NULL));struct net_device_ops bad=usb;bad.mac[0]=1;assert(!net_device_register(&bad));
    bad=usb;memset(bad.mac,0,6);assert(!net_device_register(&bad));bad=usb;bad.send=NULL;assert(!net_device_register(&bad));
    assert(net_device_register(&usb));assert(!net_device_register(&usb));
    struct net_device_ops pci=usb;pci.name="pci-model";pci.context=(void *)2;pci.priority=100;pci.mac[5]=6;
    assert(net_device_register(&pci));assert(net_device_select());assert(net_device_select());
    assert(!strcmp(net_device_name(),"pci-model") && net_device_mac()[5]==6);
    assert(!net_device_register(&usb));
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
    assert(net_device_select());net_device_inventory(inventory,4);
    assert(inventory[0].flags==SRV_NET_ADAPTER_ACTIVE && !inventory[3].flags); /* Stable tie winner. */
    puts("net-device: PASS validation, selection priority, immutable provider, frame bounds and poll budget");
}
