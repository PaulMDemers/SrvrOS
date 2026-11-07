#include <srvros/mt76_trim5.h>
/* RF0:22 is disjoint from the accepted band/frequency/A20/PA/BBP targets.
 * Original, select, independent selected read, inverse write, original read. */
static bool write_command(unsigned q){return q==1 || q==3;}
static unsigned sequence(unsigned q){return (q+11)%15+1;}
static uint32_t expected(unsigned q){return q==1 || q==2?0x44:0;}
static uint32_t guard_address(unsigned n){return n==0?0x1004:n==1?0x2000:0x208;}
static bool guard_valid(unsigned n,uint32_t v){return n==0?v==0:n==1?v==0xf000f200:v!=UINT32_MAX && !(v&15);}
static bool trim_state(const struct mt76_trim5 *s,unsigned op){
    if(!s || !s->attempted || s->trim!=0x44 || s->lna>255 || (!s->with_channel && s->lna) || op>35 || s->operations!=op)return false;
    unsigned exchanges=op/7+(op%7>3),writes=0;
    for(unsigned q=0;q<exchanges;q++){
        const uint32_t *w=s->words[q];uint32_t h=0x4a000008u|(sequence(q)<<16);
        if(w[0]!=h || w[1]!=0x80000016u || w[2]!=expected(q) || w[3]!=h)return false;
        writes+=write_command(q);
    }
    for(unsigned i=0;i<op;i++)if(i%7!=3){unsigned n=i%7,j=n<3?n:n-4;if(!guard_valid(j,s->guards[(i/7)*2+(n>=4)][j]))return false;}
    return s->reads==op-exchanges && s->exchanges==exchanges && s->writes==writes && s->received==(exchanges?16:-1);
}
static bool child_valid(const struct mt76_trim5 *s){return s->with_channel?s->channel.lna==s->lna && mt76_channel5_valid(&s->channel):!s->channel.attempted;}
bool mt76_trim5_owned(const struct mt76_trim5 *s){
    if(!s || s->complete || s->restored || !trim_state(s,s->operations))return false;
    if(s->phase==2)return s->with_channel && trim_state(s,21) && s->channel.lna==s->lna && mt76_channel5_owned(&s->channel);
    if(s->phase!=1)return false;
    if(!s->with_channel)return !s->channel.attempted;
    return s->operations<21?!s->channel.attempted:child_valid(s);
}
bool mt76_trim5_valid(const struct mt76_trim5 *s){return s && s->complete && s->restored && s->phase==3 && trim_state(s,35) && child_valid(s);}
unsigned mt76_trim5_operations(const struct mt76_trim5 *s){
    if(!s || !s->attempted || s->operations>35)return UINT32_MAX;
    unsigned n=s->channel.attempted?mt76_channel5_operations(&s->channel):0;
    return n<=3732?s->operations+n:UINT32_MAX;
}
unsigned mt76_trim5_phase(const struct mt76_trim5 *s){return !s?UINT32_MAX:s->phase==1?13:s->phase==2?mt76_channel5_phase(&s->channel):s->phase==3?14:UINT32_MAX;}
unsigned mt76_trim5_local(const struct mt76_trim5 *s){return !s?UINT32_MAX:s->phase==1?s->operations:s->phase==2?mt76_channel5_local(&s->channel):UINT32_MAX;}
bool mt76_trim5_transfer_valid(const struct mt76_trim5 *s,bool write,uint32_t a,uint32_t v){
    if(!mt76_trim5_owned(s))return false;
    if(s->phase==2)return mt76_channel5_transfer_valid(&s->channel,write,a,v);
    unsigned n=s->operations%7;return s->operations<35 && n!=3 && !write && a==guard_address(n<3?n:n-4);
}
size_t mt76_trim5_packet(const struct mt76_trim5 *s,uint8_t *out,size_t cap){
    if(!mt76_trim5_owned(s))return 0;
    if(s->phase==2)return mt76_channel5_packet(&s->channel,out,cap);
    if(s->operations>=35 || s->operations%7!=3)return 0;
    unsigned q=s->operations/7;uint8_t p[8]={0x16,0,0,0x80,0,0,0,0};
    if(write_command(q))p[4]=(uint8_t)expected(q);
    return mt76_mcu_encode(write_command(q)?12:10,sequence(q),p,8,out,cap);
}
static enum mt76_mcu_timing_status capture(struct mt76_trim5 *s,uint16_t nic0,uint16_t nic1,unsigned trim,unsigned lna,const struct mt76_mcu_timing_io *rf,const struct mt76_band_mac_io *mac,const struct mt76_pa5_io *pa,const struct mt76_bbp5_io *bbp,bool channel){
    if(!s || s->attempted || nic0!=0xfd11 || nic1!=0x1004 || trim!=0x44 || lna>255 || !rf || !rf->authorize || !rf->read || !rf->exchange)return MT76_TIMING_REFUSED;
    if(channel && (!mac || !pa || !bbp || !mac->transfer || !pa->transfer || !pa->exchange || !bbp->transfer ||
        rf->context!=mac->context || rf->context!=pa->context || rf->context!=bbp->context || rf->authorize!=mac->authorize || rf->authorize!=pa->authorize || rf->authorize!=bbp->authorize))return MT76_TIMING_REFUSED;
    *s=(struct mt76_trim5){.attempted=true,.with_channel=channel,.phase=1,.trim=trim,.lna=lna,.received=-1};
    for(unsigned op=0;op<35;op++){
        if(op==21 && channel){
            if(!trim_state(s,21) || s->channel.attempted)return MT76_TIMING_STATE;
            s->phase=2;enum mt76_mcu_timing_status status=mt76_channel5_capture(&s->channel,nic0,nic1,lna,rf,mac,pa,bbp);
            if(status!=MT76_TIMING_OK)return status;
            if(!child_valid(s))return MT76_TIMING_STATE;
            s->phase=1;
        }
        if(!mt76_trim5_owned(s))return MT76_TIMING_STATE;
        if(!rf->authorize(rf->context))return MT76_TIMING_CANCELLED;
        unsigned q=op/7,n=op%7;
        if(n!=3){
            unsigned j=n<3?n:n-4;uint32_t *v=&s->guards[q*2+(n>=4)][j];
            if(!rf->read(rf->context,guard_address(j),v))return MT76_TIMING_IO;
            s->reads++;s->operations++;if(!guard_valid(j,*v))return MT76_TIMING_STATE;
        }else{
            uint8_t packet[16],reply[MT76_MCU_RESPONSE_MAX]={0};if(mt76_trim5_packet(s,packet,16)!=16)return MT76_TIMING_STATE;
            s->received=rf->exchange(rf->context,packet,16,reply,sizeof reply);if(s->received<0)return MT76_TIMING_IO;if(s->received!=16)return MT76_TIMING_REPLY;
            for(unsigned i=0;i<4;i++)for(unsigned b=0;b<4;b++)s->words[q][i]|=(uint32_t)reply[4*i+b]<<(8*b);
            uint32_t h=0x4a000008u|(sequence(q)<<16);const uint32_t *w=s->words[q];
            if(w[0]!=h || w[1]!=0x80000016u || w[2]!=expected(q) || w[3]!=h)return MT76_TIMING_REPLY;
            s->exchanges++;s->writes+=write_command(q);s->operations++;
        }
    }
    if(!rf->authorize(rf->context))return MT76_TIMING_CANCELLED;
    if(!mt76_trim5_owned(s))return MT76_TIMING_STATE;
    s->phase=3;s->complete=s->restored=true;return mt76_trim5_valid(s)?MT76_TIMING_OK:MT76_TIMING_STATE;
}
enum mt76_mcu_timing_status mt76_trim5_capture(struct mt76_trim5 *s,uint16_t n0,uint16_t n1,unsigned trim,const struct mt76_mcu_timing_io *rf){return capture(s,n0,n1,trim,0,rf,NULL,NULL,NULL,false);}
enum mt76_mcu_timing_status mt76_trim5_channel_capture(struct mt76_trim5 *s,uint16_t n0,uint16_t n1,unsigned trim,unsigned lna,const struct mt76_mcu_timing_io *rf,const struct mt76_band_mac_io *mac,const struct mt76_pa5_io *pa,const struct mt76_bbp5_io *bbp){return capture(s,n0,n1,trim,lna,rf,mac,pa,bbp,true);}
