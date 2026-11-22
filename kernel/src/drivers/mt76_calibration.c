#include <srvros/mt76_calibration.h>

static const uint32_t saved_address[2] = {0x13b0, 0x2124};

static bool authorized(struct mt76_calibration *s,
                       const struct mt76_calibration_io *io) {
    if (io->authorize(io->context)) return true;
    s->failure = MT76_CAL_REFUSED;
    return false;
}
static bool read_word(struct mt76_calibration *s,
                      const struct mt76_calibration_io *io,
                      uint32_t address, uint32_t *value) {
    if (!authorized(s, io)) return false;
    ++s->reads;
    if (!io->read(io->context, address, value) || *value == UINT32_MAX) {
        s->failure = MT76_CAL_IO;
        return false;
    }
    return true;
}
static bool guards(struct mt76_calibration *s,
                   const struct mt76_calibration_io *io) {
    static const uint32_t a[3] = {0x1004, 0x2000, 0x208};
    static const uint32_t v[3] = {12, 0xf000f200, 0x40000050};
    for (unsigned i = 0; i < 3; ++i) {
        uint32_t value = 0;
        if (!read_word(s, io, a[i], &value)) return false;
        if (value != v[i]) { s->failure = MT76_CAL_REFUSED; return false; }
    }
    return true;
}
static bool temporary(struct mt76_calibration *s,
                      const struct mt76_calibration_io *io,
                      unsigned index, uint32_t value) {
    if (!authorized(s, io)) return false;
    /* A failed write can still have changed the device. Mark before calling. */
    s->modified_mask |= 1u << index;
    ++s->writes;
    if (!io->write(io->context, saved_address[index], value)) {
        s->failure = MT76_CAL_IO; return false;
    }
    uint32_t observed = 0;
    if (!read_word(s, io, saved_address[index], &observed)) return false;
    if (observed != value) { s->failure = MT76_CAL_IO; return false; }
    return true;
}
static bool wait_minimum(struct mt76_calibration *s,
                         const struct mt76_calibration_io *io, unsigned us) {
    if (!authorized(s, io)) return false;
    ++s->waits;
    if (io->delay(io->context, us)) return true;
    s->failure = MT76_CAL_DELAY; return false;
}
static bool command(struct mt76_calibration *s,
                     const struct mt76_calibration_io *io,
                     enum mt76_calibration_command cmd, uint32_t value) {
    if (!authorized(s, io)) return false;
    ++s->commands;
    if (io->command(io->context, cmd, value)) return true;
    s->failure = MT76_CAL_COMMAND; return false;
}
static bool restore(struct mt76_calibration *s,
                     const struct mt76_calibration_io *io) {
    /* Reverse reference write order. Try the second restoration even if the
     * first fails. These are saved-word cleanup calls, not forward operations. */
    for (unsigned n = 2; n; --n) {
        unsigned i = n - 1, bit = 1u << i;
        if (!(s->modified_mask & bit)) continue;
        s->restore_attempted |= bit;
        s->restored[i] = UINT32_MAX;
        if (io->restore(io->context, saved_address[i], s->saved[i],
                        &s->restored[i]) && s->restored[i] == s->saved[i])
            s->restored_mask |= bit;
    }
    s->cleanup_complete = s->restored_mask == s->modified_mask;
    return s->cleanup_complete;
}
enum mt76_calibration_status mt76_calibration_run(struct mt76_calibration *s,
    const struct mt76_calibration_io *io, unsigned channel, unsigned bandwidth) {
    if (!s || !io || !io->authorize || !io->read || !io->write ||
        !io->command || !io->delay || !io->restore || channel != 1 ||
        bandwidth != 20 || s->attempted) return MT76_CAL_REFUSED;
    *s = (struct mt76_calibration){.attempted = true};
    bool ok = guards(s, io);
    /* Capture BOTH saved words before the first temporary write. Refuse
     * unknown/all-ones values, rather than inheriting an observation policy. */
    for (unsigned i = 0; ok && i < 2; ++i) {
        ok = read_word(s, io, saved_address[i], &s->saved[i]);
        if (ok) s->saved_mask |= 1u << i;
    }
    ok = ok && temporary(s, io, 0, 0) && wait_minimum(s, io, 500) &&
         temporary(s, io, 1, 0xffffff7e) &&
         command(s, io, MT76_CAL_FULL, 0x600) &&
         command(s, io, MT76_CAL_LC, 0) && wait_minimum(s, io, 15000);
    if (!restore(s, io)) return MT76_CAL_RESTORE;
    if (!ok) return s->failure;
    ok = guards(s, io) && command(s, io, MT76_CAL_RXDCOC, 1) &&
         guards(s, io) && authorized(s, io);
    s->complete = ok;
    return ok ? MT76_CAL_OK : s->failure;
}

static bool probe_owned(struct mt76_calibration_probe *s,const struct mt76_calibration_io *io) {
    if(io->authorize(io->context))return true;
    s->failure=MT76_CAL_REFUSED;return false;
}
static bool probe_read(struct mt76_calibration_probe *s,const struct mt76_calibration_io *io,
                       uint32_t a,uint32_t *v) {
    *v=0;if(!probe_owned(s,io))return false;
    s->reads++;
    if(io->read(io->context,a,v) && (a==0x2124 || *v!=UINT32_MAX))return true;
    s->failure=MT76_CAL_IO;return false;
}
static bool probe_exact(struct mt76_calibration_probe *s,const struct mt76_calibration_io *io,
                        uint32_t a,uint32_t v) {
    uint32_t observed;
    if(!probe_read(s,io,a,&observed))return false;
    if(observed==v)return true;
    s->failure=MT76_CAL_REFUSED;return false;
}
static bool probe_guards(struct mt76_calibration_probe *s,const struct mt76_calibration_io *io) {
    return probe_exact(s,io,0x1004,12) && probe_exact(s,io,0x2000,0xf000f200) &&
           probe_exact(s,io,0x208,0x40000050);
}
static bool probe_write(struct mt76_calibration_probe *s,const struct mt76_calibration_io *io,
                        unsigned index,uint32_t v) {
    if(!probe_owned(s,io))return false;
    s->modified_mask|=1u<<index;s->writes++;
    if(io->write(io->context,saved_address[index],v))return true;
    s->failure=MT76_CAL_IO;return false;
}
static bool probe_delay(struct mt76_calibration_probe *s,const struct mt76_calibration_io *io) {
    if(!probe_owned(s,io))return false;
    s->waits++;
    if(io->delay(io->context,500))return true;
    s->failure=MT76_CAL_DELAY;return false;
}
enum mt76_calibration_status mt76_calibration_probe_run(
    struct mt76_calibration_probe *s,const struct mt76_calibration_io *io) {
    if(!s || !io || !io->authorize || !io->read || !io->write || !io->delay || !io->restore || s->attempted)return MT76_CAL_REFUSED;
    *s=(struct mt76_calibration_probe){.attempted=true};
    bool ok=probe_guards(s,io) && probe_read(s,io,0x13b0,&s->saved[0]);
    if(ok){s->saved_mask=1;if(s->saved[0]!=0x2f2f1b1b){s->failure=MT76_CAL_REFUSED;ok=false;}}
    ok=ok && probe_write(s,io,0,0) && probe_exact(s,io,0x13b0,0) && probe_delay(s,io) &&
       probe_read(s,io,0x2124,&s->saved[1]);
    if(ok)s->saved_mask|=2;
    ok=ok && probe_write(s,io,1,0xffffff7e) && probe_read(s,io,0x2124,&s->observed_ibi);
    if(ok){s->ibi_write_matched=s->observed_ibi==0xffffff7e;s->ibi_changed=s->observed_ibi!=s->saved[1];}
    /* Restore even an ambiguously failed write. Cleanup is independently owned
     * by the adapter and is never cancelled by forward authorization loss. */
    for(unsigned n=2;n;n--){unsigned i=n-1,bit=1u<<i;
        if(!(s->modified_mask&bit))continue;
        s->restore_attempted|=bit;s->restored[i]=0;
        if(io->restore(io->context,saved_address[i],s->saved[i],&s->restored[i]) && s->restored[i]==s->saved[i])s->raw_restore_mask|=bit;
    }
    if(s->raw_restore_mask!=s->modified_mask)return MT76_CAL_RESTORE;
    if(!ok)return s->failure;
    ok=probe_guards(s,io) && probe_owned(s,io);s->complete=ok;
    return ok?MT76_CAL_OK:s->failure;
}
