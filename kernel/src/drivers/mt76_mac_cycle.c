#include <srvros/mt76_mac_cycle.h>

static bool read_word(struct mt76_mac_cycle *s,const struct mt76_mac_cycle_io *io,
                      uint32_t a,uint32_t *v,bool cleanup,bool raw) {
    *v=0;
    if(!io->owned(io->context,cleanup)){s->failed=true;return false;}
    s->operations++;s->reads++;
    bool ok=io->read(io->context,a,v,cleanup) && (raw || *v!=UINT32_MAX);
    if(!ok)s->failed=true;
    return ok;
}
static bool write_word(struct mt76_mac_cycle *s,const struct mt76_mac_cycle_io *io,
                       uint32_t a,uint32_t v,bool cleanup) {
    if(!io->owned(io->context,cleanup)){s->failed=true;return false;}
    s->operations++;s->writes++;
    if(!cleanup && a==0x1004)s->mac_touched=true;
    if(a==0x1400)s->filter_touched=true;
    if(a==0x1340)s->txop_touched=true;
    bool ok=io->write(io->context,a,v,cleanup);
    if(!ok)s->failed=true;
    return ok;
}
static bool exact(struct mt76_mac_cycle *s,const struct mt76_mac_cycle_io *io,
                  uint32_t a,uint32_t v,bool cleanup) {
    uint32_t seen;
    bool ok=read_word(s,io,a,&seen,cleanup,false) && seen==v;
    if(!ok)s->failed=true;
    return ok;
}
static bool put(struct mt76_mac_cycle *s,const struct mt76_mac_cycle_io *io,
                uint32_t a,uint32_t v,bool cleanup) {
    /* Always attempt independent readback after an ambiguous write failure. */
    bool written=write_word(s,io,a,v,cleanup);
    bool verified=exact(s,io,a,v,cleanup);
    return written && verified;
}
static bool delay(struct mt76_mac_cycle *s,const struct mt76_mac_cycle_io *io,
                  unsigned us,bool cleanup) {
    if(!io->owned(io->context,cleanup)){s->failed=true;return false;}
    s->operations++;s->waits++;
    bool ok=io->delay(io->context,us,cleanup);
    if(!ok)s->failed=true;
    return ok;
}
static bool guards(struct mt76_mac_cycle *s,const struct mt76_mac_cycle_io *io,
                   bool cleanup) {
    return exact(s,io,0x1004,0,cleanup) && exact(s,io,0x2000,0xf000f200,cleanup) &&
           exact(s,io,0x208,0x40000050,cleanup);
}
static bool poll_clear(struct mt76_mac_cycle *s,const struct mt76_mac_cycle_io *io,
                       uint32_t a,uint32_t mask,unsigned budget,bool cleanup) {
    uint64_t start=io->now(io->context),previous=start;
    for(unsigned n=0;n<2048;n++) {
        if(n){uint64_t now=io->now(io->context);
            if(now<previous || now-start>=budget)break;
            previous=now;}
        uint32_t v;
        if(!read_word(s,io,a,&v,cleanup,false))return false;
        if(!(v&mask))return true;
        uint64_t now=io->now(io->context);
        if(now<previous || now-start>=budget)break;
        previous=now;
        if(!delay(s,io,50,cleanup))return false;
    }
    s->failed=true;return false;
}
static bool tx_drain(struct mt76_mac_cycle *s,const struct mt76_mac_cycle_io *io) {
    for(unsigned n=0;n<200;n++) {
        uint32_t a,b,c;s->tx_rounds++;
        if(!read_word(s,io,0x438,&a,true,false) || !read_word(s,io,0xa30,&b,true,false) ||
           !read_word(s,io,0xa34,&c,true,false))return false;
        if(!a && !(b&255) && !(c&0xff00ff))return true;
        if(!delay(s,io,10000,true))return false;
    }
    s->failed=true;return false;
}
static bool rx_drain(struct mt76_mac_cycle *s,const struct mt76_mac_cycle_io *io) {
    unsigned quiet=0;
    for(unsigned n=0;n<200;n++) {
        uint32_t a,b,c;s->rx_rounds++;
        if(!read_word(s,io,0x430,&a,true,false) || !read_word(s,io,0xa30,&b,true,false) ||
           !read_word(s,io,0xa34,&c,true,false))return false;
        if(!(a&0xff0000) && !b && !c){if(++quiet==7)return true;}
        else {quiet=0;if(!delay(s,io,1000,true))return false;}
    }
    s->failed=true;return false;
}
bool mt76_mac_cycle_run(struct mt76_mac_cycle *s,const struct mt76_mac_cycle_io *io) {
    if(!s || !io || !io->owned || !io->read || !io->write || !io->delay || !io->now || s->attempted)return false;
    *s=(struct mt76_mac_cycle){.attempted=true};
    bool ok=guards(s,io,false) && read_word(s,io,0x1400,&s->filter,false,false) && s->filter==0x17f97 &&
            read_word(s,io,0x1340,&s->txop,false,false);
    if(ok)ok=put(s,io,0x1004,4,false) && poll_clear(s,io,0x208,10,200000,false) &&
             put(s,io,0x1400,s->filter,false) && put(s,io,0x1004,12,false) &&
             poll_clear(s,io,0x208,10,50,false);
    s->started=ok;
    if(ok) {
        ok=exact(s,io,0x2000,0xf000f200,false) &&
           read_word(s,io,0x13b0,&s->tx_alc,false,false) && read_word(s,io,0x2124,&s->ibi9,false,true);
        s->snapshot_complete=ok;
        if(ok && io->inspect)ok=io->inspect(io->context,s->tx_alc,s->ibi9);
        if(ok){uint64_t start=io->now(io->context);ok=delay(s,io,10000,false);uint64_t end=io->now(io->context);
            s->hold_us=end>=start?end-start:0;ok=ok && s->hold_us>=10000;}
    }
    /* Once MAC may have changed, forward cancellation never skips teardown.
     * Each independent cleanup step is attempted despite earlier failures. */
    if(s->mac_touched) {
        bool step=put(s,io,0x1340,s->txop&~(1u<<20),true);ok=step && ok;
        s->tx_drained=tx_drain(s,io);ok=s->tx_drained && ok;
        step=poll_clear(s,io,0x1200,1,1000,true);ok=step && ok;
        s->mac_disabled=put(s,io,0x1004,0,true);ok=s->mac_disabled && ok;
        s->rx_drained=rx_drain(s,io);ok=s->rx_drained && ok;
        step=poll_clear(s,io,0x1200,2,1000,true);ok=step && ok;
        if(s->filter_touched){s->filter_restored=put(s,io,0x1400,s->filter,true);ok=s->filter_restored && ok;}
        if(s->txop_touched){s->txop_restored=put(s,io,0x1340,s->txop,true);ok=s->txop_restored && ok;}
        step=guards(s,io,true);ok=step && ok;
    }
    s->complete=ok && !s->failed && io->owned(io->context,false);
    if(!s->complete)s->failed=true;
    return s->complete;
}
