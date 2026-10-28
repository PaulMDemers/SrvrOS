#include <srvros/wifi_service.h>
static struct provider {
    uint64_t id;
    struct wifi_service_provider ops;
    struct mt76_selection snapshot;
    struct mt76_selection_choice choice;
    bool selected;
} providers[SRV_WIFI_ADAPTERS];
static uint64_t serial;
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static struct provider *find(uint64_t id){
    for(unsigned i=0;i<SRV_WIFI_ADAPTERS;i++)if(id && providers[i].id==id)return &providers[i];
    return NULL;
}
static void prune(void){
    for(unsigned i=0;i<SRV_WIFI_ADAPTERS;i++)if(providers[i].id && !providers[i].ops.present(providers[i].ops.context))scrub(&providers[i],sizeof providers[i]);
}
uint64_t wifi_service_publish(const struct wifi_service_provider *ops,
    const struct mt76_scan_batch *b,const struct mt76_selection_io *io){
    if(!ops || !ops->context || !ops->label || !*ops->label || !ops->present ||
       !io || io->context!=ops->context || !ops->present(ops->context))return 0;
    prune();struct provider *p=NULL;
    for(unsigned i=0;i<SRV_WIFI_ADAPTERS;i++){
        if(providers[i].id && providers[i].ops.context==ops->context)return 0;
        if(!p && !providers[i].id)p=&providers[i];
    }
    if(!p || serial==UINT64_MAX)return 0;
    uint64_t id=++serial;
    if(!mt76_selection_publish(&p->snapshot,b,io) || !ops->present(ops->context)){
        scrub(p,sizeof *p);return 0;
    }
    p->ops=*ops;p->id=id;return id;
}
bool wifi_service_refresh(uint64_t id,const struct mt76_scan_batch *b,const struct mt76_selection_io *io){
    prune();struct provider *p=find(id);if(!p)return false;
    p->selected=false;scrub(&p->choice,sizeof p->choice);
    if(!io || io->context!=p->ops.context){mt76_selection_invalidate(&p->snapshot);return false;}
    bool ok=mt76_selection_publish(&p->snapshot,b,io);
    if(!p->ops.present(p->ops.context)){scrub(p,sizeof *p);return false;}
    return ok;
}
void wifi_service_remove(uint64_t id){struct provider *p=find(id);if(p)scrub(p,sizeof *p);}
int wifi_service_snapshot(uint64_t id,struct srv_wifi_snapshot *out){
    if(!out)return SRV_WIFI_INVALID;
    *out=(struct srv_wifi_snapshot){.version=SRV_WIFI_VERSION,.size=sizeof *out,.selected=SRV_WIFI_NONE};
    prune();struct provider *p=NULL;
    for(unsigned i=0;i<SRV_WIFI_ADAPTERS;i++)if(providers[i].id){
        out->adapter_count++;
        if(!p && (!id || providers[i].id==id))p=&providers[i];
    }
    if(!p)return id?SRV_WIFI_STALE:SRV_WIFI_OK;
    out->owner=p->id;out->generation=p->snapshot.generation;
    bool after=false;
    for(unsigned i=0;i<SRV_WIFI_ADAPTERS;i++){
        if(after && providers[i].id){out->next_owner=providers[i].id;break;}
        if(&providers[i]==p)after=true;
    }
    for(unsigned i=0;i<sizeof out->adapter-1 && p->ops.label[i];i++){
        unsigned char c=p->ops.label[i];out->adapter[i]=(c>=32 && c<=126)?(char)c:'?';
    }
    if(!p->snapshot.complete)return SRV_WIFI_OK;
    out->flags=SRV_WIFI_READY;out->count=p->snapshot.count;
    if(p->selected)out->selected=p->choice.index;
    for(unsigned i=0;i<out->count;i++){
        const struct mt76_scan_row *v=&p->snapshot.row[i];const struct mt76_discovery_row *a=&v->advertisement;
        struct srv_wifi_row *r=&out->row[i];r->index=i;r->channel=a->channel;r->observations=a->observations;
        bool conflict=v->channel_conflict || a->ssid_conflict || a->security_conflict;
        r->flags=(a->hidden?SRV_WIFI_ROW_HIDDEN:0)|(conflict?SRV_WIFI_ROW_CONFLICT:0)|
            (a->privacy?SRV_WIFI_ROW_PRIVACY:0)|(a->rsn?SRV_WIFI_ROW_RSN:0)|(a->wpa?SRV_WIFI_ROW_WPA:0)|
            (!a->hidden && !conflict?SRV_WIFI_ROW_SELECTABLE:0);
        if(!mt76_selection_name(a->ssid,a->ssid_size,r->name,sizeof r->name)){
            *out=(struct srv_wifi_snapshot){.version=SRV_WIFI_VERSION,.size=sizeof *out,.selected=SRV_WIFI_NONE};return SRV_WIFI_INVALID;
        }
    }
    return SRV_WIFI_OK;
}
int wifi_service_select(const struct srv_wifi_select *request){
    if(!request || request->version!=SRV_WIFI_VERSION || request->size!=sizeof *request || request->reserved || !request->owner)return SRV_WIFI_INVALID;
    prune();struct provider *p=find(request->owner);if(!p)return SRV_WIFI_STALE;
    struct mt76_selection_choice choice={0};
    enum mt76_selection_status status=mt76_selection_choose(&p->snapshot,request->generation,request->index,&choice);
    int result=SRV_WIFI_INVALID;
    switch(status){
    case MT76_SELECTION_OK:p->choice=choice;p->selected=true;result=SRV_WIFI_OK;break;
    case MT76_SELECTION_STALE:result=SRV_WIFI_STALE;break;
    case MT76_SELECTION_UNAVAILABLE:result=SRV_WIFI_UNAVAILABLE;break;
    case MT76_SELECTION_HIDDEN:result=SRV_WIFI_HIDDEN;break;
    case MT76_SELECTION_CONFLICT:result=SRV_WIFI_CONFLICT;break;
    default:break;
    }
    scrub(&choice,sizeof choice);return result;
}
