#include <srvros/ec_smbus.h>
static int fail(struct ec_smbus *s) {
    s->failed=true;s->pending=false;ec_transport_cancel(s->ec);
    /* A timed-out SMBus command can finish late even if its last EC byte
     * completed. External recovery must resolve it before reusing the EC. */
    s->ec->poisoned=true;return -1;
}
bool ec_smbus_read_word(struct ec_smbus *s,struct ec_transport *ec,uint8_t base,
    uint8_t address,uint8_t command,uint64_t now) {
    if(!s || !ec || ec->poisoned || (ec->phase!=EC_IDLE && ec->phase!=EC_DONE) ||
       (s->ec && s->phase<8) || s->failed || base>250 || address<1 || address>0x7f) return false;
    *s=(struct ec_smbus){.ec=ec,.base=base,.address=address,.command=command,.started=now,.last=now};return true;
}
int ec_smbus_poll(struct ec_smbus *s,uint64_t now) {
    if(!s || !s->ec || s->failed) return -1;
    if(s->phase==8) return 1;
    if(now<s->last || now-s->started>=2000000) return fail(s);
    s->last=now;
    if(s->pending) {
        int result=ec_transport_poll(s->ec,now);
        if(result<0) return fail(s);
        if(!result) return 0;
        s->pending=false;uint8_t value=s->ec->result;
        if(s->phase==0 && value) return fail(s); /* Never overwrite an existing protocol. */
        if(s->phase==4 && value) {
            if(value!=9) return fail(s);
            s->next=now+1000;return 0;
        }
        if(s->phase==5) { s->status=value;if(value&31) return fail(s); }
        if(s->phase==6) s->lo=value;
        if(s->phase==7) { s->result=s->lo|((uint16_t)value<<8);s->phase=8;return 1; }
        s->phase++;
    }
    if(now<s->next) return 0;
    static const uint8_t offsets[]={0,2,3,0,0,1,4,5};
    uint8_t value=s->phase==1?(uint8_t)(s->address<<1):s->phase==2?s->command:9;
    bool write=s->phase>=1 && s->phase<=3;
    if(!ec_transport_begin(s->ec,write,s->base+offsets[s->phase],value,now,100000)) return fail(s);
    s->pending=true;return 0;
}


static int event_fail(struct ec_smbus_event *s) {
    s->failed=true;s->pending=false;ec_transport_cancel(s->ec);
    s->ec->poisoned=true;return -1;
}
bool ec_smbus_event_begin(struct ec_smbus_event *s,struct ec_transport *ec,uint8_t base,uint64_t now) {
    if(!s || !ec || base>216 || ec->poisoned ||
       (ec->phase!=EC_IDLE && ec->phase!=EC_DONE) ||
       (s->ec && s->phase<8) || s->failed)return false;
    *s=(struct ec_smbus_event){.ec=ec,.base=base,.started=now,.last=now};return true;
}
int ec_smbus_event_poll(struct ec_smbus_event *s,uint64_t now) {
    if(!s || !s->ec || s->failed)return -1;
    if(s->phase==8)return 1;
    if(now<s->last || now-s->started>=1000000)return event_fail(s);
    s->last=now;
    if(s->pending) {
        int result=ec_transport_poll(s->ec,now);
        if(result<0)return event_fail(s);
        if(!result)return 0;
        s->pending=false;uint8_t value=s->ec->result;
        if((s->phase==0 || s->phase==5) && value)return event_fail(s);
        if(s->phase==1) {
            s->status=value;
            if(value&0x3f)return event_fail(s); /* Error or reserved status. */
            s->alarm=!!(value&0x40);
            if(!s->alarm){s->phase=8;return 1;}
        }
        if(s->phase==2) {
            if((value&1) || !(value>>1))return event_fail(s);
            s->address=value>>1;
            s->recognized=s->address>=9 && s->address<=11;
        }
        if(s->phase==3)s->lo=value;
        if(s->phase==4)s->data=s->lo|((uint16_t)value<<8);
        /* The alarm stays latched until acknowledged; reject a changed status
         * or a newly active protocol instead of clearing somebody else's work. */
        if(s->phase==6 && value!=s->status)return event_fail(s);
        if(s->phase==7){s->acknowledged=true;s->phase=8;return 1;}
        s->phase++;
    }
    static const uint8_t offsets[]={0,1,0x25,0x26,0x27,0,1,1};
    bool write=s->phase==7;
    if(write)s->ack_attempted=true;
    if(!ec_transport_begin(s->ec,write,s->base+offsets[s->phase],0,now,100000))return event_fail(s);
    s->pending=true;return 0;
}
