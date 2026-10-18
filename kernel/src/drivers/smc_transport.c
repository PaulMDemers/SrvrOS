#include <srvros/smc_transport.h>
static int fail(struct smc_transport *t,enum smc_error e) {
    t->failed_phase=t->phase;t->phase=SMC_FAILED;t->error=e;t->poisoned=true;return -1;
}
void smc_transport_init(struct smc_transport *t,struct smc_io io) { *t=(struct smc_transport){.io=io}; }
bool smc_transport_begin(struct smc_transport *t,bool metadata,const char key[4],unsigned bytes,uint64_t now) {
    if(!t || !key || t->poisoned || (t->phase!=SMC_IDLE && t->phase!=SMC_DONE) || !bytes || bytes>32 ||
       (metadata && bytes!=6) || !t->io.status || !t->io.read || !t->io.command || !t->io.argument) return false;
    for(unsigned i=0;i<4;i++) if(key[i]<32 || key[i]>126) return false;
    t->phase=SMC_START;t->failed_phase=SMC_IDLE;t->error=SMC_OK;t->start_us=t->last_us=now;
    t->command=metadata?0x13:0x10;t->length=bytes;t->index=t->received=t->status=0;t->polls=0;
    for(unsigned i=0;i<4;i++)t->key[i]=key[i];
    for(unsigned i=0;i<32;i++)t->result[i]=0;
    return true;
}
int smc_transport_poll(struct smc_transport *t,uint64_t now) {
    if(!t || t->phase==SMC_IDLE || t->phase==SMC_FAILED) return -1;
    if(t->phase==SMC_DONE)return 1;
    if(now<t->last_us)return fail(t,SMC_CLOCK);
    t->last_us=now;
    if(now-t->start_us>=500000)return fail(t,SMC_DEADLINE);
    if(++t->polls>4096)return fail(t,SMC_POLL_LIMIT);
    uint8_t s=t->status=t->io.status(t->io.context);
    switch(t->phase) {
    case SMC_START:
        if(s&1)return fail(t,SMC_EXTRA_DATA);
        if(!(s&6)) { t->io.command(t->io.context,t->command);t->phase=SMC_INPUT_OPEN; }
        break;
    case SMC_INPUT_OPEN:
        if((s&1) && t->index<4)return fail(t,SMC_EXTRA_DATA);
        if(!(s&2))t->phase=SMC_INPUT_BUSY;
        break;
    case SMC_INPUT_BUSY:
        if((s&1) && t->index<4)return fail(t,SMC_EXTRA_DATA);
        if(s&2)t->phase=SMC_INPUT_OPEN;
        else if(s&4) {
            t->io.argument(t->io.context,t->index<4?t->key[t->index]:t->length);
            t->index++;t->phase=t->index==5?SMC_RESULT:SMC_INPUT_OPEN;
        }
        break;
    case SMC_RESULT:
        if((s&5)==5) {
            t->result[t->received++]=t->io.read(t->io.context);
            if(t->received==t->length)t->phase=SMC_FINISH;
        } else if(!(s&7))return fail(t,SMC_NO_RESULT);
        break;
    case SMC_FINISH:
        if(s&1)return fail(t,SMC_EXTRA_DATA);
        if(!(s&6)) { t->phase=SMC_DONE;return 1; }
        break;
    default:return -1;
    }
    return 0;
}
void smc_transport_cancel(struct smc_transport *t) { if(t && t->phase!=SMC_IDLE && t->phase!=SMC_DONE && t->phase!=SMC_FAILED)fail(t,SMC_CANCELED); }
uint32_t smc_fan_millirpm(const uint8_t b[2]) { return ((uint32_t)b[0]*256+b[1])*250; }
