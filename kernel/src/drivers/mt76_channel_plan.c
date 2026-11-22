#include <srvros/mt76_channel_plan.h>

/* Linux v7.0 mt76x0_frequency_plan US rows; non-SDM21-operation projection.
 * Variable columns: R35,R33,R32 low,R31 low,pll_n,R24 low. */
static const uint8_t fields[11][6]={
    {0x28,0xe2,2,2,0x28,3},{0x3c,0xe4,7,2,0xa1,1},
    {0x3c,0xe2,7,11,0x50,0},{0x28,0xd4,2,9,0x50,0},
    {0x3c,0xd4,7,2,0xa2,1},{0x3c,0xd4,7,7,0xa2,1},
    {0x28,0xe2,2,7,0x28,3},{0x3c,0xd4,7,2,0xa3,1},
    {0x3c,0xf2,7,13,0x28,3},{0x28,0xd4,2,9,0x51,0},
    {0x3c,0xd4,7,2,0xa4,1}
};
static const uint8_t regs[21]={37,36,35,34,33,32,32,31,31,30,30,30,29,30,28,28,28,26,27,28,24};
static const uint8_t masks[21]={255,255,255,255,255,224,31,224,31,128,124,2,255,1,192,48,12,255,255,3,3};
static const uint8_t unique[13]={37,36,35,34,33,32,31,30,29,28,26,27,24};
static const uint8_t initial[13]={2,63,40,221,226,66,66,2,40,48,0,0,51};
bool mt76_channel_plan(unsigned channel,struct mt76_channel_step out[21]) {
    if(!out)return false;
    for(unsigned i=0;i<21;i++)out[i]=(struct mt76_channel_step){0};
    if(channel<1 || channel>11)return false;
    const uint8_t *f=fields[channel-1];
    const uint8_t bits[21]={2,63,f[0],221,f[1],64,f[2],64,f[3],0,0,2,f[4],0,0,48,0,0,0,0,f[5]};
    for(unsigned i=0;i<21;i++)out[i]=(struct mt76_channel_step){regs[i],masks[i],bits[i]};
    return true;
}
bool mt76_channel_5ghz_plan(unsigned channel,struct mt76_channel_step out[21]) {
    if(!out)return false;
    for(unsigned i=0;i<21;i++)out[i]=(struct mt76_channel_step){0};
    if(channel!=161)return false;
    /* Pinned Linux v7.0 non-SDM RF_A_BAND|RF_A_BAND_HB row, 5805MHz.
     * Pure frequency data; no native caller or 2GHz saved-state reuse. */
    static const uint8_t bits[21]={2,63,104,221,210,64,16,64,9,0,0,2,48,0,0,48,0,0,0,0,3};
    for(unsigned i=0;i<21;i++)out[i]=(struct mt76_channel_step){regs[i],masks[i],bits[i]};
    return true;
}
unsigned mt76_channel_snapshot_reg(unsigned i){return i<13?unique[i]:UINT32_MAX;}
unsigned mt76_channel_snapshot_initial(unsigned i){return i<13?initial[i]:UINT32_MAX;}
static void fail(struct mt76_channel_preview *s,enum mt76_channel_status status) {
    s->failed=true;if(s->status==MT76_CHANNEL_OK)s->status=status;
}
static bool owned(struct mt76_channel_preview *s,const struct mt76_channel_preview_io *io,bool cleanup) {
    if(io->owned(io->context,cleanup))return true;
    fail(s,MT76_CHANNEL_OWNER);return false;
}
static bool guards(struct mt76_channel_preview *s,const struct mt76_channel_preview_io *io,bool cleanup) {
    const uint32_t address[4]={0x1004,0x2000,0x208,0x1200};
    const uint32_t expected[4]={0,0xf000f200,0x40000050,0};bool ok=true;
    for(unsigned i=0;i<4;i++) {
        if(!owned(s,io,cleanup)){ok=false;if(!cleanup)break;else continue;}
        uint32_t v=0;s->guard_reads++;
        if(!io->read_guard(io->context,address[i],&v,cleanup)){fail(s,MT76_CHANNEL_IO);ok=false;}
        else if(v!=expected[i]){fail(s,MT76_CHANNEL_STATE);ok=false;}
        if(!ok && !cleanup)break;
    }
    return ok;
}
static bool read_rf(struct mt76_channel_preview *s,const struct mt76_channel_preview_io *io,
                    unsigned index,uint8_t *v,bool cleanup) {
    if(!owned(s,io,cleanup))return false;
    s->rf_reads++;if(cleanup)s->cleanup_reads++;
    if(io->read_rf(io->context,unique[index],v,cleanup))return true;
    fail(s,MT76_CHANNEL_IO);return false;
}
static bool write_rf(struct mt76_channel_preview *s,const struct mt76_channel_preview_io *io,
                     unsigned index,uint8_t v,bool cleanup) {
    if(!owned(s,io,cleanup))return false;
    s->rf_writes++;if(cleanup)s->cleanup_writes++;
    else s->touched_mask|=(uint16_t)(1u<<index); /* Before ambiguous publication. */
    if(io->write_rf(io->context,unique[index],v,cleanup))return true;
    fail(s,MT76_CHANNEL_IO);return false;
}
static bool plan_valid(const struct mt76_channel_preview_plan *p) {
    if(!p || p->country[0]!='U' || p->country[1]!='S' || p->bandwidth!=20 ||
       !p->count || p->count>11)return false;
    unsigned mask=0;
    for(unsigned i=0;i<p->count;i++) {
        unsigned ch=p->channel[i];if(ch<1 || ch>11 || (mask&(1u<<ch)))return false;
        mask|=1u<<ch;
    }
    for(unsigned i=p->count;i<11;i++)if(p->channel[i])return false;
    return true;
}
bool mt76_channel_preview_run(struct mt76_channel_preview *s,
    const struct mt76_channel_preview_plan *plan,const struct mt76_channel_preview_io *io) {
    if(!s || s->attempted)return false;
    *s=(struct mt76_channel_preview){.attempted=true};
    if(!plan_valid(plan) || !io || !io->owned || !io->read_guard || !io->read_rf || !io->write_rf) {
        fail(s,MT76_CHANNEL_REFUSED);return false;
    }
    const struct mt76_channel_preview_plan p=*plan;s->plan=p;
    const struct mt76_channel_preview_io f=*io;
    uint8_t current[13]={0};bool ok=guards(s,&f,false);
    for(unsigned i=0;i<13 && ok;i++) {
        ok=read_rf(s,&f,i,&current[i],false);
        if(ok) {
            s->saved[i]=current[i];s->saved_mask|=(uint16_t)(1u<<i);
            if(current[i]!=initial[i]){fail(s,MT76_CHANNEL_STATE);ok=false;}
        }
    }
    for(unsigned c=0;c<p.count && ok;c++) {
        struct mt76_channel_step steps[21];
        if(!mt76_channel_plan(p.channel[c],steps)){fail(s,MT76_CHANNEL_STATE);ok=false;break;}
        for(unsigned k=0;k<21 && ok;k++) {
            unsigned i=0;while(i<13 && unique[i]!=steps[k].reg)i++;
            if(i==13){fail(s,MT76_CHANNEL_STATE);ok=false;break;}
            ok=guards(s,&f,false);uint8_t before=0,after=0;
            if(ok)ok=read_rf(s,&f,i,&before,false);
            if(ok && before!=current[i]){fail(s,MT76_CHANNEL_STATE);ok=false;}
            if(!ok)break;
            s->before[c][k]=before;
            uint8_t target=(uint8_t)((before&~steps[k].mask)|steps[k].bits);
            bool written=write_rf(s,&f,i,target,false);
            /* A failed write can still have changed hardware. Independently
             * verify under cleanup ownership before restoring, never retry. */
            bool verified=read_rf(s,&f,i,&after,!written);
            if(verified && after!=target){fail(s,MT76_CHANNEL_STATE);verified=false;}
            s->after[c][k]=after;
            ok=written && verified;
            if(ok){current[i]=after;s->steps_completed++;ok=guards(s,&f,false);}
        }
        if(ok)s->channels_completed++;
    }
    if(s->touched_mask) {
        s->cleanup_attempted=true;bool clean=true;
        /* A fresh cleanup guard failure forbids unsafe RF access. No polling,
         * retry or attempt to regain ownership; retain the failed evidence. */
        clean=guards(s,&f,true);
        if(clean)for(unsigned n=13;n>0;n--) {
            unsigned i=n-1;if(!(s->touched_mask&(1u<<i)))continue;
            bool written=write_rf(s,&f,i,s->saved[i],true);uint8_t seen=0;
            bool verified=read_rf(s,&f,i,&seen,true);
            if(verified && seen==s->saved[i])s->restored_mask|=(uint16_t)(1u<<i);
            else {fail(s,MT76_CHANNEL_STATE);verified=false;}
            clean=written && verified && clean;
        }
        bool edge=guards(s,&f,true);
        s->cleanup_complete=clean && edge && s->restored_mask==s->touched_mask;
    } else s->cleanup_complete=guards(s,&f,true);
    s->complete=ok && !s->failed && s->cleanup_complete && owned(s,&f,true);
    return s->complete;
}
