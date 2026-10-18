#include <srvros/net_device.h>
#include <stddef.h>
#define PROVIDERS 4
static struct net_device_ops providers[PROVIDERS];
static unsigned provider_count;
static const struct net_device_ops *selected;
static bool sealed;
static bool mac_valid(const uint8_t *mac) {
    unsigned any=0;for(unsigned i=0;i<6;i++) any|=mac[i];
    return any && !(mac[0]&1);
}
bool net_device_register(const struct net_device_ops *ops) {
    if(sealed || !ops || !ops->name || !*ops->name || !ops->send || !ops->poll || !ops->link || !mac_valid(ops->mac)) return false;
    for(unsigned i=0;i<provider_count;i++) if(providers[i].context==ops->context && providers[i].send==ops->send) return false;
    if(provider_count==PROVIDERS) return false;
    providers[provider_count++]=*ops;return true;
}
bool net_device_select(void) {
    if(sealed) return selected!=NULL;
    sealed=true;
    for(unsigned i=0;i<provider_count;i++) if(!selected || providers[i].priority>selected->priority) selected=&providers[i];
    return selected!=NULL;
}
const char *net_device_name(void) { return selected?selected->name:"none"; }
const uint8_t *net_device_mac(void) { return selected?selected->mac:NULL; }
bool net_device_send(const uint8_t *frame,uint16_t length) {
    return selected && frame && length>=14 && length<=NET_DEVICE_FRAME_MAX && selected->send(selected->context,frame,length);
}
uint64_t net_device_poll(uint64_t budget,bool verbose) {
    if(!selected || !budget) return 0;
    if(budget>32) budget=32;
    return selected->poll(selected->context,budget,verbose);
}
bool net_device_link(struct net_device_link *out) {
    if(!out) return false;
    *out=(struct net_device_link){0};return selected && selected->link(selected->context,out);
}
