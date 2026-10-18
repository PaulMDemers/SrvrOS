#include <srvros/ec_transport.h>
#define EC_OBF 1u
#define EC_IBF 2u
#define EC_MAX_POLLS 4096u
static int fail(struct ec_transport *t,enum ec_error error) {
    t->failed_phase=t->phase;t->phase=EC_FAILED;t->error=error;t->poisoned=true;return -1;
}
void ec_transport_init(struct ec_transport *t,struct ec_io io) {
    *t=(struct ec_transport){.io=io};
}
bool ec_transport_begin(struct ec_transport *t,bool write,uint8_t address,uint8_t value,
    uint64_t now,uint64_t timeout) {
    if(!t || t->poisoned || (t->phase!=EC_IDLE && t->phase!=EC_DONE) || !timeout || timeout>1000000 ||
        !t->io.status || !t->io.read_data || !t->io.write_command || !t->io.write_data) return false;
    t->phase=EC_COMMAND;t->failed_phase=EC_IDLE;t->error=EC_OK;t->write=write;
    t->address=address;t->value=value;t->result=0;t->last_status=0;t->polls=0;
    t->started_us=t->last_us=now;t->timeout_us=timeout;return true;
}
int ec_transport_poll(struct ec_transport *t,uint64_t now) {
    if(!t || t->phase==EC_IDLE || t->phase==EC_FAILED) return -1;
    if(t->phase==EC_DONE) return 1;
    if(now<t->last_us) return fail(t,EC_CLOCK);
    t->last_us=now;
    if(now-t->started_us>=t->timeout_us) return fail(t,EC_TIMEOUT);
    if(++t->polls>EC_MAX_POLLS) return fail(t,EC_POLL_LIMIT);
    uint8_t status=t->last_status=t->io.status(t->io.context);
    /* An output byte before the read-result phase belongs to another or a
     * desynchronized transaction. Do not drain it and pretend synchronization. */
    if((status&EC_OBF) && t->phase!=EC_READ_RESULT) return fail(t,EC_UNEXPECTED_OUTPUT);
    if(status&EC_IBF) return 0;
    switch(t->phase) {
    case EC_COMMAND:
        t->io.write_command(t->io.context,t->write?0x81:0x80);t->phase=EC_ADDRESS;break;
    case EC_ADDRESS:
        t->io.write_data(t->io.context,t->address);t->phase=t->write?EC_VALUE:EC_READ_RESULT;break;
    case EC_VALUE:
        t->io.write_data(t->io.context,t->value);t->phase=EC_WRITE_RESULT;break;
    case EC_READ_RESULT:
        if(!(status&EC_OBF)) return 0;
        t->result=t->io.read_data(t->io.context);t->phase=EC_DONE;return 1;
    case EC_WRITE_RESULT: t->phase=EC_DONE;return 1;
    default: return -1;
    }
    return 0;
}
void ec_transport_cancel(struct ec_transport *t) {
    if(t && t->phase!=EC_IDLE && t->phase!=EC_DONE && t->phase!=EC_FAILED) fail(t,EC_CANCELED);
}
