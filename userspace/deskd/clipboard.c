#include "clipboard.h"
#include <string.h>
static void reply(struct desk_clipboard_peer *p,uint64_t request,unsigned status) {
    p->reply=desk_packet_new(DESK_CLIPBOARD_ACK);p->reply.serial=request;p->reply.value=status;
}
void desk_clipboard_request(struct desk_clipboard_store *s,struct desk_clipboard_peer *p,int focused,const struct desk_packet *q,uint64_t tick) {
    if(!p || !p->pid || !q->serial) return;
    if(q->type==DESK_CLIPBOARD_GET) {
        p->request=q->serial;p->offset=0;p->version=s->version;p->reading=1;
        if(!focused) p->version=~s->version;
        return;
    }
    if(q->type!=DESK_CLIPBOARD_SET) return;
    if(!q->generation) {
        if(!focused || (s->writer && s->writer!=p)) { reply(p,q->serial,1);return; }
        s->writer=p;s->write_request=q->serial;s->stage_length=q->width;s->stage_offset=0;
    }
    if(s->writer!=p || s->write_request!=q->serial || q->generation!=s->stage_offset ||
       q->width!=s->stage_length || q->width>DESK_CLIPBOARD_BYTES || q->height>sizeof(q->text) ||
       q->height>s->stage_length-s->stage_offset || q->value>1 || (!q->height && !q->value)) {
        if(s->writer==p) s->writer=0;
        reply(p,q->serial,1);return;
    }
    memcpy(s->staging+s->stage_offset,q->text,q->height);s->stage_offset+=q->height;s->write_tick=tick;
    if(q->value) {
        if(s->stage_offset!=s->stage_length) reply(p,q->serial,1);
        else {
            memcpy(s->data,s->staging,s->stage_length);s->length=s->stage_length;s->version++;
            reply(p,q->serial,0);
        }
        s->writer=0;
    }
}
void desk_clipboard_service(struct desk_clipboard_store *s,struct desk_clipboard_peer *p,uint64_t tick) {
    if(!p || !p->pid) return;
    if(s->writer==p && tick-s->write_tick>1000) { reply(p,s->write_request,1);s->writer=0; }
    if(p->reply.type) { if(desk_send(p->pid,&p->reply)) return;p->reply.type=0; }
    for(unsigned batch=0;batch<4 && p->reading;batch++) {
        struct desk_packet q=desk_packet_new(DESK_CLIPBOARD_DATA);q.serial=p->request;
        q.generation=p->offset;q.width=s->length;
        if(p->version!=s->version) q.value=2;
        else {
            q.height=s->length-p->offset;if(q.height>sizeof(q.text)) q.height=sizeof(q.text);
            memcpy(q.text,s->data+p->offset,q.height);
            q.value=p->offset+q.height==s->length;
        }
        if(desk_send(p->pid,&q)) break;
        p->offset+=q.height;if(q.value) p->reading=0;
    }
}
int desk_clipboard_write_local(struct desk_clipboard_store *s,const void *data,uint32_t length) {
    if(length>DESK_CLIPBOARD_BYTES || (!data && length)) return -1;
    if(s->writer) {
        reply(s->writer,s->write_request,1);
        s->writer=0;
    }
    if(length) memmove(s->data,data,length);
    s->length=length;s->version++;return 0;
}
void desk_clipboard_forget(struct desk_clipboard_store *s,struct desk_clipboard_peer *p) {
    if(s->writer==p) s->writer=0;
    if(p) *p=(struct desk_clipboard_peer){0};
}
