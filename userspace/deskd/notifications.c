#include "notifications.h"
#include <srvros/desk_text.h>
#include <srvros/desktop_apps.h>
#include <string.h>
static int valid(const char *text,size_t capacity) {
    size_t length=0;
    while(length<capacity && text[length]) {
        unsigned c=(unsigned char)text[length];if(c<32 || c==127) return 0;length++;
    }
    return length<capacity && desk_text_utf8_valid(text,length);
}
int desktop_notification_valid(const struct desk_notification *text,unsigned tone) {
    return text && tone<=DESK_NOTIFY_MAX_TONE && text->title[0] && valid(text->title,sizeof(text->title)) && valid(text->message,sizeof(text->message));
}
static void remove_index(struct desktop_notifications *store,unsigned index) {
    memmove(store->records+index,store->records+index+1,(store->count-index-1)*sizeof(*store->records));
    store->count--;memset(store->records+store->count,0,sizeof(*store->records));
}
const struct desktop_notice *desktop_notification_find(const struct desktop_notifications *store,uint64_t id) {
    if(!id) return NULL;
    for(unsigned i=0;i<store->count;i++) if(store->records[i].id==id) return &store->records[i];
    return NULL;
}
int desktop_notification_add(struct desktop_notifications *store,uint64_t owner,unsigned application,uint64_t serial,uint64_t time_us,unsigned tone,const struct desk_notification *text,uint64_t *id) {
    if(!owner || !serial || !desk_application_info(application) || !desktop_notification_valid(text,tone)) return -1;
    for(unsigned i=0;i<store->count;i++) if(store->records[i].owner==owner && store->records[i].serial==serial) {
        const struct desktop_notice *old=&store->records[i];
        if(old->application!=application || old->tone!=tone || strcmp(old->text.title,text->title) || strcmp(old->text.message,text->message)) return -1;
        if(id) *id=old->id;
        return 0;
    }
    /* Snapshot before eviction, allowing a caller to reuse a stored message. */
    struct desk_notification snapshot={0};strcpy(snapshot.title,text->title);strcpy(snapshot.message,text->message);
    unsigned owners=0,oldest=store->count;
    for(unsigned i=0;i<store->count;i++) if(store->records[i].owner==owner) { owners++;if(oldest==store->count) oldest=i; }
    if(owners>=DESKTOP_NOTICES_PER_OWNER) remove_index(store,oldest);
    if(store->count==DESKTOP_NOTICES) remove_index(store,0);
    do { if(!++store->next_id) ++store->next_id; } while(desktop_notification_find(store,store->next_id));
    struct desktop_notice *next=&store->records[store->count++];
    *next=(struct desktop_notice){.id=store->next_id,.owner=owner,.serial=serial,.time_us=time_us,.application=application,.tone=tone,.unread=1,.text=snapshot};
    if(id) *id=next->id;
    return 1;
}
int desktop_notification_remove(struct desktop_notifications *store,uint64_t id) {
    for(unsigned i=0;i<store->count;i++) if(store->records[i].id==id) { remove_index(store,i);return 1; }
    return 0;
}
void desktop_notifications_read(struct desktop_notifications *store) { for(unsigned i=0;i<store->count;i++) store->records[i].unread=0; }
unsigned desktop_notifications_unread(const struct desktop_notifications *store) {
    unsigned count=0;for(unsigned i=0;i<store->count;i++) count+=store->records[i].unread!=0;return count;
}

int desktop_notification_target(struct desktop_notifications *store,uint64_t id,const char *path) {
    if(!path || path[0]!='/' || !valid(path,sizeof(store->records[0].target))) return -1;
    for(unsigned i=0;i<store->count;i++) if(store->records[i].id==id) {
        memmove(store->records[i].target,path,strlen(path)+1);return 0;
    }
    return -1;
}
