#include <srvros/mt76_channel_vco5.h>
static bool identity(const struct mt76_channel_vco5 *s){return s && s->attempted && s->nic0==0xfd11 && s->nic1==0x1004 && s->factory_trim==0x44 && s->lna<=255 && s->trim.with_channel && s->trim.trim==s->factory_trim && s->trim.lna==s->lna;}
bool mt76_channel_vco5_selected(const struct mt76_channel_vco5 *s){
    if(!identity(s) || !mt76_trim5_owned(&s->trim))return false;
    const struct mt76_channel5 *c=&s->trim.channel;
    return s->trim.phase==2 && s->trim.operations==21 && c->phase==3 && c->selected && c->bbp.operations==441 && c->pa.operations==294 && c->band.phase==8 && c->band.synth.paused && c->band.a20.operations==504;
}
bool mt76_channel_vco5_owned(const struct mt76_channel_vco5 *s){
    if(!identity(s) || s->complete || s->restored)return false;
    if(s->in_child)return mt76_channel_vco5_selected(s) && s->child_status==MT76_TIMING_REFUSED && mt76_cvco5_owned(&s->vco);
    if(!mt76_trim5_owned(&s->trim) && !mt76_trim5_valid(&s->trim))return false;
    if(s->vco.attempted)return s->child_status==MT76_TIMING_OK && mt76_cvco5_valid(&s->vco);
    return s->child_status==MT76_TIMING_REFUSED && (!s->trim.channel.selected || mt76_channel_vco5_selected(s));
}
bool mt76_channel_vco5_valid(const struct mt76_channel_vco5 *s){return identity(s) && s->complete && s->restored && !s->in_child && s->child_status==MT76_TIMING_OK && mt76_trim5_valid(&s->trim) && mt76_cvco5_valid(&s->vco);}
unsigned mt76_channel_vco5_operations(const struct mt76_channel_vco5 *s){
    if(!s || !s->attempted || s->vco.operations>77)return UINT32_MAX;
    unsigned n=mt76_trim5_operations(&s->trim);return n<=3767?n+s->vco.operations:UINT32_MAX;
}
unsigned mt76_channel_vco5_phase(const struct mt76_channel_vco5 *s){return !s?UINT32_MAX:s->in_child?15:mt76_trim5_phase(&s->trim);}
unsigned mt76_channel_vco5_local(const struct mt76_channel_vco5 *s){return !s?UINT32_MAX:s->in_child?s->vco.operations:mt76_trim5_local(&s->trim);}
bool mt76_channel_vco5_transfer_valid(const struct mt76_channel_vco5 *s,bool write,uint32_t a,uint32_t v){
    return mt76_channel_vco5_owned(s) && (s->in_child?mt76_cvco5_transfer_valid(&s->vco,write,a):mt76_trim5_transfer_valid(&s->trim,write,a,v));
}
size_t mt76_channel_vco5_packet(const struct mt76_channel_vco5 *s,uint8_t *out,size_t cap){
    if(!mt76_channel_vco5_owned(s))return 0;
    return s->in_child?mt76_cvco5_packet(&s->vco,out,cap):mt76_trim5_packet(&s->trim,out,cap);
}
struct bridge {struct mt76_channel_vco5 *s;const struct mt76_rf_wait_io *rf;const struct mt76_band_mac_io *mac;const struct mt76_pa5_io *pa;const struct mt76_bbp5_io *bbp;bool (*inspect)(void *);bool inspected;};
static bool child_authorize(void *p){struct bridge *b=p;return b->s->in_child && mt76_channel_vco5_owned(b->s) && b->rf->command.authorize(b->rf->command.context) && mt76_channel_vco5_owned(b->s);}
static bool child_read(void *p,uint32_t a,uint32_t *v){struct bridge *b=p;return v && b->s->in_child && mt76_channel_vco5_transfer_valid(b->s,false,a,0) && b->rf->command.read(b->rf->command.context,a,v);}
static int child_exchange(void *p,const uint8_t *packet,size_t n,uint8_t *reply,size_t cap){
 struct bridge *b=p;uint8_t allowed[16];
 if(!b->s->in_child || !packet || !reply || n!=16 || cap<MT76_MCU_RESPONSE_MAX || mt76_channel_vco5_packet(b->s,allowed,16)!=16)return -1;
 for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
 return b->rf->command.exchange(b->rf->command.context,packet,n,reply,cap);
}
static bool child_delay(void *p,uint32_t us){struct bridge *b=p;return b->s->in_child && mt76_channel_vco5_owned(b->s) && mt76_cvco5_delay_admit(&b->s->vco) && us==mt76_rf_wait_delay(b->s->vco.waits) && b->rf->delay(b->rf->command.context,us);}
static bool authorize(void *p){
    struct bridge *b=p;struct mt76_channel_vco5 *s=b->s;
    if(!mt76_channel_vco5_owned(s))return false;
    /* Only this explicit coordinator enters a child during authorization.
     * Child callbacks use separate guarded adapters, never this trigger path,
     * so no recursive authorization or replay is possible. */
    if(mt76_channel_vco5_selected(s) && !s->vco.attempted){
        s->in_child=true;const struct mt76_rf_wait_io child={{b,child_authorize,child_read,child_exchange},child_delay};s->child_status=mt76_cvco5_capture(&s->vco,s->nic0,s->nic1,s->factory_trim,&child);s->in_child=false;
        if(s->child_status!=MT76_TIMING_OK || !mt76_cvco5_valid(&s->vco))return false;
        if(b->inspect){b->inspected=true;if(!b->inspect(b->rf->command.context) || !mt76_channel_vco5_selected(s) || !mt76_cvco5_valid(&s->vco))return false;}
    }
    return mt76_channel_vco5_owned(s) && b->rf->command.authorize(b->rf->command.context) && mt76_channel_vco5_owned(s);
}
static bool read_value(void *p,uint32_t a,uint32_t *v){struct bridge *b=p;return v && !b->s->in_child && mt76_channel_vco5_transfer_valid(b->s,false,a,0) && b->rf->command.read(b->rf->command.context,a,v);}
static int exchange(void *p,const uint8_t *packet,size_t n,uint8_t *reply,size_t cap){
    struct bridge *b=p;uint8_t allowed[16];
    if(b->s->in_child || !packet || !reply || n!=16 || cap<MT76_MCU_RESPONSE_MAX || mt76_channel_vco5_packet(b->s,allowed,16)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(allowed[i]!=packet[i])return -1;
    return b->rf->command.exchange(b->rf->command.context,packet,n,reply,cap);
}
static bool mac_transfer(void *p,unsigned op,bool write,uint32_t a,uint8_t *data,unsigned n){
    struct bridge *b=p;if(!data || n!=4 || b->s->in_child)return false;uint32_t v=0;for(unsigned i=0;i<4;i++)v|=(uint32_t)data[i]<<(8*i);
    return mt76_channel_vco5_transfer_valid(b->s,write,a,v) && b->mac->transfer(b->mac->context,op,write,a,data,n);
}
static bool pa_transfer(void *p,bool write,uint32_t a,uint32_t *v){struct bridge *b=p;return v && !b->s->in_child && mt76_channel_vco5_transfer_valid(b->s,write,a,*v) && b->pa->transfer(b->pa->context,write,a,v);}
static bool bbp_transfer(void *p,bool write,uint32_t a,uint32_t *v){struct bridge *b=p;return v && !b->s->in_child && mt76_channel_vco5_transfer_valid(b->s,write,a,*v) && b->bbp->transfer(b->bbp->context,write,a,v);}
static enum mt76_mcu_timing_status capture_for(bool (*inspect)(void *),struct mt76_channel_vco5 *s,uint16_t nic0,uint16_t nic1,unsigned trim,unsigned lna,const struct mt76_rf_wait_io *rf,const struct mt76_band_mac_io *mac,const struct mt76_pa5_io *pa,const struct mt76_bbp5_io *bbp){
    if(!s || s->attempted || nic0!=0xfd11 || nic1!=0x1004 || trim!=0x44 || lna>255 || !rf || !rf->command.authorize || !rf->command.read || !rf->command.exchange || !rf->delay || !mac || !pa || !bbp || !mac->transfer || !pa->transfer || !pa->exchange || !bbp->transfer || rf->command.context!=mac->context || mac->context!=pa->context || pa->context!=bbp->context || rf->command.authorize!=mac->authorize || mac->authorize!=pa->authorize || pa->authorize!=bbp->authorize || pa->exchange!=rf->command.exchange)return MT76_TIMING_REFUSED;
    *s=(struct mt76_channel_vco5){.attempted=true,.nic0=nic0,.nic1=nic1,.factory_trim=trim,.lna=lna,.child_status=MT76_TIMING_REFUSED};
    struct bridge b={.s=s,.rf=rf,.mac=mac,.pa=pa,.bbp=bbp,.inspect=inspect};const struct mt76_mcu_timing_io outer={&b,authorize,read_value,exchange};
    const struct mt76_band_mac_io m={&b,authorize,mac_transfer};const struct mt76_pa5_io p={&b,authorize,pa_transfer,exchange};const struct mt76_bbp5_io v={&b,authorize,bbp_transfer};
    enum mt76_mcu_timing_status status=mt76_trim5_channel_capture(&s->trim,nic0,nic1,trim,lna,&outer,&m,&p,&v);
    if(status!=MT76_TIMING_OK)return s->vco.attempted && s->child_status!=MT76_TIMING_OK?s->child_status:status;
    if(!mt76_channel_vco5_owned(s) || (inspect && !b.inspected))return MT76_TIMING_STATE;
    if(!rf->command.authorize(rf->command.context))return MT76_TIMING_CANCELLED;
    if(!mt76_channel_vco5_owned(s))return MT76_TIMING_STATE;
    s->complete=s->restored=true;return mt76_channel_vco5_valid(s)?MT76_TIMING_OK:MT76_TIMING_STATE;
}

enum mt76_mcu_timing_status mt76_channel_vco5_capture(struct mt76_channel_vco5 *s,uint16_t n0,uint16_t n1,unsigned trim,unsigned lna,const struct mt76_rf_wait_io *r,const struct mt76_band_mac_io *m,const struct mt76_pa5_io *p,const struct mt76_bbp5_io *b){return capture_for(NULL,s,n0,n1,trim,lna,r,m,p,b);}
enum mt76_mcu_timing_status mt76_channel_vco5_inspect(struct mt76_channel_vco5 *s,uint16_t n0,uint16_t n1,unsigned trim,unsigned lna,const struct mt76_rf_wait_io *r,const struct mt76_band_mac_io *m,const struct mt76_pa5_io *p,const struct mt76_bbp5_io *b,bool (*inspect)(void *)){if(!inspect)return MT76_TIMING_REFUSED;return capture_for(inspect,s,n0,n1,trim,lna,r,m,p,b);}
