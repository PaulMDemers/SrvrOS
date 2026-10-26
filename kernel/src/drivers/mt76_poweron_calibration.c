#include <srvros/mt76_poweron_calibration.h>

struct transaction {
    struct mt76_calibration *s;
    const struct mt76_poweron_io *io;
    const struct mt76_identity *identity;
    const struct mt76_ant_fields *board;
};
static bool profile(const struct mt76_identity *id,const struct mt76_ant_fields *b) {
    return id && id->asic==0x76100002 && id->mac==0x76502000 &&
           mt76_ant_fields_valid(b) && !(b->nic1 & (1u<<13));
}
static bool owned(struct transaction *t) {
    if(profile(t->identity,t->board) && t->io->authorize(t->io->context) &&
       profile(t->identity,t->board))return true;
    t->s->failure=MT76_CAL_REFUSED;return false;
}
static bool exact(struct transaction *t,uint32_t address,uint32_t expected,uint32_t *saved) {
    if(!owned(t))return false;
    uint32_t value=0;t->s->reads++;
    if(!t->io->read(t->io->context,address,&value)) {
        t->s->failure=MT76_CAL_IO;return false;
    }
    /* All ones allowed only as the exact captured IBI9 saved-word profile. */
    if(value!=expected){t->s->failure=MT76_CAL_REFUSED;return false;}
    if(saved)*saved=value;
    return true;
}
static bool guards(struct transaction *t) {
    return exact(t,0x1004,12,0) && exact(t,0x2000,0xf000f200,0) &&
           exact(t,0x208,0x40000050,0);
}
static bool command(struct transaction *t,enum mt76_poweron_command cmd,uint32_t value) {
    if(!owned(t))return false;
    t->s->commands++;
    if(t->io->command(t->io->context,cmd,value))return true;
    t->s->failure=MT76_CAL_COMMAND;return false;
}
static bool delay(struct transaction *t,unsigned us) {
    if(!owned(t))return false;
    t->s->waits++;
    if(t->io->delay(t->io->context,us))return true;
    t->s->failure=MT76_CAL_DELAY;return false;
}
static bool temporary(struct transaction *t,unsigned index,uint32_t value) {
    static const uint32_t address[2]={0x13b0,0x2124};
    if(!owned(t))return false;
    /* A reported failed write may have applied. Cleanup cannot depend on ACK. */
    t->s->modified_mask|=1u<<index;t->s->writes++;
    if(!t->io->write(t->io->context,address[index],value)) {
        t->s->failure=MT76_CAL_IO;return false;
    }
    return exact(t,address[index],value,0);
}
static bool restore(struct transaction *t) {
    static const uint32_t address[2]={0x13b0,0x2124};
    for(unsigned n=2;n;n--) {
        unsigned i=n-1,bit=1u<<i;
        if(!(t->s->modified_mask&bit))continue;
        t->s->restore_attempted|=bit;
        /* Do not compare a placeholder if the callback refused/failed. */
        t->s->restored[i]=0;
        if(t->io->restore(t->io->context,address[i],t->s->saved[i],&t->s->restored[i]) &&
           t->s->restored[i]==t->s->saved[i])t->s->restored_mask|=bit;
    }
    t->s->cleanup_complete=t->s->restored_mask==t->s->modified_mask;
    return t->s->cleanup_complete;
}
enum mt76_calibration_status mt76_poweron_calibration_run(
    struct mt76_calibration *s,const struct mt76_poweron_io *io,
    const struct mt76_identity *id,const struct mt76_ant_fields *board,
    unsigned channel,unsigned bandwidth) {
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->write ||
       !io->command || !io->delay || !io->restore || channel!=1 || bandwidth!=20 ||
       !profile(id,board))return MT76_CAL_REFUSED;
    *s=(struct mt76_calibration){.attempted=true};
    struct transaction t={s,io,id,board};
    bool ok=guards(&t) && command(&t,MT76_POWERON_R,0) &&
            command(&t,MT76_POWERON_VCO,1) && delay(&t,10) && guards(&t) &&
            exact(&t,0x13b0,0x2f2f1b1b,&s->saved[0]);
    if(ok)s->saved_mask=1;
    ok=ok && temporary(&t,0,0) && delay(&t,500) &&
       exact(&t,0x2124,0xffffffff,&s->saved[1]);
    if(ok)s->saved_mask|=2;
    ok=ok && temporary(&t,1,0xffffff7e) &&
       command(&t,MT76_POWERON_FULL,0x600) &&
       command(&t,MT76_POWERON_LC,0) && delay(&t,15000);
    if(!restore(&t))return MT76_CAL_RESTORE;
    if(!ok)return s->failure;
    ok=guards(&t) && command(&t,MT76_POWERON_RXDCOC,1) && guards(&t) && owned(&t);
    s->complete=ok;
    return ok?MT76_CAL_OK:s->failure;
}
