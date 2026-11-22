#include <srvros/mt76_dac5.h>
/* address, independent expected reply value, write. Read requests carry zero.
 * Preserve the three upstream read/modify/write pairs; add independent final
 * DAC/trim reads before inverse trim restoration. VCO register is absent. */
static const uint32_t commands[13][3]={
    {0x80000016,0,0},{0x80000016,0x44,1},{0x80000016,0x44,0},
    {0x80000049,0x93,0},{0x80000049,0x93,1},{0x80000049,0x93,0},
    {0x80000049,0x13,1},{0x80000049,0x13,0},
    {0x80000049,0x93,1},{0x80000049,0x93,0},
    {0x80000016,0x44,0},{0x80000016,0,1},{0x80000016,0,0}
};
static uint32_t address(unsigned i){return i==0?0x1004:i==1?0x2000:0x208;}
static bool guard(unsigned i,uint32_t v){return i==0?v==0:i==1?v==0xf000f200:v!=UINT32_MAX && !(v&15);}
static uint32_t header(unsigned q){return 0x4a000008u|((q+2)<<16);}
static bool proof(const struct mt76_dac5 *s,unsigned op){
    if(!s || !s->attempted || s->trim!=0x44 || op>91 || s->operations!=op)return false;
    unsigned exchanges=op/7+(op%7>3),writes=0;
    for(unsigned q=0;q<exchanges;q++){
        const uint32_t *w=s->words[q];
        if(w[0]!=header(q) || w[1]!=commands[q][0] || w[2]!=commands[q][1] || w[3]!=header(q))return false;
        writes+=commands[q][2];
    }
    for(unsigned i=0;i<op;i++)if(i%7!=3){unsigned n=i%7,j=n<3?n:n-4;if(!guard(j,s->guards[(i/7)*2+(n>=4)][j]))return false;}
    return s->reads==op-exchanges && s->exchanges==exchanges && s->writes==writes && s->received==(exchanges?16:-1);
}
bool mt76_dac5_owned(const struct mt76_dac5 *s){return s && !s->complete && !s->words_restored && !s->dac_reset && proof(s,s->operations);}
bool mt76_dac5_valid(const struct mt76_dac5 *s){return s && s->complete && s->words_restored && s->dac_reset && proof(s,91);}
bool mt76_dac5_transfer_valid(const struct mt76_dac5 *s,bool write,uint32_t a){
    if(!mt76_dac5_owned(s) || s->operations>=91 || s->operations%7==3 || write)return false;
    unsigned n=s->operations%7;return a==address(n<3?n:n-4);
}
size_t mt76_dac5_packet(const struct mt76_dac5 *s,uint8_t *out,size_t cap){
    if(!mt76_dac5_owned(s) || s->operations>=91 || s->operations%7!=3)return 0;
    unsigned q=s->operations/7;uint8_t p[8]={0};
    for(unsigned b=0;b<4;b++){p[b]=(uint8_t)(commands[q][0]>>(8*b));if(commands[q][2])p[4+b]=(uint8_t)(commands[q][1]>>(8*b));}
    return mt76_mcu_encode(commands[q][2]?12:10,q+2,p,8,out,cap);
}
enum mt76_mcu_timing_status mt76_dac5_capture(struct mt76_dac5 *s,uint16_t nic0,uint16_t nic1,unsigned trim,const struct mt76_mcu_timing_io *rf){
    if(!s || s->attempted || nic0!=0xfd11 || nic1!=0x1004 || trim!=0x44 || !rf || !rf->authorize || !rf->read || !rf->exchange)return MT76_TIMING_REFUSED;
    *s=(struct mt76_dac5){.attempted=true,.trim=trim,.received=-1};
    for(unsigned op=0;op<91;op++){
        if(!mt76_dac5_owned(s))return MT76_TIMING_STATE;
        if(!rf->authorize(rf->context))return MT76_TIMING_CANCELLED;
        if(!mt76_dac5_owned(s))return MT76_TIMING_STATE;
        unsigned q=op/7,n=op%7;
        if(n!=3){
            unsigned j=n<3?n:n-4;uint32_t *v=&s->guards[q*2+(n>=4)][j];
            if(!rf->read(rf->context,address(j),v))return MT76_TIMING_IO;
            s->reads++;s->operations++;if(!guard(j,*v))return MT76_TIMING_STATE;
        }else{
            uint8_t packet[16],reply[MT76_MCU_RESPONSE_MAX]={0};if(mt76_dac5_packet(s,packet,16)!=16)return MT76_TIMING_STATE;
            s->received=rf->exchange(rf->context,packet,16,reply,sizeof reply);if(s->received<0)return MT76_TIMING_IO;if(s->received!=16)return MT76_TIMING_REPLY;
            for(unsigned i=0;i<4;i++)for(unsigned b=0;b<4;b++)s->words[q][i]|=(uint32_t)reply[4*i+b]<<(8*b);
            const uint32_t *w=s->words[q];if(w[0]!=header(q) || w[1]!=commands[q][0] || w[2]!=commands[q][1] || w[3]!=header(q))return MT76_TIMING_REPLY;
            s->operations++;s->exchanges++;s->writes+=commands[q][2];
        }
    }
    if(!rf->authorize(rf->context))return MT76_TIMING_CANCELLED;
    if(!mt76_dac5_owned(s))return MT76_TIMING_STATE;
    s->complete=s->words_restored=s->dac_reset=true;
    return mt76_dac5_valid(s)?MT76_TIMING_OK:MT76_TIMING_STATE;
}
