#include <srvros/net_device.h>
#include <stddef.h>
#define PROVIDERS SRV_NET_ADAPTER_MAX
static struct net_device_ops providers[PROVIDERS];
static unsigned provider_count;
static const struct net_device_ops *selected;
static bool sealed;
static bool retiring[PROVIDERS];
static unsigned references[PROVIDERS];
static struct net_device_ops identities[PROVIDERS];
static char names[PROVIDERS][sizeof(((struct srv_net_adapter_info *)0)->driver)];
static uint64_t registry_lock(void) {
#ifdef SRVROS_NET_DEVICE_HOST_TEST
    return 0;
#else
    uint64_t f;__asm__ volatile("pushfq; popq %0; cli":"=r"(f)::"memory");return f;
#endif
}
static void registry_unlock(uint64_t f) {
#ifdef SRVROS_NET_DEVICE_HOST_TEST
    (void)f;
#else
    if(f&(1ull<<9))__asm__ volatile("sti":::"memory");
#endif
}
static void detach(unsigned i) {
    providers[i].context=NULL;providers[i].send=NULL;providers[i].poll=NULL;
    providers[i].link=NULL;providers[i].rx_wake_ready=NULL;
}
static bool enter(const struct net_device_ops *p,struct net_device_ops *copy) {
    if(!p)return false;
    uint64_t f=registry_lock();unsigned i=(unsigned)(p-providers);
    bool ok=!retiring[i] && references[i]!=~0u;
    if(ok) { references[i]++;*copy=*p; }
    registry_unlock(f);return ok;
}
static void leave(const struct net_device_ops *p) {
    uint64_t f=registry_lock();unsigned i=(unsigned)(p-providers);
    references[i]--;if(retiring[i] && !references[i])detach(i);
    registry_unlock(f);
}
bool net_device_retire(const struct net_device_ops *identity) {
    if(!identity || !identity->send)return false;
    uint64_t f=registry_lock();
    for(unsigned i=0;i<provider_count;i++) {
        if(identities[i].context!=identity->context || identities[i].send!=identity->send)continue;
        retiring[i]=true;bool drained=!references[i];if(drained)detach(i);
        registry_unlock(f);return drained;
    }
    registry_unlock(f);return false;
}
static bool mac_valid(const uint8_t *mac) {
    unsigned any=0;for(unsigned i=0;i<6;i++) any|=mac[i];
    return any && !(mac[0]&1);
}
bool net_device_register(const struct net_device_ops *ops) {
    if(sealed || !ops || !ops->name || !*ops->name || !ops->send || !ops->poll || !ops->link || !mac_valid(ops->mac)) return false;
    for(unsigned i=0;i<provider_count;i++) if(identities[i].context==ops->context && identities[i].send==ops->send) return false;
    if(provider_count==PROVIDERS) return false;
    unsigned i=provider_count++;
    identities[i]=(struct net_device_ops){.context=ops->context,.send=ops->send};
    providers[i]=*ops;retiring[i]=false;references[i]=0;
    unsigned j=0;for(;j<sizeof(names[i])-1 && ops->name[j];j++)names[i][j]=ops->name[j];
    names[i][j]=0;providers[i].name=names[i];return true;
}
bool net_device_select(void) {
    if(sealed) return selected!=NULL;
    sealed=true;
    for(unsigned i=0;i<provider_count;i++) if(!retiring[i] && (!selected || providers[i].priority>selected->priority)) selected=&providers[i];
    return selected!=NULL;
}
const char *net_device_name(void) { return selected?selected->name:"none"; }
const uint8_t *net_device_mac(void) { return selected?selected->mac:NULL; }
bool net_device_selected_is(const struct net_device_ops *identity) {
    if (!identity || !identity->send) return false;
    uint64_t flags=registry_lock();
    bool match=false;
    if (sealed && selected) {
        for (unsigned i=0;i<provider_count;i++) {
            if (selected==&providers[i] && !retiring[i] &&
                identities[i].context==identity->context && identities[i].send==identity->send) {
                match=true;
                break;
            }
        }
    }
    registry_unlock(flags);
    return match;
}
bool net_device_send(const uint8_t *frame,uint16_t length) {
    struct net_device_ops p;const struct net_device_ops *owner=selected;
    if(!frame || length<14 || length>NET_DEVICE_FRAME_MAX || !enter(owner,&p))return false;
    bool ok=p.send(p.context,frame,length);leave(owner);return ok;
}
uint64_t net_device_poll(uint64_t budget,bool verbose) {
    if(!selected || !budget) return 0;
    if(budget>32) budget=32;
    struct net_device_ops p;const struct net_device_ops *owner=selected;
    if(!enter(owner,&p))return 0;
    uint64_t count=p.poll(p.context,budget,verbose);leave(owner);return count;
}
bool net_device_link(struct net_device_link *out) {
    if(!out) return false;
    *out=(struct net_device_link){0};struct net_device_ops p;const struct net_device_ops *owner=selected;
    if(!enter(owner,&p))return false;
    bool ok=p.link(p.context,out);leave(owner);return ok;
}

unsigned net_device_inventory(struct srv_net_adapter_info *out,unsigned capacity) {
    if(!out) return 0;
    unsigned count=provider_count<capacity?provider_count:capacity;
    for(unsigned i=0;i<count;i++) {
        const struct net_device_ops *p=&providers[i];
        out[i]=(struct srv_net_adapter_info){.id=i+1,.flags=p==selected?SRV_NET_ADAPTER_ACTIVE:0};
        for(unsigned j=0;j<6;j++) out[i].mac[j]=p->mac[j];
        for(unsigned j=0;j<sizeof(out[i].driver)-1 && p->name[j];j++) out[i].driver[j]=p->name[j];
        struct net_device_link link={0};
        struct net_device_ops callback;bool known=false;
        if(enter(p,&callback)) { known=callback.link(callback.context,&link);leave(p); }
        if(known) {
            out[i].link_flags=SRV_NET_LINK_KNOWN|(link.up?SRV_NET_LINK_UP:0);
            if(link.up) {
                out[i].link_flags|=(link.full_duplex?SRV_NET_LINK_FULL_DUPLEX:0)|(link.duplex_known?SRV_NET_LINK_DUPLEX_KNOWN:0);
                out[i].link_speed_mbps=link.speed_mbps;
            }
        }
    }
    return count;
}

bool net_device_rx_wake_ready(void) {
    struct net_device_ops p;const struct net_device_ops *owner=selected;
    if(!enter(owner,&p))return false;
    bool ok=p.rx_wake_ready && p.rx_wake_ready(p.context);leave(owner);return ok;
}
