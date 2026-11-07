#include <srvros/mt76_channel_tune.h>
struct tune {
    struct mt76_channel_tune *s;
    const struct mt76_channel_tune_io *io;
};
static bool fail(struct tune *t,enum mt76_channel_status status) {
    t->s->failed=true;
    if(t->s->status==MT76_CHANNEL_OK)t->s->status=status;
    return false;
}
static bool owned(struct tune *t,bool clean) {
    return t->io->owned(t->io->context,clean) || fail(t,MT76_CHANNEL_OWNER);
}
static bool guards(struct tune *t,bool clean) {
    static const uint32_t a[4]={0x1004,0x2000,0x208,0x1200};
    static const uint32_t expected[4]={0,0xf000f200,0x40000050,0};
    bool ok=true;
    for(unsigned i=0;i<4;i++) {
        if(!owned(t,clean))return false;
        uint32_t v=0;t->s->guard_reads++;
        if(!t->io->read_guard(t->io->context,a[i],&v,clean))ok=fail(t,MT76_CHANNEL_IO);
        else if(v!=expected[i])ok=fail(t,MT76_CHANNEL_STATE);
        if(!ok)return false;
    }
    return true;
}
static bool read_rf(struct tune *t,unsigned reg,uint8_t *v,bool clean) {
    if(!owned(t,clean))return false;
    t->s->rf_reads++;
    return t->io->read_rf(t->io->context,(uint8_t)reg,v,clean) || fail(t,MT76_CHANNEL_IO);
}
static bool write_rf(struct tune *t,unsigned reg,uint8_t v,bool clean) {
    if(!owned(t,clean))return false;
    t->s->rf_writes++;
    /* Record possible publication only after ownership, before the callback. */
    if(reg==4) {
        if(clean)t->s->cleanup_vco_attempted=true;
        else t->s->vco_attempted=true;
    } else if(!clean)for(unsigned i=0;i<MT76_CHANNEL_REGS;i++)
        if(mt76_channel_snapshot_reg(i)==reg)t->s->touched_mask|=(uint16_t)(1u<<i);
    return t->io->write_rf(t->io->context,(uint8_t)reg,v,clean) || fail(t,MT76_CHANNEL_IO);
}
static bool vco_idle(struct tune *t,bool clean) {
    uint8_t v=0;
    return read_rf(t,4,&v,clean) && (v==0x30 || fail(t,MT76_CHANNEL_STATE));
}
static bool settle(struct tune *t,bool clean) {
    /* Poll the trigger bit only; do not describe it as a lock detector. Linux
     * v7.0 scan path sets bank0R4 bit7 after frequency/band/BBP setup. Additional
     * conservative bounded waits are SrvrOS policy, not a claimed Linux timeout. */
    static const unsigned delay[6]={0,10,90,900,9000,90000};
    if(!guards(t,clean) || !vco_idle(t,clean) || !write_rf(t,4,0xb0,clean))return false;
    for(unsigned i=0;i<6;i++) {
        if(delay[i]) {
            if(!owned(t,clean))return false;
            if(!t->io->delay(t->io->context,delay[i],clean))return fail(t,MT76_CHANNEL_IO);
            if(clean)t->s->cleanup_delay_us+=delay[i];else t->s->target_delay_us+=delay[i];
        }
        if(!guards(t,clean))return false;
        uint8_t v=0;
        if(!read_rf(t,4,&v,clean))return false;
        if(clean)t->s->cleanup_polls++;else t->s->target_polls++;
        if((v&0x7f)!=0x30)return fail(t,MT76_CHANNEL_STATE);
        if(!(v&0x80)) {
            if(!guards(t,clean))return false;
            if(clean)t->s->cleanup_vco_clear=true;else t->s->target_vco_clear=true;
            return true;
        }
    }
    return fail(t,MT76_CHANNEL_STATE);
}
static bool restore(struct tune *t) {
    struct mt76_channel_tune *s=t->s;s->cleanup_attempted=true;
    /* Busy/unknown VCO forbids changing RF frequency fields during calibration.
     * No repeated attempt to recover a quarantined transport or busy radio. */
    if(!guards(t,true) || !vco_idle(t,true))return false;
    bool ok=true;
    for(unsigned n=MT76_CHANNEL_REGS;n;n--) {
        unsigned i=n-1;if(!(s->touched_mask&(1u<<i)))continue;
        if(!guards(t,true)){ok=false;break;}
        bool written=write_rf(t,mt76_channel_snapshot_reg(i),s->saved[i],true);
        uint8_t v=0;bool seen=read_rf(t,mt76_channel_snapshot_reg(i),&v,true);
        if(seen && v==s->saved[i])s->restored_mask|=(uint16_t)(1u<<i);
        else {fail(t,MT76_CHANNEL_STATE);seen=false;}
        if(!written || !seen)ok=false;
        if(!guards(t,true)){ok=false;break;}
    }
    /* Do not pulse VCO against a partially restored or unproven frequency plan. */
    if(!ok || s->restored_mask!=s->touched_mask)return false;
    if(!settle(t,true))return false;
    for(unsigned i=0;i<MT76_CHANNEL_REGS;i++) {
        uint8_t v=0;
        if(!read_rf(t,mt76_channel_snapshot_reg(i),&v,true) || v!=s->saved[i])return fail(t,MT76_CHANNEL_STATE);
    }
    return guards(t,true) && owned(t,true);
}
static bool tune_run(struct mt76_channel_tune *s,const uint8_t country[2],
    unsigned channel,unsigned bandwidth,const struct mt76_channel_tune_io *io,
    const struct mt76_channel_tune_observer *observer) {
    if(!s || s->attempted)return false;
    *s=(struct mt76_channel_tune){.attempted=true,.channel=channel};
    if(!country || country[0]!='U' || country[1]!='S' || bandwidth!=20 ||
       channel<1 || channel>11 || !io || !io->owned || !io->read_guard ||
       !io->read_rf || !io->write_rf || !io->delay) {
        s->failed=true;s->status=MT76_CHANNEL_REFUSED;return false;
    }
    const struct mt76_channel_tune_io f=*io;struct tune t={s,&f};
    struct mt76_channel_step plan[MT76_CHANNEL_STEPS];
    bool ok=mt76_channel_plan(channel,plan) && guards(&t,false) && vco_idle(&t,false);
    uint8_t current[MT76_CHANNEL_REGS]={0};
    for(unsigned i=0;i<MT76_CHANNEL_REGS && ok;i++) {
        ok=read_rf(&t,mt76_channel_snapshot_reg(i),&current[i],false);
        if(ok) {
            s->saved[i]=current[i];s->saved_mask|=(uint16_t)(1u<<i);
            if(current[i]!=mt76_channel_snapshot_initial(i))ok=fail(&t,MT76_CHANNEL_STATE);
        }
    }
    for(unsigned k=0;k<MT76_CHANNEL_STEPS && ok;k++) {
        unsigned i=0;while(i<MT76_CHANNEL_REGS && mt76_channel_snapshot_reg(i)!=plan[k].reg)i++;
        if(i==MT76_CHANNEL_REGS){ok=fail(&t,MT76_CHANNEL_STATE);break;}
        uint8_t before=0,after=0;
        ok=guards(&t,false) && read_rf(&t,plan[k].reg,&before,false);
        if(ok && before!=current[i])ok=fail(&t,MT76_CHANNEL_STATE);
        if(!ok)break;
        uint8_t target=(uint8_t)((before&~plan[k].mask)|plan[k].bits);
        bool written=write_rf(&t,plan[k].reg,target,false);
        bool seen=read_rf(&t,plan[k].reg,&after,!written);
        if(seen && after!=target){seen=false;fail(&t,MT76_CHANNEL_STATE);}
        ok=written && seen && guards(&t,false);
        if(ok){current[i]=after;s->steps++;}
    }
    ok=ok && settle(&t,false);
    for(unsigned i=0;i<MT76_CHANNEL_REGS && ok;i++) {
        uint8_t v=0;ok=read_rf(&t,mt76_channel_snapshot_reg(i),&v,false);
        if(ok && v!=current[i])ok=fail(&t,MT76_CHANNEL_STATE);
    }
    ok=ok && guards(&t,false) && owned(&t,false);s->target_complete=ok;
    if(ok && observer) {
        /* Snapshot callback identity before publication; never re-enter it on a
         * retry. Independent cleanup remains mandatory if it fails or leaves
         * unproven receiver ownership. */
        const struct mt76_channel_tune_observer o=*observer;
        s->observe_attempted=true;
        bool seen=o.observe(o.context,channel);
        if(!seen)fail(&t,MT76_CHANNEL_IO);
        ok=seen && owned(&t,false) && guards(&t,false) && vco_idle(&t,false);
        for(unsigned i=0;i<MT76_CHANNEL_REGS && ok;i++) {
            uint8_t v=0;ok=read_rf(&t,mt76_channel_snapshot_reg(i),&v,false);
            if(ok && v!=current[i])ok=fail(&t,MT76_CHANNEL_STATE);
        }
        ok=ok && guards(&t,false) && owned(&t,false);s->observe_complete=ok;
    }
    if(s->touched_mask || s->vco_attempted)s->cleanup_complete=restore(&t);
    else s->cleanup_complete=guards(&t,true);
    s->complete=ok && s->cleanup_complete && !s->failed && owned(&t,false) && owned(&t,true);
    return s->complete;
}

bool mt76_channel_tune_run(struct mt76_channel_tune *s,const uint8_t country[2],
    unsigned channel,unsigned bandwidth,const struct mt76_channel_tune_io *io) {
    return tune_run(s,country,channel,bandwidth,io,0);
}
bool mt76_channel_tune_observe_run(struct mt76_channel_tune *s,
    const uint8_t country[2],unsigned channel,unsigned bandwidth,
    const struct mt76_channel_tune_io *io,const struct mt76_channel_tune_observer *o) {
    if(!o || !o->observe)return false;
    const struct mt76_channel_tune_observer admitted=*o;
    return tune_run(s,country,channel,bandwidth,io,&admitted);
}
