#include <srvros/mt76_channel_vco5.h>
/* Fixed selected-channel child: read trim44, one RF0:4 trigger, bounded
 * observations, read trim44. No trim writes or force-clear. Parent owns all
 * selected band/synth/A20/PA/BBP state and inverse originals. */
static const uint32_t commands[11][3]={
    {0x80000016,0x44,0},{0x80000004,0x30,0},{0x80000004,0xb0,1},
    {0x80000004,0x30,0},{0x80000004,0x30,0},{0x80000004,0x30,0},
    {0x80000004,0x30,0},{0x80000004,0x30,0},{0x80000004,0x30,0},
    {0x80000004,0x30,0},{0x80000016,0x44,0}
};
static uint32_t address(unsigned i){return i==0?0x1004:i==1?0x2000:0x208;}
static bool guard(unsigned i,uint32_t v){return i==0?v==0:i==1?v==0xf000f200:v!=UINT32_MAX && !(v&15);}
static uint32_t header(unsigned q){return 0x4a000008u|(((q+1)%15+1)<<16);}
static bool response(unsigned q,const uint32_t *w){
    return w[0]==header(q) && w[1]==commands[q][0] && w[3]==header(q) &&
        (q>=3 && q<=9?(w[2]==0x30 || w[2]==0xb0):w[2]==commands[q][1]);
}
static bool proof(const struct mt76_cvco5 *s,unsigned op){
    if(!s || !s->attempted || s->trim!=0x44 || op>77 || s->operations!=op)return false;
    unsigned q=op/7,n=op%7,exchanges=q+(n>3),writes=0,first=0;
    for(unsigned j=0;j<exchanges;j++){
        if(!response(j,s->words[j]))return false;
        if(j>=3 && j<=9){if(s->words[j][2]==0x30 && !first)first=j-2;else if(s->words[j][2]==0xb0 && first)return false;}
        writes+=commands[j][2];
    }
    for(unsigned i=0;i<op;i++)if(i%7!=3){unsigned k=i%7,j=k<3?k:k-4;if(!guard(j,s->guards[(i/7)*2+(k>=4)][j]))return false;}
    unsigned minwait=q<4?0:q<=9?q-4+(n>0):6,maxwait=minwait+((q>=4 && q<=9 && !n)?1u:0u),delay=0;
    if(s->waits<minwait || s->waits>maxwait || s->waits>6)return false;
    for(unsigned j=0;j<s->waits;j++)delay+=mt76_rf_wait_delay(j);
    return s->first_clear==first && s->delay_us==delay && s->reads==op-exchanges && s->exchanges==exchanges && s->writes==writes && s->received==(exchanges?16:-1);
}
bool mt76_cvco5_owned(const struct mt76_cvco5 *s){return s && !s->complete && !s->trim_selected && !s->triggered && proof(s,s->operations);}
bool mt76_cvco5_valid(const struct mt76_cvco5 *s){return s && s->complete && s->trim_selected && s->triggered && s->first_clear && s->words[9][2]==0x30 && proof(s,77);}
bool mt76_cvco5_delay_admit(const struct mt76_cvco5 *s){return mt76_cvco5_owned(s) && s->operations>=28 && s->operations<70 && !(s->operations%7) && s->waits==s->operations/7-4;}
static bool ready(const struct mt76_cvco5 *s){
    if(!mt76_cvco5_owned(s) || s->operations>=77)return false;
    unsigned q=s->operations/7;
    return (q<4 || q>9 || s->waits==q-3) && (q<10 || (s->first_clear && s->words[9][2]==0x30));
}
bool mt76_cvco5_transfer_valid(const struct mt76_cvco5 *s,bool write,uint32_t a){
    if(!ready(s) || s->operations%7==3 || write)return false;
    unsigned n=s->operations%7;return a==address(n<3?n:n-4);
}
size_t mt76_cvco5_packet(const struct mt76_cvco5 *s,uint8_t *out,size_t cap){
    if(!ready(s) || s->operations%7!=3)return 0;
    unsigned q=s->operations/7;uint8_t p[8]={0};
    for(unsigned b=0;b<4;b++){p[b]=(uint8_t)(commands[q][0]>>(8*b));if(commands[q][2])p[4+b]=(uint8_t)(commands[q][1]>>(8*b));}
    return mt76_mcu_encode(commands[q][2]?12:10,(q+1)%15+1,p,8,out,cap);
}
enum mt76_mcu_timing_status mt76_cvco5_capture(struct mt76_cvco5 *s,uint16_t nic0,uint16_t nic1,unsigned trim,const struct mt76_rf_wait_io *io){
    if(!s || s->attempted || nic0!=0xfd11 || nic1!=0x1004 || trim!=0x44 || !io || !io->command.authorize || !io->command.read || !io->command.exchange || !io->delay)return MT76_TIMING_REFUSED;
    const struct mt76_mcu_timing_io *rf=&io->command;*s=(struct mt76_cvco5){.attempted=true,.trim=trim,.received=-1};
    for(unsigned op=0;op<77;op++){
        unsigned q=op/7,n=op%7;
        if(q>=4 && q<=9 && !n){
            if(!mt76_cvco5_delay_admit(s))return MT76_TIMING_STATE;
            if(!rf->authorize(rf->context))return MT76_TIMING_CANCELLED;
            if(!mt76_cvco5_delay_admit(s))return MT76_TIMING_STATE;
            unsigned us=mt76_rf_wait_delay(q-4);if(!io->delay(rf->context,us))return MT76_TIMING_IO;
            s->waits++;s->delay_us+=us;
        }
        if(!ready(s))return MT76_TIMING_STATE;
        if(!rf->authorize(rf->context))return MT76_TIMING_CANCELLED;
        if(!ready(s))return MT76_TIMING_STATE;
        if(n!=3){
            unsigned j=n<3?n:n-4;uint32_t *v=&s->guards[q*2+(n>=4)][j];
            if(!rf->read(rf->context,address(j),v))return MT76_TIMING_IO;
            s->reads++;s->operations++;if(!guard(j,*v))return MT76_TIMING_STATE;
        }else{
            uint8_t packet[16],reply[MT76_MCU_RESPONSE_MAX]={0};if(mt76_cvco5_packet(s,packet,16)!=16)return MT76_TIMING_STATE;
            s->received=rf->exchange(rf->context,packet,16,reply,sizeof reply);if(s->received<0)return MT76_TIMING_IO;if(s->received!=16)return MT76_TIMING_REPLY;
            for(unsigned i=0;i<4;i++)for(unsigned b=0;b<4;b++)s->words[q][i]|=(uint32_t)reply[4*i+b]<<(8*b);
            if(!response(q,s->words[q]))return MT76_TIMING_REPLY;
            if(q>=3 && q<=9 && s->words[q][2]==0x30 && !s->first_clear)s->first_clear=q-2;
            s->operations++;s->exchanges++;s->writes+=commands[q][2];
        }
    }
    if(!rf->authorize(rf->context))return MT76_TIMING_CANCELLED;
    if(!mt76_cvco5_owned(s))return MT76_TIMING_STATE;
    s->complete=s->trim_selected=s->triggered=true;
    return mt76_cvco5_valid(s)?MT76_TIMING_OK:MT76_TIMING_STATE;
}
